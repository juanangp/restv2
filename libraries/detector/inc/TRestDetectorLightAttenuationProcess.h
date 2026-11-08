#ifndef TRESTDETECTORLIGHTATTENUATIONPROCESS_H
#define TRESTDETECTORLIGHTATTENUATIONPROCESS_H

#include <string>

#include "TRestEventProcess.h"

class TRestDetectorLightResponse;
class TRestDetectorReadout;

/// \class TRestDetectorLightAttenuationProcess
/// \brief Propagates the light of the energy deposits of a scintillator to its readout (e.g. the PMT of a
/// veto): the energy of each hit is attenuated and its time delayed with the distance to the readout of its
/// channel, with the light response of the medium.
///
/// The parameters (YAML keys), the observables and an example are described in the class documentation
/// (TRestDetectorLightAttenuationProcess.cxx).
class TRestDetectorLightAttenuationProcess : public TRestEventProcess {
   private:
    const TRestDetectorReadout* fReadout = nullptr;
    const TRestDetectorLightResponse* fLight = nullptr;

    /// Observables
    int fNHitsWithoutChannel = 0;  //!

   public:
    /// Name of the readout (in TRestDetectorReadoutManager) that gives the channel of each hit and its
    /// distance to the readout. It can be omitted if the manager contains a single readout.
    std::string fReadoutName = "";

    /// Name of the TRestDetectorMedium with the light response. It can be omitted if the manager contains a
    /// single medium.
    std::string fMediumName = "";

    /// \brief Default constructor (no configuration).
    TRestDetectorLightAttenuationProcess();
    /// \brief Builds the process from an in-memory YAML section (LoadConfig).
    TRestDetectorLightAttenuationProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds the process from the section sectionName of a YAML file (LoadConfig).
    TRestDetectorLightAttenuationProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the parameters from the YAML node.
    void LoadConfig() override;
    /// \brief Gets the readout and the light response of the medium (attenuationLength and
    /// effectiveLightSpeed > 0 needed) and registers the observable NHitsWithoutChannel.
    void InitProcess() override;
    /// \brief Attenuates and delays each hit with its distance to the readout of its channel.
    /// \return false if the event has no hits.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Nothing to do at the end of the processing.
    void EndProcess() override {}

    /// \brief Input event type, "TRestDetectorHitsEvent".
    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    /// \brief Output event type, "TRestDetectorHitsEvent".
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }

    /// \brief Returns the class name, "TRestDetectorLightAttenuationProcess".
    std::string GetClassName() const override { return "TRestDetectorLightAttenuationProcess"; }
};

#endif
