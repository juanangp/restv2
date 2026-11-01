/// \class TRestDetectorHitsToTrackProcess
/// Transforms a TRestDetectorHitsEvent into a TRestTrackEvent. Each track is a group of hits where
/// every hit is closer than `clusterDistance` to another hit of the group; groups farther apart are
/// independent tracks.
///
/// Hits of different types are grouped separately (as legacy for XZ, YZ and XYZ hits, now for every
/// type), with the distance over the coordinates the type defines: XZ and YZ hits of a strip readout give
/// X and Y tracks. Hits that define no coordinate are ignored. The tracks have no parent (parent ID 0), IDs
/// 1, 2, ... in order of type (XZ, YZ, XYZ, ...), and their hits keep the input order.
///
/// The grouping is TRestHitsAnalysis::FindClusters, which only compares each hit with the hits of
/// its cell of side clusterDistance and the adjacent ones (instead of all the hits, as legacy): same
/// tracks, but the time grows with the number of hits instead of its square.
///
/// Optionally (attachEnergy > 0), each isolated hit with less energy than attachEnergy (a track of a
/// single hit, farther than clusterDistance from any other hit) joins the track of a hit closer than
/// attachDistance in each coordinate the type defines (a distance per coordinate, X, Y and Z), the
/// closest one relative to attachDistance if several. The hits of any energy of the tracks that are not
/// isolated low energy hits count, so isolated hits are never attached to each other. Meant for the time
/// walk: small pulses arrive later, so their Z (drift coordinate, from the time) is displaced and they
/// would form separate tracks, while their strip position is right; e.g. attachDistance [2.5mm, 2.5mm,
/// 4mm] relaxes Z and keeps the same or neighbour strip in X and Y. A coordinate with attachDistance 0
/// disables the attachment for the types defining it. The option only joins the isolated hits to the
/// tracks of the grouping by clusterDistance, never splits them. The numbers of isolated low energy hits
/// attached and not attached (no hit close enough) are reported at the end of the processing. Note that
/// noise hits within attachDistance are attached as well: keep it as small as the time walk allows.
///
/// Events without tracks are rejected.
///
/// ### Parameters
/// * clusterDistance: maximum distance between hits of a track (default 3 mm).
/// * attachEnergy: energy below which the isolated hits are attached to a track (default 0: disabled).
/// * attachDistance: maximum distance in X, Y and Z of an attached hit to a hit of the track (default
///   [3mm, 3mm, 10mm]).
///
/// Example:
///
///    hitsToTrack:
///      class: "TRestDetectorHitsToTrackProcess"
///      clusterDistance: "2.5mm"
///      attachEnergy: 0
///      attachDistance: [2.5mm, 2.5mm, 4mm]
///
/// Migrated from the legacy connectors library (Javier Gracia, Javier Galan). Same tracks as legacy (the same
/// grouping, faster); legacy TRestDetectorHitsToTrackFastProcess (grouping by touching cells of a mesh,
/// faster than the legacy grouping by distance) is not migrated, the grouping by distance is now as fast.

#include "TRestDetectorHitsToTrackProcess.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>
#include <vector>

#include "TRestDetectorHitsEvent.h"
#include "TRestHitsAnalysis.h"
#include "TRestTrackEvent.h"

static const bool TRestDetectorHitsToTrackProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorHitsToTrackProcess;
    reg.RegisterField<P>("clusterDistance", &P::fClusterDistance);
    reg.RegisterField<P>("attachEnergy", &P::fAttachEnergy);
    reg.RegisterField<P>("attachDistance", &P::fAttachDistance);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorHitsToTrackProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorHitsToTrackProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestDetectorHitsToTrackProcess::TRestDetectorHitsToTrackProcess() : TRestEventProcess() {
    fName = "TRestDetectorHitsToTrackProcess";
}

