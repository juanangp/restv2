
#ifndef REST_SIMULATIONMANAGER_H
#define REST_SIMULATIONMANAGER_H

#include <TH1D.h>
#include <TRandom.h>
#include <TRestGeant4Event.h>
#include <TRestGeant4Track.h>

#include <G4ThreeVector.hh>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <thread>

class G4ParticleDefinition;
class G4Step;
class G4Track;
class TRestGeant4BiasingVolume;
class TRestGeant4Metadata;
class TRestGeant4PhysicsLists;
class TRestRun;
class OutputManager;

/// \file SimulationManager.h
/// \brief Simulation state shared by the threads of restG4 (SimulationManager) and the event being
/// simulated in each worker thread (OutputManager).

/// \brief State of a particle stored when entering a biasing volume, replayed as the primary of the events
/// of the next biasing stage (Geant4 units)
struct BiasingParticle {
    const G4ParticleDefinition* definition = nullptr;  ///< Particle (shared by all the threads)
    double charge = 0;                                 ///< Charge of the dynamic particle (ions)
    double energy = 0;                                 ///< Kinetic energy
    double time = 0;                                   ///< Global time at the crossing point
    G4ThreeVector position;                            ///< Crossing point of the volume surface
    G4ThreeVector direction;                           ///< Momentum direction
    G4ThreeVector polarization;                        ///< Polarization
    int eventID = 0;   ///< Geant4 event ID where it was stored (to sort them, reproducible)
    int sequence = 0;  ///< Order of the particle in its event
};

/// \class SimulationManager
/// \brief State of a restG4 simulation shared by all the threads: the REST objects of the configuration,
/// the queue of events to write, the counters and the stop conditions.
///
/// Created once in the master thread. Each worker thread has its own OutputManager (thread_local), which
/// builds the TRestGeant4Event of the current event and submits it to the queue (InsertEvent); WriteEvents
/// writes the queued events in the TRestRun output tree, one thread at a time. The run stops when nEvents
/// are simulated, the requested entries are stored, the time limit is reached or on CTRL-C
/// (StopSimulation).
///
/// Volume biasing (`biasingVolumes` of the metadata): Application runs a Geant4 run (BeamOn) per stage.
/// During stage k (k < number of volumes) the worker threads store the particles entering volume k
/// (StoreBiasingParticle, thread safe); between the runs the master thread sorts them and makes them the
/// particles replayed by stage k + 1 (StartNextBiasingStage), read only during the run: event i replays
/// particle i / biasingFactor. Only the events of the last stage are written.
class SimulationManager {
   public:
    /// \brief Only in the master thread (a Geant4 fatal exception otherwise)
    SimulationManager();
    /// \brief Deletes the TRestRun, the TRestGeant4Metadata, the TRestGeant4PhysicsLists and the output
    /// managers
    ~SimulationManager();

    /// \brief Creates the OutputManager of the calling thread (called by ActionInitialization::Build)
    void InitializeOutputManager();
    /// \brief The OutputManager of the calling thread
    static OutputManager* GetOutputManager() { return fOutputManager; }

    TRestGeant4Event fEvent;  // Branch on EventTree

    /// \brief Queues an event to be written (thread safe); the pointer is moved
    void InsertEvent(std::unique_ptr<TRestGeant4Event>& event);

    // blocking=false returns immediately if another thread is already writing (it drains the queue)
    /// \brief Writes the queued events in the event tree of the TRestRun (the tracks are cleared if the
    /// metadata does not store them) and stops the simulation when the requested number of entries is
    /// reached
    /// \param blocking if false, returns immediately when another thread is already writing
    void WriteEvents(bool blocking = true);

    /// Thread that writes the progress every 2 s
    std::unique_ptr<std::thread> fPeriodicPrintThread = nullptr;
    std::atomic<bool> fPeriodicPrintThreadEndFlag{false};  ///< Ends the progress thread

   public:
    /// \brief The TRestRun of the configuration (output file)
    inline TRestRun* GetRestRun() const { return fRestRun; }
    /// \brief The TRestGeant4Metadata of the configuration
    inline TRestGeant4Metadata* GetRestMetadata() const { return fRestGeant4Metadata; }
    /// \brief The TRestGeant4PhysicsLists of the configuration
    inline TRestGeant4PhysicsLists* GetRestPhysicsLists() const { return fRestGeant4PhysicsLists; }

