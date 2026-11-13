
#include "SimulationManager.h"

#include <TRestGeant4BiasingVolume.h>
#include <TRestGeant4Event.h>
#include <TRestGeant4Metadata.h>
#include <TRestGeant4PhysicsLists.h>
#include <TRestRun.h>

#include <G4DynamicParticle.hh>
#include <G4Event.hh>
#include <G4EventManager.hh>
#include <G4RunManager.hh>
#include <G4Step.hh>
#include <G4SystemOfUnits.hh>
#include <G4Threading.hh>
#include <G4Track.hh>
#include <G4VPhysicalVolume.hh>
#include <Randomize.hh>
#include <algorithm>
#include <csignal>
#include <ctime>

#include "TRestTools.h"

using namespace std;

thread_local OutputManager* SimulationManager::fOutputManager = nullptr;

SimulationManager::SimulationManager() {
    // Only master thread should create the SimulationManager
    if (!G4Threading::IsMasterThread()) {
        G4Exception("SimulationManager::SimulationManager", "REST_G4_MASTER_THREAD", FatalException,
                    "Only master thread should create the SimulationManager!");
    }
}

void SimulationManager::InitializeOutputManager() {
    lock_guard<std::mutex> guard(fSimulationManagerMutex);
    fOutputManager = new OutputManager(this);
    fOutputManagerContainer.push_back(fOutputManager);
}

void PeriodicPrint(SimulationManager* simulationManager) {
    const auto restG4Metadata = simulationManager->GetRestMetadata();

    while (!simulationManager->GetPeriodicPrintThreadEndFlag()) {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (!restG4Metadata->PrintProgress() &&
            restG4Metadata->GetVerboseLevel() < TRestLogManager::REST_Verbose_Level::REST_Silent) {
            continue;
        }

        for (auto& outputManager : simulationManager->GetOutputManagerContainer()) {
            simulationManager->SyncStatsFromChild(outputManager);
        }

        string outputPercentageType = "events";
        double completionPercentage = double(simulationManager->GetNumberOfProcessedEvents()) /
                                      double(simulationManager->GetNumberOfEventsInRun()) * 100;
        if (restG4Metadata->GetNumberOfRequestedEntries() > 0) {
            auto completionPercentageEntries = double(simulationManager->GetNumberOfStoredEvents()) /
                                               double(restG4Metadata->GetNumberOfRequestedEntries()) * 100;
            if (completionPercentageEntries > completionPercentage) {
                completionPercentage = completionPercentageEntries;
                outputPercentageType = "entries";
            }
        }
        if (restG4Metadata->GetSimulationMaxTimeSeconds() > 0) {
            auto completionPercentageTime = double(simulationManager->GetElapsedTime()) /
                                            double(restG4Metadata->GetSimulationMaxTimeSeconds()) * 100;
            if (completionPercentageTime > completionPercentage) {
                completionPercentage = completionPercentageTime;
                outputPercentageType = "time";
            }
        }

        string progress;
        if (outputPercentageType == "events") {
            progress = " | " + to_string(simulationManager->GetNumberOfProcessedEvents()) +
                       " events processed / " + to_string(simulationManager->GetNumberOfEventsInRun()) +
                       " requested (" +
                       TRestTools::Format("%.2e", simulationManager->GetNumberOfProcessedEvents() /
                                                      simulationManager->GetElapsedTime()) +
                       "/s)";
        } else if (outputPercentageType == "entries") {
            progress = " | " + to_string(simulationManager->GetNumberOfStoredEvents()) + " entries / " +
                       to_string(restG4Metadata->GetNumberOfRequestedEntries()) + " requested";
        } else if (outputPercentageType == "time") {
            progress = " | " + TRestTools::ToTimeStringLong(simulationManager->GetElapsedTime()) +
                       " elapsed / " +
                       TRestTools::ToTimeStringLong(
                           simulationManager->GetRestMetadata()->GetSimulationMaxTimeSeconds());
        }

        string timeInfo;
        if (outputPercentageType != "time") {
            timeInfo = " | " + TRestTools::ToTimeStringLong(simulationManager->GetElapsedTime()) + " elapsed";
        }

        G4cout << TRestTools::Format("%5.2f", completionPercentage) << "% | "
               << simulationManager->GetNumberOfStoredEvents() << " events stored / "
               << simulationManager->GetNumberOfProcessedEvents() << " processed ("
               << TRestTools::Format("%.2e", simulationManager->GetNumberOfStoredEvents() /
                                                 simulationManager->GetElapsedTime())
               << "/s)" << progress << timeInfo << G4endl;
    }
}

