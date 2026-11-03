#include "TRestMetadataStore.h"

#include <TFile.h>

#include <iostream>

#include "TRestYAMLUtils.h"

TRestMetadataStore::TRestMetadataStore() = default;
TRestMetadataStore::~TRestMetadataStore() = default;
TRestMetadataStore::TRestMetadataStore(TRestMetadataStore&&) noexcept = default;
TRestMetadataStore& TRestMetadataStore::operator=(TRestMetadataStore&&) noexcept = default;

void TRestMetadataStore::Clear() { fMetadata.clear(); }

void TRestMetadataStore::Load(TFile* file, TRestRun* run, const std::set<std::string>& skipClasses) {
    fMetadata.clear();
    if (!file || file->IsZombie()) return;
    const YAML::Node store = TRestYAMLUtils::ReadMetadataStore(file);
    for (const auto& entry : store) {
        const std::string instance = entry.first.as<std::string>();
        const YAML::Node& config = entry.second;
        if (!config.IsMap() || !config["class"]) continue;
        const std::string className = config["class"].as<std::string>();
        if (skipClasses.count(className)) continue;
        try {
            // Written by REST: not checked
            auto metadata = MetadataClassRegistry::Instance().Create(className, instance, config, false);
            if (!metadata) continue;
            if (run) metadata->SetRunInfo(run);
            metadata->LoadFromFile(file);
            fMetadata.push_back(std::move(metadata));
        } catch (const std::exception& error) {
            std::cerr << "[-] Error loading metadata '" << instance << "' from " << file->GetName() << ": "
                      << error.what() << std::endl;
        }
    }
}

TRestMetadata* TRestMetadataStore::Get(const std::string& instanceName) const {
    for (const auto& metadata : fMetadata)
        if (metadata->GetName() == instanceName) return metadata.get();
    return nullptr;
}

TRestMetadata* TRestMetadataStore::GetClass(const std::string& className) const {
    for (const auto& metadata : fMetadata)
        if (metadata->GetClassName() == className) return metadata.get();
    return nullptr;
}
