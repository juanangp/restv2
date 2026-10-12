#ifndef TRESTRAWSIGNALADDNOISPROCESS_H
#define TRESTRAWSIGNALADDNOISPROCESS_H

#include <utility>
#include <vector>

#include "TRestEventProcess.h"
#include "TRestMetadata.h"
#include "TRestRawSignalEvent.h"

class TRestNoiseParameters : public TRestMetadata {
   public:

    double fNoiseAmplitude = 10; // ADC/Energy units
    double fNoisePeriod = 50; //index units

    std::string fNoiseMethod = "white"; //white and sinusoidal(sin) noise implemented

    TRestNoiseParameters();
    TRestNoiseParameters(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestNoiseParameters"; }

};

/// \class TRestRawSignalAddNoiseProcess
class TRestRawSignalAddNoiseProcess : public TRestEventProcess {

   public:

    std::vector<TRestNoiseParameters> fNoiseParams;    

    TRestRawSignalAddNoiseProcess();
    TRestRawSignalAddNoiseProcess(const std::string& instanceName, const YAML::Node& node);
    TRestRawSignalAddNoiseProcess(const std::string& fileName, const std::string& sectionName);

    virtual void LoadConfig() override;
    void InitProcess() override {}
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }

    std::string GetClassName() const override { return "TRestRawSignalAddNoiseProcess"; }
};

#endif
