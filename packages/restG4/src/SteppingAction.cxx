
#include "SteppingAction.h"

#include <TRestGeant4BiasingVolume.h>

#include "SimulationManager.h"

using namespace std;

SteppingAction::SteppingAction(SimulationManager* simulationManager)
    : fSimulationManager(simulationManager) {}

SteppingAction::~SteppingAction() {}

void SteppingAction::UserSteppingAction(const G4Step* step) {
    auto* outputManager = fSimulationManager->GetOutputManager();
    outputManager->RecordStep(step);
    // Volume biasing: the particles entering the biasing volume of the stage are stopped (and stored)
    if (const auto* volume = fSimulationManager->GetStoppingBiasingVolume()) {
        outputManager->ApplyBiasingVolume(step, *volume);
    }
}
