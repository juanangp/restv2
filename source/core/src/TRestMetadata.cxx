#include "TRestMetadata.h"
#include "TRestTools.h"

#include <TObjString.h>

using namespace TRestTools;

TRestMetadata::TRestMetadata(const std::string& instanceName,
                             const YAML::Node& node)
    : fName(instanceName), fNode(node) {
    if (fNode) {
        fName = ReadYAMLParamOrDefault<std::string>(fNode, "name", fName);
    }
}

TRestMetadata::TRestMetadata(const std::string& fileName, const std::string& sectionName)
    : fConfigFileName(fileName), fSectionName(sectionName) {
    YAML::Node raw = YAML::LoadFile(fileName);
    YAML::Node cfg = ResolveAllRefs(raw);
    fNode = cfg[sectionName];
    if (fNode) {
        fName = ReadYAMLParamOrDefault<std::string>(fNode, "name", sectionName);
    }
}

void TRestMetadata::ReadYAMLVerbose(YAML::Node& node) {
    const std::string verboseValue = ReadYAMLParamOrDefault<std::string>(
        node, "verbose", TRestLogManager::GetStringFromVerbose(TRestLogManager::globalVerboseLevel));
    fVerboseLevel = TRestLogManager::GetVerboseLevelFromString(verboseValue);
    TRestTools::SetNodeParameter(node, "verbose", TRestLogManager::GetStringFromVerbose(fVerboseLevel));
}

void TRestMetadata::WriteMetadata(TFile* file, 
                        const std::string& instanceName, 
                        const YAML::Node& configNode) {
    if (!file || file->IsZombie()) {
        throw std::runtime_error("TRestMetadata::WriteRESTMetadata: El archivo ROOT no es válido.");
    }
    if (!file->IsWritable()) {
        throw std::runtime_error("TRestMetadata::WriteRESTMetadata: El archivo ROOT es de solo lectura.");
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

    std::string yamlDump = YAML::Dump(configNode);

    auto* rootYamlString = new TObjString(yamlDump.c_str());
    
    rootYamlString->Write(instanceName.c_str(), TObject::kOverwrite);
    
    delete rootYamlString;

    file->cd();
}

YAML::Node TRestMetadata::ReadMetadata(TFile* file, const std::string& instanceName) {
if (!file || file->IsZombie()) return YAML::Node();
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
