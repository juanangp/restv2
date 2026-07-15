#include "TRestManager.h"

#include <stdexcept>

#include "TRestProcessManager.h"

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

TRestManager::TRestManager(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestManager::TRestManager(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

void TRestManager::LoadConfig() {
    fMetaObjects.clear();
    fProcessManager = nullptr;
    fConfiguredRun = nullptr;

    for (const auto& element : fNode) {
        const auto key = element.first.as<std::string>();
        auto value = element.second;

         if (!value || value.IsScalar() || !value.IsMap()) continue;

        auto meta = MetadataClassRegistry::Instance().Create(key, value);
        if (!meta) continue;

        if (auto* processManager = dynamic_cast<TRestProcessManager*>(meta.get())) {
            fProcessManager = processManager;
        }
        if (auto* run = dynamic_cast<TRestRun*>(meta.get())) {
            fConfiguredRun = run;
        }

        meta->PrintMetadata();
        fMetaObjects.emplace_back(std::move(meta));
    }
}

void TRestManager::Run() {
    if (fConfiguredRun == nullptr) {
        throw std::runtime_error("TRestManager::Run - no TRestRun configured under manager section.");
    }

    fProcessManager->SetRunInfo(fConfiguredRun);
    fProcessManager->Run();
}

void TRestManager::SaveMetadata(){
    if (fConfiguredRun == nullptr) {
        throw std::runtime_error("TRestManager::Run - no TRestRun configured under manager section.");
    }

 for(const auto &meta: fMetaObjects){
   fConfiguredRun->AddMetadata(meta->GetName(), meta->GetYAMLNode());
 }

}
