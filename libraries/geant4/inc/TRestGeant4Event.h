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
class G4VPhysicalVolume;

// ============================================================================
// TRestGeant4EventData
//
// ============================================================================
struct TRestGeant4EventData {
    // Positions (mm) and directions as their X, Y and Z components (floats), names as the IDs of the
    // TRestGeant4Metadata tables: particles (PDG encoding, TRestGeant4PhysicsInfo), processes
    // (TRestGeant4PhysicsInfo) and volumes (TRestGeant4GeometryInfo)
    float primaryPositionX = 0, primaryPositionY = 0, primaryPositionZ = 0;
    std::vector<int> primaryParticleIDs;
    std::vector<double> primaryEnergies;
    std::vector<float> primaryDirectionX, primaryDirectionY, primaryDirectionZ;

    int subEventParticleID = 0;
    double subEventEnergy = 0.0;
    float subEventPositionX = 0, subEventPositionY = 0, subEventPositionZ = 0;
    float subEventDirectionX = 0, subEventDirectionY = 0, subEventDirectionZ = 0;

    double totalDepositedEnergy = 0.0;
    double sensitiveVolumeEnergy = 0.0;
    double eventTimeWall = 0.0;
    double eventTimeWallPrimaryGeneration = 0.0;

    /// Energy deposited per volume (only the volumes with energy)
    std::vector<int> volumeIDs;
    std::vector<double> volumeDepositedEnergy;

    std::vector<int> trackIDs;
    std::vector<int> parentIDs;
    std::vector<int> trackParticleIDs;
    std::vector<int> trackCreatorProcessIDs;
    std::vector<double> trackDepositedEnergy;
    std::vector<double> trackInitialEnergies;
    std::vector<int> trackStartIndices;  // offsets
    std::vector<int> trackNHits;

    std::vector<double> trackGlobalTimestamps;
    std::vector<double> trackTimeOffsets;
    std::vector<double> trackTimeLengths;
    std::vector<double> trackLengths;
    std::vector<double> trackWeights;
    std::vector<int> trackSecondariesIDs;
    std::vector<int> trackSecondariesOffsets;
    std::vector<int> trackSecondariesIndices;
    std::vector<float> trackInitialPositionX, trackInitialPositionY, trackInitialPositionZ;

    /// Energy deposited per volume, particle and process
    std::vector<int> crossVolumeIDs;
    std::vector<int> crossParticleIDs;
    std::vector<int> crossProcessIDs;
    std::vector<double> crossDepositedEnergies;

    TRestHitsData hitsStorage;

    std::vector<int> hitProcessID;
    std::vector<int> hitVolumeID;
    std::vector<float> hitKineticEnergy;
    std::vector<float> hitMomentumDirectionX;
    std::vector<float> hitMomentumDirectionY;
    std::vector<float> hitMomentumDirectionZ;

    /// Target nucleus of the hadronic processes (0 if none), with storeHadronicTargetInfo
    std::vector<int> hitHadronicTargetIsotopeA;
    std::vector<int> hitHadronicTargetIsotopeZ;

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

    TRestGeant4EventData fEventData;
    mutable const TRestGeant4Metadata* fMetadata = nullptr;

    void RebuildTrackIndex() const;
    void RebuildVolumeIndex();
    void RebuildCrossIndex();

    void ClearTracks();

    mutable std::map<int, int> fTrackIDToTrackIndex;

    /// \brief Builds the event from one Geant4 event payload.
    explicit TRestGeant4Event(const G4Event* event);
    /// \brief Inserts a G4Track
    bool InsertTrack(const G4Track* track);
    /// \brief Updates an existing track with the latest Geant4 state.
    void UpdateTrack(const G4Track* track);
    /// \brief  Appends one Geant4 step into the corresponding track hit sequence.
    void InsertStep(const G4Step* step);
    /// \brief Synchronizes primary vertex data after Geant4 primary generation.
    void UpdatePrimaryData(const G4Event* event);

    std::string GetClassName() const override { return "TRestGeant4Event"; }

    /// Version 1 (2026-11-02): X, Y, Z float branches and metadata IDs instead of XYZVector and names; the
    /// files without version have the older branches, not read
    int GetMinimumFormatVersion() const override { return 1; }

    void Initialize() override {
        TRestEvent::Initialize();
        fEventData.clear();
        fVolumeIndexMap.clear();
        fCrossIndexMap.clear();
        fTrackIDToTrackIndex.clear();
        fHasPendingInitialStep = false;
        fPendingInitialStepIndex = 0;
    }

