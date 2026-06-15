#include "TRestManager.h"

using namespace TRestTools;

// ---------------------------------------------------------------------------
// Self-registration
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    MetadataRegistry::Instance().Register(
        "TRestManager",
        [](const std::string& name, const YAML::Node& params) {
            return std::make_unique<TRestManager>(name, params);
        });
    return true;
}();
}  // namespace

// ---------------------------------------------------------------------------
TRestManager::TRestManager(const std::string& sectionName, const YAML::Node& node)
    : TRestMetadata(sectionName, node) {
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

        RESTInfo << "Loading metadata – name: " << name
                 << "  class: " << className << RESTendl;

        fMetaObjects.emplace_back(
            MetadataRegistry::Instance().Create(className, name, params));
    }

    for (auto& meta : fMetaObjects) meta->PrintMetadata();
}

// ---------------------------------------------------------------------------
void TRestManager::LoadPipeline(YAML::Node& pipeline) {
    for (auto it = pipeline.begin(); it != pipeline.end(); ++it) {
        const auto key   = it->first.as<std::string>();
        auto       value = it->second;

        RESTInfo << "Loading pipeline node: " << key << RESTendl;

        if (key == "params") {
            fParams = value;
            LoadParams();
            continue;
        }

        const auto className = ReadYAMLParam<std::string>(value["class"]);
        const auto name      = ReadYAMLParam<std::string>(value["name"]);
        auto       params    = value["params"];

        RESTInfo << "Loading process – name: " << name
                 << "  class: " << className << RESTendl;

        auto meta = MetadataRegistry::Instance().Create(className, name, params);

        if (auto* proc = dynamic_cast<TRestEventProcess*>(meta.get())) {
            meta.release();
            fProcessChain.emplace_back(std::unique_ptr<TRestEventProcess>(proc));
        } else {
            RESTError << "'" << className << "' is not a TRestEventProcess – skipping" << RESTendl;
        }
    }
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