TRestDetectorHitsToTrackProcess::TRestDetectorHitsToTrackProcess(const std::string& instanceName,
                                                                 const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestDetectorHitsToTrackProcess::TRestDetectorHitsToTrackProcess(const std::string& fileName,
                                                                 const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorHitsToTrackProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestDetectorHitsToTrackProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestDetectorHitsToTrackProcess>(fNode);
    UpdateYAMLFromParams<TRestDetectorHitsToTrackProcess>(fNode);
}

void TRestDetectorHitsToTrackProcess::InitProcess() {
    fNAttached = 0;
    fNNotAttached = 0;
    if (fAttachEnergy > 0 &&
        std::any_of(fAttachDistance.begin(), fAttachDistance.end(), [](double d) { return d < 0; })) {
        throw std::runtime_error("TRestDetectorHitsToTrackProcess: attachDistance must not be negative");
    }
}

void TRestDetectorHitsToTrackProcess::EndProcess() {
    if (fAttachEnergy > 0) {
        RESTInfo << "TRestDetectorHitsToTrackProcess: " << fNAttached
                 << " isolated hits below attachEnergy attached to a track, " << fNNotAttached
                 << " with no hit close enough" << RESTendl;
    }
}

bool TRestDetectorHitsToTrackProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* hitsEvent = dynamic_cast<const TRestDetectorHitsEvent*>(&input);
    auto* trackEvent = dynamic_cast<TRestTrackEvent*>(&output);
    if (!hitsEvent || !trackEvent) {
        throw std::runtime_error(
            "TRestDetectorHitsToTrackProcess: input/output is not a TRestDetectorHitsEvent/TRestTrackEvent");
    }

    trackEvent->Initialize();
    trackEvent->SetEventInfo(&input);

    const TRestHits& hits = *hitsEvent->GetHits();
    std::map<int, std::vector<int>> hitsByType;
    for (size_t n = 0; n < hits.GetNumberOfHits(); n++) hitsByType[hits.GetType(n)].push_back(n);

    for (const auto& [type, indices] : hitsByType) {
        std::array<bool, 3> coordinates;
        bool definesCoordinate = false;
        for (int a = 0; a < 3; a++) {
            coordinates[a] = TRestHitsData::IsDefined(type, a);
            definesCoordinate |= coordinates[a];
        }
        if (!definesCoordinate) continue;

        // Groups of hits (indices in the event) closer than clusterDistance, as without attachEnergy
        std::vector<ROOT::Math::XYZVector> points;
        points.reserve(indices.size());
        for (int n : indices) points.push_back(hits.GetPosition(n));
        const auto clusters = TRestHitsAnalysis::FindClusters(points, fClusterDistance, coordinates);

        // Group of the track of each group: itself, or the group an isolated hit is attached to
        std::vector<size_t> root(clusters.size());
        std::iota(root.begin(), root.end(), 0);

        // With attachEnergy, each isolated low energy hit (a group of one hit) joins the group of the
        // closest hit, relative to attachDistance, closer than attachDistance in every coordinate of the
        // type among the hits of the other groups (any energy): isolated hits are not attached to each other
        size_t nAttached = 0;
        if (fAttachEnergy > 0) {
            auto isolated = [&](size_t c) {
                return clusters[c].size() == 1 && hits.GetEnergy(indices[clusters[c][0]]) < fAttachEnergy;
            };
            std::vector<std::pair<int, size_t>> targets;  // hit and its group
            for (size_t c = 0; c < clusters.size(); c++) {
                if (isolated(c)) continue;
                for (size_t k : clusters[c]) targets.emplace_back(indices[k], c);
            }
            // (d / attachDistance)^2 summed over the coordinates, -1 if outside attachDistance in one
            auto relative2 = [&](int a, int b) {
                double r2 = 0;
                for (int axis = 0; axis < 3; axis++) {
                    if (!coordinates[axis]) continue;
                    const double d = std::abs(hits.GetCoordinate(a, axis) - hits.GetCoordinate(b, axis));
                    if (d >= fAttachDistance[axis]) return -1.;
                    r2 += d * d / (fAttachDistance[axis] * fAttachDistance[axis]);
                }
                return r2;
            };
            for (size_t c = 0; c < clusters.size(); c++) {
                if (!isolated(c)) continue;
                const int n = indices[clusters[c][0]];
                double closestR2 = std::numeric_limits<double>::max();
                for (const auto& [m, group] : targets) {
                    const double r2 = relative2(n, m);
                    if (r2 >= 0 && r2 < closestR2) {
                        closestR2 = r2;
                        root[c] = group;
                    }
                }
                if (root[c] == c) {
                    fNNotAttached++;
                } else {
                    nAttached++;
                }
            }
            fNAttached += nAttached;
        }

        // Tracks in order of their first group, the hits keep the input order
        std::vector<std::vector<int>> tracks;
        std::map<size_t, size_t> trackOfRoot;
        for (size_t c = 0; c < clusters.size(); c++) {
            const size_t r = root[c];
            if (!trackOfRoot.count(r)) {
                trackOfRoot[r] = tracks.size();
                tracks.emplace_back();
            }
            for (size_t k : clusters[c]) tracks[trackOfRoot[r]].push_back(indices[k]);
        }
        for (auto& track : tracks) {
            std::sort(track.begin(), track.end());
            trackEvent->AddTrack(trackEvent->GetNumberOfTracks() + 1, 0);
            for (int n : track) {
                trackEvent->AddHitToTrack(hits.GetPosition(n), hits.GetEnergy(n), hits.GetTime(n),
                                          hits.GetType(n));
            }
        }
        RESTDebug << "TRestDetectorHitsToTrackProcess: " << indices.size() << " hits of type " << type
                  << " give " << tracks.size() << " tracks (" << nAttached << " isolated hits attached)"
                  << RESTendl;
    }

    if (GetVerboseLevel() >= TRestLogManager::REST_Verbose_Level::REST_Debug) trackEvent->PrintOnlyTracks();

    return trackEvent->GetNumberOfTracks() > 0;
}
