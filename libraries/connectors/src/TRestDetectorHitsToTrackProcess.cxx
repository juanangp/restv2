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
/// Events without tracks are rejected.
///
/// Example:
///
///    hitsToTrack:
///      class: "TRestDetectorHitsToTrackProcess"
///      clusterDistance: "2.5mm"
///
/// Migrated from the legacy connectors library (Javier Gracia, Javier Galan).

#include "TRestDetectorHitsToTrackProcess.h"

#include <array>
#include <map>
#include <stdexcept>
#include <vector>

#include "TRestDetectorHitsEvent.h"
#include "TRestHitsAnalysis.h"
#include "TRestTrackEvent.h"

static const bool TRestDetectorHitsToTrackProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorHitsToTrackProcess;
    reg.RegisterField<P>("clusterDistance", &P::fClusterDistance);
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

        std::vector<ROOT::Math::XYZVector> points;
        points.reserve(indices.size());
        for (int n : indices) points.push_back(hits.GetPosition(n));

        const auto clusters = TRestHitsAnalysis::FindClusters(points, fClusterDistance, coordinates);
        for (const auto& cluster : clusters) {
            trackEvent->AddTrack(trackEvent->GetNumberOfTracks() + 1, 0);
            for (size_t k : cluster) {
                const int n = indices[k];
                trackEvent->AddHitToTrack(hits.GetPosition(n), hits.GetEnergy(n), hits.GetTime(n),
                                          hits.GetType(n));
            }
        }
        RESTDebug << "TRestDetectorHitsToTrackProcess: " << indices.size() << " hits of type " << type
                  << " give " << clusters.size() << " tracks" << RESTendl;
    }

    if (GetVerboseLevel() >= TRestLogManager::REST_Verbose_Level::REST_Debug) trackEvent->PrintOnlyTracks();

    return trackEvent->GetNumberOfTracks() > 0;
}
