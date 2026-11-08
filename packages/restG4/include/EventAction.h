
#ifndef EventAction_h
#define EventAction_h 1

#include <G4UserEventAction.hh>
#include <globals.hh>

class SimulationManager;

/// \class EventAction
/// \brief Begin and end of each Geant4 event, forwarded to the OutputManager of the thread.
class EventAction : public G4UserEventAction {
   public:
    /// \param simulationManager the simulation manager (not owned)
    EventAction(SimulationManager*);
    ~EventAction();

   public:
    /// \brief Starts the event of the OutputManager (it also checks the abort flag and the time limit)
    virtual void BeginOfEventAction(const G4Event*);
    /// \brief Submits the event to be stored if it is valid (OutputManager::FinishAndSubmitEvent)
    virtual void EndOfEventAction(const G4Event*);

   private:
    SimulationManager* fSimulationManager;
};

#endif
