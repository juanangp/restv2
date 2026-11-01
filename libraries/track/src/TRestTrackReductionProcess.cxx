/// \class TRestTrackReductionProcess
/// Reduces each track of a TRestTrackEvent, of any type (XZ, YZ, XYZ, ...), to nodes, merging its hits
/// (TRestHitsAnalysis::MergePoints):
/// 1. The pairs of hits closer than startingDistance are merged (energy weighted position, summed energy)
///    until there are none.
/// 2. The distance is multiplied by distanceStepFactor and step 1 repeated while it is below
///    minimumDistance or the track has more than maxNodes nodes.
/// 3. With kMeans, the nodes are moved to their hits with k-means (each hit goes to its closest node, which
///    moves to the energy weighted mean of its hits, up to maxIterations iterations,
///    TRestHitsAnalysis::KMeansClustering, nodes with no hits dropped), and steps 1 to 3 are repeated on
///    the nodes until their number does not change.
///
/// Each node gets the energy of its hits and, as sigma, their energy weighted spread in each coordinate
/// (including the sigmas of the input hits, e.g. for tracks already reduced). The coordinates the track
/// does not define stay NaN and the nodes have no time (0).
///
/// The output event holds one track per input track (parent ID = ID of the input track): the nodes, or
/// the input track as it is if it has hits of different types (the undefined coordinates would mix with
/// the defined ones). The number of tracks not reduced is reported at the end of the processing (and each
/// one in debug).
///
/// ### Parameters
/// * startingDistance: first merging distance (default 0.5 mm).
/// * minimumDistance: the merging distance grows at least up to this distance (default 3 mm).
/// * distanceStepFactor: factor applied to the merging distance at each step, > 1 (default 1.5).
/// * maxNodes: maximum number of nodes of a track (default 30).
/// * kMeans: refine the nodes with k-means (default false).
/// * maxIterations: maximum k-means iterations (default 100).
///
/// Example:
///
///    trackReduction:
///      class: "TRestTrackReductionProcess"
///      startingDistance: 0.5mm
///      minimumDistance: 3mm
///      distanceStepFactor: 1.5
///      maxNodes: 30
///      kMeans: true
///      maxIterations: 100
///
/// Migrated from the legacy track library (Javier Galan). Differences: `kmeans` and `maxIt` renamed
/// kMeans and maxIterations; the node sigmas are the spread of their hits (legacy averaged the sigmas of
/// the merged hits) and the nodes have no time (legacy energy weighted mean); the output event holds only
/// the tracks of this stage (no levels).

#include "TRestTrackReductionProcess.h"

#include <limits>
#include <stdexcept>

#include "TRestTrack.h"
#include "TRestTrackEvent.h"

static const bool TRestTrackReductionProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestTrackReductionProcess;
    reg.RegisterField<P>("startingDistance", &P::fStartingDistance);
    reg.RegisterField<P>("minimumDistance", &P::fMinimumDistance);
    reg.RegisterField<P>("distanceStepFactor", &P::fDistanceStepFactor);
    reg.RegisterField<P>("maxNodes", &P::fMaxNodes);
    reg.RegisterField<P>("kMeans", &P::fKMeans);
    reg.RegisterField<P>("maxIterations", &P::fMaxIterations);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestTrackReductionProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestTrackReductionProcess>(instanceName, params);
        });
    return true;
}();

using TRestHitsAnalysis::Node;
using TRestHitsAnalysis::XYZVector;

std::vector<XYZVector> NodePositions(const std::vector<Node>& nodes) {
    std::vector<XYZVector> positions;
    for (const auto& node : nodes) positions.push_back(node.position);
    return positions;
}

std::vector<double> NodeEnergies(const std::vector<Node>& nodes) {
    std::vector<double> energies;
    for (const auto& node : nodes) energies.push_back(node.energy);
    return energies;
}

std::vector<XYZVector> NodeSigmas(const std::vector<Node>& nodes) {
    std::vector<XYZVector> sigmas;
    for (const auto& node : nodes) sigmas.push_back(node.sigma);
    return sigmas;
}
}  // namespace

TRestTrackReductionProcess::TRestTrackReductionProcess() : TRestEventProcess() {
    fName = "TRestTrackReductionProcess";
}

