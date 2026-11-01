#ifndef TRESTDETECTORHITSTOTRACKPROCESS_H
#define TRESTDETECTORHITSTOTRACKPROCESS_H

#include <array>
#include <string>

#include "TRestEventProcess.h"

/// \class TRestDetectorHitsToTrackProcess
/// \brief Transforms a TRestDetectorHitsEvent into a TRestTrackEvent, grouping in tracks the hits closer
/// than clusterDistance. Optionally, the isolated low energy hits are attached to a track with a
/// distance per coordinate (time walk).
class TRestDetectorHitsToTrackProcess : public TRestEventProcess {
   private:
    /// Isolated low energy hits attached to a track and with no hit close enough, reported at EndProcess
    long fNAttached = 0;
    long fNNotAttached = 0;

   public:
    /// Distance at which two hits are considered to belong to the same track
    TRestWithUnits fClusterDistance = 3;

    /// If > 0, each isolated hit (track of one hit) with less energy joins the track of a hit closer than
    /// attachDistance in each coordinate (time walk: small pulses delayed). 0: disabled
    double fAttachEnergy = 0;

    /// Maximum distance in X, Y and Z of an isolated low energy hit to the hit of the track it is attached to
    std::array<TRestWithUnits, 3> fAttachDistance = {3.0, 3.0, 10.0};

    TRestDetectorHitsToTrackProcess();
    TRestDetectorHitsToTrackProcess(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorHitsToTrackProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override;

    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    std::string GetOutputEvent() const override { return "TRestTrackEvent"; }

    std::string GetClassName() const override { return "TRestDetectorHitsToTrackProcess"; }
};

#endif
