
#ifndef DetectorConstruction_h
#define DetectorConstruction_h 1

#include <Rtypes.h>

#include <G4PhysicalVolumeStore.hh>
#include <G4VSolid.hh>
#include <G4VUserDetectorConstruction.hh>

class G4GDMLParser;
class SimulationManager;

/// \file DetectorConstruction.h
/// \brief Geometry of restG4, read from the GDML file.

/// \class DetectorConstruction
/// \brief Builds the Geant4 geometry from the GDML file of the TRestGeant4Metadata.
///
/// The GDML is read with its entities and the parameters of the configuration resolved
/// (TRestGDMLParser::WriteResolved). Construct checks that the physical and logical volume names are
/// unique and that the sensitive, active, kill and generator volumes exist, sets the maximum step of the
/// active volumes, the magnetic field (uniform, from the metadata) and the placement of the generator
/// volume (gdml generators). ConstructSDandField attaches a SensitiveDetector to the logical volumes of
/// the sensitive volumes.
class DetectorConstruction : public G4VUserDetectorConstruction {
   private:
    SimulationManager* fSimulationManager;

    G4GDMLParser* fGdmlParser;
    G4VSolid* fGeneratorSolid;
    G4LogicalVolume* fGeneratorLogicalVolume;
    G4ThreeVector fGeneratorTranslation;
    G4RotationMatrix fGeneratorRotation;

    double fBoundBoxXMin, fBoundBoxXMax, fBoundBoxYMin, fBoundBoxYMax, fBoundBoxZMin, fBoundBoxZMax;

   public:
    /// \brief The physical volume with this name: the Geant4 name, or an alternative (REST) name of
    /// TRestGeant4GeometryInfo (e.g. inside an assembly)
    /// \return the volume, nullptr if not found
    G4VPhysicalVolume* GetPhysicalVolume(const G4String& physVolName) const;
    /// \brief Whether a point (in the frame of logVol) is inside any daughter of logVol (not
    /// recursive). Used to sample positions in the generator volume excluding its daughters
    bool IsPointInsideAnyDaughterVolume(const G4LogicalVolume* logVol, const G4ThreeVector& point) const;
    /// \brief Solid of the generator volume (gdml generators), nullptr otherwise
    inline G4VSolid* GetGeneratorSolid() const { return fGeneratorSolid; }
    /// \brief Logical volume of the generator volume (gdml generators)
    inline G4LogicalVolume* GetGeneratorLogicalVolume() const { return fGeneratorLogicalVolume; }
    /// \brief Translation of the generator volume in the world (world = rotation * local + translation)
    inline G4ThreeVector GetGeneratorTranslation() const { return fGeneratorTranslation; }
    /// \brief Rotation of the generator volume in the world (world = rotation * local + translation)
    inline G4RotationMatrix GetGeneratorRotation() const { return fGeneratorRotation; }

    /// \name Bounding box of the generator solid
    /// Local coordinates (mm) with a margin of 1%, to sample positions inside the generator volume
    ///@{
    inline double GetBoundBoxXMin() const { return fBoundBoxXMin; }
    inline double GetBoundBoxXMax() const { return fBoundBoxXMax; }
    inline double GetBoundBoxYMin() const { return fBoundBoxYMin; }
    inline double GetBoundBoxYMax() const { return fBoundBoxYMax; }
    inline double GetBoundBoxZMin() const { return fBoundBoxZMin; }
    inline double GetBoundBoxZMax() const { return fBoundBoxZMax; }
    ///@}

    /// \param simulationManager gives the TRestGeant4Metadata (not owned)
    DetectorConstruction(SimulationManager*);
    ~DetectorConstruction();

   public:
    /// \brief Reads the GDML and checks and configures the geometry (see the class description). A
    /// configuration error ends the program
    /// \return the world volume
    G4VPhysicalVolume* Construct() override;
    /// \brief A SensitiveDetector (and a G4Region) for each logical volume of the sensitive volumes:
    /// physical volume names (Geant4 or REST), logical volume names or regular expressions of logical
    /// volume names
    void ConstructSDandField() override;

    friend class TRestGeant4GeometryInfo;
};
#endif
