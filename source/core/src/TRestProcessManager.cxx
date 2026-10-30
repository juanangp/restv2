#include "TRestProcessManager.h"

#include <algorithm>
#include <set>
#include <stdexcept>

#include "TRestManager.h"
#include "TRestRun.h"
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
    // The processes and the metadata of the pipeline
    reg.RegisterChildMetadata<TRestProcessManager>();
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
    // Sync resolved parameters to the node
    UpdateYAMLFromParams<TRestProcessManager>(fNode);

    LoadProcesses();
}

void TRestProcessManager::LoadProcesses() {
    fMetaObjects.clear();
    fProcessChain.clear();
    fStageOutputs.clear();
    fPipelineConnections.clear();
    fPipelineOutputClasses.clear();

    for (const auto& element : fNode) {
        const auto key = element.first.as<std::string>();
        auto value = element.second;

        if (!value || value.IsScalar() || !value.IsMap()) continue;

        auto meta = CreateChildMetadata(key, value);

        if (!meta) continue;
        meta->PrintMetadata();

        if (auto* proc = dynamic_cast<TRestEventProcess*>(meta.get())) {
            std::unique_ptr<TRestEventProcess> processPtr(proc);
            meta.release();

            std::string inputEvent = proc->GetInputEvent();
            std::string outputEvent = proc->GetOutputEvent();
            // Processes for any event type (e.g. TRestSelectionProcess) take the event of the previous one
            if (inputEvent.empty()) {
                if (fPipelineConnections.empty()) {
                    throw std::runtime_error("TRestProcessManager: process '" + proc->GetName() +
                                             "' has no input event and it is the first process");
                }
                inputEvent = outputEvent = fPipelineConnections.back().second;
            }

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

    if (!fRunInfo->HasOutputFileOpen()) {
        fRunInfo->OpenOutputFile();
    }

    for (auto& proc : fProcessChain) {
        proc->SetRunInfo(fRunInfo);
        proc->SetManager(fManager);
        TRestLogScope logScope(proc->GetLogContext());
        proc->Initialize();
    }
    // Once all the observables are registered
    for (auto& proc : fProcessChain) proc->InitCuts();

    fStageInputs.assign(fProcessChain.size(), nullptr);
    fStageOutputs.clear();
    fStageOutputs.resize(fProcessChain.size());
    // Output of the latest stage of each event name, and the names in order of first appearance
    std::map<std::string, TRestEvent*> latestOutputs;
    std::vector<std::string> outputNames;

    for (size_t i = 0; i < fProcessChain.size(); ++i) {
        const std::string& inputName = fPipelineConnections[i].first;
        const std::string& outputName = fPipelineConnections[i].second;
        const std::string procClassName = fProcessChain[i]->GetClassName();

        // A previous stage output takes precedence over the input file, otherwise chained processes
        // on the same event type would all read the unmodified file event
        if (inputName != "None" && !inputName.empty()) {
            if (latestOutputs.find(inputName) != latestOutputs.end()) {
                fStageInputs[i] = latestOutputs[inputName];
            } else if (fRunInfo->HasEvent(inputName)) {
                fStageInputs[i] = fRunInfo->GetEvent(inputName);
            } else {
                throw std::runtime_error("TRestProcessManager::Run - input event '" + inputName +
                                         "' of process '" + procClassName +
                                         "' is neither in the input file nor produced by a previous process");
            }
        }

        // Processes that change the event type declare it in GetOutputEvent
        std::string outputClassName = fPipelineOutputClasses[i];
        if (outputClassName.empty()) {
            if (!fStageInputs[i]) {
                throw std::runtime_error(
                    "TRestProcessManager::Run - outputClass must be specified for generator process '" +
                    procClassName + "'");
            }
            outputClassName = fStageInputs[i]->GetClassName();
        }
        fStageOutputs[i] = EventRegistry::Instance().Create(outputClassName, outputName);
        if (latestOutputs.find(outputName) == latestOutputs.end()) outputNames.push_back(outputName);
        latestOutputs[outputName] = fStageOutputs[i].get();
    }

    // One tree per event name, filled with the output of its last stage. Registered once per name in
    // order of first appearance (the first one binds the event info of the analysis tree). Without
    // outputEventStorage only the final event (output of the last process) is stored
    std::set<std::string> storedNames;
    if (fOutputEventStorage) {
        for (const auto& outputName : outputNames) {
            fRunInfo->RegisterEvent<TRestEvent>(outputName, *latestOutputs[outputName]);
            storedNames.insert(outputName);
        }
    } else if (!fStageOutputs.empty()) {
        fRunInfo->RegisterEvent<TRestEvent>(fPipelineConnections.back().second, *fStageOutputs.back());
        storedNames.insert(fPipelineConnections.back().second);
    }
    // With inputEventStorage the events of the input file are stored as read, unless a stored process
    // output has the same name (it takes precedence, a single tree per name)
    if (fInputEventStorage) {
        for (const auto& [inputName, inputEvent] : fRunInfo->GetEventMap()) {
            if (!inputEvent || storedNames.count(inputName) > 0) continue;
            fRunInfo->RegisterEvent<TRestEvent>(inputName, *inputEvent);
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
    if (fEntryEnd != -1) loopEnd = fEntryEnd;

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
            RESTWarning << "TRestProcessManager: stop requested, breaking event loop at entry " << entry
                        << RESTendl;
            break;
        }

        if (hasInputTree) {
            fRunInfo->GetEntry(entry);
        }

        bool eventAccepted = true;
        for (size_t i = 0; i < fProcessChain.size(); ++i) {
            TRestEvent& outputEvent = *fStageOutputs[i];
            const TRestEvent& inputEvent = fStageInputs[i] ? *fStageInputs[i] : outputEvent;

            // The messages of the events and tools used by the process take its verbose
            TRestLogScope logScope(fProcessChain[i]->GetLogContext());
            if (!fProcessChain[i]->ProcessEvent(inputEvent, outputEvent) || !fProcessChain[i]->PassCuts()) {
                eventAccepted = false;
                break;
            }
            // Events created by a generator (e.g. from DAQ files) take the run and subrun of this run,
            // the others keep the ones of their input
            if (!fStageInputs[i]) outputEvent.InitializeReferences(fRunInfo);
        }

        if (eventAccepted) {
            acceptedCount++;
            fRunInfo->Fill();
        } else {
            rejectedCount++;
        }

        if (showProgress && ((entry - loopStart) % 10 == 0 || entry == loopEnd - 1)) {
            RESTProgress.Update(entry - loopStart + 1);
        }
    }

    std::cout << "\n";

    for (auto& proc : fProcessChain) {
        TRestLogScope logScope(proc->GetLogContext());
        proc->EndProcess();
    }

    RESTInfo << "TRestProcessManager: Pipeline finished. Accepted: " << acceptedCount
             << " events, Rejected: " << rejectedCount << " events" << RESTendl;
    RESTInfo << "Output file name: " << fRunInfo->GetOutputFileName() << RESTendl;
}