    /// \brief Sets the TRestRun (owned by the SimulationManager from now on)
    inline void SetRestRun(TRestRun* run) { fRestRun = run; }
    /// \brief Sets the TRestGeant4Metadata (owned by the SimulationManager from now on)
    inline void SetRestMetadata(TRestGeant4Metadata* metadata) { fRestGeant4Metadata = metadata; }
    /// \brief Sets the TRestGeant4PhysicsLists (owned by the SimulationManager from now on)
    inline void SetRestPhysicsLists(TRestGeant4PhysicsLists* physicsLists) {
        fRestGeant4PhysicsLists = physicsLists;
    }

    /// \brief Whether the progress thread must end
    bool GetPeriodicPrintThreadEndFlag() const { return fPeriodicPrintThreadEndFlag; }

    /// \brief Start of the run (master or serial thread only): CTRL-C handler, start time (at the first run
    /// only, the time limit is for the whole simulation), events of the run, processed events to 0 and
    /// progress thread
    void BeginOfRunAction();
    /// \brief End of the run (master or serial thread only): writes the events left, ends the progress
    /// thread and, in the first biasing stage (or without biasing), sets the number of events of the metadata
    /// to the events processed (and the CRY exposure for the cry generator). With biasing it also sets the
    /// events simulated and the particles stored in the stage in the metadata
    void EndOfRunAction();

    /// \brief Whether the simulation was asked to stop
    inline bool GetAbortFlag() const { return fAbortFlag; }
    /// \brief Aborts the run (G4RunManager::AbortRun) and sets the abort flag, so the other threads stop
    /// at their next event
    void StopSimulation();

    /// \brief Seconds since the start of the run (BeginOfRunAction)
    inline double GetElapsedTime() const {
        return 1E-9 * (std::chrono::steady_clock::now().time_since_epoch().count() - fTimeStartUnix);
    }

    /// \brief Records the generation of the primaries of an event now (any thread): the first one is the
    /// start of the run, the last one its end
    void SetLastEventGenerated() {
        const std::chrono::duration<double> now = std::chrono::system_clock::now().time_since_epoch();
        double none = 0;
        fFirstEventGeneratedTime.compare_exchange_strong(none, now.count());
        fLastEventGeneratedTime = now.count();
    }
    /// \brief Unix time (s) of the first generation of primaries (0 if none)
    inline double GetFirstEventGeneratedTime() const { return fFirstEventGeneratedTime; }
    /// \brief Unix time (s) of the last generation of primaries, stored or not (0 if none)
    inline double GetLastEventGeneratedTime() const { return fLastEventGeneratedTime; }

    /// \brief Adds the events processed by a thread since the last call to the number of processed events
    void SyncStatsFromChild(OutputManager* = fOutputManager);

    /// \brief Events processed (simulated) so far, as last synchronized from the threads
    int GetNumberOfProcessedEvents() const { return fNumberOfProcessedEvents; }
    /// \brief Events stored in the output file so far
    int GetNumberOfStoredEvents() const { return fNumberOfStoredEvents; }
    /// \brief Events to simulate in the current Geant4 run (stage), set at its start
    int64_t GetNumberOfEventsInRun() const { return fNumberOfEventsInRun; }

    /* Volume biasing */
    /// \brief Whether the metadata has biasing volumes
    bool IsBiasingActive() const;
    /// \brief Current biasing stage (0 for the generator, up to the number of biasing volumes)
    inline int GetBiasingStage() const { return fBiasingStage; }
    /// \brief Whether the events of the current stage are written: the last stage, or always without biasing
    bool IsOutputStage() const;
    /// \brief The biasing volume where the particles are stopped in the current stage (nullptr in the last
    /// stage or without biasing)
    const TRestGeant4BiasingVolume* GetStoppingBiasingVolume() const;
    /// \brief Stores a particle entering the stopping biasing volume (thread safe)
    void StoreBiasingParticle(const BiasingParticle& particle);
    /// \brief Particles stored so far in the current stage
    size_t GetNumberOfStoredBiasingParticles();
    /// \brief The particle replayed by the event (stage > 0): particle eventID / biasingFactor of the
    /// previous stage. A Geant4 fatal exception if out of range
    const BiasingParticle& GetBiasingReplayParticle(int eventID) const;
    /// \brief Master thread, between runs: the particles stored in the current stage, sorted by event ID and
    /// order, become the ones replayed by the next stage, which starts
    /// \return the events of the next stage (the particles times the biasing factor of the current volume)
    int64_t StartNextBiasingStage();
    /// \brief Counts a primary of the generator started inside the outermost biasing volume (it is never
    /// stopped by it, a warning)
    inline void CountBiasingPrimaryInside() { fBiasingPrimariesInside++; }
    /// \brief Primaries of the generator started inside the outermost biasing volume
    inline int64_t GetBiasingPrimariesInside() const { return fBiasingPrimariesInside; }

