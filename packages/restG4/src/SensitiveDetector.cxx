
#include "SensitiveDetector.h"

#include <TRestGeant4Metadata.h>

#include <G4Geantino.hh>
#include <G4Step.hh>
#include <G4SystemOfUnits.hh>
#include <G4Track.hh>

#include "SimulationManager.h"

using namespace std;

SensitiveDetector::SensitiveDetector(SimulationManager* simulationManager, const G4String& name)
    : G4VSensitiveDetector(name), fSimulationManager(simulationManager) {}

G4bool SensitiveDetector::ProcessHits(G4Step* step, G4TouchableHistory*) {
    // return value will always be ignored, its present for backwards compatibility (I guess)
    TRestGeant4Metadata* restG4Metadata = fSimulationManager->GetRestMetadata();

    /* Sensitive detector are assigned to logical volumes, but several physical volumes
    can share the same logical volume. Now we get the physical volume where the pre-step
    is located and filter if it is one of the declared sensitive volumes by the user*/

    // The Geant4 physical volume name identifies it (the names are unique, DetectorConstruction checks it),
    // as the sensitive volumes (the physical volumes matching them): also for nested volumes and assemblies
    const std::string physicalName = step->GetPreStepPoint()->GetPhysicalVolume()->GetName();

    // search and filter if this physical volume is sensitive
    const auto& sensitiveVolumes = restG4Metadata->GetSensitiveVolumes();
    if (std::find(sensitiveVolumes.begin(), sensitiveVolumes.end(), physicalName) == sensitiveVolumes.end()) {
        return false;
    }

    const bool isGeantino = step->GetTrack()->GetParticleDefinition() == G4Geantino::Definition();

    if (isGeantino) {
        // Since geantinos don't deposit energy, the length traveled inside the volumes is stored as energy
        // (mm as keV)
        const auto length = step->GetStepLength() / CLHEP::mm;
        fSimulationManager->GetOutputManager()->AddSensitiveEnergy(length);
        return true;
    } else {
        auto energy = step->GetTotalEnergyDeposit() / keV;

        if (energy <= 0) {
            return true;
        }
        fSimulationManager->GetOutputManager()->AddSensitiveEnergy(energy);
        return true;
    }
}
