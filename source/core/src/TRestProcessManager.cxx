#include "TRestProcessManager.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "TRestRun.h"
#include "TRestManager.h"
#include "TRestTools.h"

using namespace TRestTools;

static const bool TRestProcessManager_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestProcessManager>("inputAnalysisStorage",
                                           &TRestProcessManager::fInputAnalysisStorage);
    reg.RegisterField<TRestProcessManager>("inputEventStorage", &TRestProcessManager::fInputEventStorage);
    reg.RegisterField<TRestProcessManager>("outputEventStorage", &TRestProcessManager::fOutputEventStorage);
    reg.RegisterField<TRestProcessManager>("eventsToProcess", &TRestProcessManager::fEventsToProcess);
    reg.RegisterField<TRestProcessManager>("nThreads", &TRestProcessManager::fNThreads);
    return true;
}();

// ---------------------------------------------------------------------------
// Self-registration
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestProcessManager", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestProcessManager>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestProcessManager::TRestProcessManager() : TRestMetadata() { fName = "TRestProcessManager"; }

TRestProcessManager::TRestProcessManager(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestProcessManager::TRestProcessManager(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

void TRestProcessManager::LoadConfig() {
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestProcessManager::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestProcessManager>(fNode);
    ReadYAMLVerbose(fNode);
    // Sync resolved parameters to the node
    UpdateYAMLFromParams<TRestProcessManager>(fNode);

    LoadProcesses();
}

void TRestProcessManager::LoadProcesses() {
    fMetaObjects.clear();
    fProcessChain.clear();
    fEventPool.clear();
    fPipelineConnections.clear();
    fPipelineOutputClasses.clear();

    for (const auto& element : fNode) {
        const auto key = element.first.as<std::string>();
        auto value = element.second;

        if (!value || value.IsScalar() || !value.IsMap()) continue;

        auto meta = MetadataClassRegistry::Instance().Create(key, value);

        if (!meta) continue;
        meta->PrintMetadata();

        if (auto* proc = dynamic_cast<TRestEventProcess*>(meta.get())) {
            std::unique_ptr<TRestEventProcess> processPtr(proc);
            meta.release();

            const std::string inputEvent = proc->GetInputEvent();
            const std::string outputEvent = proc->GetOutputEvent();

            RESTInfo << "Loading process: " << proc->GetName() << " (" << proc->GetClassName() << ")"
                     << RESTendl;
            RESTInfo << "  Route: " << inputEvent << " -> " << outputEvent << RESTendl;

            fPipelineConnections.emplace_back(inputEvent, outputEvent);
            fPipelineOutputClasses.emplace_back(outputEvent);
            fProcessChain.emplace_back(std::move(processPtr));
        } else {
            fMetaObjects.emplace_back(std::move(meta));
        }
    }
}

void TRestProcessManager::Run() {
    if (fRunInfo == nullptr) {
        throw std::runtime_error("TRestProcessManager::Run - run context is null");
    }

    if (fProcessChain.empty()) {
        return;
    }

    if (fOutputEventStorage && !fRunInfo->HasOutputFileOpen()) {
        fRunInfo->OpenOutputFile();
    }

    for (auto& proc : fProcessChain) {
        proc->SetRunInfo(fRunInfo);
        proc->SetManager(fManager);
        proc->Initialize();
    }

    for (size_t i = 0; i < fProcessChain.size(); ++i) {
        const std::string& inputName = fPipelineConnections[i].first;
        const std::string& outputName = fPipelineConnections[i].second;
        const std::string procClassName = fProcessChain[i]->GetClassName();

        if ((inputName != "None" && !inputName.empty()) && i == 0) {
            if (!fRunInfo->HasEvent(inputName)) {
                RESTError << "Input event " << inputName << " not found in file" << RESTendl;
            }
        }

        if (fEventPool.find(outputName) == fEventPool.end()) {
            std::string inputClassName;
            if (inputName != "None" && !inputName.empty()) {
                inputClassName = (fEventPool.find(inputName) != fEventPool.end())
                                     ? fEventPool[inputName]->GetClassName()
                                     : fRunInfo->GetInputEvent(inputName)->GetClassName();
            } else {
                inputClassName = fPipelineOutputClasses[i];
                if (inputClassName.empty()) {
                    throw std::runtime_error(
                        "TRestProcessManager::Run - outputClass must be specified for generator process '" +
                        procClassName + "'");
                }
            }

            auto outEvent = EventRegistry::Instance().Create(inputClassName, outputName);

            if (fOutputEventStorage) {
                fRunInfo->RegisterEvent<TRestEvent>(outputName, *outEvent);
            }

            fEventPool[outputName] = std::move(outEvent);
        }
    }

    Long64_t totalEntries = fRunInfo->GetEntries();
    if (totalEntries <= 0) {
        for (const auto& process : fProcessChain) {
            const Long64_t processEntries = process->GetInputEventCount();
            if (processEntries >= 0) {
                totalEntries = processEntries;
                break;
            }
        }
    }

    Long64_t loopStart = 0;
    Long64_t loopEnd = totalEntries;

    if (fEntryStart != -1) loopStart = fEntryStart;
    if (fEntryEnd != -1)   loopEnd = fEntryEnd;

    if (fEntryEnd == -1 && fEventsToProcess > 0) {
        loopEnd = loopStart + fEventsToProcess;
        if (totalEntries > 0) {
            loopEnd = std::min(loopEnd, totalEntries);
        }
    }

    const bool showProgress = (fEntryStart <= 0);

    RESTInfo << "TRestProcessManager: Starting event loop. Entries [" << loopStart << ", " << loopEnd << ")"
             << RESTendl;

    if (showProgress) RESTProgress.Reset(loopEnd - loopStart);

    Long64_t acceptedCount = 0;
    Long64_t rejectedCount = 0;

    const bool hasInputTree = (totalEntries > 0);

    for (Long64_t entry = loopStart; entry < loopEnd; ++entry) {
        if (TRestManager::StopRequested()) {
          RESTWarning << "TRestProcessManager: stop requested, breaking event loop at entry "
                    << entry << RESTendl;
          break;
        }

        if (hasInputTree) {
            fRunInfo->GetEntry(entry);
        }

        bool eventAccepted = true;
        for (size_t i = 0; i < fProcessChain.size(); ++i) {
            const std::string& inputName = fPipelineConnections[i].first;
            const std::string& outputName = fPipelineConnections[i].second;

            TRestEvent* inputEventPtr = nullptr;
            if (inputName != "None" && !inputName.empty()) {
                if (fRunInfo->HasEvent(inputName)) {
                    inputEventPtr = fRunInfo->GetInputEvent(inputName);
                } else if (fEventPool.find(inputName) != fEventPool.end()) {
                    inputEventPtr = fEventPool[inputName].get();
                }
            }

            TRestEvent& outputEvent = *fEventPool[outputName];
            const TRestEvent& inputEvent = inputEventPtr ? *inputEventPtr : outputEvent;

            if (!fProcessChain[i]->ProcessEvent(inputEvent, outputEvent)) {
                eventAccepted = false;
                break;
            }
        }

        if (eventAccepted) {
            acceptedCount++;
            if (fOutputEventStorage) {
                fRunInfo->Fill();
            }
        } else {
            rejectedCount++;
        }

        if (showProgress && ((entry - loopStart) % 10 == 0 || entry == loopEnd - 1)) {
            RESTProgress.Update(entry - loopStart + 1);
        }
    }

    std::cout << "\n";

    for (auto& proc : fProcessChain) {
        proc->EndProcess();
    }

    RESTInfo << "TRestProcessManager: Pipeline run finished. Accepted: " << acceptedCount
             << ", Rejected: " << rejectedCount << RESTendl;
}
