
#ifndef PrimaryGeneratorAction_h
#define PrimaryGeneratorAction_h 1

#include <TRestGeant4ParticleState.h>
#include <TRestGeant4PrimaryGenerator.h>

#include <G4ParticleGun.hh>
#include <G4VUserPrimaryGeneratorAction.hh>
#include <map>
#include <memory>
#include <string>
#include <utility>

class G4Event;
class G4ParticleDefinition;
class SimulationManager;

/// \class PrimaryGeneratorAction
/// \brief Fills the particle gun with the primaries sampled by TRestGeant4PrimaryGenerator (one per
/// thread)
///
/// The generator is configured by the generator section of the TRestGeant4Metadata
/// (TRestGeant4PrimaryGeneratorInfo); its seed is the metadata seed plus the thread ID plus 1. The positions
/// in GDML volumes are sampled with the Geant4 solid of the generator volume (DetectorConstruction).
class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction {
   public:
    /// \param simulationManager the simulation manager (not owned)
    PrimaryGeneratorAction(SimulationManager*);
    ~PrimaryGeneratorAction() override;

    /// \brief A primary vertex per generated particle (particle, energy, direction, position and time).
    /// After the first biasing stage, a single vertex with the stored particle replayed by the event
    /// (SimulationManager::GetBiasingReplayParticle), the generator is not used. With the cry generator, when
    /// all the showers of the file were simulated, the simulation is stopped and the event aborted. It also
    /// records the wall time of the generation in the OutputManager
    void GeneratePrimaries(G4Event*) override;

   private:
    SimulationManager* fSimulationManager;

    /// Created at the first event, once the geometry (GDML placement, cosmic radius) is known
    std::unique_ptr<TRestGeant4PrimaryGenerator> fGenerator;

    G4ParticleGun fParticleGun;

    /// Particle definitions by name and excitation (keV)
    std::map<std::pair<std::string, double>, G4ParticleDefinition*> fParticles;

    G4ParticleDefinition* FindParticle(const TRestGeant4ParticleState& particle);
};

#endif
