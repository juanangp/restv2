
#include "RunAction.h"

#include <TRestGeant4Metadata.h>
#include <TRestRun.h>

#include <G4RunManager.hh>

#include "SimulationManager.h"
#include "SteppingVerbose.h"
#include "TRestTools.h"

using namespace std;

RunAction::RunAction(SimulationManager* simulationManager)
    : G4UserRunAction(), fSimulationManager(simulationManager) {}

RunAction::~RunAction() = default;

void RunAction::BeginOfRunAction(const G4Run*) {
    if (G4Threading::IsMasterThread() || !G4Threading::IsMultithreadedApplication()) {
        G4cout << "========================== Begin of Run Action ========================" << G4endl;
        if (fSimulationManager->IsBiasingActive()) {
            G4cout << "Biasing stage " << fSimulationManager->GetBiasingStage() << " of "
                   << fSimulationManager->GetRestMetadata()->GetNumberOfBiasingVolumes()
                   << (fSimulationManager->IsOutputStage()
                           ? " (last stage, events written)"
                           : " (particles entering the biasing volume " +
                                 std::to_string(fSimulationManager->GetBiasingStage()) +
                                 " are stopped and stored)")
                   << G4endl;
        }
        G4cout << "Events to be simulated: "
               << G4RunManager::GetRunManager()->GetNumberOfEventsToBeProcessed() << G4endl;
        if (fSimulationManager->GetRestMetadata()->GetNumberOfRequestedEntries() > 0) {
            G4cout << "Requested number of entries in file: "
                   << fSimulationManager->GetRestMetadata()->GetNumberOfRequestedEntries() << G4endl;
        }
        if (fSimulationManager->GetRestMetadata()->GetSimulationMaxTimeSeconds() > 0) {
            G4cout << "Maximum simulation time: "
                   << TRestTools::ToTimeStringLong(
                          fSimulationManager->GetRestMetadata()->GetSimulationMaxTimeSeconds())
                   << G4endl;
        }
        G4cout << "=======================================================================" << G4endl;
    }

    fSimulationManager->BeginOfRunAction();

    auto steppingVerbose = ((SteppingVerbose*)G4VSteppingVerbose::GetInstance());
    steppingVerbose->SetSteppingVerbose(1);

    // inform the runManager to save random number seed
    G4RunManager::GetRunManager()->SetRandomNumberStore(false);
}

void RunAction::EndOfRunAction(const G4Run*) {
    fSimulationManager->EndOfRunAction();

    TRestRun* restRun = fSimulationManager->GetRestRun();
    const auto metadata = fSimulationManager->GetRestMetadata();

    if (G4Threading::IsMasterThread() || !G4Threading::IsMultithreadedApplication()) {
        G4cout << "============================= Run Summary =============================" << endl;
        const int stage = fSimulationManager->GetBiasingStage();
        if (!fSimulationManager->IsBiasingActive()) {
            G4cout << restRun->GetSavedEntries() << " events stored out of " << metadata->GetNumberOfEvents()
                   << " simulated events" << endl;
        } else if (!fSimulationManager->IsOutputStage()) {
            G4cout << "Biasing stage " << stage << ": " << metadata->GetBiasingStageEvents(stage)
                   << " events simulated, " << metadata->GetBiasingStageParticles(stage)
                   << " particles stored entering the biasing volume " << stage << endl;
        } else {
            G4cout << "Biasing stage " << stage << ": " << restRun->GetSavedEntries()
                   << " events stored out of " << metadata->GetBiasingStageEvents(stage)
                   << " simulated events" << endl;
        }
        G4cout << "=======================================================================" << endl;
    }
}
