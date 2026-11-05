#ifndef TRESTTRACKPATHMINIMIZATIONPROCESS_H
#define TRESTTRACKPATHMINIMIZATIONPROCESS_H

#include <string>

#include "TRestEventProcess.h"

/// \class TRestTrackPathMinimizationProcess
/// \brief Orders the hits (nodes) of each track along the shortest path through all of them, e.g. after
/// TRestTrackReductionProcess. The output event holds one track per input track.
class TRestTrackPathMinimizationProcess : public TRestEventProcess {
   private:
    /// Tracks copied as they are (not ordered) and tracks processed, reported at EndProcess
    long fNotOrdered = 0;
    long fNTracks = 0;

    /// Observable: largest number of jumps (steps longer than branchFactor times the median step) of a
    /// track of the event
    int fBranches = 0;

   public:
    /// If true, the path comes back to its first hit (closed); otherwise it goes from any hit to any other
    bool fCyclic = false;

    /// Tracks with up to this number of hits (at most 16) get the shortest path (exact); longer tracks a
    /// heuristic one (nearest neighbour + 2-opt)
    int fMaxExactNodes = 12;

    /// A step between consecutive hits of the ordered track is a jump (Branches observable) if it is
    /// longer than branchFactor times the median step of the track
    double fBranchFactor = 3;

    TRestTrackPathMinimizationProcess();
    TRestTrackPathMinimizationProcess(const std::string& instanceName, const YAML::Node& node);
    TRestTrackPathMinimizationProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override;

    std::string GetInputEvent() const override { return "TRestTrackEvent"; }
    std::string GetOutputEvent() const override { return "TRestTrackEvent"; }

    std::string GetClassName() const override { return "TRestTrackPathMinimizationProcess"; }
};

#endif
