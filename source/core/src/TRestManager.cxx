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
            return std::make_unique<TRestRun>(instanceName, sectionName, params);
        });
    return true;
}();
} // namespace

// ---------------------------------------------------------------------------
TRestManager::TRestManager(const std::string& instanceName, const std::string& sectionName, const YAML::Node& node)
    : TRestMetadata(instanceName, sectionName, node) {
    LoadConfig();
}

TRestManager::TRestManager(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    YAML::Node raw = YAML::LoadFile(fileName);
    fNode          = ResolveAllRefs(raw);
    if (!sectionName.empty()) fNode = fNode[sectionName];
    LoadConfig();
}

// ---------------------------------------------------------------------------
void TRestManager::LoadConfig() {
    for (auto it = fNode.begin(); it != fNode.end(); ++it) {
        const auto key   = it->first.as<std::string>();
        auto       value = it->second;

        RESTInfo << "Loading YAML node: " << key << RESTendl;

        if (key == "pipeline") {
            LoadPipeline(value);
            continue;
        }

        const auto className = ReadYAMLParam<std::string>(value["class"]);
        const auto name      = ReadYAMLParam<std::string>(value["name"]);
        auto       params    = value["params"];

        RESTInfo << "Loading metadata - name: " << name
                 << "  class: " << className << RESTendl;

        fMetaObjects.emplace_back(
            MetadataRegistry::Instance().Create(className, name, "manager", params));
    }

    for (auto& meta : fMetaObjects) meta->PrintMetadata();
}


// ---------------------------------------------------------------------------
void TRestManager::LoadPipeline(YAML::Node& pipeline) {
    for (auto it = pipeline.begin(); it != pipeline.end(); ++it) {
        const auto key   = it->first.as<std::string>();
        auto       value = it->second;

        if (key == "params") {
            fParams = value;
            LoadParams();
            continue;
        }

        const auto className = ReadYAMLParam<std::string>(value["class"]);
        const auto name      = ReadYAMLParam<std::string>(value["name"]);
        auto       params    = value["params"];

        std::string inputBranch  = ReadYAMLParamOrDefault<std::string>(value, "input", "DefaultInput");
        std::string outputBranch = ReadYAMLParamOrDefault<std::string>(value, "output", "DefaultOutput");

        RESTInfo << "Loading process: " << name << " (" << className << ")" << RESTendl;
        RESTInfo << "  Route: " << inputBranch << " -> " << outputBranch << RESTendl;

        auto meta = MetadataRegistry::Instance().Create(className, name, "pipeline", params);

        if (auto* proc = dynamic_cast<TRestEventProcess*>(meta.get())) {
            meta.release();
            fProcessChain.emplace_back(std::unique_ptr<TRestEventProcess>(proc));
            
            fPipelineConnections.push_back({inputBranch, outputBranch});
        } else {
            RESTError << "'" << className << "' is not a TRestEventProcess - skipping" << RESTendl;
        }
    }
}

void TRestManager::Run(TRestRun& restRun) {
    for (auto& proc : fProcessChain) {
        proc->Initialize();
    }

    for (size_t i = 0; i < fProcessChain.size(); ++i) {
        std::string inputName     = fPipelineConnections[i].first;
        std::string outputName    = fPipelineConnections[i].second;
        std::string procClassName = fProcessChain[i]->GetClassName();

        try {
            restRun.GetInputEvent(inputName);
        } catch (...) {
            if (fEventPool.find(inputName) == fEventPool.end()) {
                throw std::runtime_error("TRestManager::Run - Branch name '" + inputName + "' not found in pool.");
            }
        }

        if (fEventPool.find(outputName) == fEventPool.end()) {
            std::string inputClassName = (fEventPool.find(inputName) != fEventPool.end())
                ? fEventPool[inputName]->GetClassName()
                : restRun.GetInputEvent(inputName).GetClassName();

            auto outEvent = EventRegistry::Instance().Create(inputClassName, outputName);

            if (fOutputEventStorage) {
                restRun.RegisterEventBranch<TRestEvent>(outputName, *outEvent);
            }

            fEventPool[outputName] = std::move(outEvent);
        }
    }

    Long64_t totalEntries = restRun.GetEntries();
    Long64_t entriesToRun = (fEventsToProcess > 0) ? std::min((Long64_t)fEventsToProcess, totalEntries) : totalEntries;
    
    RESTInfo << "TRestManager: Iniciando el bucle principal. Eventos a procesar: " << entriesToRun << RESTendl;

    for (Long64_t entry = 0; entry < entriesToRun; ++entry) {
        restRun.GetEntry(entry);

        for (size_t i = 0; i < fProcessChain.size(); ++i) {
            std::string inputName  = fPipelineConnections[i].first;
            std::string outputName = fPipelineConnections[i].second;

            TRestEvent& inputEvent = (fEventPool.find(inputName) != fEventPool.end())
                ? *fEventPool[inputName]
                : restRun.GetInputEvent(inputName);

            TRestEvent& outputEvent = *fEventPool[outputName];

            fProcessChain[i]->ProcessEvent(inputEvent, outputEvent);
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

    fEventsToProcess      = ReadYAMLParamOrDefault<int>(fParams,  "eventsToProcess",      fEventsToProcess);
    fInputAnalysisStorage = ReadYAMLParamOrDefault<bool>(fParams, "inputAnalysisStorage",  fInputAnalysisStorage);
    fInputEventStorage    = ReadYAMLParamOrDefault<bool>(fParams, "inputEventStorage",     fInputEventStorage);
    fOutputEventStorage   = ReadYAMLParamOrDefault<bool>(fParams, "outputEventStorage",    fOutputEventStorage);

    TRestMetadata::ReadYAMLVerbose(fParams);
}

// ---------------------------------------------------------------------------
void TRestManager::PrintMetadata() {
    RESTMetadata << "=== TRestManager ===" << RESTendl;
    RESTMetadata << fParams << RESTendl;
}
