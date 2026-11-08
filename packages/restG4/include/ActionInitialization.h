
#ifndef REST_ACTIONINITIALIZATION_H
#define REST_ACTIONINITIALIZATION_H

#include <G4VUserActionInitialization.hh>

class DetectorConstruction;
class G4VSteppingVerbose;
class SimulationManager;

/// \file ActionInitialization.h
/// \brief User action initialization of restG4.

/// \class ActionInitialization
/// \brief Creates the user actions of restG4 for the master and the worker threads.
///
/// The master thread only gets a RunAction. Each worker thread (or the single thread in serial mode) gets
/// its OutputManager (SimulationManager::InitializeOutputManager) and the PrimaryGeneratorAction, RunAction,
/// EventAction, SteppingAction, StackingAction and TrackingAction.
class ActionInitialization : public G4VUserActionInitialization {
   public:
    /// \param simulationManager the simulation manager shared by all the actions (not owned)
    ActionInitialization(SimulationManager*);
    virtual ~ActionInitialization();

    /// \brief Actions of the master thread: a RunAction
    virtual void BuildForMaster() const;
    /// \brief Actions of a worker thread: creates its OutputManager and all the user actions
    virtual void Build() const;

    /// \brief The SteppingVerbose of restG4, which records the first step of each track
    virtual G4VSteppingVerbose* InitializeSteppingVerbose() const;

   private:
    SimulationManager* fSimulationManager;
};

#endif  // REST_ACTIONINITIALIZATION_H
