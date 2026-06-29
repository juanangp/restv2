#include "TRestManager.h"

using namespace TRestTools;

// ---------------------------------------------------------------------------
// Self-registration
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    MetadataRegistry::Instance().Register(
        "TRestManager",
        [](const std::string& instanceName, const std::string& sectionName, const YAML::Node& params) {
            return std::make_unique<TRestManager>(instanceName, sectionName, params);
        });
    return true;
}();
}  // namespace

// ---------------------------------------------------------------------------
TRestManager::TRestManager(const std::string& instanceName, const std::string& sectionName,
                           const YAML::Node& node)
    : TRestMetadata(instanceName, sectionName, node) {
    LoadConfig();
}

TRestManager::TRestManager(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    YAML::Node raw = YAML::LoadFile(fileName);
    fNode = ResolveAllRefs(raw);
    if (!sectionName.empty()) fNode = fNode[sectionName];
    LoadConfig();
}

// ---------------------------------------------------------------------------
void TRestManager::LoadConfig() {
    for (auto it = fNode.begin(); it != fNode.end(); ++it) {
        const auto key = it->first.as<std::string>();
        auto value = it->second;

        RESTInfo << "Loading YAML node: " << key << RESTendl;

        if (key == "pipeline") {
            LoadPipeline(value);
            continue;
        }

        const auto className = ReadYAMLParam<std::string>(value["class"]);
        const auto name = ReadYAMLParam<std::string>(value["name"]);
        auto params = value["params"];

        RESTInfo << "Loading metadata - name: " << name << "  class: " << className << RESTendl;

        fMetaObjects.emplace_back(MetadataRegistry::Instance().Create(className, name, "manager", params));
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

        auto meta = MetadataRegistry::Instance().Create(className, name, "pipeline", params);

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