    bool IsSubEvent() const { return GetSubID() > 0; }

    void ClearCache() {}

    void RemoveTrackHits(std::size_t trackIndex);

    void CreateBranches(TTree* tree) override;
    void SetBranchAddresses(TTree* tree) override;
    void RefreshViews() const override;

    void CopyFrom(const TRestEvent* other) override;
    void MoveFrom(TRestEvent* other) override;
    void MoveFrom(TRestGeant4Event&& source);

    void PopulateFromGeant4World(const G4VPhysicalVolume* world);

    const TRestGeant4Metadata* GetGeant4Metadata() const;
    inline void SetGeant4Metadata(const TRestGeant4Metadata* metadata) { fMetadata = metadata; }

    size_t GetNumberOfTracks() const { return fEventData.trackIDs.size(); }
    size_t GetNumberOfPrimaries() const { return fEventData.primaryParticleIDs.size(); }
    XYZVector GetPrimaryPosition() const {
        return {fEventData.primaryPositionX, fEventData.primaryPositionY, fEventData.primaryPositionZ};
    }
    XYZVector GetPrimaryDirection(size_t n) const {
        return {fEventData.primaryDirectionX.at(n), fEventData.primaryDirectionY.at(n),
                fEventData.primaryDirectionZ.at(n)};
    }
    std::string GetPrimaryParticleName(size_t n) const {
        return GetParticleName(fEventData.primaryParticleIDs.at(n));
    }
    std::string GetSubEventParticleName() const { return GetParticleName(fEventData.subEventParticleID); }
    XYZVector GetSubEventPosition() const {
        return {fEventData.subEventPositionX, fEventData.subEventPositionY, fEventData.subEventPositionZ};
    }
    XYZVector GetSubEventDirection() const {
        return {fEventData.subEventDirectionX, fEventData.subEventDirectionY, fEventData.subEventDirectionZ};
    }

    /// \brief Names of the IDs stored in the event, from the tables of the TRestGeant4Metadata ("" without
    /// metadata or if not found)
    std::string GetParticleName(int particleID) const;
    std::string GetProcessName(int processID) const;
    std::string GetVolumeName(int volumeID) const;

    TRestGeant4Track GetTrack(std::size_t n);
    TRestGeant4Track GetTrack(std::size_t n) const;

    std::vector<TRestGeant4Track> GetTracks();
    std::vector<TRestGeant4Track> GetTracks() const;

    TRestGeant4Track GetTrackByID(int id);
    TRestGeant4Track GetTrackByID(int id) const;

    std::map<int, int>& GetTrackIDToTrackIndex() { return fTrackIDToTrackIndex; }
    const std::map<int, int>& GetTrackIDToTrackIndex() const { return fTrackIDToTrackIndex; }

    TRestHits GetEventHits() const {
        return TRestHits(const_cast<TRestHitsData*>(&fEventData.hitsStorage), 0,
                         static_cast<int>(fEventData.hitsStorage.x.size()));
    }

    inline double GetTotalDepositedEnergy() const { return fEventData.totalDepositedEnergy; }
    inline double GetSensitiveVolumeEnergy() const { return fEventData.sensitiveVolumeEnergy; }
    inline void AddEnergyToSensitiveVolume(double en) { fEventData.sensitiveVolumeEnergy += en; }
    /// \brief Volumes with deposited energy
    inline int GetNumberOfActiveVolumes() const { return static_cast<int>(fEventData.volumeIDs.size()); }

    /// \brief Energy deposited in a volume, by its ID (TRestGeant4GeometryInfo) or name (needs the metadata)
    inline double GetEnergyInVolume(int volumeID) const {
        auto it = fVolumeIndexMap.find(volumeID);
        if (it != fVolumeIndexMap.end()) return fEventData.volumeDepositedEnergy[it->second];
        return 0.0;
    }
    double GetEnergyInVolume(const std::string& volumeName) const;

    /// \brief Adds energy to the total, to the volume and to the volume, particle and process
    void AddEnergyInVolumeForParticleForProcess(double energy, int volumeID, int particleID, int processID);

    void PrintG4Event(int maxTracks = -1, int maxHits = -1) const;
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

    TRestGeant4Event() = default;
    ~TRestGeant4Event() override = default;
};

#endif