static SimulationManager* gCurrentSimulationManager = nullptr;

void interruptSignalHandler(int sig) {
    cout << "\nStopping Run! Program was manually stopped by user (CTRL+C)!" << endl;

    if (gCurrentSimulationManager) {
        gCurrentSimulationManager->StopSimulation();
    }
}

void SimulationManager::BeginOfRunAction() {
    if (G4Threading::IsMultithreadedApplication() && G4Threading::G4GetThreadId() != -1) {
        return;
    }

    gCurrentSimulationManager = this;

    std::signal(SIGINT, interruptSignalHandler);

    // The start of the simulation: the biasing stages are several runs, the time limit is for all of them
    if (fTimeStartUnix == 0) fTimeStartUnix = chrono::steady_clock::now().time_since_epoch().count();
    // The counters of the threads were taken at the end of the previous run
    fNumberOfProcessedEvents = 0;
    fNumberOfEventsInRun = G4RunManager::GetRunManager()->GetNumberOfEventsToBeProcessed();
    fPeriodicPrintThreadEndFlag = false;
#ifndef GEANT4_WITHOUT_G4RunManagerFactory
    // gives segfault in old Geant4 versions such as 10.4.3, didn't look into it
    if (GetRestMetadata()->PrintProgress() ||
        GetRestMetadata()->GetVerboseLevel() >= TRestLogManager::REST_Verbose_Level::REST_Silent) {
        fPeriodicPrintThread = make_unique<thread>(&PeriodicPrint, this);
    }
#endif
}

void SimulationManager::EndOfRunAction() {
    if (G4Threading::IsMultithreadedApplication() && G4Threading::G4GetThreadId() != -1) {
        return;  // Only call this once from the main thread
    }

    fPeriodicPrintThreadEndFlag = true;

    WriteEvents();

    if (fPeriodicPrintThread != nullptr) {
        if (fPeriodicPrintThread->joinable()) {
            fPeriodicPrintThread->join();  // need to join thread, it may block for up to 1 thread period
        }
    }

    // The output managers (one per worker thread, built once by ActionInitialization::Build) are kept for
    // the next run (/run/beamOn again), deleted by the destructor
    for (auto& outputManager : fOutputManagerContainer) {
        fNumberOfProcessedEvents += outputManager->TakeEventCounter();
    }
    // The primaries of the generator: the first biasing stage (the only one without biasing)
    const int stage = fBiasingStage;
    if (stage == 0) GetRestMetadata()->SetNumberOfEvents(fNumberOfProcessedEvents);
    if (IsBiasingActive()) {
        GetRestMetadata()->SetBiasingStageEvents(stage, fNumberOfProcessedEvents);
        lock_guard<mutex> guard(fBiasingMutex);
        GetRestMetadata()->SetBiasingStageParticles(stage, static_cast<int64_t>(fBiasingParticles.size()));
    }

    // cry generator: the showers simulated and their exposure, to normalize to a rate
    auto& generatorInfo = GetRestMetadata()->GetGeant4PrimaryGeneratorInfo();
    if (generatorInfo.IsCRY() && stage == 0) {
        generatorInfo.UpdateCRYExposure();
        G4cout << "CRY showers simulated: " << generatorInfo.GetCRYShowersUsed() << ", exposure "
               << generatorInfo.GetCRYExposureTime() / 1e6 << " s" << G4endl;
    }
}

SimulationManager::~SimulationManager() {
    delete fRestRun;
    delete fRestGeant4Metadata;
    delete fRestGeant4PhysicsLists;

    for (auto& outputManager : fOutputManagerContainer) {
        delete outputManager;
    }
}

