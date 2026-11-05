#include "TRestDetectorSignalRecoveryProcess.h"

#include <map>
#include <set>
#include <stdexcept>

#include "TRestDetectorReadout.h"
#include "TRestDetectorReadoutManager.h"
#include "TRestDetectorSignalEvent.h"
#include "TRestManager.h"

//---Example---
// signalRecovery:
//   class: "TRestDetectorSignalRecoveryProcess"
//   name: "recoverChannels"
//   channelIds: [4541, 4412, 4377]
//   readout: ""

static const bool TRestDetectorSignalRecoveryProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorSignalRecoveryProcess;
    reg.RegisterField<P>("channelIds", &P::fChannelIds);
    reg.RegisterField<P>("readout", &P::fReadoutName);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorSignalRecoveryProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorSignalRecoveryProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestDetectorSignalRecoveryProcess::TRestDetectorSignalRecoveryProcess() : TRestEventProcess() {
    fName = "TRestDetectorSignalRecoveryProcess";
}

TRestDetectorSignalRecoveryProcess::TRestDetectorSignalRecoveryProcess(const std::string& instanceName,
                                                                       const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestDetectorSignalRecoveryProcess::TRestDetectorSignalRecoveryProcess(const std::string& fileName,
                                                                       const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorSignalRecoveryProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestDetectorSignalRecoveryProcess::LoadConfig YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestDetectorSignalRecoveryProcess>(fNode);
    UpdateYAMLFromParams<TRestDetectorSignalRecoveryProcess>(fNode);
}

void TRestDetectorSignalRecoveryProcess::InitProcess() {
    auto* readouts = fManager ? dynamic_cast<TRestDetectorReadoutManager*>(
                                    fManager->GetMetadataClass("TRestDetectorReadoutManager"))
                              : nullptr;
    if (!readouts) {
        throw std::runtime_error("TRestDetectorSignalRecoveryProcess: no TRestDetectorReadoutManager found");
    }
    fReadout = readouts->SelectReadout(fReadoutName, "TRestDetectorSignalRecoveryProcess");
    // The stored metadata keeps the readout used
    UpdateYAMLFromParams<TRestDetectorSignalRecoveryProcess>(fNode);

    // Groups of consecutive dead channels, from the first one (its previous channel is alive) to the last
    const std::set<int> dead(fChannelIds.begin(), fChannelIds.end());
    std::set<int> grouped;
    fGroups.clear();
    for (const int id : fChannelIds) {
        if (grouped.count(id)) continue;
        if (fReadout->GetPhysicalID(id) < 0) {
            RESTWarning << "TRestDetectorSignalRecoveryProcess: channel " << id
                        << " is not in the readout, not recovered" << RESTendl;
            grouped.insert(id);
            continue;
        }

        int first = id;
        for (size_t n = 0; n < dead.size(); n++) {
            const int previous = fReadout->GetAdjacentChannels(first).first;
            if (previous < 0 || !dead.count(previous) || previous == id) break;
            first = previous;
        }

        DeadGroup group;
        group.left = fReadout->GetAdjacentChannels(first).first;
        int channel = first;
        while (true) {
            group.dead.push_back(channel);
            grouped.insert(channel);
            const int next = fReadout->GetAdjacentChannels(channel).second;
            if (next >= 0 && dead.count(next) && !grouped.count(next)) {
                channel = next;
            } else {
                group.right = next;
                break;
            }
        }

        if (group.left < 0 || group.right < 0) {
            RESTWarning << "TRestDetectorSignalRecoveryProcess: dead channels from " << group.dead.front()
                        << " to " << group.dead.back()
                        << " at the edge of the readout (no adjacent channel), not recovered" << RESTendl;
            continue;
        }
        RESTDebug << "Dead channels from " << group.dead.front() << " to " << group.dead.back()
                  << ", adjacent channels " << group.left << " and " << group.right << RESTendl;
        fGroups.push_back(group);
    }
}

bool TRestDetectorSignalRecoveryProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* inEvent = dynamic_cast<const TRestDetectorSignalEvent*>(&input);
    auto* outEvent = dynamic_cast<TRestDetectorSignalEvent*>(&output);
    if (!inEvent || !outEvent) {
        throw std::runtime_error(
            "TRestDetectorSignalRecoveryProcess: input/output is not a TRestDetectorSignalEvent");
    }
    outEvent->CopyFrom(inEvent);

    auto points = [inEvent](int id) {
        return inEvent->HasSignalID(id) ? inEvent->GetSignalByID(id).GetPointsVector()
                                        : std::vector<std::pair<double, double>>{};
    };

    // Number of groups each neighbour gives half of its charge to
    std::map<int, int> uses;
    for (const auto& group : fGroups) {
        const auto left = points(group.left);
        const auto right = points(group.right);
        if (left.empty() && right.empty()) continue;

        const int k = static_cast<int>(group.dead.size());
        const double sum = k * (k + 1) / 2.;
        for (int j = 1; j <= k; j++) {
            const double leftWeight = 0.5 * (k + 1 - j) / sum;
            const double rightWeight = 0.5 * j / sum;
            std::map<double, double> recovered;
            for (const auto& [time, charge] : left) recovered[time] += leftWeight * charge;
            for (const auto& [time, charge] : right) recovered[time] += rightWeight * charge;

            const int id = group.dead[j - 1];
            outEvent->RemoveSignalByID(id);
            outEvent->AddSignal(id,
                                std::vector<std::pair<double, double>>(recovered.begin(), recovered.end()));
        }
        if (!left.empty()) uses[group.left]++;
        if (!right.empty()) uses[group.right]++;

        RESTDebug << "Event " << inEvent->GetID() << ": channels " << group.dead.front() << " to "
                  << group.dead.back() << " recovered" << RESTendl;
    }

    // The neighbours keep the charge not given to the dead channels
    for (const auto& [id, n] : uses) outEvent->GetSignalByID(id).MultiplySignalBy(1 - 0.5 * n);

    return true;
}