    /// \brief The generator has no more primaries (all the showers of the cry file): the run is stopped
    /// as a normal end of the primaries, not as an abort
    inline void SetGeneratorExhausted() { fGeneratorExhausted = true; }
    /// \brief Whether the generator has no more primaries
    inline bool IsGeneratorExhausted() const { return fGeneratorExhausted; }
    /// \brief Clears the abort flag (master thread, between runs: the end of the primaries of the first
    /// biasing stage is not an abort of the next stages)
    inline void ResetAbortFlag() { fAbortFlag = false; }

    /// \brief The output managers of all the threads
    std::vector<OutputManager*> GetOutputManagerContainer() const { return fOutputManagerContainer; }

   private:
    static thread_local OutputManager* fOutputManager;
    std::mutex fSimulationManagerMutex;
    std::mutex fWriteMutex;  // serializes fEvent and TTree::Fill, never held while pushing events
    std::queue<std::unique_ptr<TRestGeant4Event> > fEventContainer;

    TRestRun* fRestRun = nullptr;
    TRestGeant4PhysicsLists* fRestGeant4PhysicsLists = nullptr;
    TRestGeant4Metadata* fRestGeant4Metadata = nullptr;

    std::atomic<int> fNumberOfProcessedEvents{0};
    std::atomic<int> fNumberOfStoredEvents{0};
    std::atomic<int64_t> fNumberOfEventsInRun{0};

    std::atomic<bool> fAbortFlag{false};
    std::atomic<bool> fGeneratorExhausted{false};
    std::atomic<double> fFirstEventGeneratedTime{0};
    std::atomic<double> fLastEventGeneratedTime{0};

    std::atomic<int> fBiasingStage{0};
    /// Biasing factor of the previous stage: the replicas of each replayed particle
    int fBiasingReplicas = 1;
    std::mutex fBiasingMutex;
    /// Particles stored in the current stage (fBiasingMutex)
    std::vector<BiasingParticle> fBiasingParticles;
    /// Particles replayed in the current stage (read only during the run)
    std::vector<BiasingParticle> fBiasingReplayParticles;
    std::atomic<int64_t> fBiasingPrimariesInside{0};

    std::vector<OutputManager*> fOutputManagerContainer = {};
    long fTimeStartUnix = 0;

    /* Primary generation */
   public:
    /// \brief Loads the distributions of the generator (formulas, histograms, Decay0 files, CRY showers
    /// file), shared by the generators of all the threads. A Geant4 fatal exception if there are no
    /// sources nor cry generator, or if they cannot be loaded
    void InitializeUserDistributions();
};

/// \class OutputManager
/// \brief The event being simulated in a worker thread (one instance per thread).
///
/// It builds the TRestGeant4Event of the current Geant4 event (or sub-event) from the tracks, steps and
/// energies recorded by the user actions, and submits it to the SimulationManager when it is valid.
class OutputManager {
   public:
    /// \brief Only in a worker thread, or in the single thread in serial mode (a Geant4 fatal exception
    /// in the master thread of a multithreaded run)
    OutputManager(const SimulationManager*);
    /// \brief Starts a new (or reset) TRestGeant4Event for the current Geant4 event: ID, time, metadata
    /// and primaries. A Geant4 fatal exception if the number of primaries does not match the number of
    /// sources (except Decay0 sources and the cry generator)
    void UpdateEvent();
    /// \brief If the event is valid (and the biasing stage is the output one), removes the unwanted tracks
    /// (if configured) and, without registerEmptyTracks, the empty tracks, sets its wall times and submits it
    /// to be written; then starts a new event
    void FinishAndSubmitEvent();

    /// \brief Whether the event has no tracks
    bool IsEmptyEvent() const;

    /// \brief Whether the event is stored: not empty and, unless saveAllEvents, with an energy in the
    /// sensitive volumes above 0 and inside the energy range stored of the metadata
    bool IsValidEvent() const;
    bool IsValidTrack(const G4Track*) const;  // TODO
    bool IsValidStep(const G4Step*) const;    // TODO