TRestTrackReductionProcess::TRestTrackReductionProcess(const std::string& instanceName,
                                                       const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestTrackReductionProcess::TRestTrackReductionProcess(const std::string& fileName,
                                                       const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestTrackReductionProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestTrackReductionProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestTrackReductionProcess>(fNode);
    UpdateYAMLFromParams<TRestTrackReductionProcess>(fNode);
}

void TRestTrackReductionProcess::InitProcess() {
    fNotReduced = 0;
    fNTracks = 0;
    if (fStartingDistance <= 0 || fDistanceStepFactor <= 1) {
        throw std::runtime_error(
            "TRestTrackReductionProcess: startingDistance must be > 0 and distanceStepFactor > 1");
    }
    if (fMaxNodes < 1) {
        RESTWarning << "TRestTrackReductionProcess: maxNodes must be at least 1, using 1" << RESTendl;
        fMaxNodes = 1;
    }
    if (fKMeans && fMaxIterations < 1) {
        RESTWarning << "TRestTrackReductionProcess: maxIterations must be at least 1, using 1" << RESTendl;
        fMaxIterations = 1;
    }
}

void TRestTrackReductionProcess::EndProcess() {
    if (fNotReduced > 0) {
        RESTWarning << "TRestTrackReductionProcess: " << fNotReduced << " of " << fNTracks
                    << " tracks could not be reduced (hits of different types), kept as they were"
                    << RESTendl;
    }
}

std::vector<Node> TRestTrackReductionProcess::Reduce(const TRestHits& hits,
                                                     const std::vector<XYZVector>& sigmas) const {
    if (hits.GetNumberOfHits() == 0) return {};
    for (size_t h = 1; h < hits.GetNumberOfHits(); h++) {
        if (hits.GetType(h) != hits.GetType(0)) return {};
    }

    const auto points = hits.GetPoints();
    const auto energies = hits.GetEnergies();
    auto merge = [this](const std::vector<XYZVector>& p, const std::vector<double>& e,
                        const std::vector<XYZVector>& s) {
        return TRestHitsAnalysis::MergePoints(p, e, fStartingDistance, fMinimumDistance, fDistanceStepFactor,
                                              fMaxNodes, s);
    };

    std::vector<Node> nodes = merge(points, energies, sigmas);
    if (!fKMeans) return nodes;

    // As legacy: k-means of the hits around the nodes, merging the nodes again until their number is stable
    size_t nodesBefore = 0;
    do {
        nodesBefore = nodes.size();
        auto moved =
            TRestHitsAnalysis::KMeansClustering(points, energies, NodePositions(nodes), fMaxIterations);
        if (moved.empty()) break;  // less than 2 nodes
        nodes = merge(NodePositions(moved), NodeEnergies(moved), NodeSigmas(moved));
    } while (nodes.size() != nodesBefore);
    return nodes;
}

bool TRestTrackReductionProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* inputEvent = dynamic_cast<const TRestTrackEvent*>(&input);
    auto* outputEvent = dynamic_cast<TRestTrackEvent*>(&output);
    if (!inputEvent || !outputEvent) {
        throw std::runtime_error("TRestTrackReductionProcess: input/output is not a TRestTrackEvent");
    }

    outputEvent->Initialize();
    outputEvent->SetEventInfo(inputEvent);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (int t = 0; t < inputEvent->GetNumberOfTracks(); t++) {
        const TRestHits hits = inputEvent->GetTrack(t).GetHits();
        const int trackID = inputEvent->GetTrackID(t);
        std::vector<XYZVector> sigmas;
        for (size_t h = 0; h < hits.GetNumberOfHits(); h++) sigmas.push_back(inputEvent->GetSigma(t, (int)h));
        const auto nodes = Reduce(hits, sigmas);
        fNTracks++;

        if (nodes.empty()) {
            fNotReduced++;
            RESTDebug << "TRestTrackReductionProcess: event " << inputEvent->GetID() << ", track " << trackID
                      << " not reduced (hits of different types), kept as it is" << RESTendl;
            outputEvent->AddTrack(outputEvent->GetNumberOfTracks() + 1, trackID, hits, sigmas);
            continue;
        }

        const auto type = hits.GetType(0);
        outputEvent->AddTrack(outputEvent->GetNumberOfTracks() + 1, trackID);
        for (const auto& node : nodes) {
            // Undefined coordinate back to NaN, as in the hits
            const XYZVector position(TRestHitsData::IsDefined(type, 0) ? node.position.X() : nan,
                                     TRestHitsData::IsDefined(type, 1) ? node.position.Y() : nan,
                                     TRestHitsData::IsDefined(type, 2) ? node.position.Z() : nan);
            outputEvent->AddHitToTrack(position, node.energy, 0, type, node.sigma);
        }
        RESTDebug << "TRestTrackReductionProcess: track " << trackID << " with " << hits.GetNumberOfHits()
                  << " hits reduced to " << nodes.size() << " nodes" << RESTendl;
    }

    if (GetVerboseLevel() >= TRestLogManager::REST_Verbose_Level::REST_Debug) outputEvent->PrintOnlyTracks();
    return true;
}
