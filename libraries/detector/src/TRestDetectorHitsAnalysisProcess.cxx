/// \class TRestDetectorHitsAnalysisProcess
/// Computes observables of the hits of a TRestDetectorHitsEvent, for any readout. The event is not
/// modified; events without hits are rejected.
///
/// The type of each hit (TRestHitsData::REST_HitType: XZ = 10, YZ = 15, XYZ = 30, ...) gives the
/// coordinates it defines; each coordinate observable uses the hits that define it, and is NaN if no hit
/// defines it.
///
/// Observables:
/// * NHits, Energy; MaxHitEnergy, MinHitEnergy, MeanHitEnergy.
/// * MeanX/Y/Z: energy weighted mean position; SigmaX/Y/Z: energy weighted size (TRestHits::GetSigmaX,
///   ...); SkewXY, SkewZ: skewness (TRestHits::GetSkewXY, GetSkewZ).
///
/// For each projection P in `projections` (hit types, e.g. [XZ, YZ] for strip readouts):
/// * NHits_P, Energy_P: number of hits and energy of type P.
/// * For each pair P1, P2 (in the given order): NHitsBalance_P1_P2, EnergyBalance_P1_P2 and
///   SigmaBalance_P1_P2, as (a - b) / (a + b) of the number of hits, the energies and the sigmas of the
///   coordinate only each one defines (X of XZ and Y of YZ, from the hits of each type; NaN if there is not
///   exactly one). 0 if a + b = 0.
///
/// Legacy observables (strips with projections [XZ, YZ]): nHits → NHits, nHitsX/Y → NHits_XZ/YZ,
/// balanceXYnHits → NHitsBalance_XZ_YZ, energy → Energy, energyX/Y → Energy_XZ/YZ (legacy: the hits
/// defining X or Y, also XYZ), balanceXYenergy → EnergyBalance_XZ_YZ, maxHitEnergy, minHitEnergy and
/// meanHitEnergy → MaxHitEnergy, MinHitEnergy, MeanHitEnergy, xMean → MeanX, xSigma → SigmaX, xy2Sigma →
/// SigmaX² + SigmaY², z2Sigma → SigmaZ², xySigmaBalance → SigmaBalance_XZ_YZ, xySkew → SkewXY, zSkew →
/// SkewZ. Removed: nHitsSizeXY (sqrt(NHits_XZ² + NHits_YZ²)), meanHitEnergyBalance (1 / NHits) and the
/// fiducial cylinder and prism observables (see below).
///
/// Example (strip readout):
///
///    hitsAnalysis:
///      class: "TRestDetectorHitsAnalysisProcess"
///      projections: ["XZ", "YZ"]
///      observables:
///        - "Energy"
///        - "NHits_XZ"
///        - "NHits_YZ"
///        - "EnergyBalance_XZ_YZ"
///        - "SigmaZ"
///
/// Migrated from the legacy detector library (Javier Galan), redesigned to be readout independent: the
/// legacy X and Y observables are given by the projections. Veto hits are not excluded (there is no veto
/// hit type, veto hits go in their own pipeline). The fiducial cylinder and prism (cylinderFiducialization,
/// prismFiducialization) are not migrated: they only worked with XYZ hits (simulations, where
/// TRestDetectorHitsToSignalProcess already rejects the hits out of the drift volume or the readout), and
/// the selection of events by position is a TRestCut on MeanX/Y/Z, e.g.
/// `MeanX * MeanX + MeanY * MeanY < 2500`.

#include "TRestDetectorHitsAnalysisProcess.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

#include "TRestDetectorHitsEvent.h"
#include "TRestRun.h"
#include "TRestTools.h"

static const bool TRestDetectorHitsAnalysisProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorHitsAnalysisProcess;
    reg.RegisterField<P>("projections", &P::fProjections);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorHitsAnalysisProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorHitsAnalysisProcess>(instanceName, params);
        });
    return true;
}();

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

