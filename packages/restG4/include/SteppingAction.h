
#ifndef SteppingAction_h
#define SteppingAction_h 1

#include <G4UserSteppingAction.hh>
#include <fstream>
#include <globals.hh>
#include <iostream>

class SimulationManager;

/// \class SteppingAction
/// \brief Records every step in the event of the OutputManager of the thread and, in a biasing stage with a
/// stopping volume, stops the particles entering it (OutputManager::ApplyBiasingVolume).
class SteppingAction : public G4UserSteppingAction {
   public:
    /// \param simulationManager the simulation manager (not owned)
    SteppingAction(SimulationManager*);
    ~SteppingAction();

    /// \brief Records the step (OutputManager::RecordStep) and applies the stopping biasing volume of the
    /// stage
    void UserSteppingAction(const G4Step*) override;

   private:
    SimulationManager* fSimulationManager;
};
#endif
