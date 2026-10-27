#include "TRestGeant4GeometryInfo.h"

#include <TGeoManager.h>
#include <TGeoMatrix.h>
#include <TGeoNode.h>
#include <TGeoVolume.h>
#include <TPRegexp.h>

#include <filesystem>
#include <functional>


using namespace std;

namespace {

/// Visitor of a physical node: the node, its path (the node names below the top one joined by the path
/// separator) and its placement in world coordinates; it returns true to stop the visit
using NodeVisitor = function<bool(TGeoNode*, const string&, const TGeoHMatrix&)>;

/// Visits all the physical nodes, the top one first. TGeoManager::GetListOfNodes is only the current
/// navigation branch and FindNode(const char*) is FindNode(Bool_t), so the tree is walked
void VisitPhysicalNodes(TGeoManager* geoManager, const string& separator, const NodeVisitor& visit) {
    TGeoNode* top = geoManager ? geoManager->GetTopNode() : nullptr;
    if (!top) return;
    NodeVisitor walk = [&](TGeoNode* node, const string& path, const TGeoHMatrix& placement) {
        if (visit(node, path, placement)) return true;
        for (int i = 0; i < node->GetNdaughters(); i++) {
            TGeoNode* daughter = node->GetDaughter(i);
            TGeoHMatrix daughterPlacement = placement;
            daughterPlacement.Multiply(daughter->GetMatrix());
            const string name = daughter->GetName();
            if (walk(daughter, node == top ? name : path + separator + name, daughterPlacement)) return true;
        }
        return false;
    };
    walk(top, top->GetName(), TGeoHMatrix());
}

/// The node with this name or path, and its placement in world coordinates
TGeoNode* FindPhysicalNode(TGeoManager* geoManager, const string& separator, const string& name,
                           TGeoHMatrix& placement) {
    TGeoNode* found = nullptr;
    const auto visit = [&](TGeoNode* node, const string& path, const TGeoHMatrix& matrix) {
        if (name != node->GetName() && name != path) return false;
        found = node;
        placement = matrix;
        return true;
    };
    VisitPhysicalNodes(geoManager, separator, visit);
    return found;
}

}  // namespace

/// \brief Modernized entry point to load GDML geometry using ROOT's native TGeoManager engine.
void TRestGeant4GeometryInfo::LoadGeometryFromGdml(const std::string& gdmlFilename) {
    cout << "TRestGeant4GeometryInfo::LoadGeometryFromGdml - Importing via TGeoManager: " << gdmlFilename
         << endl;

    // Import the GDML file directly into ROOT's geometry core engine
    // This replaces the old legacy TXMLEngine manual parsing completely
    // ROOT resolves GDML external entities relative to the working directory, so import from the GDML directory
    const auto currentDirectory = filesystem::current_path();
    const auto gdmlDirectory = filesystem::path(gdmlFilename).parent_path();
    if (!gdmlDirectory.empty()) filesystem::current_path(gdmlDirectory);
    fGeoManager = TGeoManager::Import(filesystem::path(gdmlFilename).filename().c_str());
    filesystem::current_path(currentDirectory);
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
    std::set<std::string> names;
    const auto visit = [&](TGeoNode* node, const string&, const TGeoHMatrix&) {
        if (names.insert(node->GetName()).second) physicalVolumes.push_back(node->GetName());
        return false;
    };
    VisitPhysicalNodes(fGeoManager, fPathSeparator, visit);
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

    if (matched.empty()) {
        std::set<std::string> uniqueLogicalNames;
        for (const auto& pair : fG4ToAltNames) {
            uniqueLogicalNames.insert(pair.second.begin(), pair.second.end());
        }
        
        for (const auto& volName : uniqueLogicalNames) {
            if (regex.MatchB(volName.c_str())) {
                matched.push_back(volName);
            }
        }
    }

    return matched;
}

