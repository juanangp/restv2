#include "TRestMetadata.h"

#include "TRestTools.h"

TRestMetadata::TRestMetadata(const std::string& sectionName, const YAML::Node& node)
    : fSectionName(sectionName), fNode(node) {}

TRestMetadata::TRestMetadata(const std::string& fileName, const std::string& sectionName)
    : fConfigFileName(fileName), fSectionName(sectionName) {}

void TRestMetadata::ReadYAMLVerbose(YAML::Node& node) {
    if (node["verbose"]) {
        const std::string v = TRestTools::ReadYAMLParam<std::string>(node["verbose"]);
        fVerboseLevel = TRestLogManager::GetVerboseLevelFromString(v);
    } else {
        fVerboseLevel = TRestLogManager::globalVerboseLevel;
        node["verbose"] = TRestLogManager::GetStringFromVerbose(fVerboseLevel);
    }
}
