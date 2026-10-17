#ifndef TRESTGEANT4GEOMETRYINFO_H
#define TRESTGEANT4GEOMETRYINFO_H

#include <map>
#include <set>
#include <string>
#include <vector>
#include <unordered_map>

// Modern ROOT vector headers (MathCore GenVector)
#include <Math/GenVector/Rotation3D.h>
#include <Math/Vector3D.h>

// ROOT Geometry Manager headers
class G4VPhysicalVolume;
class TGeoManager;

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
    std::unordered_map<std::string, std::set<std::string>> fG4ToAltNames;
    std::unordered_map<std::string, std::string> fAltToG4Name;


   public:
    TRestGeant4GeometryInfo() = default;
    virtual ~TRestGeant4GeometryInfo() = default;

    /// \brief Binds an existing geometry manager or loads a GDML file into ROOT.
    void LoadGeometry(TGeoManager* geoManager) { fGeoManager = geoManager; }
    /// \brief Imports geometry from a GDML file and initializes the ROOT geometry manager.
    void LoadGeometryFromGdml(const std::string& gdmlFilename);

    // --- Fast Validation Methods relying directly on TGeoManager ---
    bool IsValidLogicalVolume(const std::string& volume) const;

    bool IsValidPhysicalVolume(const std::string& volume) const;

    // --- Modernized Spatial Analytics Getters (Replacing TVector3/TRotation) ---

    /// \brief Gets the position in world coordinates of a given physical node.
    ROOT::Math::XYZVector GetPosition(const std::string& physicalVolumeName) const;

    /// \brief Gets the rotation matrix in world coordinates of a given physical node.
    ROOT::Math::Rotation3D GetRotation(const std::string& physicalVolumeName) const;

    // --- Material Extraction ---
    std::string GetMaterialNameOfLogical(const std::string& logicalVolumeName) const;

    inline TGeoManager* GetGeometry() const { return fGeoManager; }

    static std::string GetGeant4NameFiltered (const std::string& g4Name);

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
    /// \brief Converts a Geant4 path into the REST alternative naming convention.
    std::string GetAlternativePathFromGeant4Path(const std::string& g4Path) const;
    /// \brief Converts one Geant4 physical volume name into its alternative REST name.
    std::string GetAlternativeNameFromGeant4PhysicalName(const std::string& g4Name) const;
    /// \brief Returns all compatible alternative names for one Geant4 physical volume name.
    std::set<std::string> GetAlternativeNamesFromGeant4PhysicalName(const std::string& g4Name) const;
    /// \brief Resolves a REST alternative volume name back to a Geant4 physical name.
    std::string GetGeant4PhysicalNameFromAlternativeName(const std::string& altName) const;

    /// \brief Returns all logical volume names available in the loaded geometry.
    std::vector<std::string> GetAllLogicalVolumes() const;
    /// \brief Returns all physical volume names available in the loaded geometry.
    std::vector<std::string> GetAllPhysicalVolumes() const;
    /// \brief Returns physical volumes translated to alternative REST naming.
    std::vector<std::string> GetAllAlternativePhysicalVolumes() const;

    void InitializeOnDetectorConstruction(const std::string& gdmlFilename, const G4VPhysicalVolume* world) {
        if (/*fGeoManager == nullptr &&*/ !gdmlFilename.empty()) {
            LoadGeometryFromGdml(gdmlFilename);
        }
        PopulateFromGeant4World(world);
    }

    /// \brief Traverses the Geant4 world to populate logical/physical volume lookup maps.
    void PopulateFromGeant4World(const G4VPhysicalVolume* world);

    std::vector<std::string> GetAllPhysicalVolumesFromLogical(const std::string& logicalVolumeName) const;

    /// \brief Returns logical volume names matching a wildcard/regex-like expression.
    std::vector<std::string> GetAllLogicalVolumesMatchingExpression(const std::string& expression) const;
    /// \brief Returns physical volume names matching a wildcard/regex-like expression.
    std::vector<std::string> GetAllPhysicalVolumesMatchingExpression(const std::string& expression) const;

    /// \brief Prints geometry summary and cached mapping information.
    void Print() const;

    friend class DetectorConstruction;
    friend class TRestGeant4Metadata;
};

#endif
