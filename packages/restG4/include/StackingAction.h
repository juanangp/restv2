
#ifndef REST_STACKINGACTION_H
#define REST_STACKINGACTION_H

#include <G4ParticleDefinition.hh>
#include <G4UserStackingAction.hh>
#include <globals.hh>
#include <set>

class OutputManager;
class SimulationManager;

/// \class StackingAction
/// \brief Classifies the new tracks and splits the radioactive decays in sub-events.
///
/// Neutrinos are killed. The products of the decay of an unstable nucleus whose lifetime is above the
/// subEventTimeDelay of the metadata are delayed to a new sub-event (full chain) or killed (no full chain).
/// The isotopes of the full chain stop list are killed, and the secondaries created after the crossing point
/// of a step entering a biasing volume (OutputManager::ApplyBiasingVolume).
class StackingAction : public G4UserStackingAction {
   public:
    /// \param simulationManager gives the subEventTimeDelay and the full chain options of the metadata
    explicit StackingAction(SimulationManager*);
    ~StackingAction();

    /// \brief fUrgent for the primaries and for most tracks, fKill for the ignored particles and the full
    /// chain stop isotopes, and fWaiting (full chain) or fKill for the decays of long lived nuclei
    G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track*);
    /// \brief Called when the urgent stack is empty: the current sub-event is submitted and a new one
    /// (sub-event ID + 1) starts with the waiting tracks
    void NewStage();

    /// \brief The particles killed when created (by default the neutrinos)
    inline std::set<const G4ParticleDefinition*> GetParticlesToIgnore() const { return fParticlesToIgnore; }
    /// \brief Adds a particle to kill when created
    inline void AddParticleToIgnore(const G4ParticleDefinition* particle) {
        fParticlesToIgnore.insert(particle);
    }

   private:
    SimulationManager* fSimulationManager;
    double fMaxAllowedLifetime;
    G4String fMaxAllowedLifetimeWithUnit;

    std::set<const G4ParticleDefinition*> fParticlesToIgnore;
};

#endif  // REST_STACKINGACTION_H
