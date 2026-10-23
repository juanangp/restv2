#ifndef TRESTDETECTORHITSTOTRACKPROCESS_H
#define TRESTDETECTORHITSTOTRACKPROCESS_H

#include <string>

#include "TRestEventProcess.h"

/// \class TRestDetectorHitsToTrackProcess
/// \brief Transforms a TRestDetectorHitsEvent into a TRestTrackEvent, grouping in tracks the hits closer
/// than clusterDistance.
class TRestDetectorHitsToTrackProcess : public TRestEventProcess {
   public:
    /// Distance at which two hits are considered to belong to the same track
    TRestWithUnits fClusterDistance = 3;

    TRestDetectorHitsToTrackProcess();
    TRestDetectorHitsToTrackProcess(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorHitsToTrackProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override {}
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    std::string GetOutputEvent() const override { return "TRestTrackEvent"; }

    std::string GetClassName() const override { return "TRestDetectorHitsToTrackProcess"; }
};

#endif
