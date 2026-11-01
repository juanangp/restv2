#ifndef TRESTTRACKREDUCTIONPROCESS_H
#define TRESTTRACKREDUCTIONPROCESS_H

#include <string>
#include <vector>

#include "TRestEventProcess.h"
#include "TRestHitsAnalysis.h"

class TRestHits;

/// \class TRestTrackReductionProcess
/// \brief Reduces the hits of each track (any type) to nodes, merging the hits closer than a growing
/// distance up to a maximum number of nodes, optionally refined with k-means. The output event holds one
/// track per input track.
class TRestTrackReductionProcess : public TRestEventProcess {
   private:
    /// \brief Nodes of the reduced track, empty if it cannot be reduced (hits of different types).
    std::vector<TRestHitsAnalysis::Node> Reduce(
        const TRestHits& hits, const std::vector<TRestHitsAnalysis::XYZVector>& sigmas) const;

    /// Tracks copied as they are (not reduced) and tracks processed, reported at EndProcess
    long fNotReduced = 0;
    long fNTracks = 0;

   public:
    /// First merging distance
    TRestWithUnits fStartingDistance = 0.5;

    /// The merging distance grows (distanceStepFactor) at least up to this distance
    TRestWithUnits fMinimumDistance = 3;

    /// Factor applied to the merging distance at each step (> 1)
    double fDistanceStepFactor = 1.5;

    /// Maximum number of nodes of a track: the merging distance grows until there are no more
    int fMaxNodes = 30;

    /// If true, the merged nodes are moved to their hits with k-means, merging and moving them again until
    /// the number of nodes does not change
    bool fKMeans = false;

    /// Maximum k-means iterations
    int fMaxIterations = 100;

    TRestTrackReductionProcess();
    TRestTrackReductionProcess(const std::string& instanceName, const YAML::Node& node);
    TRestTrackReductionProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override;

    std::string GetInputEvent() const override { return "TRestTrackEvent"; }
    std::string GetOutputEvent() const override { return "TRestTrackEvent"; }

    std::string GetClassName() const override { return "TRestTrackReductionProcess"; }
};

#endif
