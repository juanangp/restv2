//
// Created by lobis on 6/20/2022.
//

#ifndef REST_STEPPINGVERBOSE_H
#define REST_STEPPINGVERBOSE_H

#include <G4SteppingManager.hh>
#include <G4SteppingVerbose.hh>
#include <G4VSteppingVerbose.hh>

class SimulationManager;
class G4Step;

/// \class SteppingVerbose
/// \brief Stepping verbose used to record the initial step of each track (the track start point), which
/// the stepping action does not see. It prints nothing.
class SteppingVerbose : public G4SteppingVerbose {
   public:
    /// \param simulationManager the simulation manager (not owned)
    SteppingVerbose(SimulationManager*);
    ~SteppingVerbose();

    /// \brief Records the initial step of the track (OutputManager::RecordStep)
    virtual void TrackingStarted();
    /// \brief Nothing (no printout)
    virtual void StepInfo();

    /// \brief Verbose level of the stepping manager
    int GetSteppingVerbose() { return fManager->GetverboseLevel(); }
    /// \brief Sets the verbose level of the stepping manager (RunAction sets 1, so TrackingStarted is
    /// called)
    void SetSteppingVerbose(int level) { fManager->SetVerboseLevel(level); }

   private:
    SimulationManager* fSimulationManager;
};

#endif  // REST_STEPPINGVERBOSE_H
