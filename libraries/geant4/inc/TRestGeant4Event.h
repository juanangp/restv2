#ifndef TRESTGEANT4EVENT_H
#define TRESTGEANT4EVENT_H

#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "TRestEvent.h"
#include "TRestHits.h"

class TRestGeant4Metadata;

class TTree;
class TRestGeant4Event;
class TRestGeant4Track;
class G4Event;
class G4Track;
class G4Step;

// ============================================================================
// TRestGeant4EventData
//
// ============================================================================
/// \struct TRestGeant4EventData
/// \brief Flat (structure of arrays) storage of a TRestGeant4Event, one TTree branch per member.
///
/// Positions (mm) and directions as their X, Y and Z components (floats), names as the IDs of the
/// TRestGeant4Metadata tables: particles (PDG encoding, TRestGeant4PhysicsInfo), processes
/// (TRestGeant4PhysicsInfo) and volumes (TRestGeant4GeometryInfo). Energies in keV, times in us.
/// The track vectors have one entry per track; the hit vectors (and hitsStorage) one per hit, the hits
/// of the track n being [trackStartIndices[n], trackStartIndices[n] + trackNHits[n]).
struct TRestGeant4EventData {
    // Positions (mm) and directions as their X, Y and Z components (floats), names as the IDs of the
    // TRestGeant4Metadata tables: particles (PDG encoding, TRestGeant4PhysicsInfo), processes
    // (TRestGeant4PhysicsInfo) and volumes (TRestGeant4GeometryInfo)
    /// Position of the first primary vertex (mm)
    float primaryPositionX = 0, primaryPositionY = 0, primaryPositionZ = 0;
    std::vector<int> primaryParticleIDs;  ///< Particle ID (PDG encoding) of each primary
    std::vector<double> primaryEnergies;  ///< Kinetic energy of each primary (keV)
    /// Unit direction of each primary
    std::vector<float> primaryDirectionX, primaryDirectionY, primaryDirectionZ;

    int subEventParticleID = 0;   ///< Particle starting the sub-event (SubID > 0)
    double subEventEnergy = 0.0;  ///< Kinetic energy (keV) of the particle starting the sub-event
    /// Position (mm) of the particle starting the sub-event
    float subEventPositionX = 0, subEventPositionY = 0, subEventPositionZ = 0;
    /// Direction of the particle starting the sub-event
    float subEventDirectionX = 0, subEventDirectionY = 0, subEventDirectionZ = 0;

    double totalDepositedEnergy = 0.0;            ///< Energy deposited in all the volumes (keV)
    double sensitiveVolumeEnergy = 0.0;           ///< Energy deposited in the sensitive volumes (keV)
    double eventTimeWall = 0.0;                   ///< Wall time of the simulation of the event (s)
    double eventTimeWallPrimaryGeneration = 0.0;  ///< Wall time of the primary generation (s)

    /// Energy deposited per volume (only the volumes with energy)
    std::vector<int> volumeIDs;
    std::vector<double> volumeDepositedEnergy;  ///< Energy (keV) of each entry of volumeIDs

    std::vector<int> trackIDs;                 ///< Geant4 track ID of each track
    std::vector<int> parentIDs;                ///< Track ID of the parent (0 for a primary)
    std::vector<int> trackParticleIDs;         ///< Particle ID (PDG encoding) of each track
    std::vector<int> trackCreatorProcessIDs;   ///< Process ID of the creator process of each track
    std::vector<double> trackDepositedEnergy;  ///< Energy deposited by each track (keV)
    std::vector<double> trackInitialEnergies;  ///< Initial kinetic energy of each track (keV)
    std::vector<int> trackStartIndices;        ///< Index of the first hit of each track (offsets)
    std::vector<int> trackNHits;               ///< Number of hits of each track

