#ifndef TRESTTRACKPATHMINIMIZATIONPROCESS_H
#define TRESTTRACKPATHMINIMIZATIONPROCESS_H

#include <string>

#include "TRestEventProcess.h"

/// \class TRestTrackPathMinimizationProcess
/// \brief Orders the hits (nodes) of each track along the shortest path through all of them, e.g. after
/// TRestTrackReductionProcess. The output event holds one track per input track.
///
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
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

    /// \brief Default constructor, with the default parameters.
    TRestTrackPathMinimizationProcess();
    /// \brief Constructor from the YAML section `node` of the instance `instanceName` (calls LoadConfig).
    TRestTrackPathMinimizationProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructor from the section `sectionName` of the YAML file `fileName` (calls LoadConfig).
    TRestTrackPathMinimizationProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the registered fields from the YAML section.
    void LoadConfig() override;
    /// \brief Checks the parameters (maxExactNodes clipped to [0, 16], branchFactor > 1 or throws) and
    /// registers the observable Branches.
    void InitProcess() override;
    /// \brief Writes to the output event one ordered track per input track and computes Branches.
    /// \return Always true.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Warns of the number of tracks that could not be ordered.
    void EndProcess() override;

    std::string GetInputEvent() const override { return "TRestTrackEvent"; }
    std::string GetOutputEvent() const override { return "TRestTrackEvent"; }

    std::string GetClassName() const override { return "TRestTrackPathMinimizationProcess"; }
};

#endif
