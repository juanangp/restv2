/// \class TRestTrackPathMinimizationProcess
/// Orders the hits of each track of a TRestTrackEvent, of any type (XZ, YZ, XYZ, ...), along the shortest
/// path through all of them (travelling salesman, TRestHitsAnalysis::GetShortestPath), so that the hit
/// order follows the track, e.g. for the length and twist observables of TRestTrackAnalysisProcess. Meant
/// for reduced tracks (TRestTrackReductionProcess), with a few tens of nodes:
/// * Up to maxExactNodes hits (at most 16): the shortest path (Held-Karp dynamic programming, time and
///   memory growing as 2^n).
/// * More hits: the shortest of the nearest neighbour paths starting at every hit (at the farthest hit
///   from the centroid above 200 hits), improved with 2-opt (reversing segments of the path while it gets
///   shorter). Usually the shortest path or close to it, but not always.
///
/// The path is open (from any hit to any other) or, with cyclic, closed (it comes back to the first hit).
/// The distances use the coordinates the track defines. The direction of the path is arbitrary (see
/// TRestTrackLineAnalysisProcess for the origin and end of a track).
///
/// Observable Branches: the largest number of jumps of a track of the event, a jump being a step between
/// consecutive hits of the ordered track longer than branchFactor times the median step of the track
/// (0 for tracks with less than 2 steps or not ordered). 0 for a continuous track; more than 0 for gaps or
/// branches, e.g. two particles or a fluorescence photon joined to the track.
///
/// The output event holds one track per input track (parent ID = ID of the input track) with the same
/// hits (position, energy, time, type and sigma) in the new order, or as they are if the track has hits of
/// different types (the undefined coordinates would mix with the defined ones). The number of tracks not
/// ordered is reported at the end of the processing (and each one in debug).
///
/// ### Parameters
/// * cyclic: closed path (default false).
/// * maxExactNodes: maximum number of hits for the exact shortest path (default 12, at most 16).
/// * branchFactor: a step longer than branchFactor times the median step is a jump (default 3).
///
/// Example:
///
///    trackPathMinimization:
///      class: "TRestTrackPathMinimizationProcess"
///      cyclic: false
///      maxExactNodes: 12
///      branchFactor: 3
///      observables:
///        - "Branches"
///
/// Migrated from the legacy track library (Javier Galan, JuanAn Garcia). Differences: the legacy methods
/// (`minMethod`: default, bruteforce, closestN) are replaced by maxExactNodes; the default method used the
/// Held-Karp solver of Concorde (academic licence, not included), which solves a closed path and then
/// rotated it, now the exact path is open unless cyclic; nearest neighbour is improved with 2-opt;
/// `weightHits` (unused) removed; the output event holds only the tracks of this stage (no levels). The
/// Branches observable comes from the legacy TRestTrackReconnectionProcess (`branches`, not migrated: the
/// path is already the shortest one), which counted the steps longer than mean + nSigmas * sqrt(mean)
/// after breaking and reconnecting the track; the median keeps the jumps themselves from raising the
/// threshold.

#include "TRestTrackPathMinimizationProcess.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "TRestHitsAnalysis.h"
#include "TRestRun.h"
#include "TRestTrack.h"
#include "TRestTrackEvent.h"

static const bool TRestTrackPathMinimizationProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestTrackPathMinimizationProcess;
    reg.RegisterField<P>("cyclic", &P::fCyclic);
    reg.RegisterField<P>("maxExactNodes", &P::fMaxExactNodes);
    reg.RegisterField<P>("branchFactor", &P::fBranchFactor);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestTrackPathMinimizationProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestTrackPathMinimizationProcess>(instanceName, params);
        });
    return true;
}();

// Number of steps between consecutive points longer than factor times the median step
int CountJumps(const std::vector<ROOT::Math::XYZVector>& points, double factor) {
    if (points.size() < 3) return 0;
    std::vector<double> steps;
    for (size_t n = 1; n < points.size(); n++) steps.push_back(std::sqrt((points[n] - points[n - 1]).Mag2()));
    std::vector<double> sorted = steps;
    const size_t half = sorted.size() / 2;
    std::nth_element(sorted.begin(), sorted.begin() + half, sorted.end());
    double median = sorted[half];
    if (sorted.size() % 2 == 0)
        median = (median + *std::max_element(sorted.begin(), sorted.begin() + half)) / 2;
    return (int)std::count_if(steps.begin(), steps.end(), [&](double s) { return s > factor * median; });
}
}  // namespace

