
#ifndef PhysicsList_h
#define PhysicsList_h 1

#include <G4EmConfigurator.hh>
#include <G4VModularPhysicsList.hh>
#include <globals.hh>

class SimulationManager;
class TRestGeant4PhysicsLists;
/// \class PhysicsList
/// \brief The Geant4 physics list of restG4, built from the physics lists of a TRestGeant4PhysicsLists.
///
/// Supported lists: G4DecayPhysics, G4RadioactiveDecayPhysics, one electromagnetic list
/// (G4EmLivermorePhysics, G4EmPenelopePhysics, G4EmStandardPhysics_option3 or
/// G4EmStandardPhysics_option4; more than one is an error) and the hadronic G4HadronPhysicsQGSP_BIC_HP,
/// G4IonBinaryCascadePhysics, G4HadronElasticPhysicsHP, G4NeutronTrackingCut and G4EmExtraPhysics. The
/// production cuts and their energy range come from the TRestGeant4PhysicsLists.
class PhysicsList : public G4VModularPhysicsList {
   public:
    PhysicsList() = delete;
    /// \param simulationManager the simulation manager (not owned)
    /// \param restPhysicsLists the physics lists and cuts of the configuration (not owned)
    explicit PhysicsList(SimulationManager* simulationManager, TRestGeant4PhysicsLists* restPhysicsLists);
    ~PhysicsList() override;

   protected:
    // Construct particle and physics
    /// \brief Creates the physics constructors of the lists enabled in the TRestGeant4PhysicsLists
    virtual void InitializePhysicsLists();
    /// \brief Applies the ParticleHP options of the TRestGeant4PhysicsLists (global G4ParticleHPManager
    /// settings), before the hadronic processes are built
    void ApplyParticleHPOptions() const;

    /// \brief Defines the geantino and the particles of the enabled physics constructors
    void ConstructParticle() override;
    /// \brief Builds the processes of the enabled physics constructors, plus the step limiters (user
    /// limits of the active volumes)
    void ConstructProcess() override;
    /// \brief Production cuts of gamma, e-, e+, mu+/mu- and neutron, from the TRestGeant4PhysicsLists
    void SetCuts() override;

   private:
    SimulationManager* fSimulationManager = nullptr;

    G4EmConfigurator fEmConfig;

    G4VPhysicsConstructor* fEmPhysicsList = nullptr;
    std::string fEmPhysicsListName;  // Can be different from the output of GetPhysicsName

    G4VPhysicsConstructor* fDecPhysicsList = nullptr;
    G4VPhysicsConstructor* fRadDecPhysicsList = nullptr;
    std::vector<G4VPhysicsConstructor*> fHadronPhys;

    TRestGeant4PhysicsLists* fRestPhysicsLists = nullptr;
};

#endif