    std::vector<double> trackGlobalTimestamps;  ///< Global time at the creation of each track (us)
    /// Global time offset of each track after a reset of the global time (resetGlobalTime, us)
    std::vector<double> trackTimeOffsets;
    std::vector<double> trackTimeLengths;      ///< Local (proper) time of each track (us)
    std::vector<double> trackLengths;          ///< Length of each track (mm)
    std::vector<double> trackWeights;          ///< Geant4 weight of each track
    std::vector<int> trackSecondariesIDs;      ///< Track IDs of the children, see RebuildSecondaries
    std::vector<int> trackSecondariesOffsets;  ///< Number of children of each track
    std::vector<int> trackSecondariesIndices;  ///< Index in trackSecondariesIDs of the first child
    /// Position where each track was created (mm)
    std::vector<float> trackInitialPositionX, trackInitialPositionY, trackInitialPositionZ;

    /// Energy deposited per volume, particle and process
    std::vector<int> crossVolumeIDs;
    std::vector<int> crossParticleIDs;           ///< Particle ID of each cross entry
    std::vector<int> crossProcessIDs;            ///< Process ID of each cross entry
    std::vector<double> crossDepositedEnergies;  ///< Energy (keV) of each cross entry

    TRestHitsData hitsStorage;  ///< Position, time, energy and type of the hits of all the tracks

    std::vector<int> hitProcessID;             ///< Process ID of each hit (0: initial step "Init")
    std::vector<int> hitVolumeID;              ///< Volume ID of each hit
    std::vector<float> hitKineticEnergy;       ///< Kinetic energy of the particle at each hit (keV)
    std::vector<float> hitMomentumDirectionX;  ///< Momentum direction X at each hit
    std::vector<float> hitMomentumDirectionY;  ///< Momentum direction Y at each hit
    std::vector<float> hitMomentumDirectionZ;  ///< Momentum direction Z at each hit

    /// Target nucleus of the hadronic processes (0 if none), with storeHadronicTargetInfo
    std::vector<int> hitHadronicTargetIsotopeA;
    std::vector<int> hitHadronicTargetIsotopeZ;  ///< Z of the target nucleus (0 if none)

    /// \brief Clears all the vectors and resets the scalars to 0.
    void clear() {
        auto clear_all = [](auto&... vecs) { (vecs.clear(), ...); };

        clear_all(primaryParticleIDs, primaryEnergies, primaryDirectionX, primaryDirectionY,
                  primaryDirectionZ, volumeIDs, volumeDepositedEnergy, trackIDs, parentIDs, trackParticleIDs,
                  trackCreatorProcessIDs, trackDepositedEnergy, trackInitialEnergies, trackStartIndices,
                  trackNHits, trackGlobalTimestamps, trackTimeOffsets, trackTimeLengths, trackLengths,
                  trackWeights, trackSecondariesIDs, trackSecondariesOffsets, trackSecondariesIndices,
                  trackInitialPositionX, trackInitialPositionY, trackInitialPositionZ, crossVolumeIDs,
                  crossParticleIDs, crossProcessIDs, crossDepositedEnergies, hitProcessID, hitVolumeID,
                  hitKineticEnergy, hitMomentumDirectionX, hitMomentumDirectionY, hitMomentumDirectionZ,
                  hitHadronicTargetIsotopeA, hitHadronicTargetIsotopeZ);

        hitsStorage.clear();
        primaryPositionX = primaryPositionY = primaryPositionZ = 0;
        subEventParticleID = 0;
        subEventPositionX = subEventPositionY = subEventPositionZ = 0;
        subEventDirectionX = subEventDirectionY = subEventDirectionZ = 0;
        subEventEnergy = totalDepositedEnergy = sensitiveVolumeEnergy = eventTimeWall =
            eventTimeWallPrimaryGeneration = 0.0;
    }
};

// Branches are declared explicitly in CreateBranches/SetBranchAddresses.
// Keeping the list in the implementation avoids preprocessor metaprogramming
// and makes the ROOT I/O interface directly visible.

