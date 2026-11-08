#ifndef TRESTGEANT4GEOMETRYINFO_H
#define TRESTGEANT4GEOMETRYINFO_H

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// Modern ROOT vector headers (MathCore GenVector)
#include <Math/GenVector/Rotation3D.h>
#include <Math/Vector3D.h>

// ROOT Geometry Manager headers
class G4VPhysicalVolume;
class TGeoManager;

/// \class TRestGeant4GeometryInfo
/// \brief Lightweight API and cache manager built on top of ROOT TGeoManager for simulation geometries.
///
/// Part of TRestGeant4Metadata. It keeps:
/// - the TGeoManager of the geometry (from the GDML in restG4, or the `Geometry` stored in the file);
/// - the volume ID map: the ID of each Geant4 physical volume name, the IDs stored in the hits of
///   TRestGeant4Event (filled by PopulateFromGeant4World in restG4, stored in the metadata);
/// - in restG4 only, the logical volume name ("alternative name") of each Geant4 physical volume.
///
/// The volume expressions are regular expressions (TPRegexp, matched anywhere in the name, e.g. "^gas"
/// for the names starting with gas) of physical or logical volume names.
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

    /// \brief Binds an existing geometry manager (not owned).
    void LoadGeometry(TGeoManager* geoManager) { fGeoManager = geoManager; }
    /// \brief Imports geometry from a GDML file (TRestGDMLParser::Import, entities and parameters resolved)
    /// and clears the volume ID map. It throws std::runtime_error if the file cannot be loaded.
    void LoadGeometryFromGdml(const std::string& gdmlFilename);

    // --- Fast Validation Methods relying directly on TGeoManager ---
    /// \brief True if the geometry has a logical volume (TGeoVolume) with this name.
    bool IsValidLogicalVolume(const std::string& volume) const;

    /// \brief True if a physical node has this name or path (node names below the top joined by the path
    /// separator). As all the physical node queries, it walks the node tree: TGeoManager::GetListOfNodes is
    /// only the current navigation branch and FindNode(const char*) is FindNode(bool)
    bool IsValidPhysicalVolume(const std::string& volume) const;

    // --- Modernized Spatial Analytics Getters (Replacing TVector3/TRotation) ---

    /// \brief Gets the position in world coordinates of a given physical node (name or path), (0, 0, 0)
    /// if not found.
    ROOT::Math::XYZVector GetPosition(const std::string& physicalVolumeName) const;

    /// \brief Gets the rotation matrix in world coordinates of a given physical node (name or path), the
    /// identity if not found.
    ROOT::Math::Rotation3D GetRotation(const std::string& physicalVolumeName) const;

    // --- Material Extraction ---
    /// \brief Material name of a logical volume ("" if not found).
    std::string GetMaterialNameOfLogical(const std::string& logicalVolumeName) const;

    /// \brief The geometry manager (nullptr if not loaded).
    inline TGeoManager* GetGeometry() const { return fGeoManager; }

    /// \brief The logical volume name in a Geant4 assembly physical name (av_<n>_impr_<n>_<logical>_pv_<n>),
    /// the name itself otherwise.
    static std::string GetGeant4NameFiltered(const std::string& g4Name);

    // --- Structural Interfaces & ID Maps ---
    /// \brief True if the top volume of the geometry is an assembly.
    inline bool IsAssembly() const { return fIsAssembly; }
    /// \brief Separator of the node names in a path ("_" by default).
    inline std::string GetPathSeparator() const { return fPathSeparator; }
    /// \brief Sets the separator of the node names in a path.
    inline void SetPathSeparator(const std::string& separator) { fPathSeparator = separator; }

    /// \brief Adds a volume (physical name) to the volume ID map.
    inline void InsertVolumeName(int id, const std::string& volumeName) {
        fVolumeNameMap[id] = volumeName;
        fVolumeNameReverseMap[volumeName] = id;
    }

    /// \brief Physical volume name of a volume ID ("" if not found).
    inline std::string GetVolumeFromID(int id) const {
        return fVolumeNameMap.count(id) ? fVolumeNameMap.at(id) : "";
    }

    /// \brief Volume ID of a physical volume name (-1 if not found).
    inline int GetIDFromVolume(const std::string& volumeName) const {
        return fVolumeNameReverseMap.count(volumeName) ? fVolumeNameReverseMap.at(volumeName) : -1;
    }

    // --- Geometric Path and Translation Methods ---
    /// \brief Converts a Geant4 path into the REST alternative naming convention: the '/' after the first
    /// one are replaced by the path separator.
    std::string GetAlternativePathFromGeant4Path(const std::string& g4Path) const;
    /// \brief Converts one Geant4 physical volume name into its alternative REST name (its logical volume
    /// name in restG4; the name itself if unknown).
    std::string GetAlternativeNameFromGeant4PhysicalName(const std::string& g4Name) const;
    /// \brief Returns all compatible alternative names for one Geant4 physical volume name (the name itself
    /// if unknown).
    std::set<std::string> GetAlternativeNamesFromGeant4PhysicalName(const std::string& g4Name) const;
    /// \brief Resolves a REST alternative volume name back to a Geant4 physical name (the name itself if
    /// unknown).
    std::string GetGeant4PhysicalNameFromAlternativeName(const std::string& altName) const;

    /// \brief Returns all logical volume names available in the loaded geometry.
    std::vector<std::string> GetAllLogicalVolumes() const;
    /// \brief Returns all physical volume (TGeo node) names available in the loaded geometry, without
    /// duplicates.
    std::vector<std::string> GetAllPhysicalVolumes() const;
    /// \brief Returns physical volumes translated to alternative REST naming (GetAllPhysicalVolumes if there
    /// are no alternative names).
    std::vector<std::string> GetAllAlternativePhysicalVolumes() const;

    /// \brief restG4: loads the GDML (if given) and fills the volume maps from the Geant4 world.
    void InitializeOnDetectorConstruction(const std::string& gdmlFilename, const G4VPhysicalVolume* world) {
        if (/*fGeoManager == nullptr &&*/ !gdmlFilename.empty()) {
            LoadGeometryFromGdml(gdmlFilename);
        }
        PopulateFromGeant4World(world);
    }

    /// \brief Traverses the Geant4 world to populate logical/physical volume lookup maps (implemented in
    /// restG4: the volume IDs follow the order of the traversal, daughters first).
    void PopulateFromGeant4World(const G4VPhysicalVolume* world);

    /// \brief Names of the physical nodes of a logical volume (the Geant4 physical names in restG4).
    std::vector<std::string> GetAllPhysicalVolumesFromLogical(const std::string& logicalVolumeName) const;

    /// \brief Returns logical volume names matching a regular expression (if none, the alternative names
    /// matching it).
    std::vector<std::string> GetAllLogicalVolumesMatchingExpression(const std::string& expression) const;
    /// \brief Returns physical volume names matching a regular expression: the Geant4 physical names
    /// (matching them or their logical name) in restG4, the TGeo node names otherwise.
    std::vector<std::string> GetAllPhysicalVolumesMatchingExpression(const std::string& expression) const;
    /// \brief Returns the alternative REST names of the physical volumes matching the expression or, if
    /// none, of the physical volumes of the logical volumes matching it (as legacy volume selections).
    std::set<std::string> GetVolumesMatchingExpression(const std::string& expression) const;
    /// \brief Returns the IDs of the volumes (volume ID map) whose name, or filtered name, is in `names`.
    std::set<int> GetVolumeIDsFromNames(const std::set<std::string>& names) const;
    /// \brief Returns the IDs of the volumes matching the expression, as GetVolumesMatchingExpression.
    std::set<int> GetVolumeIDsMatchingExpression(const std::string& expression) const {
        return GetVolumeIDsFromNames(GetVolumesMatchingExpression(expression));
    }

    /// \brief Prints a geometry summary (assembly, path separator, number of volumes and nodes).
    void Print() const;

    friend class DetectorConstruction;
    friend class TRestGeant4Metadata;
};

#endif
