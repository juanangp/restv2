/// \class TRestDetectorHitsDriftProcess
/// Converts the energy of each hit into ionization electrons (energy / wValue of the medium, with the
/// chosen fluctuations) and diffuses each electron with the TRestDetectorDrift (gaussian diffusion
/// and attachment, from the YAML or a Magboltz transport table). Each surviving electron becomes a hit
/// with the energy of the original hit divided by its number of electrons, the diffused position and the
/// time of the original hit: only the position changes, the drift time is added when converting to
/// signals.
///
/// The drift distance and its direction are given by TRestDetectorReadout::GetDistanceToReadout (e.g. the
/// Micromegas plane and its normal). Hits without a distance or outside the drift volume (distance < 0 or
/// beyond TRestDetectorReadout::GetMaxDistanceToReadout, the cathode) are removed. Diffused electrons are
/// kept between the readout plane and the cathode, as legacy.
///
/// Differences with legacy: the attachment is exp(-attachment * distance) with attachment in 1/cm (as
/// Garfield), the gas parameters come from the medium instead of the process, there are no veto planes
/// (veto hits are split beforehand) and there is no microscopic Garfield drift: in a uniform field it uses
/// the same transport table as the gaussian diffusion (microscopic simulations are done with restGarfield).
///
/// Parameters: readout and driftMedium (names, optional if there is only one), fluctuations ("fano",
/// "poisson" or "none"), maxHits (1000, 0: no limit), seed (0).
///
/// Example:
///
///    hitsDrift:
///      class: "TRestDetectorHitsDriftProcess"
///      maxHits: 1000

#include "TRestDetectorHitsDriftProcess.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "TRestDetectorDrift.h"
#include "TRestDetectorHitsEvent.h"
#include "TRestDetectorReadout.h"
#include "TRestDetectorReadoutManager.h"
#include "TRestManager.h"

using ROOT::Math::XYZVector;

static const bool TRestDetectorHitsDriftProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorHitsDriftProcess;
    reg.RegisterField<P>("readout", &P::fReadoutName);
    reg.RegisterField<P>("driftMedium", &P::fDriftMediumName);
    reg.RegisterField<P>("fluctuations", &P::fFluctuations);
    reg.RegisterField<P>("maxHits", &P::fMaxHits);
    reg.RegisterField<P>("seed", &P::fSeed);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorHitsDriftProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorHitsDriftProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestDetectorHitsDriftProcess::TRestDetectorHitsDriftProcess() : TRestEventProcess() {
    fName = "TRestDetectorHitsDriftProcess";
}

TRestDetectorHitsDriftProcess::TRestDetectorHitsDriftProcess(const std::string& instanceName,
                                                             const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestDetectorHitsDriftProcess::TRestDetectorHitsDriftProcess(const std::string& fileName,
                                                             const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorHitsDriftProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestDetectorHitsDriftProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }
    UpdateParamsFromYAML<TRestDetectorHitsDriftProcess>(fNode);
    UpdateYAMLFromParams<TRestDetectorHitsDriftProcess>(fNode);
}

