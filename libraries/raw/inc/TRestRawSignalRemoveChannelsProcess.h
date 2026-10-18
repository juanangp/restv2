#ifndef TRESTRAWSIGNALREMOVECHANNELSPROCESS_H
#define TRESTRAWSIGNALREMOVECHANNELSPROCESS_H

#include <utility>
#include <vector>

#include "TRestEventProcess.h"

class TRestRawSignalEvent;

/// \class TRestRawSignalRemoveChannelsProcess
/// \brief Removes a selection of signal IDs from a TRestRawSignalEvent.
///
/// Useful to evaluate the impact of dead channels on the detector response.
class TRestRawSignalRemoveChannelsProcess : public TRestEventProcess {
   public:
    /// Individual signal IDs to remove.
    std::vector<int> fChannelIds;

    /// Inclusive [min, max] signal ID ranges to remove.
    std::vector<std::pair<int, int>> fChannelRange;

    TRestRawSignalRemoveChannelsProcess();
    TRestRawSignalRemoveChannelsProcess(const std::string& instanceName, const YAML::Node& node);
    TRestRawSignalRemoveChannelsProcess(const std::string& fileName, const std::string& sectionName);

    virtual void LoadConfig() override;
    void InitProcess() override {}
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetClassName() const override { return "TRestRawSignalRemoveChannelsProcess"; }

    /// \brief True if `signalID` is in fChannelIds or inside any fChannelRange.
    bool IsRemoved(int signalID) const;
};

#endif
