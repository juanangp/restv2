#ifndef TRESTDETECTORHITSTOSIGNALPROCESS_H
#define TRESTDETECTORHITSTOSIGNALPROCESS_H

#include <string>

#include "TRestEventProcess.h"

class TRestDetectorReadout;

/// \class TRestDetectorHitsToSignalProcess
/// \brief Collects the hits of a TRestDetectorHitsEvent in the readout channels: a TRestDetectorSignalEvent
/// with the charge (energy) of each channel versus the arrival time.
///
/// The parameters (YAML keys), the observables and an example are described in the class documentation
/// (TRestDetectorHitsToSignalProcess.cxx).
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
    /// Name of the TRestDetectorMedium that gives the drift velocity (at its drift field). It can be omitted
    /// if the manager contains a single one.
    std::string fMediumName = "";
    /// Drift velocity. If > 0 it overrides the one of the drift medium
    TRestWithUnits fDriftVelocity = 0;  // mm/us
    /// Time bin of the signals: the arrival times are rounded down to multiples of it (0: exact times)
    TRestWithUnits fSampling = 0;  // us

    /// \brief Default constructor (no configuration).
    TRestDetectorHitsToSignalProcess();
    /// \brief Builds the process from an in-memory YAML section (LoadConfig).
    TRestDetectorHitsToSignalProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds the process from the section sectionName of a YAML file (LoadConfig).
    TRestDetectorHitsToSignalProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the parameters from the YAML node.
    void LoadConfig() override;
    /// \brief Gets the readout and the drift velocity (driftVelocity, or that of the medium at its drift
    /// field; written to the metadata) and registers the observable NHitsRejected.
    void InitProcess() override;
    /// \brief Adds the energy of each hit to the signal of its channel at its arrival time (see the class
    /// description).
    /// \return false if the event has no signals.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Nothing to do at the end of the processing.
    void EndProcess() override {}

    /// \brief Input event type, "TRestDetectorHitsEvent".
    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    /// \brief Output event type, "TRestDetectorSignalEvent".
    std::string GetOutputEvent() const override { return "TRestDetectorSignalEvent"; }

    /// \brief Returns the class name, "TRestDetectorHitsToSignalProcess".
    std::string GetClassName() const override { return "TRestDetectorHitsToSignalProcess"; }
};

#endif
