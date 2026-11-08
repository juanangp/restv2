#ifndef TRESTTRACKLINEARIZATIONPROCESS_H
#define TRESTTRACKLINEARIZATIONPROCESS_H

#include <string>
#include <vector>

#include "TRestEventProcess.h"
#include "TRestHitsAnalysis.h"

class TRestHits;

/// \class TRestTrackLinearizationProcess
/// \brief Reduces each track (any type) to nodes along the line that best fits its hits, e.g. for alpha
/// tracks. The output event holds one track per input track.
class TRestTrackLinearizationProcess : public TRestEventProcess {
   private:
    /// \brief Nodes of the linearized track, empty if it cannot be linearized (no energy).
    std::vector<TRestHitsAnalysis::Node> Linearize(const TRestHits& hits) const;

    /// Tracks copied as they are (not linearized) and tracks processed, reported at EndProcess
    long fNotLinearized = 0;
    long fNTracks = 0;

   public:
    /// Number of nodes along the line (some can be dropped if no hit is closest to them). With
    /// nodeDistance, the maximum number of nodes
    int fMaxNodes = 6;

    /// If > 0, distance between nodes: the number of nodes is length / nodeDistance + 1, between 2 and
    /// maxNodes. 0: always maxNodes
    TRestWithUnits fNodeDistance = 0;

    /// k-means iterations moving the nodes to their hits (1 as legacy)
    int fMaxIterations = 1;

    /// If true, the first and last nodes stay at the ends of the line, with no energy. Adding 2 nodes
    /// to maxNodes is recommended, the nodes are not pulled towards the centre of the track (more accurate
    /// length, worse resolution)
    bool fFixBoundaries = false;

    /// \brief Default constructor, with the default parameters.
    TRestTrackLinearizationProcess();
    /// \brief Constructor from the YAML section `node` of the instance `instanceName` (calls LoadConfig).
    TRestTrackLinearizationProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructor from the section `sectionName` of the YAML file `fileName` (calls LoadConfig).
    TRestTrackLinearizationProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the registered fields from the YAML section.
    void LoadConfig() override;
    /// \brief Resets the counters and checks the parameters (maxNodes >= 2, maxIterations >= 1).
    void InitProcess() override;
    /// \brief Writes to the output event one linearized track per input track.
    /// \return Always true.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Warns of the number of tracks that could not be linearized.
    void EndProcess() override;

    std::string GetInputEvent() const override { return "TRestTrackEvent"; }
    std::string GetOutputEvent() const override { return "TRestTrackEvent"; }

    std::string GetClassName() const override { return "TRestTrackLinearizationProcess"; }
};

#endif
