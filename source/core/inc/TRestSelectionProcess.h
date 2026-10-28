#ifndef TRESTSELECTIONPROCESS_H
#define TRESTSELECTIONPROCESS_H

#include <string>

#include "TRestEventProcess.h"

/// \class TRestSelectionProcess
/// \brief Selects the events that pass its TRestCut (`cuts`), for any event type: the event is copied
/// unchanged. The cuts are on the observables of the previous processes or of the input file.
class TRestSelectionProcess : public TRestEventProcess {
   public:
    /// Event type (e.g. TRestTrackEvent). Empty: the output event of the previous process
    std::string fEvent = "";

    TRestSelectionProcess();
    TRestSelectionProcess(const std::string& instanceName, const YAML::Node& node);
    TRestSelectionProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;

    std::string GetInputEvent() const override { return fEvent; }
    std::string GetOutputEvent() const override { return fEvent; }

    std::string GetClassName() const override { return "TRestSelectionProcess"; }
};

#endif
