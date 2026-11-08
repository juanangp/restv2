
#ifndef RunAction_h
#define RunAction_h 1

#include <G4UserRunAction.hh>
#include <fstream>
#include <globals.hh>
#include <iostream>
#include <map>

class G4Run;
class PrimaryGeneratorAction;
class SimulationManager;

/// \class RunAction
/// \brief Begin and end of the Geant4 run: writes a summary and calls SimulationManager::BeginOfRunAction
/// and SimulationManager::EndOfRunAction (which only act in the master thread).
class RunAction : public G4UserRunAction {
   public:
    /// \param simulationManager the simulation manager (not owned)
    RunAction(SimulationManager*);
    ~RunAction();

    /// \brief Writes the number of events and the limits of the run (master or serial thread) and starts
    /// the run in the SimulationManager (CTRL-C handler, progress thread)
    virtual void BeginOfRunAction(const G4Run*);
    /// \brief Ends the run in the SimulationManager (writes the events left) and writes the number of
    /// events stored (with biasing, the events simulated and the particles stored in the stage)
    virtual void EndOfRunAction(const G4Run*);

   private:
    SimulationManager* fSimulationManager;

    std::map<G4String, G4int> fParticleCount;
    std::map<G4String, G4double> fEmean;
    std::map<G4String, G4double> fEmin;
    std::map<G4String, G4double> fEmax;
};

#endif
