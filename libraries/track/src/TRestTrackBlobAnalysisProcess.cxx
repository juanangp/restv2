/// \class TRestTrackBlobAnalysisProcess
/// Observables of the ends (blobs) of the main track of a TRestTrackEvent, e.g. to tell the two blobs of a
/// double beta event from the single blob of an electron. The event is not modified.
///
/// The nodes of the track must be ordered along it: the process goes after TRestTrackReductionProcess and
/// TRestTrackPathMinimizationProcess. Blob 1 and blob 2 are the most energetic nodes of the first and of
/// the last hitsToCheckFraction of the nodes (at least one node each); as legacy, blob 1 is the one closer
/// to z = 0 (|z| smaller; the order of the path if the track does not define z). The energies in spheres
/// sum the nodes of the track (the node of the blob included) at a distance up to the radius from the
/// blob, so they depend on the reduction: each node counts entirely or not at all (more nodes give a finer
/// value).
///
/// Observables, for the most energetic track (no suffix) or, for each projection P in `projections`
/// (hit types, e.g. [XZ, YZ] for strip readouts), for the most energetic track of type P (suffix _P):
/// * X1, Y1, Z1, X2, Y2, Z2: positions of the blobs (NaN along the coordinates the track does not define).
/// * Distance: distance between the blobs (in the projection the track defines).
/// * Vectors with one entry per radius of `radii`: Q1, Q2 (energy in the sphere around blob 1 and blob 2),
///   QHigh, QLow (the highest and the lowest of both), QBalance ((QHigh - QLow) / (QHigh + QLow), 0 if
///   both are 0) and QRatio (QHigh / QLow, 0 if QLow is 0).
/// Without a track (of type P), positions and distance are NaN and the Q entries 0.
///
/// Parameters: projections ([]), hitsToCheckFraction (0.2), radii ([]).
///
/// Example (strip readout):
///
///    trackBlobAnalysis:
///      class: "TRestTrackBlobAnalysisProcess"
///      projections: ["XZ", "YZ"]
///      hitsToCheckFraction: 0.2
///      radii: [5mm, 10mm]
///      observables:
///        - "Distance_XZ"
///        - "Q1_XZ"
///        - "QBalance_XZ"
///
/// Migrated from the legacy track library (Javier Galan). Differences: the radius is not given in the
/// observable names (Q1_R5, Qhigh_X_R10...) but in `radii`, with one entry per radius in each Q
/// observable; the legacy XYZ, X and Y tracks are given by `projections` (legacy x1_X = X1_XZ, Q1_X_R5 =
/// Q1_XZ[i]); the energies in spheres use the nodes of the track instead of the original hits (the legacy
/// event kept every stage, the restv2 one only the last); positions and distance without a track are NaN
/// (legacy 0); QRatio is 0 instead of infinite when QLow is 0; at least one node is checked at each end.

#include "TRestTrackBlobAnalysisProcess.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

#include "TRestHits.h"
#include "TRestRun.h"
#include "TRestTrack.h"
#include "TRestTrackEvent.h"

static const bool TRestTrackBlobAnalysisProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestTrackBlobAnalysisProcess;
    reg.RegisterField<P>("projections", &P::fProjections);
    reg.RegisterField<P>("hitsToCheckFraction", &P::fHitsToCheckFraction);
    reg.RegisterField<P>("radii", &P::fRadii);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestTrackBlobAnalysisProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestTrackBlobAnalysisProcess>(instanceName, params);
        });
    return true;
}();

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

TRestTrackBlobAnalysisProcess::TRestTrackBlobAnalysisProcess() : TRestEventProcess() {
    fName = "TRestTrackBlobAnalysisProcess";
}

