#ifndef TRESTRAWZEROSUPRESSIONTORAWPROCESS_H
#define TRESTRAWZEROSUPRESSIONTORAWPROCESS_H

#include "TRestEventProcess.h"

class TRestRawSignalEvent;

class TRestRawZeroSupressionToRawProcess : public TRestEventProcess {

   public:

    TRestRawZeroSupressionToRawProcess();
    TRestRawZeroSupressionToRawProcess(const std::string& instanceName, const YAML::Node& node);
    TRestRawZeroSupressionToRawProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads input file names and conversion options from YAML.
    virtual void LoadConfig() override;
    /// \brief Opens input ROOT resources and connects branch addresses.
    void InitProcess() override;
    /// \brief Converts one FemDAQ entry into a TRestRawSignalEvent payload.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Closes input resources and updates run-level timestamps.
    void EndProcess() override;

    static void ZeroSuppressionToRaw(TRestRawSignalEvent* rawEvent);

    std::string GetInputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }

    std::string GetClassName() const override { return "TRestRawZeroSupressionToRawProcess"; }
};



#endif
