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

    TRestDetectorLightAttenuationProcess();
    TRestDetectorLightAttenuationProcess(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorLightAttenuationProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }

    std::string GetClassName() const override { return "TRestDetectorLightAttenuationProcess"; }
};

#endif