TRestTrackBlobAnalysisProcess::TRestTrackBlobAnalysisProcess(const std::string& instanceName,
                                                             const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestTrackBlobAnalysisProcess::TRestTrackBlobAnalysisProcess(const std::string& fileName,
                                                             const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestTrackBlobAnalysisProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestTrackBlobAnalysisProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestTrackBlobAnalysisProcess>(fNode);
    UpdateYAMLFromParams<TRestTrackBlobAnalysisProcess>(fNode);
}

void TRestTrackBlobAnalysisProcess::InitProcess() {
    if (fHitsToCheckFraction <= 0 || fHitsToCheckFraction > 0.5) {
        throw std::runtime_error("TRestTrackBlobAnalysisProcess: hitsToCheckFraction must be in (0, 0.5]");
    }
    for (const auto& radius : fRadii) {
        if (radius <= 0) throw std::runtime_error("TRestTrackBlobAnalysisProcess: the radii must be > 0");
    }

    const std::map<std::string, int> hitTypes = {{"X", TRestHitsData::X},    {"Y", TRestHitsData::Y},
                                                 {"Z", TRestHitsData::Z},    {"XY", TRestHitsData::XY},
                                                 {"XZ", TRestHitsData::XZ},  {"YZ", TRestHitsData::YZ},
                                                 {"XYZ", TRestHitsData::XYZ}};
    // The vectors do not change after this, so the registered references stay valid
    fBlobObs.clear();
    if (fProjections.empty()) fBlobObs.emplace_back();
    for (const auto& name : fProjections) {
        auto it = hitTypes.find(name);
        if (it == hitTypes.end()) {
            throw std::runtime_error("TRestTrackBlobAnalysisProcess: unknown projection '" + name +
                                     "' (X, Y, Z, XY, XZ, YZ or XYZ)");
        }
        BlobObservables obs;
        obs.name = name;
        obs.type = it->second;
        fBlobObs.push_back(obs);
    }

    for (auto& obs : fBlobObs) {
        for (auto* q : {&obs.q1, &obs.q2, &obs.qHigh, &obs.qLow, &obs.qBalance, &obs.qRatio})
            q->assign(fRadii.size(), 0);

        const std::string suffix = obs.name.empty() ? "" : "_" + obs.name;
        RegisterObservable("X1" + suffix, obs.x1);
        RegisterObservable("Y1" + suffix, obs.y1);
        RegisterObservable("Z1" + suffix, obs.z1);
        RegisterObservable("X2" + suffix, obs.x2);
        RegisterObservable("Y2" + suffix, obs.y2);
        RegisterObservable("Z2" + suffix, obs.z2);
        RegisterObservable("Distance" + suffix, obs.distance);
        RegisterObservable("Q1" + suffix, obs.q1);
        RegisterObservable("Q2" + suffix, obs.q2);
        RegisterObservable("QHigh" + suffix, obs.qHigh);
        RegisterObservable("QLow" + suffix, obs.qLow);
        RegisterObservable("QBalance" + suffix, obs.qBalance);
        RegisterObservable("QRatio" + suffix, obs.qRatio);
    }
}

void TRestTrackBlobAnalysisProcess::FillObservables(const TRestHits* hits, BlobObservables& obs) const {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    // Assignments keep the addresses of the registered members
    obs.x1 = obs.y1 = obs.z1 = obs.x2 = obs.y2 = obs.z2 = obs.distance = nan;
    for (auto* q : {&obs.q1, &obs.q2, &obs.qHigh, &obs.qLow, &obs.qBalance, &obs.qRatio})
        std::fill(q->begin(), q->end(), 0);

    const int nHits = hits ? (int)hits->GetNumberOfHits() : 0;
    if (nHits == 0) return;

    // Most energetic node of each end (the first or last node if none has energy)
    const int nCheck = std::max(1, (int)(nHits * fHitsToCheckFraction));
    int hit1 = hits->GetMostEnergeticHitInRange(0, nCheck);
    int hit2 = hits->GetMostEnergeticHitInRange(nHits - nCheck, nHits);
    if (hit1 < 0) hit1 = 0;
    if (hit2 < 0) hit2 = nHits - 1;

    const int type = GetHitsType(*hits);
    // Blob 1 closer to z = 0
    if (TRestHitsData::IsDefined(type, 2) && std::abs(hits->GetZ(hit2)) < std::abs(hits->GetZ(hit1)))
        std::swap(hit1, hit2);

    // Undefined coordinates are 0 in the points, so the distances are those of the projection
    const auto points = hits->GetPoints();
    const auto blob1 = points[hit1], blob2 = points[hit2];
    obs.distance = std::sqrt((blob1 - blob2).Mag2());

    const double coords1[3] = {blob1.X(), blob1.Y(), blob1.Z()};
    const double coords2[3] = {blob2.X(), blob2.Y(), blob2.Z()};
    double* obs1[3] = {&obs.x1, &obs.y1, &obs.z1};
    double* obs2[3] = {&obs.x2, &obs.y2, &obs.z2};
    for (int a = 0; a < 3; a++) {
        if (!TRestHitsData::IsDefined(type, a)) continue;
        *obs1[a] = coords1[a];
        *obs2[a] = coords2[a];
    }

    for (size_t r = 0; r < fRadii.size(); r++) {
        const double radius2 = fRadii[r] * fRadii[r];
        double q1 = 0, q2 = 0;
        for (int n = 0; n < nHits; n++) {
            if ((points[n] - blob1).Mag2() <= radius2) q1 += hits->GetEnergy(n);
            if ((points[n] - blob2).Mag2() <= radius2) q2 += hits->GetEnergy(n);
        }
        const double high = std::max(q1, q2), low = std::min(q1, q2);
        obs.q1[r] = q1;
        obs.q2[r] = q2;
        obs.qHigh[r] = high;
        obs.qLow[r] = low;
        obs.qBalance[r] = high + low != 0 ? (high - low) / (high + low) : 0;
        obs.qRatio[r] = low != 0 ? high / low : 0;
    }
}

bool TRestTrackBlobAnalysisProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    output.CopyFrom(&input);
    auto* event = dynamic_cast<TRestTrackEvent*>(&output);
    if (!event) throw std::runtime_error("TRestTrackBlobAnalysisProcess: output is not a TRestTrackEvent");

    if (GetVerboseLevel() >= TRestLogManager::REST_Verbose_Level::REST_Debug) event->PrintOnlyTracks();

    for (auto& obs : fBlobObs) {
        // Most energetic track of the projection type (of any type without projections)
        int mainTrack = -1;
        for (int t = 0; t < event->GetNumberOfTracks(); t++) {
            if (obs.type != 0 && GetHitsType(event->GetTrack(t).GetHits()) != obs.type) continue;
            if (mainTrack < 0 || event->GetTrackEnergy(t) > event->GetTrackEnergy(mainTrack)) mainTrack = t;
        }
        if (mainTrack < 0) {
            FillObservables(nullptr, obs);
            continue;
        }
        const TRestHits hits = event->GetTrack(mainTrack).GetHits();
        FillObservables(&hits, obs);
    }

    return true;
}
