#include "TRestProcessManager.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "TRestRun.h"
#include "TRestTools.h"

using namespace TRestTools;

static const bool TRestProcessManager_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestProcessManager>("inputAnalysisStorage", &TRestProcessManager::fInputAnalysisStorage);
    reg.RegisterField<TRestProcessManager>("inputEventStorage", &TRestProcessManager::fInputEventStorage);
    reg.RegisterField<TRestProcessManager>("outputEventStorage", &TRestProcessManager::fOutputEventStorage);
    reg.RegisterField<TRestProcessManager>("eventsToProcess", &TRestProcessManager::fEventsToProcess);
    return true;
}();

// ---------------------------------------------------------------------------
// Self-registration
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestProcessManager",
        [](const std::string& instanceName, const YAML::Node& params) {
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
    //Sync resolved parameters to the node
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

            RESTInfo << "Loading process: " << proc->GetName() << " (" << proc->GetClassName() << ")" << RESTendl;
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
        proc->Initialize();
    }

    for (size_t i = 0; i < fProcessChain.size(); ++i) {
        const std::string& inputName = fPipelineConnections[i].first;
        const std::string& outputName = fPipelineConnections[i].second;
        const std::string procClassName = fProcessChain[i]->GetClassName();

        if (inputName != "None" && !inputName.empty()) {
            try {
                fRunInfo->GetInputEvent(inputName);
            } catch (...) {
                if (fEventPool.find(inputName) == fEventPool.end()) {
                    throw std::runtime_error("TRestProcessManager::Run - Branch name '" + inputName +
                                             "' not found in input events or pool.");
                }
            }
        }

        if (fEventPool.find(outputName) == fEventPool.end()) {
            std::string inputClassName;
            if (inputName != "None" && !inputName.empty()) {
                inputClassName = (fEventPool.find(inputName) != fEventPool.end())
                                     ? fEventPool[inputName]->GetClassName()
                                     : fRunInfo->GetInputEvent(inputName).GetClassName();
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

    Long64_t entriesToRun = fEventsToProcess > 0 ? fEventsToProcess : totalEntries;
    if (totalEntries > 0 && fEventsToProcess > 0) {
        entriesToRun = std::min(static_cast<Long64_t>(fEventsToProcess), totalEntries);
    } else if (totalEntries == 0 && fEventsToProcess == 0) {
        entriesToRun = std::numeric_limits<Long64_t>::max();
    }

    RESTInfo << "TRestProcessManager: Starting event loop. Entries to process: " << entriesToRun << RESTendl;

RESTProgress.Reset(entriesToRun);

    for (Long64_t entry = 0; entry < entriesToRun; ++entry) {
        if (totalEntries > 0) {
            fRunInfo->GetEntry(entry);
        }

        bool processOk = true;
        for (size_t i = 0; i < fProcessChain.size(); ++i) {
            const std::string& inputName = fPipelineConnections[i].first;
            const std::string& outputName = fPipelineConnections[i].second;

            TRestEvent* inputEventPtr = nullptr;
            if (inputName != "None" && !inputName.empty()) {
                if (fEventPool.find(inputName) != fEventPool.end()) {
                    inputEventPtr = fEventPool[inputName].get();
                } else {
                    inputEventPtr = &fRunInfo->GetInputEvent(inputName);
                }
            }

            TRestEvent& outputEvent = *fEventPool[outputName];
            const TRestEvent& inputEvent = inputEventPtr ? *inputEventPtr : outputEvent;

            if (!fProcessChain[i]->ProcessEvent(inputEvent, outputEvent)) {
                processOk = false;
                break;
            }
        }

        if (!processOk) {
            RESTInfo << "TRestProcessManager: Process returned false, stopping loop." << RESTendl;
            break;
        }

        if (fOutputEventStorage) {
            fRunInfo->Fill();
        }

        if (entry % 10 == 0 || entry == entriesToRun - 1) {
          RESTProgress.Update(entry + 1);
        }
    }

    std::cout<<"\n";

    for (auto& proc : fProcessChain) {
        proc->EndProcess();
    }

    RESTInfo << "TRestProcessManager: Pipeline run succeeded." << RESTendl;
}

