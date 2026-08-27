#include "TRestGeant4GeometryInfo.h"

#include <TPRegexp.h>

#include <algorithm>
#include <iostream>
#include <regex>

using namespace std;

/// \brief Modernized entry point to load GDML geometry using ROOT's native TGeoManager engine.
void TRestGeant4GeometryInfo::LoadGeometryFromGdml(const std::string& gdmlFilename) {
    cout << "TRestGeant4GeometryInfo::LoadGeometryFromGdml - Importing via TGeoManager: " << gdmlFilename
         << endl;

    // Import the GDML file directly into ROOT's geometry core engine
    // This replaces the old legacy TXMLEngine manual parsing completely
    fGeoManager = TGeoManager::Import(gdmlFilename.c_str());
    if (!fGeoManager) {
        cout << "TRestGeant4GeometryInfo::LoadGeometryFromGdml - ERROR: Failed to load GDML: " << gdmlFilename
             << endl;
        exit(1);
    }

    // Check if the root geometry structure utilizes assembly constructs
    fIsAssembly = false;
    TGeoVolume* topVolume = fGeoManager->GetTopVolume();
    if (topVolume && topVolume->IsAssembly()) {
        fIsAssembly = true;
    }

    // Lazily clear internal cache dictionaries
    fVolumeNameMap.clear();
    fVolumeNameReverseMap.clear();
}

/// \brief Returns all logical volume names defined inside the current geometry.
std::vector<std::string> TRestGeant4GeometryInfo::GetAllLogicalVolumes() const {
    std::vector<std::string> logicalVolumes;
    if (!fGeoManager) return logicalVolumes;

    TObjArray* volumes = fGeoManager->GetListOfVolumes();
    if (volumes) {
        for (int i = 0; i < volumes->GetEntries(); ++i) {
            TGeoVolume* vol = static_cast<TGeoVolume*>(volumes->At(i));
            if (vol) logicalVolumes.push_back(vol->GetName());
        }
    }
    return logicalVolumes;
}

/// \brief Returns all unique physical volume/node names defined inside the current geometry.
std::vector<std::string> TRestGeant4GeometryInfo::GetAllPhysicalVolumes() const {
    std::vector<std::string> physicalVolumes;
    if (!fGeoManager) return physicalVolumes;

    TObjArray* nodes = fGeoManager->GetListOfNodes();
    if (nodes) {
        for (int i = 0; i < nodes->GetEntries(); ++i) {
            TGeoNode* node = static_cast<TGeoNode*>(nodes->At(i));
            if (node) physicalVolumes.push_back(node->GetName());
        }
    }
    return physicalVolumes;
}

/// \brief Helper method to filter logical volume structures matching an explicit regular expression pattern.
std::vector<std::string> TRestGeant4GeometryInfo::GetAllLogicalVolumesMatchingExpression(
    const std::string& expression) const {
    std::vector<std::string> matched;
    std::vector<std::string> allLogical = GetAllLogicalVolumes();
    TPRegexp regex(expression.c_str());

    for (const auto& volName : allLogical) {
        if (regex.MatchB(volName.c_str())) {
            matched.push_back(volName);
        }
    }
    return matched;
}

/// \brief Helper method to filter physical volume structures matching an explicit regular expression pattern.
std::vector<std::string> TRestGeant4GeometryInfo::GetAllPhysicalVolumesMatchingExpression(
    const std::string& expression) const {
    std::vector<std::string> matched;
    std::vector<std::string> allPhysical = GetAllPhysicalVolumes();
    TPRegexp regex(expression.c_str());

    for (const auto& nodeName : allPhysical) {
        if (regex.MatchB(nodeName.c_str())) {
            matched.push_back(nodeName);
        }
    }
    return matched;
}

/// \brief Compatibility wrapper translating alternative paths into Geant4 formatted ones.
std::string TRestGeant4GeometryInfo::GetAlternativePathFromGeant4Path(const std::string& g4Path) const {
    const auto firstSeparator = g4Path.find('/');
    if (firstSeparator == std::string::npos) return g4Path;

    std::string alternativePath = g4Path;
    for (size_t pos = firstSeparator; pos != std::string::npos;
         pos = alternativePath.find('/', pos + fPathSeparator.size())) {
        alternativePath.replace(pos, 1, fPathSeparator);
    }
    return alternativePath;
}

std::string TRestGeant4GeometryInfo::GetAlternativeNameFromGeant4PhysicalName(
    const std::string& g4Name) const {
    return g4Name;  // Simplified fallback mapping since TGeoManager references the true unique node paths
}

std::set<std::string> TRestGeant4GeometryInfo::GetAlternativeNamesFromGeant4PhysicalName(
    const std::string& g4Name) const {
    return {g4Name};
}

std::string TRestGeant4GeometryInfo::GetGeant4PhysicalNameFromAlternativeName(
    const std::string& altName) const {
    return altName;
}

std::vector<std::string> TRestGeant4GeometryInfo::GetAllAlternativePhysicalVolumes() const {
    return GetAllPhysicalVolumes();
}

/// \brief Prints structural layout summaries retrieved dynamically from the active geometry manager.
void TRestGeant4GeometryInfo::Print() const {
    cout << "=== TRestGeant4GeometryInfo ===" << endl;
    if (!fGeoManager) {
        cout << " - Geometry data is not loaded (TGeoManager is null)" << endl;
        return;
    }
    cout << " - Top Geometry Assembly: " << (fIsAssembly ? "YES" : "NO") << endl;
    cout << " - Path Separator Token: '" << fPathSeparator << "'" << endl;
    cout << " - Total Registered Logical Volumes: " << fGeoManager->GetListOfVolumes()->GetEntries() << endl;
    cout << " - Total Registered Physical Nodes:   " << fGeoManager->GetListOfNodes()->GetEntries() << endl;
}