/// \class TRestGeant4Event
/// \brief Event container for Geant4 output with flat AOD views and track synchronization.
///
/// The event of restG4: the primaries, the energy deposited per volume (and per volume, particle and
/// process) and, with storeTracks, the tracks and their hits (TRestGeant4Track is a light view of a
/// track of the event). All the data is in fEventData (TRestGeant4EventData); the names of the
/// particle, process and volume IDs are in the TRestGeant4Metadata of the run (GetGeant4Metadata). An
/// event with SubID > 0 is a sub-event (e.g. a delayed radioactive decay, subEventTimeDelay).
///
/// The constructor from a G4Event and InsertTrack, UpdateTrack, InsertStep and UpdatePrimaryData are
/// implemented in restG4 (they need Geant4).
class TRestGeant4Event : public TRestEvent {
   private:
    /// Index in volumeIDs of each volume, and in cross* of each volume, particle and process
    std::unordered_map<int, size_t> fVolumeIndexMap;
    std::map<std::tuple<int, int, int>, size_t> fCrossIndexMap;

    bool fHasPendingInitialStep = false;       //!
    std::size_t fPendingInitialStepIndex = 0;  //!

    std::vector<int>* fPtr_primaryParticleIDs = nullptr;
    std::vector<double>* fPtr_primaryEnergies = nullptr;
    std::vector<float>* fPtr_primaryDirectionX = nullptr;
    std::vector<float>* fPtr_primaryDirectionY = nullptr;
    std::vector<float>* fPtr_primaryDirectionZ = nullptr;
    std::vector<int>* fPtr_volumeIDs = nullptr;
    std::vector<double>* fPtr_volumeDepositedEnergy = nullptr;
    std::vector<int>* fPtr_trackIDs = nullptr;
    std::vector<int>* fPtr_trackParentIDs = nullptr;
    std::vector<int>* fPtr_trackParticleIDs = nullptr;
    std::vector<int>* fPtr_trackCreatorProcessIDs = nullptr;
    std::vector<double>* fPtr_trackDepositedEnergy = nullptr;
    std::vector<double>* fPtr_trackInitialEnergies = nullptr;
    std::vector<int>* fPtr_trackStartIndices = nullptr;
    std::vector<int>* fPtr_trackNHits = nullptr;
    std::vector<double>* fPtr_trackGlobalTimestamp = nullptr;
    std::vector<double>* fPtr_trackTimeOffset = nullptr;
    std::vector<double>* fPtr_trackTimeLength = nullptr;
    std::vector<double>* fPtr_trackLength = nullptr;
    std::vector<double>* fPtr_trackWeight = nullptr;
    std::vector<int>* fPtr_trackSecondariesIDs = nullptr;
    std::vector<int>* fPtr_trackSecondariesIndex = nullptr;
    std::vector<int>* fPtr_trackSecondariesOffsets = nullptr;
    std::vector<float>* fPtr_trackInitialPositionX = nullptr;
    std::vector<float>* fPtr_trackInitialPositionY = nullptr;
    std::vector<float>* fPtr_trackInitialPositionZ = nullptr;
    std::vector<float>* fPtr_hitX = nullptr;
    std::vector<float>* fPtr_hitY = nullptr;
    std::vector<float>* fPtr_hitZ = nullptr;
    std::vector<float>* fPtr_hitEnergy = nullptr;
    std::vector<float>* fPtr_hitTime = nullptr;
    std::vector<int>* fPtr_hitType = nullptr;
    std::vector<int>* fPtr_hitProcessID = nullptr;
    std::vector<int>* fPtr_hitVolumeID = nullptr;
    std::vector<float>* fPtr_hitKineticEnergy = nullptr;
    std::vector<float>* fPtr_hitMomentumDirectionX = nullptr;
    std::vector<float>* fPtr_hitMomentumDirectionY = nullptr;
    std::vector<float>* fPtr_hitMomentumDirectionZ = nullptr;
    std::vector<int>* fPtr_hitHadronicTargetIsotopeA = nullptr;
    std::vector<int>* fPtr_hitHadronicTargetIsotopeZ = nullptr;
    std::vector<int>* fPtr_crossVolumeIDs = nullptr;
    std::vector<int>* fPtr_crossParticleIDs = nullptr;
    std::vector<int>* fPtr_crossProcessIDs = nullptr;
    std::vector<double>* fPtr_crossDepositedEnergies = nullptr;