void TRestDetectorHitsDriftProcess::InitProcess() {
    if (kFluctuations.count(fFluctuations) == 0) {
        throw std::runtime_error("TRestDetectorHitsDriftProcess: unknown fluctuations '" + fFluctuations +
                                 "' (fano, poisson or none)");
    }
    if (!fManager) throw std::runtime_error("TRestDetectorHitsDriftProcess: no TRestManager");

    auto* readouts =
        dynamic_cast<TRestDetectorReadoutManager*>(fManager->GetMetadataClass("TRestDetectorReadoutManager"));
    if (!readouts) {
        throw std::runtime_error("TRestDetectorHitsDriftProcess: no TRestDetectorReadoutManager found");
    }
    if (fReadoutName.empty() && readouts->fReadoutMap.size() == 1) {
        fReadoutName = readouts->fReadoutMap.begin()->first;
    }
    fReadout = readouts->GetReadout(fReadoutName);
    if (!fReadout) {
        throw std::runtime_error("TRestDetectorHitsDriftProcess: readout '" + fReadoutName +
                                 "' not found. Set the 'readout' parameter");
    }

    const auto media = fManager->GetMetadataOfType<TRestDetectorDrift>();
    fMedium = nullptr;
    if (fDriftMediumName.empty() && media.size() == 1) fMedium = media.front();
    for (const auto* medium : media) {
        if (medium->GetName() == fDriftMediumName) fMedium = medium;
    }
    if (!fMedium) {
        throw std::runtime_error("TRestDetectorHitsDriftProcess: drift medium '" + fDriftMediumName +
                                 "' not found. Define a TRestDetectorDrift and set 'driftMedium' if "
                                 "there is more than one");
    }
    if (fFluctuations == "fano" && fMedium->GetFanoFactor() <= 0) {
        throw std::runtime_error(
            "TRestDetectorHitsDriftProcess: fluctuations 'fano' need fanoFactor > 0 in '" +
            fMedium->GetName() + "'");
    }

    fRandom.SetSeed(fSeed);
}

bool TRestDetectorHitsDriftProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* inputEvent = dynamic_cast<const TRestDetectorHitsEvent*>(&input);
    auto* outputEvent = dynamic_cast<TRestDetectorHitsEvent*>(&output);
    if (!inputEvent || !outputEvent) {
        throw std::runtime_error(
            "TRestDetectorHitsDriftProcess: input/output is not a TRestDetectorHitsEvent");
    }

    outputEvent->Initialize();
    outputEvent->SetEventInfo(inputEvent);

    double totalEnergy = 0;
    for (size_t n = 0; n < inputEvent->GetNumberOfHits(); n++) {
        if (inputEvent->GetEnergy(n) > 0) totalEnergy += inputEvent->GetEnergy(n);
    }
    const double wValue = fMedium->GetWValue();
    // Effective W-value: each electron carries the energy of several if there are more than fMaxHits
    const double effectiveW =
        fMaxHits > 0 && totalEnergy / wValue > fMaxHits ? totalEnergy / fMaxHits : wValue;

    for (size_t n = 0; n < inputEvent->GetNumberOfHits(); n++) {
        const double energy = inputEvent->GetEnergy(n);
        if (energy <= 0) continue;

        const XYZVector position = inputEvent->GetPosition(n);
        const double distance = fReadout->GetDistanceToReadout(position);
        const double maxDistance = fReadout->GetMaxDistanceToReadout(position);
        if (std::isnan(distance) || distance < 0 || distance > maxDistance) continue;
        const XYZVector normal = fReadout->GetReadoutNormal(position);

        double electrons = energy / wValue;
        if (fFluctuations == "fano") {
            electrons = fRandom.Gaus(electrons, std::sqrt(fMedium->GetFanoFactor() * electrons));
        } else if (fFluctuations == "poisson") {
            electrons = fRandom.Poisson(electrons);
        }
        const long nElectrons = std::max(1L, std::lround(electrons * wValue / effectiveW));
        const double electronEnergy = energy / nElectrons;

        for (long e = 0; e < nElectrons; e++) {
            XYZVector electron = position;
            if (!fMedium->DiffuseElectron(electron, normal, distance, fRandom)) continue;
            // Kept between the readout plane and the cathode
            const double diffusedDistance = fReadout->GetDistanceToReadout(electron);
            if (diffusedDistance < 0) electron -= diffusedDistance * normal;
            if (diffusedDistance > maxDistance) electron -= (diffusedDistance - maxDistance) * normal;
            outputEvent->AddHit(electron, electronEnergy, inputEvent->GetTime(n), inputEvent->GetType(n));
        }
    }

    return outputEvent->GetNumberOfHits() > 0;
}
