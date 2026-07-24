#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include <memory>

// Modern ROOT vector headers (MathCore GenVector)
#include <Math/Vector3D.h>
#include <Math/GenVector/Rotation3D.h>

// ROOT Geometry Manager headers
class G4VPhysicalVolume;
#include "TGeoManager.h"
#include "TGeoVolume.h"
#include "TGeoNode.h"
#include "TGeoMatrix.h"
#include "TList.h"

/// \class TRestGeant4GeometryInfo
/// \brief Lightweight API and cache manager built on top of ROOT TGeoManager for simulation geometries.
class TRestGeant4GeometryInfo {
   private:
    bool fIsAssembly = false;
    std::string fPathSeparator = "_";

    /// Pointer to delegate geometry lookup directly to ROOT's native engine
    TGeoManager* fGeoManager = nullptr;

    // Cache dictionaries mapping fast unique integer IDs to volume paths
    std::map<int, std::string> fVolumeNameMap;
    std::map<std::string, int> fVolumeNameReverseMap;

   public:
    TRestGeant4GeometryInfo() = default;
    virtual ~TRestGeant4GeometryInfo() = default;

    /// \brief Binds an existing geometry manager or loads a GDML file into ROOT.
    void LoadGeometry(TGeoManager* geoManager) { fGeoManager = geoManager; }
    void LoadGeometryFromGdml(const std::string& gdmlFilename);

    // --- Fast Validation Methods relying directly on TGeoManager ---
    inline bool IsValidLogicalVolume(const std::string& volume) const {
        return fGeoManager ? (fGeoManager->GetVolume(volume.c_str()) != nullptr) : false;
    }

    inline bool IsValidPhysicalVolume(const std::string& volume) const {
        return fGeoManager ? (fGeoManager->FindNode(volume.c_str()) != nullptr) : false;
    }

    // --- Modernized Spatial Analytics Getters (Replacing TVector3/TRotation) ---
    
    /// \brief Gets the position in world coordinates of a given physical node.
    inline ROOT::Math::XYZVector GetPosition(const std::string& physicalVolumeName) const {
        if (!fGeoManager) return {0.0, 0.0, 0.0};
        TGeoNode* node = fGeoManager->FindNode(physicalVolumeName.c_str());
        if (!node || !node->GetMatrix()) return {0.0, 0.0, 0.0};
        
        const double* translation = node->GetMatrix()->GetTranslation();
        return ROOT::Math::XYZVector(translation[0], translation[1], translation[2]);
    }

    /// \brief Gets the rotation matrix in world coordinates of a given physical node.
    inline ROOT::Math::Rotation3D GetRotation(const std::string& physicalVolumeName) const {
        if (!fGeoManager) return {};
        TGeoNode* node = fGeoManager->FindNode(physicalVolumeName.c_str());
        if (!node || !node->GetMatrix()) return {};
        
        const double* r = node->GetMatrix()->GetRotationMatrix();
        // Construct standard MathCore 3D rotation from raw matrix elements array directly
        return ROOT::Math::Rotation3D(
            r[0], r[1], r[2],
            r[3], r[4], r[5],
            r[6], r[7], r[8]
        );
    }

    // --- Material Extraction ---
    inline std::string GetMaterialNameOfLogical(const std::string& logicalVolumeName) const {
        if (!fGeoManager) return "";
        TGeoVolume* vol = fGeoManager->GetVolume(logicalVolumeName.c_str());
        return (vol && vol->GetMaterial()) ? vol->GetMaterial()->GetName() : "";
    }

    // --- Structural Interfaces & ID Maps ---
    inline bool IsAssembly() const { return fIsAssembly; }
    inline std::string GetPathSeparator() const { return fPathSeparator; }
    inline void SetPathSeparator(const std::string& separator) { fPathSeparator = separator; }

    inline void InsertVolumeName(int id, const std::string& volumeName) {
        fVolumeNameMap[id] = volumeName;
        fVolumeNameReverseMap[volumeName] = id;
    }

    inline std::string GetVolumeFromID(int id) const {
        return fVolumeNameMap.count(id) ? fVolumeNameMap.at(id) : "";
    }

    inline int GetIDFromVolume(const std::string& volumeName) const {
        return fVolumeNameReverseMap.count(volumeName) ? fVolumeNameReverseMap.at(volumeName) : -1;
    }

    // --- Geometric Path and Translation Methods ---
    std::string GetAlternativePathFromGeant4Path(const std::string& g4Path) const;
    std::string GetAlternativeNameFromGeant4PhysicalName(const std::string& g4Name) const;
    std::set<std::string> GetAlternativeNamesFromGeant4PhysicalName(const std::string& g4Name) const;
    std::string GetGeant4PhysicalNameFromAlternativeName(const std::string& altName) const;

    std::vector<std::string> GetAllLogicalVolumes() const;
    std::vector<std::string> GetAllPhysicalVolumes() const;
    std::vector<std::string> GetAllAlternativePhysicalVolumes() const;

    void InitializeOnDetectorConstruction(const std::string& gdmlFilename, const G4VPhysicalVolume* world) {
        if (fGeoManager == nullptr && !gdmlFilename.empty()) {
            LoadGeometryFromGdml(gdmlFilename);
        }
        PopulateFromGeant4World(world);
    }

    void PopulateFromGeant4World(const G4VPhysicalVolume* world);

    std::vector<std::string> GetAllPhysicalVolumesFromLogical(const std::string& logicalVolumeName) const {
        std::vector<std::string> result;
        if (!fGeoManager) return result;
        auto* nodes = fGeoManager->GetListOfNodes();
        if (!nodes) return result;
        for (int i = 0; i < nodes->GetEntries(); ++i) {
            auto* node = dynamic_cast<TGeoNode*>(nodes->At(i));
            if (!node || !node->GetVolume()) continue;
            if (logicalVolumeName == node->GetVolume()->GetName()) {
                result.emplace_back(node->GetName());
            }
        }
        return result;
    }

    std::vector<std::string> GetAllLogicalVolumesMatchingExpression(const std::string& expression) const;
    std::vector<std::string> GetAllPhysicalVolumesMatchingExpression(const std::string& expression) const;

    void Print() const;

    friend class DetectorConstruction;
};