   public:
    using XYZVector = ROOT::Math::XYZVector;

    TRestGeant4EventData fEventData;  ///< The data of the event (the TTree branches)
    /// The metadata with the names of the IDs (set or found in the run, see GetGeant4Metadata)
    mutable const TRestGeant4Metadata* fMetadata = nullptr;

    /// \brief Rebuilds fTrackIDToTrackIndex from trackIDs.
    void RebuildTrackIndex() const;
    /// \brief Rebuilds the index of the volumes with energy (used by GetEnergyInVolume).
    void RebuildVolumeIndex();
    /// \brief Rebuilds the index of the volume, particle and process entries (cross*).
    void RebuildCrossIndex();

    /// \brief Clears the tracks and their hits only (the energies, volumes and primaries are kept): the
    /// event without tracks (storeTracks false). Initialize resets the whole event
    void ClearTracks();

    /// \brief Fills the secondaries of each track (trackSecondariesIDs, Index and Offsets) from the parent
    /// IDs: the IDs of the children of the track n, in storage order, are trackSecondariesIDs[i] for i in
    /// [trackSecondariesIndices[n], trackSecondariesIndices[n] + trackSecondariesOffsets[n]). The children
    /// of a track are not consecutive while simulating, restG4 calls it when the event is submitted
    void RebuildSecondaries();

    /// Index in the track vectors of each track ID (rebuilt when its size differs from the tracks)
    mutable std::map<int, int> fTrackIDToTrackIndex;

    /// \brief Builds the event from one Geant4 event payload (restG4): ID, time and primaries.
    explicit TRestGeant4Event(const G4Event* event);
    /// \brief Inserts a G4Track (restG4), whose initial step must have been inserted with InsertStep
    bool InsertTrack(const G4Track* track);
    /// \brief Updates an existing track with the latest Geant4 state.
    void UpdateTrack(const G4Track* track);
    /// \brief  Appends one Geant4 step into the corresponding track hit sequence.
    void InsertStep(const G4Step* step);
    /// \brief Synchronizes primary vertex data after Geant4 primary generation.
    void UpdatePrimaryData(const G4Event* event);

    /// \brief Returns "TRestGeant4Event".
    std::string GetClassName() const override { return "TRestGeant4Event"; }

    /// Version 1 (2026-11-02): X, Y, Z float branches and metadata IDs instead of XYZVector and names; the
    /// files without version have the older branches, not read
    int GetMinimumFormatVersion() const override { return 1; }

    /// \brief Resets the whole event (data and indices).
    void Initialize() override {
        TRestEvent::Initialize();
        fEventData.clear();
        fVolumeIndexMap.clear();
        fCrossIndexMap.clear();
        fTrackIDToTrackIndex.clear();
        fHasPendingInitialStep = false;
        fPendingInitialStepIndex = 0;
    }

    /// \brief True for a sub-event (SubID > 0)
    bool IsSubEvent() const { return GetSubID() > 0; }

    /// \brief No-op, kept for compatibility.
    void ClearCache() {}

    /// \brief Removes the hits of the track at `trackIndex` (its number of hits becomes 0 and the start
    /// indices of the next tracks are shifted). Out of range: nothing is done.
    void RemoveTrackHits(std::size_t trackIndex);

