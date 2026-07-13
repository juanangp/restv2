#include "TRestManager.h"

using namespace TRestTools;

// ---------------------------------------------------------------------------
// Self-registration
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestManager",
        [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestManager>(instanceName, params);
        });
    return true;
}();
}  // namespace

// ---------------------------------------------------------------------------
TRestManager::TRestManager(const std::string& instanceName,
                           const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestManager::TRestManager(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

// ---------------------------------------------------------------------------
void TRestManager::LoadConfig() {
    for (const auto& element : fNode){
        const auto key = element.first.as<std::string>();
        auto value = element.second;

        if (key == "pipeline") {
            LoadPipeline(value);
            continue;
        }

        if(value.IsScalar())continue;

        RESTInfo << "Loading YAML node: " << key << RESTendl;

        const auto className = ReadYAMLParam<std::string>(value["class"]);
        const auto name = ReadYAMLParam<std::string>(value["name"]);
        auto params = value["params"];

        RESTInfo << "Loading metadata "<< key <<" name: " << name << "  class: " << className << RESTendl;

        fMetaObjects.emplace_back(MetadataClassRegistry::Instance().Create(className, name, params));
    }

    for (auto& meta : fMetaObjects) meta->PrintMetadata();
}

// ---------------------------------------------------------------------------
void TRestManager::LoadPipeline(YAML::Node& pipeline) {
    for (auto it = pipeline.begin(); it != pipeline.end(); ++it) {
        const auto key = it->first.as<std::string>();
        auto value = it->second;

        if (key == "params") {
            fParams = value;
            LoadParams();
            continue;
        }

        const auto className = ReadYAMLParam<std::string>(value["class"]);
        const auto name = ReadYAMLParam<std::string>(value["name"]);
        auto params = value["params"];

        std::string inputBranch = ReadYAMLParamOrDefault<std::string>(value, "input", "DefaultInput");
        std::string outputBranch = ReadYAMLParamOrDefault<std::string>(value, "output", "DefaultOutput");

        std::string outputClass = ReadYAMLParamOrDefault<std::string>(value, "outputClass", "");

        RESTInfo << "Loading process: " << name << " (" << className << ")" << RESTendl;
        RESTInfo << "  Route: " << inputBranch << " -> " << outputBranch << RESTendl;

        auto meta = MetadataClassRegistry::Instance().Create(className, name, params);

        if (auto* proc = dynamic_cast<TRestEventProcess*>(meta.get())) {
            meta.release();
            fProcessChain.emplace_back(std::unique_ptr<TRestEventProcess>(proc));

            fPipelineConnections.push_back({inputBranch, outputBranch});
            fPipelineOutputClasses.push_back(outputClass);
        } else {
            RESTError << "'" << className << "' is not a TRestEventProcess - skipping" << RESTendl;
        }
    }
}

void TRestManager::Run(TRestRun& restRun) {
    for (auto& proc : fProcessChain) {
        proc->SetRunInfo(&restRun);
        proc->Initialize();
    }

    if (fOutputEventStorage && !restRun.HasOutputFileOpen()) {
        restRun.OpenOutputFile();
    }

    for (size_t i = 0; i < fProcessChain.size(); ++i) {
        std::string inputName = fPipelineConnections[i].first;
        std::string outputName = fPipelineConnections[i].second;
        std::string procClassName = fProcessChain[i]->GetClassName();

        if (inputName != "None" && inputName != "") {
            try {
                restRun.GetInputEvent(inputName);
            } catch (...) {
                if (fEventPool.find(inputName) == fEventPool.end()) {
                    throw std::runtime_error("TRestManager::Run - Branch name '" + inputName +
                                             "' not found in pool.");
                }
            }
        }

        if (fEventPool.find(outputName) == fEventPool.end()) {
            std::string inputClassName;
            if (inputName != "None" && inputName != "") {
                inputClassName = (fEventPool.find(inputName) != fEventPool.end())
                                     ? fEventPool[inputName]->GetClassName()
                                     : restRun.GetInputEvent(inputName).GetClassName();
            } else {
                inputClassName = fPipelineOutputClasses[i];
                if (inputClassName.empty()) {
                    throw std::runtime_error("TRestManager::Run - outputClass must be specified for generator process '" + procClassName + "'");
                }
            }

            auto outEvent = EventRegistry::Instance().Create(inputClassName, outputName);

            if (fOutputEventStorage) {
                restRun.RegisterEvent<TRestEvent>(outputName, *outEvent);
            }

            fEventPool[outputName] = std::move(outEvent);
        }
    }

    Long64_t totalEntries = restRun.GetEntries();
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
        entriesToRun = std::min((Long64_t)fEventsToProcess, totalEntries);
    } else if (totalEntries == 0 && fEventsToProcess == 0) {
        entriesToRun = std::numeric_limits<Long64_t>::max();
    }

    RESTInfo << "TRestManager: Iniciando el bucle principal. Eventos a procesar: " << entriesToRun
             << RESTendl;

    for (Long64_t entry = 0; entry < entriesToRun; ++entry) {
        if (totalEntries > 0) {
            restRun.GetEntry(entry);
        }

        bool processOk = true;
        for (size_t i = 0; i < fProcessChain.size(); ++i) {
            std::string inputName = fPipelineConnections[i].first;
            std::string outputName = fPipelineConnections[i].second;

            TRestEvent* inputEventPtr = nullptr;
            if (inputName != "None" && inputName != "") {
                if (fEventPool.find(inputName) != fEventPool.end()) {
                    inputEventPtr = fEventPool[inputName].get();
                } else {
                    inputEventPtr = &restRun.GetInputEvent(inputName);
                }
            }

            TRestEvent& outputEvent = *fEventPool[outputName];
            
            // For generators, we can just pass the outputEvent as a dummy input if inputEventPtr is null
            const TRestEvent& inputEvent = inputEventPtr ? *inputEventPtr : outputEvent;

            if (!fProcessChain[i]->ProcessEvent(inputEvent, outputEvent)) {
                processOk = false;
                break;
            }
        }

        if (!processOk) {
            RESTInfo << "TRestManager: Process returned false, stopping loop." << RESTendl;
            break;
        }

        if (fOutputEventStorage) {
            restRun.Fill();
        }
    }

    for (auto& proc : fProcessChain) {
        proc->EndProcess();
    }

    RESTInfo << "TRestManager: Pipeline run succeed." << RESTendl;
}

// ---------------------------------------------------------------------------
void TRestManager::LoadParams() {
    if (!fParams || fParams.IsNull()) {
        RESTError << "TRestManager::LoadParams – parameter node is missing" << RESTendl;
        return;
    }

    fEventsToProcess = ReadYAMLParamOrDefault<int>(fParams, "eventsToProcess", fEventsToProcess);
    fInputAnalysisStorage =
        ReadYAMLParamOrDefault<bool>(fParams, "inputAnalysisStorage", fInputAnalysisStorage);
    fInputEventStorage = ReadYAMLParamOrDefault<bool>(fParams, "inputEventStorage", fInputEventStorage);
    fOutputEventStorage = ReadYAMLParamOrDefault<bool>(fParams, "outputEventStorage", fOutputEventStorage);

    TRestMetadata::ReadYAMLVerbose(fParams);
}

// ---------------------------------------------------------------------------
void TRestManager::PrintMetadata() {
    RESTMetadata << "=== TRestManager ===" << RESTendl;
    RESTMetadata << fParams << RESTendl;
}