void SimulationManager::InsertEvent(std::unique_ptr<TRestGeant4Event>& event) {
    lock_guard<mutex> guard(fSimulationManagerMutex);
    fEventContainer.push(std::move(event));
}

void SimulationManager::WriteEvents(bool blocking) {
    unique_lock<mutex> writeLock(fWriteMutex, defer_lock);
    if (blocking) {
        writeLock.lock();
    } else if (!writeLock.try_lock()) {
        return;
    }

    bool wrote = false;
    while (true) {
        unique_ptr<TRestGeant4Event> event;
        {
            lock_guard<mutex> guard(fSimulationManagerMutex);
            if (fEventContainer.empty()) {
                break;
            }
            event = std::move(fEventContainer.front());
            fEventContainer.pop();
        }

        fEvent.MoveFrom(std::move(*event));
        event.reset();

        if (!fRestGeant4Metadata->GetStoreTracks()) {
            fEvent.ClearTracks();
        }

        fRestRun->Fill();
        wrote = true;
    }

    if (!wrote) {
        return;
    }

    fNumberOfStoredEvents = fRestRun->GetSavedEntries();

    const auto nRequestedEntries = GetRestMetadata()->GetNumberOfRequestedEntries();
    if (nRequestedEntries > 0 && !fAbortFlag && fNumberOfStoredEvents >= nRequestedEntries) {
        G4cout << "Stopping Run! We have reached the number of requested entries (" << nRequestedEntries
               << ")" << endl;
        StopSimulation();
    }
}

void SimulationManager::InitializeUserDistributions() {
    if (fRestGeant4Metadata == nullptr || (fRestGeant4Metadata->GetNumberOfSources() == 0 &&
                                           !fRestGeant4Metadata->GetGeant4PrimaryGeneratorInfo().IsCRY())) {
        G4Exception("SimulationManager::InitializeUserDistributions", "REST_G4_NO_PARTICLE_SOURCE",
                    FatalException, "No generator or particle sources defined");
    }
    // Formulas, histograms and Decay0 files, read once and copied by the generator of each thread, and the
    // CRY generator, shared
    try {
        auto& generatorInfo = fRestGeant4Metadata->GetGeant4PrimaryGeneratorInfo();
        generatorInfo.LoadDistributions(fRestGeant4Metadata->GetSeed());
    } catch (const std::exception& error) {
        G4Exception("SimulationManager::InitializeUserDistributions", "REST_G4_GENERATOR", FatalException,
                    error.what());
    }
}

void SimulationManager::StopSimulation() {
    // Still needs to be propagated to other threads, this is done in the BeginOfEventAction
    G4RunManager::GetRunManager()->AbortRun(true);
    fAbortFlag = true;
}

bool SimulationManager::IsBiasingActive() const {
    return fRestGeant4Metadata != nullptr && fRestGeant4Metadata->GetNumberOfBiasingVolumes() > 0;
}

bool SimulationManager::IsOutputStage() const {
    return !IsBiasingActive() ||
           fBiasingStage >= static_cast<int>(fRestGeant4Metadata->GetNumberOfBiasingVolumes());
}

const TRestGeant4BiasingVolume* SimulationManager::GetStoppingBiasingVolume() const {
    if (fRestGeant4Metadata == nullptr) return nullptr;
    const auto& volumes = fRestGeant4Metadata->GetBiasingVolumes();
    const int stage = fBiasingStage;
    if (stage >= static_cast<int>(volumes.size())) return nullptr;
    return &volumes[stage];
}

void SimulationManager::StoreBiasingParticle(const BiasingParticle& particle) {
    lock_guard<mutex> guard(fBiasingMutex);
    fBiasingParticles.push_back(particle);
}

size_t SimulationManager::GetNumberOfStoredBiasingParticles() {
    lock_guard<mutex> guard(fBiasingMutex);
    return fBiasingParticles.size();
}

