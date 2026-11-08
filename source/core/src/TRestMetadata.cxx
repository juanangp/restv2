#include "TRestMetadata.h"

#include <TObjString.h>

#include "TRestTools.h"
#include "TRestYAMLUtils.h"

using namespace TRestTools;

TRestMetadata::TRestMetadata(const std::string& instanceName, const YAML::Node& node)
    : fName(instanceName), fNode(node) {
    if (fNode) {
        fName = TRestYAMLUtils::ReadParamOrDefault<std::string>(fNode, "name", fName);
    }
    ReadVerbose();
}

TRestMetadata::TRestMetadata(const std::string& fileName, const std::string& sectionName)
    : fConfigFileName(fileName), fSectionName(sectionName) {
    YAML::Node raw = YAML::LoadFile(fileName);
    // Its own parameters, without replacing those of the main configuration
    YAML::Node cfg = TRestYAMLUtils::ResolveRefs(raw, {}, false);
    fNode = cfg[sectionName];
    if (fNode) {
        fName = TRestYAMLUtils::ReadParamOrDefault<std::string>(fNode, "name", sectionName);
    }
    ReadVerbose();
}

std::unique_ptr<TRestMetadata> TRestMetadata::CreateChildMetadata(const std::string& instanceName,
                                                                  YAML::Node node) const {
    if (node.IsMap() && !node["verbose"]) {
        node["verbose"] = TRestLogManager::GetStringFromVerbose(fVerboseLevel);
    }
    return MetadataClassRegistry::Instance().Create(instanceName, node);
}

void TRestMetadata::ReadVerbose() {
    if (!fNode || !fNode.IsMap()) return;
    if (fNode["verbose"]) {
        try {
            fVerboseLevel = TRestLogManager::GetVerboseLevelFromString(fNode["verbose"].as<std::string>());
        } catch (const std::exception& error) {
            throw std::runtime_error(GetClassName() + " '" + fName + "': " + error.what());
        }
    }
    TRestYAMLUtils::SetParam(fNode, "verbose", TRestLogManager::GetStringFromVerbose(fVerboseLevel));
}

void TRestMetadata::WriteMetadata(TFile* file) {
    TRestMetadataFieldRegistry::Instance().ApplyFieldsToYAML(this, fNode);

    TRestMetadata::WriteMetadata(file, fName, fNode);
}

void TRestMetadata::WriteMetadata(TFile* file, const std::string& instanceName,
                                  const YAML::Node& configNode) {
    if (!file || file->IsZombie()) {
        throw std::runtime_error("TRestMetadata::WriteMetadata: the ROOT file is not valid (null or zombie)");
    }
    if (!file->IsWritable()) {
        throw std::runtime_error("TRestMetadata::WriteMetadata: the ROOT file is read-only");
    }
    if (!configNode || configNode.IsNull()) return;

    file->cd();

    TDirectory* metadataDir = file->GetDirectory("RESTMetadataStore");
    if (!metadataDir) {
        metadataDir = file->mkdir("RESTMetadataStore");
        if (!metadataDir) {
            throw std::runtime_error("TRestMetadata::WriteMetadata: cannot create RESTMetadataStore");
        }
    }

    metadataDir->cd();

    std::string yamlDump = YAML::Dump(TRestYAMLUtils::Detach(configNode));

    auto* rootYamlString = new TObjString(yamlDump.c_str());

    rootYamlString->Write(instanceName.c_str(), TObject::kOverwrite);

    delete rootYamlString;

    file->cd();
}

YAML::Node TRestMetadata::ReadMetadata(TFile* file, const std::string& instanceName) {
    if (!file || file->IsZombie()) return YAML::Node();

    file->cd();

    TDirectory* metadataDir = file->GetDirectory("RESTMetadataStore");
    if (!metadataDir) {
        return YAML::Node();
    }

    TObjString* yamlObj = nullptr;
    metadataDir->GetObject(instanceName.c_str(), yamlObj);

    YAML::Node node;
    if (yamlObj) {
        try {
            node = YAML::Load(yamlObj->GetString().Data());
        } catch (const std::exception&) {
            throw std::runtime_error("TRestMetadata::ReadMetadata: Cannot read RESTMetadataStore");
        }
    }

    file->cd();

    return node;
}

void TRestMetadata::PrintMetadata() const {
    if (fNode && !fNode.IsNull()) {
        RESTMetadata << "===" << GetClassName() << "===" << RESTendl;
        RESTMetadata << YAML::Dump(fNode) << RESTendl;
    }
}

void TRestMetadataFieldRegistry::ApplyFields(TRestMetadata* instance, const YAML::Node& params) {
    if (!instance || !params) return;
    for (const auto& [type, fields] : fFieldMaps) {
        for (const auto& field : fields) {
            if (!field.appliesTo(instance)) continue;
            try {
                field.readFunc(instance, params);
            } catch (const std::exception& error) {
                throw std::runtime_error(instance->GetClassName() + " '" + instance->GetName() + "': '" +
                                         field.yamlKey + "': " + error.what());
            }
        }
    }
}

void TRestMetadataFieldRegistry::ApplyFieldsToYAML(TRestMetadata* instance, YAML::Node& params) {
    if (!instance) return;
    for (const auto& [type, fields] : fFieldMaps) {
        for (const auto& field : fields) {
            if (field.appliesTo(instance)) field.writeFunc(instance, params);
        }
    }
}

std::set<std::string> TRestMetadataFieldRegistry::GetFieldKeys(const TRestMetadata* instance) const {
    std::set<std::string> keys;
    if (!instance) return keys;
    for (const auto& [type, fields] : fFieldMaps) {
        for (const auto& field : fields) {
            if (field.appliesTo(instance)) keys.insert(field.yamlKey);
        }
    }
    return keys;
}

void TRestMetadataFieldRegistry::CheckFields(const TRestMetadata* instance, const YAML::Node& node) const {
    if (!instance || !node || !node.IsMap()) return;
    std::set<std::string> keys = GetFieldKeys(instance);
    for (const auto& key : instance->GetSharedNodeKeys()) keys.insert(key);
    bool hasChildren = false;
    for (const auto& isContainer : fChildContainers) hasChildren = hasChildren || isContainer(instance);

    std::string unknown;
    for (const auto& element : node) {
        const auto key = element.first.as<std::string>();
        if (keys.count(key) || key == "class" || key == "name" || key == "verbose") continue;
        if (hasChildren && element.second.IsMap() && element.second["class"]) continue;
        unknown += " '" + key + "'";
    }
    if (unknown.empty()) return;

    std::string valid;
    for (const auto& key : keys) valid += " " + key;
    throw std::runtime_error(instance->GetClassName() + " '" + instance->GetName() +
                             "': unknown parameter(s)" + unknown + ". Valid parameters: class name verbose" +
                             valid + (hasChildren ? " and metadata (maps with class)" : ""));
}

std::unique_ptr<TRestMetadata> MetadataClassRegistry::Create(const std::string& type,
                                                             const std::string& instanceName,
                                                             const YAML::Node& params, bool check) const {
    auto it = creators.find(type);
    if (it == creators.end()) throw std::runtime_error("MetadataClassRegistry: unknown type '" + type + "'");
    auto metadata = it->second(instanceName, params);
    if (check && metadata) TRestMetadataFieldRegistry::Instance().CheckFields(metadata.get(), params);
    return metadata;
}

void TRestMetadata::CheckConfig() const { TRestMetadataFieldRegistry::Instance().CheckFields(this, fNode); }