double Balance(double a, double b) { return a + b != 0 ? (a - b) / (a + b) : 0; }

// Energy weighted sigma of one coordinate of the hits of a type (NaN without hits or energy)
double GetSigma(const TRestHits& hits, int type, int axis) {
    double sum = 0, sum2 = 0, energy = 0;
    for (size_t n = 0; n < hits.GetNumberOfHits(); n++) {
        if (hits.GetType(n) != type) continue;
        const double e = hits.GetEnergy(n);
        const double c = hits.GetCoordinate(n, axis);
        sum += e * c;
        sum2 += e * c * c;
        energy += e;
    }
    if (energy <= 0) return kNaN;
    const double mean = sum / energy;
    return std::sqrt(std::max(sum2 / energy - mean * mean, 0.));
}
}  // namespace

TRestDetectorHitsAnalysisProcess::TRestDetectorHitsAnalysisProcess() : TRestEventProcess() {
    fName = "TRestDetectorHitsAnalysisProcess";
}

TRestDetectorHitsAnalysisProcess::TRestDetectorHitsAnalysisProcess(const std::string& instanceName,
                                                                   const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestDetectorHitsAnalysisProcess::TRestDetectorHitsAnalysisProcess(const std::string& fileName,
                                                                   const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorHitsAnalysisProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestDetectorHitsAnalysisProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestDetectorHitsAnalysisProcess>(fNode);
    for (auto& projection : fProjections) {
        projection = TRestTools::NormalizeOption("projections", projection, TRestHitsData::kTypeNames);
    }
    UpdateYAMLFromParams<TRestDetectorHitsAnalysisProcess>(fNode);
}

void TRestDetectorHitsAnalysisProcess::InitProcess() {
    // Projections and their pairs; the vectors do not change after this, so the registered references
    // stay valid
    const std::map<std::string, int> hitTypes = {{"X", TRestHitsData::X},    {"Y", TRestHitsData::Y},
                                                 {"Z", TRestHitsData::Z},    {"XY", TRestHitsData::XY},
                                                 {"XZ", TRestHitsData::XZ},  {"YZ", TRestHitsData::YZ},
                                                 {"XYZ", TRestHitsData::XYZ}};
    fProjectionObs.clear();
    for (const auto& name : fProjections) {
        auto it = hitTypes.find(name);
        if (it == hitTypes.end()) {
            throw std::runtime_error("TRestDetectorHitsAnalysisProcess: unknown projection '" + name +
                                     "' (X, Y, Z, XY, XZ, YZ or XYZ)");
        }
        ProjectionObservables projection;
        projection.name = name;
        projection.type = it->second;
        fProjectionObs.push_back(projection);
    }

    fBalances.clear();
    for (size_t i = 0; i < fProjectionObs.size(); i++) {
        for (size_t j = i + 1; j < fProjectionObs.size(); j++) {
            BalanceObservables balance;
            balance.first = i;
            balance.second = j;
            // The coordinate only each projection defines, if there is exactly one
            int nFirst = 0, nSecond = 0;
            for (int a = 0; a < 3; a++) {
                const bool inFirst = TRestHitsData::IsDefined(fProjectionObs[i].type, a);
                const bool inSecond = TRestHitsData::IsDefined(fProjectionObs[j].type, a);
                if (inFirst && !inSecond) {
                    balance.firstAxis = a;
                    nFirst++;
                }
                if (inSecond && !inFirst) {
                    balance.secondAxis = a;
                    nSecond++;
                }
            }
            if (nFirst != 1 || nSecond != 1) balance.firstAxis = balance.secondAxis = -1;
            fBalances.push_back(balance);
        }
    }

    RegisterObservable("NHits", fNHits);
    RegisterObservable("Energy", fEnergy);
    RegisterObservable("MaxHitEnergy", fMaxHitEnergy);
    RegisterObservable("MinHitEnergy", fMinHitEnergy);
    RegisterObservable("MeanHitEnergy", fMeanHitEnergy);
    RegisterObservable("MeanX", fMeanX);
    RegisterObservable("MeanY", fMeanY);
    RegisterObservable("MeanZ", fMeanZ);
    RegisterObservable("SigmaX", fSigmaX);
    RegisterObservable("SigmaY", fSigmaY);
    RegisterObservable("SigmaZ", fSigmaZ);
    RegisterObservable("SkewXY", fSkewXY);
    RegisterObservable("SkewZ", fSkewZ);

    for (auto& projection : fProjectionObs) {
        RegisterObservable("NHits_" + projection.name, projection.nHits);
        RegisterObservable("Energy_" + projection.name, projection.energy);
    }
    for (auto& balance : fBalances) {
        const std::string pair =
            fProjectionObs[balance.first].name + "_" + fProjectionObs[balance.second].name;
        RegisterObservable("NHitsBalance_" + pair, balance.nHits);
        RegisterObservable("EnergyBalance_" + pair, balance.energy);
        RegisterObservable("SigmaBalance_" + pair, balance.sigma);
    }
}

bool TRestDetectorHitsAnalysisProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    output.CopyFrom(&input);
    const auto* event = dynamic_cast<const TRestDetectorHitsEvent*>(&output);
    if (!event) {
        throw std::runtime_error("TRestDetectorHitsAnalysisProcess: output is not a TRestDetectorHitsEvent");
    }

    const TRestHits& hits = *event->GetHits();
    // Assignments keep the addresses of the registered members
    fNHits = hits.GetNumberOfHits();
    if (fNHits == 0) return false;

    fEnergy = hits.GetTotalEnergy();
    fMaxHitEnergy = hits.GetMaximumHitEnergy();
    fMinHitEnergy = hits.GetMinimumHitEnergy();
    fMeanHitEnergy = hits.GetMeanHitEnergy();

    // Coordinates any hit defines
    bool defined[3] = {false, false, false};
    for (size_t n = 0; n < hits.GetNumberOfHits(); n++) {
        for (int a = 0; a < 3; a++) defined[a] |= TRestHitsData::IsDefined(hits.GetType(n), a);
    }
    fMeanX = defined[0] ? hits.GetMeanPositionX() : kNaN;
    fMeanY = defined[1] ? hits.GetMeanPositionY() : kNaN;
    fMeanZ = defined[2] ? hits.GetMeanPositionZ() : kNaN;
    fSigmaX = defined[0] ? hits.GetSigmaX() : kNaN;
    fSigmaY = defined[1] ? hits.GetSigmaY() : kNaN;
    fSigmaZ = defined[2] ? hits.GetSigmaZ() : kNaN;
    fSkewXY = defined[0] || defined[1] ? hits.GetSkewXY() : kNaN;
    fSkewZ = defined[2] ? hits.GetSkewZ() : kNaN;

    for (auto& projection : fProjectionObs) {
        projection.nHits = 0;
        projection.energy = 0;
        for (size_t n = 0; n < hits.GetNumberOfHits(); n++) {
            if (hits.GetType(n) != projection.type) continue;
            projection.nHits++;
            projection.energy += hits.GetEnergy(n);
        }
        for (int a = 0; a < 3; a++) {
            projection.sigma[a] =
                TRestHitsData::IsDefined(projection.type, a) ? GetSigma(hits, projection.type, a) : kNaN;
        }
    }
    for (auto& balance : fBalances) {
        const auto& first = fProjectionObs[balance.first];
        const auto& second = fProjectionObs[balance.second];
        balance.nHits = Balance(first.nHits, second.nHits);
        balance.energy = Balance(first.energy, second.energy);
        balance.sigma = balance.firstAxis < 0
                            ? kNaN
                            : Balance(first.sigma[balance.firstAxis], second.sigma[balance.secondAxis]);
    }

    RESTDebug << "TRestDetectorHitsAnalysisProcess: event " << event->GetID() << ", " << fNHits
              << " hits, energy " << fEnergy << RESTendl;
    return true;
}
