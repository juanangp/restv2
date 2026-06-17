#include "TRestMetadata.h"

#include "TRestTools.h"

TRestMetadata::TRestMetadata(const std::string& instanceName, const std::string& sectionName,
                             const YAML::Node& node)
    : fName(instanceName), fSectionName(sectionName), fNode(node) {
    if (fNode && fNode["name"]) {
        fName = fNode["name"].as<std::string>();
    }
}

TRestMetadata::TRestMetadata(const std::string& fileName, const std::string& sectionName)
    : fConfigFileName(fileName), fSectionName(sectionName) {
    if (fNode && fNode["name"]) {
        fName = fNode["name"].as<std::string>();
    } else {
        fName = sectionName;
    }
}

void TRestMetadata::ReadYAMLVerbose(YAML::Node& node) {
    if (node["verbose"]) {
        const std::string v = TRestTools::ReadYAMLParam<std::string>(node["verbose"]);
        fVerboseLevel = TRestLogManager::GetVerboseLevelFromString(v);
    } else {
        fVerboseLevel = TRestLogManager::globalVerboseLevel;
        node["verbose"] = TRestLogManager::GetStringFromVerbose(fVerboseLevel);
    }
}
