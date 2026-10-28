#ifndef TRESTDETECTORHITSTOSIGNALPROCESS_H
#define TRESTDETECTORHITSTOSIGNALPROCESS_H

#include <string>

#include "TRestEventProcess.h"

class TRestDetectorReadout;

/// \class TRestDetectorHitsToSignalProcess
/// \brief Collects the hits of a TRestDetectorHitsEvent in the readout channels: a TRestDetectorSignalEvent
/// with the charge (energy) of each channel versus the arrival time.
class TRestDetectorHitsToSignalProcess : public TRestEventProcess {
   private:
    const TRestDetectorReadout* fReadout = nullptr;
    /// Drift velocity used (driftVelocity or the one of the drift medium), mm/us
    double fVelocity = 0;  //!
    /// Hits outside the drift volume or without channel in the event
    int fNHitsRejected = 0;  //!

   public:
    /// Name of the readout (in TRestDetectorReadoutManager). It can be omitted if the manager contains a
    /// single readout.
    std::string fReadoutName = "";
    /// Name of the TRestDetectorDrift that gives the drift velocity. It can be omitted if the manager
    /// contains a single medium.
    std::string fDriftMediumName = "";
    /// Drift velocity. If > 0 it overrides the one of the drift medium
    TRestWithUnits fDriftVelocity = 0;  // mm/us
    /// Time bin of the signals: the arrival times are rounded down to multiples of it (0: exact times)
    TRestWithUnits fSampling = 0;  // us

    TRestDetectorHitsToSignalProcess();
    TRestDetectorHitsToSignalProcess(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorHitsToSignalProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    std::string GetOutputEvent() const override { return "TRestDetectorSignalEvent"; }

    std::string GetClassName() const override { return "TRestDetectorHitsToSignalProcess"; }
};

#endif
