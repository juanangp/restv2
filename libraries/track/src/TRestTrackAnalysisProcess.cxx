/// \class TRestTrackAnalysisProcess
/// Computes observables of the tracks of a TRestTrackEvent, for any readout. The event is not modified.
///
/// The type of each track is the type of its hits (TRestHitsData::REST_HitType: XZ = 10, YZ = 15,
/// XYZ = 30, ...). Coordinates a track does not define are NaN (mean, sigma and gaussian sigma).
///
/// Track observables (<name>): Energy, NHits; Length (TRestHits::GetTotalDistance) and Volume
/// (squared distance of the farthest hits); MeanX/Y/Z (energy weighted); SigmaX/Y/Z (energy weighted
/// size, TRestHits::GetSigmaX, ...); GaussSigmaX/Y/Z (gaussian fit, TRestHitsAnalysis::GetGaussSigma,
/// only with gaussFit: true, otherwise 0; -1 if the fit fails); Twist, TwistWeighted
/// (TRestHitsAnalysis::GetTwist, GetTwistWeighted) and TwistLow, TwistHigh (lowest and highest twist of the
/// first and last twistTailPercentage% of the hits).
///
/// Observables:
/// * NTracks, TotalEnergy.
/// * MaxTrack<name>: the main track. Without projections, the most energetic track, whatever its type
///   (all the track observables). With projections, the hits of the main track of every projection
///   together, as legacy for the most energetic X and Y tracks (MaxTrackEnergy, MaxTrack_XYZ_...):
///   MaxTrackEnergy, MaxTrackNHits and MaxTrackMean, MaxTrackSigma, MaxTrackGaussSigma of the
///   coordinates any projection defines (X, Y and Z for XZ and YZ), each one from the hits that define
///   it; length and twists are those of MaxTrack_P_<name>.
/// * MaxTrackEnergyRatio: (TotalEnergy - MaxTrackEnergy) / TotalEnergy; MaxTrackSkewXY, MaxTrackSkewZ
///   (TRestHits::GetSkewXY, GetSkewZ).
/// * Tracks<name>: vectors with the tracks out of the main track (with projections, those that are not
///   the main track of any projection), in decreasing energy, and TracksType with the type of each one.
///
/// For each projection P in `projections` (hit types, e.g. [XZ, YZ] for strip readouts, which give X
/// and Y tracks):
/// * NTracks_P, MaxTrack_P_<name>: number of tracks of type P and main track of type P (Mean, Sigma and
///   GaussSigma only along the coordinates P defines, e.g. X and Z for XZ); MaxTrack_P_EnergyRatio:
///   (E_P - MaxTrack_P_Energy) / E_P, with E_P the energy of the tracks of type P.
/// * For each pair P1, P2 (in the given order): EnergyBalance_P1_P2 and SigmaBalance_P1_P2, as
///   (a - b) / (a + b) of the energies and of the sigmas of the coordinate only each one defines (X of
///   XZ and Y of YZ; NaN if there is not exactly one). 0 if a + b = 0.
///
/// `projections` lists the hit types the readout gives: [XZ, YZ] for strips, [XYZ] for a 3D readout
/// (only needed together for a readout giving both). Legacy summed the X, Y and XYZ tracks to cover both
/// cases at once, the terms of the types the readout does not give being 0.
///
/// Legacy observables for strips with projections [XZ, YZ]: nTracks_X/Y = NTracks_XZ/YZ, MaxTrackEnergy,
/// MaxTrackEnergyRatio as legacy, MaxTrack_XZ_nHits = MaxTrack_XZ_NHits, MaxTrack_XYZ_SigmaZ2 =
/// MaxTrackSigmaZ^2, MaxTrack_XYZ_GaussSigmaZ = MaxTrackGaussSigmaZ, MaxTrack_XYZ_skewXY/Z =
/// MaxTrackSkewXY/Z, MaxTrackEnergyBalanceXY = EnergyBalance_XZ_YZ, MaxTrackxySigmaBalance =
/// SigmaBalance_XZ_YZ.
///
/// With cutsEnabled, events are rejected before the analysis when the number of tracks of any projection
/// (NTracks_P) is out of nTracksCut; without projections the cut applies to the total NTracks. Legacy
/// nTracksXCut/nTracksYCut correspond to nTracksCut with projections [XZ, YZ] (a single range for both).
/// With a minimum of 1 the events with no tracks in a projection are rejected; without the cut, the
/// coordinates only that projection defines are NaN in MaxTrack<name>.
///
/// Example (strip readout):
///
///    trackAnalysis:
///      class: "TRestTrackAnalysisProcess"
///      projections: ["XZ", "YZ"]
///      observables:
///        - "NTracks_XZ"
///        - "NTracks_YZ"
///        - "MaxTrackEnergyRatio"
///        - "MaxTrack_XZ_NHits"
///        - "MaxTrack_YZ_NHits"
///        - "MaxTrackSigmaZ"
///        - "EnergyBalance_XZ_YZ"
///        - "SigmaBalance_XZ_YZ"
///
/// Migrated from the legacy track library (Javier Galan), redesigned to be readout independent: the
/// legacy observables of the most energetic X, Y and XYZ tracks are given by the projections. The time
/// observables (EventTimeDelay, MeanRate_InHz) are not track observables and are not included.

