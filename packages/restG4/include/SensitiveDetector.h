
#ifndef REST_SENSITIVEDETECTOR_H
#define REST_SENSITIVEDETECTOR_H

#include <G4VSensitiveDetector.hh>

class G4TouchableHistory;
class SimulationManager;

/// \class SensitiveDetector
/// \brief Adds the energy deposited in the sensitive volumes to the event (TRestGeant4Event sensitive
/// volume energy).
///
/// It is attached to logical volumes, but only the steps in the physical volumes declared sensitive in
/// the TRestGeant4Metadata are counted. For geantinos the step length (mm) is added as energy (keV).
class SensitiveDetector : public G4VSensitiveDetector {
   public:
    /// \param simulationManager the simulation manager (not owned)
    /// \param name name of the detector (the logical volume name)
    explicit SensitiveDetector(SimulationManager* simulationManager, const G4String& name);
    virtual ~SensitiveDetector() = default;

    /// \brief Nothing to do (no hits collection)
    virtual void Initialize(G4HCofThisEvent*) {}
    /// \brief Adds the energy deposited in the step to the OutputManager of the thread
    /// \return false if the step is not in a sensitive physical volume, true otherwise (the value is not
    /// used by Geant4)
    virtual G4bool ProcessHits(G4Step* aStep, G4TouchableHistory*);

   private:
    SimulationManager* fSimulationManager;
};
#endif  // REST_SENSITIVEDETECTOR_H