    /// \brief Inserts a new track in the event
    void RecordTrack(const G4Track*);
    /// \brief Updates a track of the event at its end
    void UpdateTrack(const G4Track*);

    /// \brief Inserts a step (a hit) in the event
    void RecordStep(const G4Step*);

    /// \brief Adds energy (keV) to the energy of the sensitive volumes of the event
    void AddSensitiveEnergy(double energy);
    /// \brief Adds energy (keV) to the energy deposited in a volume by a particle and a process
    void AddEnergyToVolumeForParticleForProcess(double energy, int volumeID, int particleID, int processID);

    /// \brief The events processed since the last TakeEventCounter
    inline int GetEventCounter() const { return fProcessedEventsCounter; }
    /// \brief The events processed since the last call, reset atomically (the worker thread increments it)
    inline int TakeEventCounter() { return fProcessedEventsCounter.exchange(0); }

    /// \brief Start of a Geant4 event: a new event, the processed counter, the stop conditions (abort
    /// flag of another thread, time limit of the metadata) and the active volumes whose hits are stored in
    /// this event (each one with the probability `chance` of its activeVolumes entry)
    void BeginOfEventAction();

    /// \brief False if the hits of the active volume are not stored in the current Geant4 event (its
    /// `chance` draw failed at BeginOfEventAction; the same for all its sub-events)
    inline bool IsVolumeStoredInEvent(const std::string& volume) const {
        return fVolumesNotStored.count(volume) == 0;
    }
    /// \brief Updates the primaries of the event from the Geant4 event
    void UpdatePrimaryData(const G4Event* event);

    /// \brief ID of the event being simulated
    int GetCurrentEventID() const { return fEvent->GetID(); }

    /// \brief Volume biasing: if the step enters the stopping biasing volume of the stage, the track is
    /// killed, its secondaries created after the crossing point are killed when stacked and, unless the step
    /// is in a kill volume or the energy is out of the range of the volume, the particle is stored with the
    /// state of the pre-step point (energy, direction, polarization) at the crossing point of the chord of
    /// the step with the volume surface (time interpolated)
    void ApplyBiasingVolume(const G4Step* step, const TRestGeant4BiasingVolume& volume);
    /// \brief True (and forgets it) if the new track is a secondary killed by the biasing (StackingAction)
    bool TakeBiasingKilledSecondary(const G4Track* track);

    /// \brief Wall time (s) of the primary generation of the current event
    void SetEventTimeWallPrimaryGeneration(double durationSeconds) {
        fEventTimeWallPrimaryGeneration = durationSeconds;
    }

    /// \brief Start time of the current event, for its wall time
    void SetEventTimeStart(std::chrono::high_resolution_clock::time_point time) { fEventTimeStart = time; }

   private:
    std::unique_ptr<TRestGeant4Event> fEvent{};
    SimulationManager* fSimulationManager = nullptr;

    std::atomic<int> fProcessedEventsCounter{0};

    // start time
    double fEventTimeWallPrimaryGeneration = 0;
    std::chrono::high_resolution_clock::time_point fEventTimeStart;

    /// The active volumes whose hits are not stored in the current Geant4 event (`chance` < 1)
    std::set<std::string> fVolumesNotStored;

    /// Secondaries of the steps entering a biasing volume, killed when stacked (cleared at each event)
    std::set<const G4Track*> fBiasingKilledSecondaries;
    /// Particles stored by the biasing in the current Geant4 event
    int fBiasingSequence = 0;

    /// \brief removeUnwantedTracks: keeps only the tracks with hits (with energy, unless
    /// removeUnwantedTracksKeepZeroEnergyTracks) in the active or sensitive volumes, and their ancestors;
    /// the parent ID of a track whose parent is removed is set to 0
    void RemoveUnwantedTracks();
    /// \brief registerEmptyTracks false: removes the tracks without hits in the volumes stored, the ones
    /// with only their initial hit (always stored) outside them. The parent ID of the other tracks is kept
    void RemoveEmptyTracks();
    /// \brief Removes from the event the tracks (and their hits) not in `trackIDsToKeep`, and rebuilds the
    /// track ID index
    void RemoveTracks(const std::set<int>& trackIDsToKeep);

    friend class StackingAction;
};

#endif  // REST_SIMULATIONMANAGER_H