#include "TRestTrackAnalysisProcess.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>

#include "TRestHitsAnalysis.h"
#include "TRestRun.h"
#include "TRestTrack.h"
#include "TRestTrackEvent.h"

static const bool TRestTrackAnalysisProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestTrackAnalysisProcess;
    reg.RegisterField<P>("projections", &P::fProjections);
    reg.RegisterField<P>("gaussFit", &P::fGaussFit);
    reg.RegisterField<P>("twistTailPercentage", &P::fTwistTailPercentage);
    reg.RegisterField<P>("cutsEnabled", &P::fCutsEnabled);
    reg.RegisterField<P>("nTracksCut", &P::fNTracksCut);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestTrackAnalysisProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestTrackAnalysisProcess>(instanceName, params);
        });
    return true;
}();

double Balance(double a, double b) { return a + b != 0 ? (a - b) / (a + b) : 0; }

// Type of a set of hits: product of the coordinates any hit defines (the hit type for a track)
int GetHitsType(const TRestHits& hits) {
    bool defined[3] = {false, false, false};
    for (size_t n = 0; n < hits.GetNumberOfHits(); n++) {
        for (int a = 0; a < 3; a++) defined[a] |= TRestHitsData::IsDefined(hits.GetType(n), a);
    }
    int type = 1;
    for (int a = 0; a < 3; a++) {
        if (defined[a]) type *= TRestHitsData::kAxisTypes[a];
    }
    return type;
}
}  // namespace

void TRestTrackAnalysisProcess::TracksObservables::clear() {
    auto clearAll = [](auto&... vectors) { (vectors.clear(), ...); };
    clearAll(energy, type, nHits, length, volume, meanX, meanY, meanZ, sigmaX, sigmaY, sigmaZ, gaussSigmaX,
             gaussSigmaY, gaussSigmaZ, twist, twistWeighted, twistLow, twistHigh);
}

void TRestTrackAnalysisProcess::TracksObservables::push_back(const TrackObservables& t) {
    energy.push_back(t.energy);
    type.push_back(t.type);
    nHits.push_back(t.nHits);
    length.push_back(t.length);
    volume.push_back(t.volume);
    meanX.push_back(t.meanX);
    meanY.push_back(t.meanY);
    meanZ.push_back(t.meanZ);
    sigmaX.push_back(t.sigmaX);
    sigmaY.push_back(t.sigmaY);
    sigmaZ.push_back(t.sigmaZ);
    gaussSigmaX.push_back(t.gaussSigmaX);
    gaussSigmaY.push_back(t.gaussSigmaY);
    gaussSigmaZ.push_back(t.gaussSigmaZ);
    twist.push_back(t.twist);
    twistWeighted.push_back(t.twistWeighted);
    twistLow.push_back(t.twistLow);
    twistHigh.push_back(t.twistHigh);
}

TRestTrackAnalysisProcess::TRestTrackAnalysisProcess() : TRestEventProcess() {
    fName = "TRestTrackAnalysisProcess";
}

