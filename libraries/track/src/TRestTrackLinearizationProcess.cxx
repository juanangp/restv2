/// \class TRestTrackLinearizationProcess
/// Reduces each track of a TRestTrackEvent, of any type (XZ, YZ, XYZ, ...), to nodes along a line, meant
/// for linear tracks such as alpha tracks:
/// 1. The hits are fitted to a line: through their energy weighted mean, along the direction of largest
///    spread (energy weighted principal axis, TRestHitsAnalysis::GetLine), in the coordinates the track
///    defines, between the extreme projections of the hits on it.
/// 2. Equally spaced nodes are placed along the fitted line: maxNodes or, with nodeDistance, one every
///    nodeDistance (length / nodeDistance + 1 nodes, between 2 and maxNodes), so short tracks get less
///    nodes than long ones.
/// 3. Each hit goes to its closest node, which moves to the energy weighted mean of its hits and gets
///    their energy and, as sigma, their energy weighted spread in each coordinate (maxIterations k-means
///    iterations, TRestHitsAnalysis::KMeansClustering). Nodes with no hits are dropped. With
///    fixBoundaries, the first and last nodes stay at the ends of the line, with no hits, no energy and
///    no sigma.
///
/// The output event holds one track per input track (parent ID = ID of the input track): the nodes, or
/// the input track as it is if it cannot be linearized: no energy, maxNodes < 2 or hits of different
/// types in the track (the undefined coordinates would mix with the defined ones). The number of tracks
/// not linearized is reported at the end of the processing (and each one in debug).
///
/// ### Parameters
/// * maxNodes: number of nodes (default 6), the maximum with nodeDistance.
/// * nodeDistance: distance between nodes, 0 (default) for always maxNodes.
/// * maxIterations: k-means iterations (default 1, as legacy). More iterations move the nodes closer to
///   the hits, with less influence of the fitted line.
/// * fixBoundaries: fix the first and last nodes at the ends of the line (default false). Adding 2 nodes
///   to maxNodes is recommended: the end nodes are not pulled towards the centre of the track, which
///   gives a more accurate track length, but a worse length resolution.
///
/// Example:
///
///    trackLinearization:
///      class: "TRestTrackLinearizationProcess"
///      maxNodes: 8
///      nodeDistance: 2mm
///      maxIterations: 1
///      fixBoundaries: false
///
/// Migrated from the legacy track library (JuanAn Garcia, 2022). Differences: any track type (legacy only
/// XZ, YZ and XY, the others were kept as they were); the line is the principal axis, which minimizes the
/// distances to it, instead of a fit of one coordinate against the other in both orientations keeping the
/// smallest chi2 (legacy repeated each hit in a TGraph in proportion to its energy), similar for linear
/// tracks; nodeDistance, maxIterations and the node sigmas are new; the output event holds only the
/// tracks of this stage (no levels).

#include "TRestTrackLinearizationProcess.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "TRestTrack.h"
#include "TRestTrackEvent.h"

static const bool TRestTrackLinearizationProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestTrackLinearizationProcess;
    reg.RegisterField<P>("maxNodes", &P::fMaxNodes);
    reg.RegisterField<P>("nodeDistance", &P::fNodeDistance);
    reg.RegisterField<P>("maxIterations", &P::fMaxIterations);
    reg.RegisterField<P>("fixBoundaries", &P::fFixBoundaries);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestTrackLinearizationProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestTrackLinearizationProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestTrackLinearizationProcess::TRestTrackLinearizationProcess() : TRestEventProcess() {
    fName = "TRestTrackLinearizationProcess";
}