TRestTrackPathMinimizationProcess::TRestTrackPathMinimizationProcess() : TRestEventProcess() {
    fName = "TRestTrackPathMinimizationProcess";
}

TRestTrackPathMinimizationProcess::TRestTrackPathMinimizationProcess(const std::string& instanceName,
                                                                     const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestTrackPathMinimizationProcess::TRestTrackPathMinimizationProcess(const std::string& fileName,
                                                                     const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestTrackPathMinimizationProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestTrackPathMinimizationProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestTrackPathMinimizationProcess>(fNode);
    UpdateYAMLFromParams<TRestTrackPathMinimizationProcess>(fNode);
}

void TRestTrackPathMinimizationProcess::InitProcess() {
    fNotOrdered = 0;
    fNTracks = 0;
    if (fMaxExactNodes > 16) {
        RESTWarning << "TRestTrackPathMinimizationProcess: maxExactNodes is at most 16, using 16" << RESTendl;
        fMaxExactNodes = 16;
    }
    if (fMaxExactNodes < 0) fMaxExactNodes = 0;
    if (fBranchFactor <= 1)
        throw std::runtime_error("TRestTrackPathMinimizationProcess: branchFactor must be > 1");
    RegisterObservable("Branches", fBranches);
}

void TRestTrackPathMinimizationProcess::EndProcess() {
    if (fNotOrdered > 0) {
        RESTWarning << "TRestTrackPathMinimizationProcess: " << fNotOrdered << " of " << fNTracks
                    << " tracks could not be ordered (hits of different types), kept as they were"
                    << RESTendl;
    }
}

bool TRestTrackPathMinimizationProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* inputEvent = dynamic_cast<const TRestTrackEvent*>(&input);
    auto* outputEvent = dynamic_cast<TRestTrackEvent*>(&output);
    if (!inputEvent || !outputEvent) {
        throw std::runtime_error("TRestTrackPathMinimizationProcess: input/output is not a TRestTrackEvent");
    }

    outputEvent->Initialize();
    outputEvent->SetEventInfo(inputEvent);
    fBranches = 0;

    for (int t = 0; t < inputEvent->GetNumberOfTracks(); t++) {
        const TRestHits hits = inputEvent->GetTrack(t).GetHits();
        const int trackID = inputEvent->GetTrackID(t);
        const size_t nHits = hits.GetNumberOfHits();
        fNTracks++;

        bool sameType = true;
        for (size_t h = 1; h < nHits; h++) sameType &= hits.GetType(h) == hits.GetType(0);

        std::vector<size_t> path;
        if (sameType) {
            path = TRestHitsAnalysis::GetShortestPath(hits.GetPoints(), fCyclic, fMaxExactNodes);
        } else {
            fNotOrdered++;
            RESTDebug << "TRestTrackPathMinimizationProcess: event " << inputEvent->GetID() << ", track "
                      << trackID << " not ordered (hits of different types), kept as it is" << RESTendl;
            for (size_t h = 0; h < nHits; h++) path.push_back(h);
        }

        outputEvent->AddTrack(outputEvent->GetNumberOfTracks() + 1, trackID);
        for (size_t h : path) {
            outputEvent->AddHitToTrack(hits.GetPosition(h), hits.GetEnergy(h), hits.GetTime(h),
                                       hits.GetType(h), inputEvent->GetSigma(t, (int)h));
        }

        if (sameType) {
            const auto points = hits.GetPoints();
            std::vector<ROOT::Math::XYZVector> ordered;
            for (size_t h : path) ordered.push_back(points[h]);
            fBranches = std::max(fBranches, CountJumps(ordered, fBranchFactor));
        }
    }

    if (GetVerboseLevel() >= TRestLogManager::REST_Verbose_Level::REST_Debug) outputEvent->PrintOnlyTracks();
    return true;
}
