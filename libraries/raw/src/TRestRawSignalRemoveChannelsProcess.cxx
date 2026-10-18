#include "TRestRawSignalRemoveChannelsProcess.h"

#include "TRestRawSignalEvent.h"

#include <algorithm>
#include <stdexcept>


//---Example---
//removeChannels:
//  class: "TRestRawSignalRemoveChannelsProcess"
//  name: "rmChannels"
//  channelIds: [17, 19, 27]
//  channelRange:
//    - [67, 76]
//

static const bool TRestRawSignalRemoveChannelsProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestRawSignalRemoveChannelsProcess>(
        "channelIds", &TRestRawSignalRemoveChannelsProcess::fChannelIds);
    reg.RegisterField<TRestRawSignalRemoveChannelsProcess>(
        "channelRange", &TRestRawSignalRemoveChannelsProcess::fChannelRange);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestRawSignalRemoveChannelsProcess",
        [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestRawSignalRemoveChannelsProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestRawSignalRemoveChannelsProcess::TRestRawSignalRemoveChannelsProcess() : TRestEventProcess() {
    fName = "TRestRawSignalRemoveChannelsProcess";
}

TRestRawSignalRemoveChannelsProcess::TRestRawSignalRemoveChannelsProcess(const std::string& instanceName,
                                                                         const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestRawSignalRemoveChannelsProcess::TRestRawSignalRemoveChannelsProcess(const std::string& fileName,
                                                                         const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestRawSignalRemoveChannelsProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestRawSignalRemoveChannelsProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestRawSignalRemoveChannelsProcess>(fNode);

    fChannelRange.erase(std::remove_if(fChannelRange.begin(), fChannelRange.end(),
                                       [this](const std::pair<int, int>& r) {
                                           if (r.first <= r.second) return false;
                                           RESTWarning << "TRestRawSignalRemoveChannelsProcess: range ["
                                                       << r.first << ", " << r.second
                                                       << "] is inverted and will be ignored" << RESTendl;
                                           return true;
                                       }),
                        fChannelRange.end());

    UpdateYAMLFromParams<TRestRawSignalRemoveChannelsProcess>(fNode);
}

bool TRestRawSignalRemoveChannelsProcess::IsRemoved(int signalID) const {
    if (std::find(fChannelIds.begin(), fChannelIds.end(), signalID) != fChannelIds.end()) return true;
    for (const auto& [lo, hi] : fChannelRange) {
        if (signalID >= lo && signalID <= hi) return true;
    }
    return false;
}

bool TRestRawSignalRemoveChannelsProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* inEvent = dynamic_cast<const TRestRawSignalEvent*>(&input);
    auto* outEvent = dynamic_cast<TRestRawSignalEvent*>(&output);
    if (!inEvent || !outEvent) {
        throw std::runtime_error(
            "TRestRawSignalRemoveChannelsProcess: input/output is not a TRestRawSignalEvent");
    }

    outEvent->CopyFrom(&input);

    const int nSignals = outEvent->GetNumberOfSignals();
    for (int s = nSignals - 1; s >= 0; --s) {
        const auto& sig = outEvent->GetSignal(s);
        
        if (IsRemoved(sig.GetSignalID())) {
            outEvent->RemoveSignal(s); 
        }
    }

    outEvent->RefreshViews();

    return true;
}