    /// \brief Creates the branches of fEventData in the tree.
    void CreateBranches(TTree* tree) override;
    /// \brief Sets the addresses of the branches of fEventData to read the tree.
    void SetBranchAddresses(TTree* tree) override;
    /// \brief Rebuilds the track, volume and cross indices after reading an entry.
    void RefreshViews() const override;

    /// \brief Copies another TRestGeant4Event (data, metadata and run), rebuilding the indices.
    void CopyFrom(const TRestEvent* other) override;
    /// \brief Moves the data of another TRestGeant4Event, which is left empty.
    void MoveFrom(TRestEvent* other) override;
    /// \brief Moves the data of `source`, which is left empty.
    void MoveFrom(TRestGeant4Event&& source);

    /// \brief The metadata set with SetGeant4Metadata or, if none, the TRestGeant4Metadata of the run of
    /// the event (nullptr if not found).
    const TRestGeant4Metadata* GetGeant4Metadata() const;
    /// \brief Sets the metadata used to name the IDs.
    inline void SetGeant4Metadata(const TRestGeant4Metadata* metadata) { fMetadata = metadata; }

    /// \brief Number of stored tracks
    size_t GetNumberOfTracks() const { return fEventData.trackIDs.size(); }
    /// \brief Number of primary particles
    size_t GetNumberOfPrimaries() const { return fEventData.primaryParticleIDs.size(); }
    /// \brief Position of the first primary vertex (mm)
    XYZVector GetPrimaryPosition() const {
        return {fEventData.primaryPositionX, fEventData.primaryPositionY, fEventData.primaryPositionZ};
    }
    /// \brief Direction of the primary n
    XYZVector GetPrimaryDirection(size_t n) const {
        return {fEventData.primaryDirectionX.at(n), fEventData.primaryDirectionY.at(n),
                fEventData.primaryDirectionZ.at(n)};
    }
    /// \brief Kinetic energy (keV) of the primary n
    double GetPrimaryEnergy(size_t n) const { return fEventData.primaryEnergies.at(n); }
    /// \brief Particle name of the primary n ("" without metadata)
    std::string GetPrimaryParticleName(size_t n) const {
        return GetParticleName(fEventData.primaryParticleIDs.at(n));
    }
    /// \brief Name of the particle starting the sub-event
    std::string GetSubEventParticleName() const { return GetParticleName(fEventData.subEventParticleID); }
    /// \brief Position (mm) of the particle starting the sub-event
    XYZVector GetSubEventPosition() const {
        return {fEventData.subEventPositionX, fEventData.subEventPositionY, fEventData.subEventPositionZ};
    }
    /// \brief Direction of the particle starting the sub-event
    XYZVector GetSubEventDirection() const {
        return {fEventData.subEventDirectionX, fEventData.subEventDirectionY, fEventData.subEventDirectionZ};
    }

    /// \brief Names of the IDs stored in the event, from the tables of the TRestGeant4Metadata ("" without
    /// metadata or if not found)
    std::string GetParticleName(int particleID) const;
    /// \brief Name of a process ID ("" without metadata or if not found)
    std::string GetProcessName(int processID) const;
    /// \brief Name of a volume ID ("" without metadata or if not found)
    std::string GetVolumeName(int volumeID) const;

    /// \brief View of the track at index n (throws std::out_of_range)
    TRestGeant4Track GetTrack(std::size_t n);
    /// \brief View of the track at index n (throws std::out_of_range)
    TRestGeant4Track GetTrack(std::size_t n) const;

    /// \brief Views of all the tracks, in storage order
    std::vector<TRestGeant4Track> GetTracks();
    /// \brief Views of all the tracks, in storage order
    std::vector<TRestGeant4Track> GetTracks() const;

    /// \brief View of the track with Geant4 track ID `id` (throws std::runtime_error if not found)
    TRestGeant4Track GetTrackByID(int id);
    /// \brief View of the track with Geant4 track ID `id` (throws std::runtime_error if not found)
    TRestGeant4Track GetTrackByID(int id) const;