TRestTrackLinearizationProcess::TRestTrackLinearizationProcess(const std::string& instanceName,
                                                               const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestTrackLinearizationProcess::TRestTrackLinearizationProcess(const std::string& fileName,
                                                               const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestTrackLinearizationProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestTrackLinearizationProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestTrackLinearizationProcess>(fNode);
    UpdateYAMLFromParams<TRestTrackLinearizationProcess>(fNode);
}

void TRestTrackLinearizationProcess::InitProcess() {
    fNotLinearized = 0;
    fNTracks = 0;
    if (fMaxNodes < 2) {
        RESTError << "TRestTrackLinearizationProcess: maxNodes must be at least 2, the tracks are not linearized"
                  << RESTendl;
    }
    if (fMaxIterations < 1) {
        RESTWarning << "TRestTrackLinearizationProcess: maxIterations must be at least 1, using 1" << RESTendl;
        fMaxIterations = 1;
    }
}

void TRestTrackLinearizationProcess::EndProcess() {
    if (fNotLinearized > 0) {
        RESTWarning << "TRestTrackLinearizationProcess: " << fNotLinearized << " of " << fNTracks
                    << " tracks could not be linearized (no energy or hits of different types), kept as they were"
                    << RESTendl;
    }
}

std::vector<TRestHitsAnalysis::Node> TRestTrackLinearizationProcess::Linearize(const TRestHits& hits) const {
    if (fMaxNodes < 2 || hits.GetNumberOfHits() == 0) return {};
    for (size_t h = 1; h < hits.GetNumberOfHits(); h++) {
        if (hits.GetType(h) != hits.GetType(0)) return {};
    }

    const auto points = hits.GetPoints();
    const auto energies = hits.GetEnergies();
    TRestHitsAnalysis::XYZVector first, last;
    if (!TRestHitsAnalysis::GetLine(points, energies, first, last)) return {};

    int nodes = fMaxNodes;
    if (fNodeDistance > 0) {
        nodes = std::clamp((int)std::lround((last - first).R() / fNodeDistance) + 1, 2, fMaxNodes);
    }
    std::vector<TRestHitsAnalysis::XYZVector> line(nodes);
    for (int n = 0; n < nodes; n++) line[n] = first + (double)n / (nodes - 1) * (last - first);

    return TRestHitsAnalysis::KMeansClustering(points, energies, line, fMaxIterations, fFixBoundaries);
}

bool TRestTrackLinearizationProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* inputEvent = dynamic_cast<const TRestTrackEvent*>(&input);
    auto* outputEvent = dynamic_cast<TRestTrackEvent*>(&output);
    if (!inputEvent || !outputEvent) {
        throw std::runtime_error("TRestTrackLinearizationProcess: input/output is not a TRestTrackEvent");
    }

    // Input and output are the same event when chained after another track process: work on a copy, the
    // output is cleared below
    TRestTrackEvent inputCopy;
    if (&input == &output) {
        inputCopy.CopyFrom(&input);
        inputEvent = &inputCopy;
    }

    outputEvent->Initialize();
    outputEvent->SetEventInfo(inputEvent);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (int t = 0; t < inputEvent->GetNumberOfTracks(); t++) {
        const TRestHits hits = inputEvent->GetTrack(t).GetHits();
        const int trackID = inputEvent->GetTrackID(t);
        const auto nodes = Linearize(hits);
        fNTracks++;

        if (nodes.empty()) {
            fNotLinearized++;
            RESTDebug << "TRestTrackLinearizationProcess: event " << inputEvent->GetID() << ", track " << trackID
                      << " not linearized (no energy or hits of different types), kept as it is" << RESTendl;
            std::vector<TRestHitsAnalysis::XYZVector> sigmas;
            for (size_t h = 0; h < hits.GetNumberOfHits(); h++) sigmas.push_back(inputEvent->GetSigma(t, (int)h));
            outputEvent->AddTrack(outputEvent->GetNumberOfTracks() + 1, trackID, hits, sigmas);
            continue;
        }

        const auto type = hits.GetType(0);
        outputEvent->AddTrack(outputEvent->GetNumberOfTracks() + 1, trackID);
        for (const auto& node : nodes) {
            // Undefined coordinate back to NaN, as in the hits
            const TRestHitsAnalysis::XYZVector position(TRestHitsData::IsDefined(type, 0) ? node.position.X() : nan,
                                                        TRestHitsData::IsDefined(type, 1) ? node.position.Y() : nan,
                                                        TRestHitsData::IsDefined(type, 2) ? node.position.Z() : nan);
            outputEvent->AddHitToTrack(position, node.energy, 0, type, node.sigma);
        }
        RESTDebug << "TRestTrackLinearizationProcess: track " << trackID << " with " << hits.GetNumberOfHits()
                  << " hits reduced to " << nodes.size() << " nodes" << RESTendl;
    }

    if (GetVerboseLevel() >= TRestLogManager::REST_Verbose_Level::REST_Debug) outputEvent->PrintOnlyTracks();
    return true;
}
