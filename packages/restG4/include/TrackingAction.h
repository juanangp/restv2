
#ifndef TrackingAction_h
#define TrackingAction_h 1

#include <G4Track.hh>
#include <G4UserTrackingAction.hh>
#include <globals.hh>

class RunAction;
class EventAction;
class SimulationManager;

/// \class TrackingAction
/// \brief Records the tracks in the event of the OutputManager of the thread.
class TrackingAction : public G4UserTrackingAction {
   public:
    /// \param simulationManager the simulation manager (not owned)
    TrackingAction(SimulationManager*);
    ~TrackingAction();

    /// \brief Inserts the new track in the event (OutputManager::RecordTrack)
    virtual void PreUserTrackingAction(const G4Track*);
    /// \brief Updates the track at its end (OutputManager::UpdateTrack)
    virtual void PostUserTrackingAction(const G4Track*);

   private:
    SimulationManager* fSimulationManager;
};

#endif
