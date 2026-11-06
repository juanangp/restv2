#include "TRestRawSignalChannelSelectionProcess.h"

#include <algorithm>
#include <stdexcept>

#include "TRestRawSignalEvent.h"

namespace {
/// True if `id` is in `ids` or inside any of `ranges`
bool InSelection(int id, const std::vector<int>& ids, const std::vector<std::pair<int, int>>& ranges) {
    if (std::find(ids.begin(), ids.end(), id) != ids.end()) return true;
    for (const auto& [lo, hi] : ranges) {
        if (id >= lo && id <= hi) return true;
    }
    return false;
}
}  // namespace

static const bool TRestRawSignalChannelSelectionProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestRawSignalChannelSelectionProcess>(
        "removeChannelIds", &TRestRawSignalChannelSelectionProcess::fRemoveChannelIds);
    reg.RegisterField<TRestRawSignalChannelSelectionProcess>(
        "removeChannelRange", &TRestRawSignalChannelSelectionProcess::fRemoveChannelRange);
    reg.RegisterField<TRestRawSignalChannelSelectionProcess>(
        "keepChannelIds", &TRestRawSignalChannelSelectionProcess::fKeepChannelIds);
    reg.RegisterField<TRestRawSignalChannelSelectionProcess>(
        "keepChannelRange", &TRestRawSignalChannelSelectionProcess::fKeepChannelRange);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestRawSignalChannelSelectionProcess",
        [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestRawSignalChannelSelectionProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestRawSignalChannelSelectionProcess::TRestRawSignalChannelSelectionProcess() : TRestEventProcess() {
    fName = "TRestRawSignalChannelSelectionProcess";
}

TRestRawSignalChannelSelectionProcess::TRestRawSignalChannelSelectionProcess(const std::string& instanceName,
                                                                             const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestRawSignalChannelSelectionProcess::TRestRawSignalChannelSelectionProcess(const std::string& fileName,
                                                                             const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestRawSignalChannelSelectionProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestRawSignalChannelSelectionProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestRawSignalChannelSelectionProcess>(fNode);

    for (auto* ranges : {&fRemoveChannelRange, &fKeepChannelRange}) {
        ranges->erase(std::remove_if(ranges->begin(), ranges->end(),
                                     [this](const std::pair<int, int>& r) {
                                         if (r.first <= r.second) return false;
                                         RESTWarning << "TRestRawSignalChannelSelectionProcess: range ["
                                                     << r.first << ", " << r.second
                                                     << "] is inverted and will be ignored" << RESTendl;
                                         return true;
                                     }),
                      ranges->end());
    }

    UpdateYAMLFromParams<TRestRawSignalChannelSelectionProcess>(fNode);
}

bool TRestRawSignalChannelSelectionProcess::IsRemoved(int signalID) const {
    // Removed even if it is also kept
    if (InSelection(signalID, fRemoveChannelIds, fRemoveChannelRange)) return true;
    const bool keepAll = fKeepChannelIds.empty() && fKeepChannelRange.empty();
    return !keepAll && !InSelection(signalID, fKeepChannelIds, fKeepChannelRange);
}

bool TRestRawSignalChannelSelectionProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* inEvent = dynamic_cast<const TRestRawSignalEvent*>(&input);
    auto* outEvent = dynamic_cast<TRestRawSignalEvent*>(&output);
    if (!inEvent || !outEvent) {
        throw std::runtime_error(
            "TRestRawSignalChannelSelectionProcess: input/output is not a TRestRawSignalEvent");
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

    if (outEvent->GetNumberOfSignals() == 0) return false;

    return true;
}