const BiasingParticle& SimulationManager::GetBiasingReplayParticle(int eventID) const {
    const size_t index = static_cast<size_t>(eventID) / static_cast<size_t>(fBiasingReplicas);
    if (eventID < 0 || index >= fBiasingReplayParticles.size()) {
        G4Exception("SimulationManager::GetBiasingReplayParticle", "REST_G4_BIASING", FatalException,
                    ("No biasing particle for event ID " + to_string(eventID)).c_str());
    }
    return fBiasingReplayParticles[index];
}

int64_t SimulationManager::StartNextBiasingStage() {
    const auto& volumes = fRestGeant4Metadata->GetBiasingVolumes();
    const int stage = fBiasingStage;
    lock_guard<mutex> guard(fBiasingMutex);
    // The order of the threads is not reproducible, the event IDs (and the order inside each event) are
    std::sort(fBiasingParticles.begin(), fBiasingParticles.end(),
              [](const BiasingParticle& a, const BiasingParticle& b) {
                  return a.eventID != b.eventID ? a.eventID < b.eventID : a.sequence < b.sequence;
              });
    fBiasingReplayParticles = std::move(fBiasingParticles);
    fBiasingParticles.clear();
    fBiasingReplicas = static_cast<int>(volumes.at(stage).GetBiasingFactor());
    fBiasingStage = stage + 1;
    return static_cast<int64_t>(fBiasingReplayParticles.size()) * fBiasingReplicas;
}

void SimulationManager::SyncStatsFromChild(OutputManager* outputManager) {
    if (outputManager == nullptr) {
        return;
    }
    lock_guard<mutex> guard(fSimulationManagerMutex);
    fNumberOfProcessedEvents += outputManager->TakeEventCounter();
}

// OutputManager
OutputManager::OutputManager(const SimulationManager* simulationManager)
    : fSimulationManager(const_cast<SimulationManager*>(simulationManager)) {
    // this class should only exist on the threads performing the simulation
    if (G4Threading::IsMasterThread() && G4Threading::IsMultithreadedApplication()) {
        G4Exception("OutputManager::OutputManager", "REST_G4_MULTITHREADING", FatalException,
                    "Error in 'OutputManager', this instance should never exist");
    }
}

void OutputManager::UpdatePrimaryData(const G4Event* event) {
    if (fEvent) fEvent->UpdatePrimaryData(event);
}

void OutputManager::BeginOfEventAction() {
    // This should only be executed once at BeginOfEventAction
    UpdateEvent();
    fProcessedEventsCounter++;
    fBiasingKilledSecondaries.clear();
    fBiasingSequence = 0;

    if (fSimulationManager->GetAbortFlag()) {
        G4RunManager::GetRunManager()->AbortRun(true);
    }

    // activeVolumes chance: the hits of each active volume are stored in this event with its probability
    // (legacy semantics; the Geant4 random engine of the thread, reproducible with the seed)
    fVolumesNotStored.clear();
    const auto* metadata = fSimulationManager->GetRestMetadata();
    for (unsigned int i = 0; i < metadata->GetNumberOfActiveVolumes(); i++) {
        const auto& volume = metadata->GetActiveVolumeName(i);
        const double chance = metadata->GetStorageChance(volume);
        if (chance < 1 && G4UniformRand() >= chance) fVolumesNotStored.insert(volume);
    }

    if (fSimulationManager->GetRestMetadata()->GetSimulationMaxTimeSeconds() != 0 &&
        !fSimulationManager->GetAbortFlag() &&
        fSimulationManager->GetElapsedTime() >
            fSimulationManager->GetRestMetadata()->GetSimulationMaxTimeSeconds()) {
        G4cout << "Stopping Run! We have reached the time limit of "
               << TRestTools::ToTimeStringLong(
                      fSimulationManager->GetRestMetadata()->GetSimulationMaxTimeSeconds())
               << endl;
        fSimulationManager->StopSimulation();
    }
}