    /// \brief Index in the track vectors of each track ID
    std::map<int, int>& GetTrackIDToTrackIndex() { return fTrackIDToTrackIndex; }
    /// \brief Index in the track vectors of each track ID
    const std::map<int, int>& GetTrackIDToTrackIndex() const { return fTrackIDToTrackIndex; }

    /// \brief All the hits of the event (of all the tracks), a view of hitsStorage
    TRestHits GetEventHits() const {
        return TRestHits(const_cast<TRestHitsData*>(&fEventData.hitsStorage), 0,
                         static_cast<int>(fEventData.hitsStorage.x.size()));
    }

    /// \brief Energy deposited in all the volumes (keV)
    inline double GetTotalDepositedEnergy() const { return fEventData.totalDepositedEnergy; }
    /// \brief Energy deposited in the sensitive volumes (keV)
    inline double GetSensitiveVolumeEnergy() const { return fEventData.sensitiveVolumeEnergy; }
    /// \brief Adds energy (keV) to the sensitive volume energy
    inline void AddEnergyToSensitiveVolume(double en) { fEventData.sensitiveVolumeEnergy += en; }
    /// \brief Volumes with deposited energy
    inline int GetNumberOfActiveVolumes() const { return static_cast<int>(fEventData.volumeIDs.size()); }

    /// \brief Energy deposited in a volume, by its ID (TRestGeant4GeometryInfo) or name (needs the metadata)
    /// \return The energy (keV), 0 if the volume has no energy (or, by name, without metadata).
    inline double GetEnergyInVolume(int volumeID) const {
        auto it = fVolumeIndexMap.find(volumeID);
        if (it != fVolumeIndexMap.end()) return fEventData.volumeDepositedEnergy[it->second];
        return 0.0;
    }
    /// \brief Energy deposited in a volume by its physical name (0 without metadata)
    double GetEnergyInVolume(const std::string& volumeName) const;

    /// \brief Adds energy to the total, to the volume and to the volume, particle and process
    /// (nothing for energy <= 0)
    void AddEnergyInVolumeForParticleForProcess(double energy, int volumeID, int particleID, int processID);

    /// \brief Prints the event summary, the primaries and the tracks with their hits
    /// \param maxTracks Maximum number of tracks printed (-1: all).
    /// \param maxHits Maximum number of hits printed per track (-1: all).
    void PrintG4Event(int maxTracks = -1, int maxHits = -1) const;
    /// \brief PrintG4Event with all the tracks and hits
    void PrintEvent() const override { PrintG4Event(); }
    /// \brief Draws the event, one row of pads per option. Options separated by ':':
    /// - graph[process1,process2,...]: the hits (also those without energy, the trajectories) in the XZ,
    ///   YZ and XY projections, coloured by particle (TRestGeant4Track::GetParticleColor), the marker size
    ///   growing with the energy; the hits of the given Geant4 processes (e.g. phot, compt, eBrem) are
    ///   marked with open black markers, with a legend of particles and processes;
    /// - hist(rootOption)[binSize]: histograms weighted by energy of the hits with energy
    ///   (TRestHits::DrawHistograms): projections, 3D view and X, Y and Z distributions; binSize in mm,
    ///   3 by default (as legacy);
    /// - print: PrintEvent.
    /// Default: "graph:hist(Cont0,colz)", as legacy graphXZ:graphYZ:histXZ(Cont0,colz):histYZ(Cont0,colz).
    TPad* DrawEvent(const std::string& option = "") const override;
    /// \brief Draws the graph option of DrawEvent in the pad, divided in the XZ, YZ and XY projections.
    void DrawGraphs(TVirtualPad* pad, const std::vector<std::string>& processes = {}) const;

    /// \brief Empty event.
    TRestGeant4Event() = default;
    ~TRestGeant4Event() override = default;
};

#endif