std::vector<std::string> TRestGeant4GeometryInfo::GetAllPhysicalVolumesMatchingExpression(
    const std::string& expression) const {
    std::vector<std::string> matched;
    TPRegexp regex(expression.c_str());

    // The Geant4 physical names (with their logical names), the ones of the hits, when the Geant4
    // geometry is known (restG4): inside an assembly they differ from the GDML/TGeo ones (e.g.
    // av_1_impr_1_<logical>_pv_0 instead of the physvol name)
    for (const auto& [physName, alternativeNames] : fG4ToAltNames) {
        const std::string cleanName = alternativeNames.empty() ? physName : *alternativeNames.begin();
        if (regex.MatchB(cleanName.c_str()) || regex.MatchB(physName.c_str())) matched.push_back(physName);
    }
    if (!matched.empty() || !fG4ToAltNames.empty()) return matched;

    // Otherwise the TGeo node names (the GDML physvol names)
    for (const auto& nodeName : GetAllPhysicalVolumes()) {
        if (regex.MatchB(nodeName.c_str())) matched.push_back(nodeName);
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

std::set<std::string> TRestGeant4GeometryInfo::GetVolumesMatchingExpression(
    const std::string& expression) const {
    auto physicalVolumes = GetAllPhysicalVolumesMatchingExpression(expression);
    if (physicalVolumes.empty()) {
        for (const auto& logicalVolume : GetAllLogicalVolumesMatchingExpression(expression)) {
            for (const auto& physicalVolume : GetAllPhysicalVolumesFromLogical(logicalVolume)) {
                physicalVolumes.push_back(physicalVolume);
            }
        }
    }

    std::set<std::string> volumes;
    for (const auto& physicalVolume : physicalVolumes) {
        volumes.insert(GetAlternativeNameFromGeant4PhysicalName(physicalVolume));
    }
    return volumes;
}

std::set<int> TRestGeant4GeometryInfo::GetVolumeIDsFromNames(const std::set<std::string>& names) const {
    std::set<int> ids;
    for (const auto& [id, name] : fVolumeNameMap) {
        if (names.count(name) || names.count(GetGeant4NameFiltered(name))) ids.insert(id);
    }
    return ids;
}

std::string TRestGeant4GeometryInfo::GetGeant4NameFiltered(const std::string& g4Name) {
    if (g4Name.find("av_") != std::string::npos && g4Name.find("_impr_") != std::string::npos) {
        size_t imprPos = g4Name.find("_impr_");
        size_t startPos = g4Name.find("_", imprPos + 6);
        size_t endPos = g4Name.rfind("_pv_");
        
        if (startPos != std::string::npos && endPos != std::string::npos && endPos > startPos) {
            return g4Name.substr(startPos + 1, endPos - startPos - 1);
        }
    }
    return g4Name;
}

std::string TRestGeant4GeometryInfo::GetAlternativeNameFromGeant4PhysicalName(
    const std::string& g4Name) const {

    auto it = fG4ToAltNames.find(g4Name);
    if (it != fG4ToAltNames.end() && !it->second.empty()) {
        return *(it->second.begin());
    }
    return g4Name;
}

std::set<std::string> TRestGeant4GeometryInfo::GetAlternativeNamesFromGeant4PhysicalName(
    const std::string& g4Name) const {
    auto it = fG4ToAltNames.find(g4Name);
    if (it != fG4ToAltNames.end()) {
        return it->second;
    }
    return {g4Name};
}

std::string TRestGeant4GeometryInfo::GetGeant4PhysicalNameFromAlternativeName(
    const std::string& altName) const {
    auto it = fAltToG4Name.find(altName);
    if (it != fAltToG4Name.end()) {
        return it->second;
    }
    return altName;
}

std::vector<std::string> TRestGeant4GeometryInfo::GetAllAlternativePhysicalVolumes() const {
    std::vector<std::string> altVolumes;
    altVolumes.reserve(fAltToG4Name.size());
    for (const auto& pair : fAltToG4Name) {
        altVolumes.push_back(pair.first);
    }
    return altVolumes.empty() ? GetAllPhysicalVolumes() : altVolumes;
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

bool TRestGeant4GeometryInfo::IsValidLogicalVolume(const std::string& volume) const {
    return fGeoManager ? (fGeoManager->GetVolume(volume.c_str()) != nullptr) : false;
}

bool TRestGeant4GeometryInfo::IsValidPhysicalVolume(const std::string& volume) const {
    TGeoHMatrix placement;
    return FindPhysicalNode(fGeoManager, fPathSeparator, volume, placement) != nullptr;
}

ROOT::Math::XYZVector TRestGeant4GeometryInfo::GetPosition(const std::string& physicalVolumeName) const {
    TGeoHMatrix placement;
    if (!FindPhysicalNode(fGeoManager, fPathSeparator, physicalVolumeName, placement)) return {0.0, 0.0, 0.0};

    const double* translation = placement.GetTranslation();
    return ROOT::Math::XYZVector(translation[0], translation[1], translation[2]);
}

ROOT::Math::Rotation3D TRestGeant4GeometryInfo::GetRotation(const std::string& physicalVolumeName) const {
    TGeoHMatrix placement;
    if (!FindPhysicalNode(fGeoManager, fPathSeparator, physicalVolumeName, placement)) return {};

    const double* r = placement.GetRotationMatrix();
    // Construct standard MathCore 3D rotation from raw matrix elements array directly
    return ROOT::Math::Rotation3D(r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]);
}

std::string TRestGeant4GeometryInfo::GetMaterialNameOfLogical(const std::string& logicalVolumeName) const {
    if (!fGeoManager) return "";
    TGeoVolume* vol = fGeoManager->GetVolume(logicalVolumeName.c_str());
    return (vol && vol->GetMaterial()) ? vol->GetMaterial()->GetName() : "";
}

std::vector<std::string> TRestGeant4GeometryInfo::GetAllPhysicalVolumesFromLogical(
    const std::string& logicalVolumeName) const {
    std::vector<std::string> result;
    // The Geant4 physical names when the Geant4 geometry is known (restG4), as in
    // GetAllPhysicalVolumesMatchingExpression: inside an assembly they differ from the TGeo ones
    for (const auto& [physName, logicalNames] : fG4ToAltNames) {
        if (logicalNames.count(logicalVolumeName)) result.push_back(physName);
    }
    if (!fG4ToAltNames.empty()) return result;

    // Otherwise the TGeo node names (the GDML physvol names)
    const auto visit = [&](TGeoNode* node, const string&, const TGeoHMatrix&) {
        if (node->GetVolume() && logicalVolumeName == node->GetVolume()->GetName()) {
            result.emplace_back(node->GetName());
        }
        return false;
    };
    VisitPhysicalNodes(fGeoManager, fPathSeparator, visit);
    return result;
}