void OutputManager::UpdateEvent() {
    auto event = G4EventManager::GetEventManager()->GetConstCurrentEvent();

    if (!fEvent) {
        fEvent = make_unique<TRestGeant4Event>();
    } else {
        // The event not submitted (not valid) is reused: everything is reset (tracks, hits, energies,
        // volumes, sub-event data and sub-event ID, the caller sets it for the next sub-event)
        fEvent->Initialize();
    }
    fEvent->SetID(event->GetEventID());
    fEvent->SetOK(true);
    // Unix time with nanoseconds: a rate in bins shorter than a second
    const std::chrono::duration<double> now = std::chrono::system_clock::now().time_since_epoch();
    fEvent->SetTime(now.count());

    if (fSimulationManager->GetRestMetadata() != nullptr) {
        fEvent->SetGeant4Metadata(fSimulationManager->GetRestMetadata());
    }

    if (fSimulationManager->GetRestRun() != nullptr) {
        fEvent->InitializeReferences(fSimulationManager->GetRestRun());
    }

    // The primaries of the current Geant4 event (also set by the primary generator, before)
    fEvent->UpdatePrimaryData(event);

    const auto metadata = fSimulationManager->GetRestMetadata();
    if (metadata == nullptr) {
        std::ostringstream errMsg;
        errMsg << "\n---------------------------------------------------------\n"
               << "TRestGeant4Event: Missing Geant4 metadata for event ID: " << fEvent->GetID();
        G4Exception("OutputManager::UpdateEvent", "REST_G4_MISSING_METADATA", FatalException,
                    errMsg.str().c_str());
    }

    // The primary generator gives a vertex per particle: the particles of all the vertices
    int primaryParticles = 0;
    for (int v = 0; v < event->GetNumberOfPrimaryVertex(); ++v) {
        const auto* primaryVertex = event->GetPrimaryVertex(v);
        if (primaryVertex != nullptr) primaryParticles += primaryVertex->GetNumberOfParticle();
    }
    const int numberOfSources = metadata->GetNumberOfSources();

    // A Decay0 source and the cry generator give the particles of their event; a biasing stage after the
    // first one replays a stored particle
    const auto& generatorInfo = metadata->GetGeant4PrimaryGeneratorInfo();
    const bool variableParticles = generatorInfo.HasDecay0Sources() || generatorInfo.IsCRY() ||
                                   fSimulationManager->GetBiasingStage() > 0;
    if (!variableParticles && primaryParticles != numberOfSources &&
        int(fEvent->GetNumberOfPrimaries()) != numberOfSources) {
        std::ostringstream errMsg;
        errMsg << "\n---------------------------------------------------------\n"
               << "TRestGeant4Event: Number of particles on primary vertex does not match number of sources "
                  "for event ID: "
               << fEvent->GetID();
        G4Exception("OutputManager::UpdateEvent", "REST_G4_SOURCE_MISTMATCH", FatalException,
                    errMsg.str().c_str());
    }
}

bool OutputManager::IsEmptyEvent() const { return !fEvent || fEvent->GetNumberOfTracks() == 0; }

bool OutputManager::IsValidEvent() const {
    if (IsEmptyEvent()) {
        return false;
    }
    if (fSimulationManager->GetRestMetadata()->GetSaveAllEvents()) {
        return true;
    }
    const auto energy = fEvent->GetSensitiveVolumeEnergy();
    if (energy <= 0) {
        return false;
    }
    if (energy < fSimulationManager->GetRestMetadata()->GetMinimumEnergyStored() ||
        energy > fSimulationManager->GetRestMetadata()->GetMaximumEnergyStored()) {
        return false;
    }
    return true;
}

void OutputManager::FinishAndSubmitEvent() {
    // Only the events of the last biasing stage are written
    if (fSimulationManager->IsOutputStage() && IsValidEvent()) {
        if (fSimulationManager->GetRestMetadata()->GetRemoveUnwantedTracks()) RemoveUnwantedTracks();
        if (!fSimulationManager->GetRestMetadata()->RegisterEmptyTracks()) RemoveEmptyTracks();
        fEvent->RebuildSecondaries();

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - fEventTimeStart;

        fEvent->fEventData.eventTimeWall = elapsed.count() / 1000.;  // seconds
        fEvent->fEventData.eventTimeWallPrimaryGeneration = fEventTimeWallPrimaryGeneration;

        fSimulationManager->InsertEvent(fEvent);
        fSimulationManager->WriteEvents(false);
    }
    UpdateEvent();
}