TRestTrackAnalysisProcess::TRestTrackAnalysisProcess(const std::string& instanceName, const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestTrackAnalysisProcess::TRestTrackAnalysisProcess(const std::string& fileName,
                                                     const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestTrackAnalysisProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestTrackAnalysisProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestTrackAnalysisProcess>(fNode);
    UpdateYAMLFromParams<TRestTrackAnalysisProcess>(fNode);
}

void TRestTrackAnalysisProcess::RegisterTrackObservables(const std::string& prefix, TrackObservables& obs,
                                                         int type) {
    RegisterObservable(prefix + "Energy", obs.energy);
    RegisterObservable(prefix + "NHits", obs.nHits);
    RegisterCoordinateObservables(prefix, obs, type);
    RegisterObservable(prefix + "Length", obs.length);
    RegisterObservable(prefix + "Volume", obs.volume);
    RegisterObservable(prefix + "Twist", obs.twist);
    RegisterObservable(prefix + "TwistWeighted", obs.twistWeighted);
    RegisterObservable(prefix + "TwistLow", obs.twistLow);
    RegisterObservable(prefix + "TwistHigh", obs.twistHigh);
}

void TRestTrackAnalysisProcess::RegisterCoordinateObservables(const std::string& prefix,
                                                              TrackObservables& obs, int type) {
    const char* axisNames[3] = {"X", "Y", "Z"};
    double* means[3] = {&obs.meanX, &obs.meanY, &obs.meanZ};
    double* sigmas[3] = {&obs.sigmaX, &obs.sigmaY, &obs.sigmaZ};
    double* gaussSigmas[3] = {&obs.gaussSigmaX, &obs.gaussSigmaY, &obs.gaussSigmaZ};
    for (int a = 0; a < 3; a++) {
        if (!TRestHitsData::IsDefined(type, a)) continue;
        RegisterObservable(prefix + "Mean" + axisNames[a], *means[a]);
        RegisterObservable(prefix + "Sigma" + axisNames[a], *sigmas[a]);
        RegisterObservable(prefix + "GaussSigma" + axisNames[a], *gaussSigmas[a]);
    }
}

void TRestTrackAnalysisProcess::InitProcess() {
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
            RESTError << "TRestTrackAnalysisProcess: unknown projection '" << name
                      << "' (X, Y, Z, XY, XZ, YZ or XYZ)" << RESTendl;
            continue;
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

    RegisterObservable("NTracks", fNTracks);
    RegisterObservable("TotalEnergy", fTotalEnergy);
    RegisterObservable("MaxTrackEnergyRatio", fMaxTrackEnergyRatio);
    if (fProjectionObs.empty()) {
        // Any type: all the coordinates (NaN for those the track does not define)
        RegisterTrackObservables("MaxTrack", fMaxTrack, TRestHitsData::XYZ);
    } else {
        // Hits of different projections together: no length or twist, they are those of MaxTrack_P_<name>.
        // The coordinates any projection defines
        int type = 1;
        for (int a = 0; a < 3; a++) {
            for (const auto& projection : fProjectionObs) {
                if (!TRestHitsData::IsDefined(projection.type, a)) continue;
                type *= TRestHitsData::kAxisTypes[a];
                break;
            }
        }
        RegisterObservable("MaxTrackEnergy", fMaxTrack.energy);
        RegisterObservable("MaxTrackNHits", fMaxTrack.nHits);
        RegisterCoordinateObservables("MaxTrack", fMaxTrack, type);
    }
    RegisterObservable("MaxTrackSkewXY", fMaxTrackSkewXY);
    RegisterObservable("MaxTrackSkewZ", fMaxTrackSkewZ);

    RegisterObservable("TracksEnergy", fTracks.energy);
    RegisterObservable("TracksType", fTracks.type);
    RegisterObservable("TracksNHits", fTracks.nHits);
    RegisterObservable("TracksLength", fTracks.length);
    RegisterObservable("TracksVolume", fTracks.volume);
    RegisterObservable("TracksMeanX", fTracks.meanX);
    RegisterObservable("TracksMeanY", fTracks.meanY);
    RegisterObservable("TracksMeanZ", fTracks.meanZ);
    RegisterObservable("TracksSigmaX", fTracks.sigmaX);
    RegisterObservable("TracksSigmaY", fTracks.sigmaY);
    RegisterObservable("TracksSigmaZ", fTracks.sigmaZ);
    RegisterObservable("TracksGaussSigmaX", fTracks.gaussSigmaX);
    RegisterObservable("TracksGaussSigmaY", fTracks.gaussSigmaY);
    RegisterObservable("TracksGaussSigmaZ", fTracks.gaussSigmaZ);
    RegisterObservable("TracksTwist", fTracks.twist);
    RegisterObservable("TracksTwistWeighted", fTracks.twistWeighted);
    RegisterObservable("TracksTwistLow", fTracks.twistLow);
    RegisterObservable("TracksTwistHigh", fTracks.twistHigh);

    for (auto& projection : fProjectionObs) {
        RegisterObservable("NTracks_" + projection.name, projection.nTracks);
        RegisterObservable("MaxTrack_" + projection.name + "_EnergyRatio", projection.maxTrackEnergyRatio);
        RegisterTrackObservables("MaxTrack_" + projection.name + "_", projection.maxTrack, projection.type);
    }
    for (auto& balance : fBalances) {
        const std::string pair =
            fProjectionObs[balance.first].name + "_" + fProjectionObs[balance.second].name;
        RegisterObservable("EnergyBalance_" + pair, balance.energy);
        RegisterObservable("SigmaBalance_" + pair, balance.sigma);
    }
}

TRestTrackAnalysisProcess::TrackObservables TRestTrackAnalysisProcess::GetHitsObservables(
    const TRestHits& hits) const {
    TrackObservables obs;
    const size_t nHits = hits.GetNumberOfHits();
    obs.energy = hits.GetTotalEnergy();
    obs.nHits = nHits;
    if (nHits == 0) return obs;

    obs.type = GetHitsType(hits);

    const auto mean = hits.GetMeanPosition();
    const double means[3] = {mean.X(), mean.Y(), mean.Z()};
    const double sigmas[3] = {hits.GetSigmaX(), hits.GetSigmaY(), hits.GetSigmaZ()};
    double* meanObs[3] = {&obs.meanX, &obs.meanY, &obs.meanZ};
    double* sigmaObs[3] = {&obs.sigmaX, &obs.sigmaY, &obs.sigmaZ};
    double* gaussObs[3] = {&obs.gaussSigmaX, &obs.gaussSigmaY, &obs.gaussSigmaZ};

    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (int a = 0; a < 3; a++) {
        const bool defined = TRestHitsData::IsDefined(obs.type, a);
        *meanObs[a] = defined ? means[a] : nan;
        *sigmaObs[a] = defined ? sigmas[a] : nan;
        if (!defined) {
            *gaussObs[a] = nan;
        } else if (fGaussFit) {
            // Only the hits that define the coordinate
            std::vector<double> values, energies;
            for (size_t n = 0; n < nHits; n++) {
                if (!TRestHitsData::IsDefined(hits.GetType(n), a)) continue;
                values.push_back(hits.GetCoordinate(n, a));
                energies.push_back(hits.GetEnergy(n));
            }
            *gaussObs[a] = TRestHitsAnalysis::GetGaussSigma(values, energies);
        }
    }

    obs.length = hits.GetTotalDistance();
    obs.volume = hits.GetMaximumHitDistance2();

    const auto points = hits.GetPoints();
    const auto energies = hits.GetEnergies();
    obs.twist = TRestHitsAnalysis::GetTwist(points);
    obs.twistWeighted = TRestHitsAnalysis::GetTwistWeighted(points, energies);
    // -1 if the tails have no hits (GetTwist would take the whole track)
    const size_t nTail = (size_t)(fTwistTailPercentage * nHits / 100.);
    const double twistStart = nTail > 0 ? TRestHitsAnalysis::GetTwist(points, 0, nTail) : -1;
    const double twistEnd = nTail > 0 ? TRestHitsAnalysis::GetTwist(points, nHits - nTail, nHits) : -1;
    obs.twistLow = std::min(twistStart, twistEnd);
    obs.twistHigh = std::max(twistStart, twistEnd);
    return obs;
}

bool TRestTrackAnalysisProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    output.CopyFrom(&input);
    auto* event = dynamic_cast<TRestTrackEvent*>(&output);
    if (!event) throw std::runtime_error("TRestTrackAnalysisProcess: output is not a TRestTrackEvent");

    if (GetVerboseLevel() >= TRestLogManager::REST_Verbose_Level::REST_Debug) event->PrintOnlyTracks();

    // Assignments keep the addresses of the registered members
    fNTracks = event->GetNumberOfTracks();
    // Rejected events skip the analysis: the cut applies to the number of tracks of each projection,
    // or to the total without projections
    if (fCutsEnabled) {
        auto outOfRange = [this](int n) { return n < fNTracksCut.first || n > fNTracksCut.second; };
        if (fProjectionObs.empty() && outOfRange(fNTracks)) return false;
        for (const auto& projection : fProjectionObs) {
            int nTracks = 0;
            for (int t = 0; t < fNTracks; t++)
                nTracks += GetHitsType(event->GetTrack(t).GetHits()) == projection.type;
            if (outOfRange(nTracks)) return false;
        }
    }
    fTotalEnergy = event->GetEnergy("");
    fTracks.clear();
    for (auto& projection : fProjectionObs) {
        projection.nTracks = 0;
        projection.energy = 0;
        projection.maxTrackEnergyRatio = 0;
        projection.maxTrack = TrackObservables();
        projection.maxTrackIndex = -1;
    }

    std::vector<int> order(fNTracks);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
              [event](int a, int b) { return event->GetTrackEnergy(a) > event->GetTrackEnergy(b); });

    // Main tracks: the most energetic one or, with projections, the most energetic of each projection
    std::vector<TrackObservables> trackObs(order.size());
    std::vector<bool> isMain(order.size(), false);
    if (fProjectionObs.empty() && !order.empty()) isMain[0] = true;
    for (size_t i = 0; i < order.size(); i++) {
        trackObs[i] = GetHitsObservables(event->GetTrack(order[i]).GetHits());

        // In decreasing energy: the first track of each projection is its main track
        for (auto& projection : fProjectionObs) {
            if (trackObs[i].type != projection.type) continue;
            projection.energy += trackObs[i].energy;
            if (projection.nTracks++ == 0) {
                projection.maxTrack = trackObs[i];
                projection.maxTrackIndex = order[i];
                isMain[i] = true;
            }
        }
    }
    for (size_t i = 0; i < order.size(); i++) {
        if (!isMain[i]) fTracks.push_back(trackObs[i]);
    }

    TRestHitsData data;
    TRestHits mainHits(&data);
    for (size_t i = 0; i < order.size(); i++) {
        if (isMain[i]) mainHits.AddHits(event->GetTrack(order[i]).GetHits());
    }
    fMaxTrack = GetHitsObservables(mainHits);
    const bool hasHits = mainHits.GetNumberOfHits() > 0;
    fMaxTrackSkewXY = hasHits ? mainHits.GetSkewXY() : 0;
    fMaxTrackSkewZ = hasHits ? mainHits.GetSkewZ() : 0;

    auto ratio = [](double energy, double mainEnergy) {
        return energy != 0 ? (energy - mainEnergy) / energy : 0;
    };
    fMaxTrackEnergyRatio = ratio(fTotalEnergy, fMaxTrack.energy);
    for (auto& projection : fProjectionObs) {
        projection.maxTrackEnergyRatio = ratio(projection.energy, projection.maxTrack.energy);
    }

    auto sigmaOf = [](const TrackObservables& obs, int axis) {
        return axis == 0 ? obs.sigmaX : axis == 1 ? obs.sigmaY : obs.sigmaZ;
    };
    for (auto& balance : fBalances) {
        const auto& a = fProjectionObs[balance.first].maxTrack;
        const auto& b = fProjectionObs[balance.second].maxTrack;
        balance.energy = Balance(a.energy, b.energy);
        balance.sigma = balance.firstAxis < 0
                            ? std::numeric_limits<double>::quiet_NaN()
                            : Balance(sigmaOf(a, balance.firstAxis), sigmaOf(b, balance.secondAxis));
    }

    return true;
}