void OutputManager::RecordTrack(const G4Track* track) {
    if (!IsValidTrack(track)) {
        return;
    }
    if (!fEvent->InsertTrack(track)) {
        return;
    }

    if (fEvent->fInfo.subEventID > 0) {
        const auto storedTrack = fEvent->GetTrack(fEvent->GetNumberOfTracks() - 1);
        assert(storedTrack.GetTrackID() == track->GetTrackID());
    }
}

void OutputManager::UpdateTrack(const G4Track* track) {
    if (!IsValidTrack(track)) {
        return;
    }
    fEvent->UpdateTrack(track);
}

void OutputManager::RecordStep(const G4Step* step) { fEvent->InsertStep(step); }

void OutputManager::AddSensitiveEnergy(double energy) { fEvent->AddEnergyToSensitiveVolume(energy); }

void OutputManager::AddEnergyToVolumeForParticleForProcess(double energy, int volumeID, int particleID,
                                                           int processID) {
    fEvent->AddEnergyInVolumeForParticleForProcess(energy, volumeID, particleID, processID);
}

void OutputManager::ApplyBiasingVolume(const G4Step* step, const TRestGeant4BiasingVolume& volume) {
    const auto* pre = step->GetPreStepPoint();
    const auto* post = step->GetPostStepPoint();
    const G4ThreeVector& from = pre->GetPosition();
    const G4ThreeVector& to = post->GetPosition();
    const double t = volume.GetEntryFraction({from.x() / mm, from.y() / mm, from.z() / mm},
                                             {to.x() / mm, to.y() / mm, to.z() / mm});
    if (t < 0) return;

    // A kill volume (TRestGeant4Event::InsertStep) has already stopped the track: nothing to bias
    const auto* metadata = fSimulationManager->GetRestMetadata();
    const auto* preVolume = pre->GetPhysicalVolume();
    if (preVolume != nullptr && metadata->IsKillVolume(preVolume->GetName())) return;

    auto* track = const_cast<G4Track*>(step->GetTrack());
    track->SetTrackStatus(fStopAndKill);

    // The secondaries created after the crossing point (the ones of the interaction at the post-step point)
    // are part of the history replayed by the next stage
    const G4ThreeVector segment = to - from;
    const double length2 = segment.mag2();
    if (const auto* secondaries = step->GetSecondaryInCurrentStep()) {
        for (const auto* secondary : *secondaries) {
            const double u = length2 > 0 ? (secondary->GetPosition() - from).dot(segment) / length2 : 1.0;
            if (u >= t) fBiasingKilledSecondaries.insert(secondary);
        }
    }

    // The state before the crossing point: exact for neutral particles, for charged ones the energy loss
    // and the deflection in the part of the step outside the volume are neglected
    const double energy = pre->GetKineticEnergy();
    if (energy / keV < volume.GetMinEnergy() || energy / keV > volume.GetMaxEnergy()) return;

    BiasingParticle particle;
    particle.definition = track->GetParticleDefinition();
    particle.charge = track->GetDynamicParticle()->GetCharge();
    particle.energy = energy;
    particle.time = pre->GetGlobalTime() + t * (post->GetGlobalTime() - pre->GetGlobalTime());
    particle.position = from + t * segment;
    particle.direction = pre->GetMomentumDirection();
    particle.polarization = pre->GetPolarization();
    particle.eventID = G4EventManager::GetEventManager()->GetConstCurrentEvent()->GetEventID();
    particle.sequence = fBiasingSequence++;
    fSimulationManager->StoreBiasingParticle(particle);
}

bool OutputManager::TakeBiasingKilledSecondary(const G4Track* track) {
    if (fBiasingKilledSecondaries.empty()) return false;
    return fBiasingKilledSecondaries.erase(track) > 0;
}
