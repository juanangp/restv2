#ifndef TRESTGEANT4EVENT_H
#define TRESTGEANT4EVENT_H

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "TRestGeant4Metadata.h"
#include "TRestEvent.h"
#include "TRestGeant4Track.h"
#include "TRestRun.h"

class TTree;
class TRestGeant4Event;
class G4Event;
class G4Track;
class G4Step;
class G4VPhysicalVolume;

// ============================================================================
// TRestGeant4EventData
//
// ============================================================================
struct TRestGeant4EventData {
    ROOT::Math::XYZVector primaryPosition;
    std::vector<std::string> primaryParticleNames;
    std::vector<double> primaryEnergies;
    std::vector<ROOT::Math::XYZVector> primaryDirections;

    std::string subEventParticleName;
    double subEventEnergy = 0.0;
    ROOT::Math::XYZVector subEventPosition;
    ROOT::Math::XYZVector subEventDirection;

    double totalDepositedEnergy = 0.0;
    double sensitiveVolumeEnergy = 0.0;
    double eventTimeWall = 0.0;
    double eventTimeWallPrimaryGeneration = 0.0;

    int nVolumes = 0;
    std::vector<int> volumeStored;
    std::vector<std::string> volumeStoredNames;
    std::vector<double> volumeDepositedEnergy;

    std::vector<int> trackIDs;
    std::vector<int> parentIDs;
    std::vector<std::string> trackParticleNames;
    std::vector<std::string> trackCreatorProcesses;
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
    std::vector<ROOT::Math::XYZVector> trackInitialPositions;

    std::vector<std::string> crossVolumeNames;
    std::vector<std::string> crossParticleNames;
    std::vector<std::string> crossProcessNames;
    std::vector<double> crossDepositedEnergies;

    TRestHitsData hitsStorage;

    std::vector<int> hitProcessID;
    std::vector<int> hitVolumeID;
    std::vector<float> hitKineticEnergy;
    std::vector<ROOT::Math::XYZVector> hitMomentumDirection;

    std::vector<std::string> hitHadronicTargetIsotopeName;
    std::vector<int> hitHadronicTargetIsotopeA;
    std::vector<int> hitHadronicTargetIsotopeZ;

    void clear() {
        auto clear_all = [](auto&... vecs) { (vecs.clear(), ...); };

        clear_all(primaryParticleNames, primaryEnergies, primaryDirections, volumeStored,
                   volumeStoredNames, volumeDepositedEnergy, trackIDs, parentIDs, trackParticleNames,
                   trackCreatorProcesses, trackDepositedEnergy, trackInitialEnergies, trackStartIndices,
                   trackNHits, trackGlobalTimestamps, trackTimeOffsets, trackTimeLengths, trackLengths,
                   trackWeights, trackSecondariesIDs, trackSecondariesOffsets, trackSecondariesIndices,
                   trackInitialPositions, crossVolumeNames, crossParticleNames, crossProcessNames,
                   crossDepositedEnergies, hitProcessID, hitVolumeID, hitKineticEnergy,
                   hitMomentumDirection, hitHadronicTargetIsotopeName, hitHadronicTargetIsotopeA,
                   hitHadronicTargetIsotopeZ);

        hitsStorage.clear();
        subEventParticleName.clear();
        primaryPosition = {};
        subEventPosition = {};
        subEventDirection = {};
        subEventEnergy = totalDepositedEnergy = sensitiveVolumeEnergy = eventTimeWall =
            eventTimeWallPrimaryGeneration = 0.0;
        nVolumes = 0;
    }
};

// Branches are declared explicitly in CreateBranches/SetBranchAddresses.
// Keeping the list in the implementation avoids preprocessor metaprogramming
// and makes the ROOT I/O interface directly visible.

/// \class TRestGeant4Event
/// \brief Event container for Geant4 output with flat AOD views and track synchronization.
class TRestGeant4Event : public TRestEvent {
   private:
    std::unordered_map<std::string, size_t> fVolumeIndexMap;

    std::map<std::tuple<std::string, std::string, std::string>, size_t> fCrossIndexMap;

    bool fHasPendingInitialStep = false; //!
    std::size_t fPendingInitialStepIndex = 0; //!

    ROOT::Math::XYZVector* fPtr_primaryPosition = nullptr;
    std::vector<std::string>* fPtr_primaryParticleNames = nullptr;
    std::vector<double>* fPtr_primaryEnergies = nullptr;
    std::vector<ROOT::Math::XYZVector>* fPtr_primaryDirections = nullptr;

    ROOT::Math::XYZVector* fPtr_subEventPosition = nullptr;
    ROOT::Math::XYZVector* fPtr_subEventDirection = nullptr;
    std::string* fPtr_subEventParticleName = nullptr;

    std::vector<int>* fPtr_volumeStored = nullptr;
    std::vector<std::string>* fPtr_volumeStoredNames = nullptr;
    std::vector<double>* fPtr_volumeDepositedEnergy = nullptr;

    std::vector<int>* fPtr_trackIDs = nullptr;
    std::vector<int>* fPtr_trackParentIDs = nullptr;
    std::vector<std::string>* fPtr_trackParticleNames = nullptr;
    std::vector<std::string>* fPtr_trackCreatorProcesses = nullptr;
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
    std::vector<ROOT::Math::XYZVector>* fPtr_trackInitialPosition = nullptr;

    std::vector<float>* fPtr_hitX = nullptr;
    std::vector<float>* fPtr_hitY = nullptr;
    std::vector<float>* fPtr_hitZ = nullptr;
    std::vector<float>* fPtr_hitEnergy = nullptr;
    std::vector<float>* fPtr_hitTime = nullptr;
    std::vector<int>* fPtr_hitType = nullptr;
    std::vector<int>* fPtr_hitProcessID = nullptr;
    std::vector<int>* fPtr_hitVolumeID = nullptr;
    std::vector<float>* fPtr_hitKineticEnergy = nullptr;
    std::vector<ROOT::Math::XYZVector>* fPtr_hitMomentumDirection = nullptr;
    std::vector<std::string>* fPtr_hitHadronicTargetIsotopeName = nullptr;
    std::vector<int>* fPtr_hitHadronicTargetIsotopeA = nullptr;
    std::vector<int>* fPtr_hitHadronicTargetIsotopeZ = nullptr;

    std::vector<std::string>* fPtr_crossVolumeNames = nullptr;
    std::vector<std::string>* fPtr_crossParticleNames = nullptr;
    std::vector<std::string>* fPtr_crossProcessNames = nullptr;
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

    void Initialize() override {
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
    size_t GetNumberOfPrimaries() const { return fEventData.primaryParticleNames.size(); }

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
    inline int GetNumberOfActiveVolumes() const { return fEventData.nVolumes; }
    inline void SetEnergyDepositedInVolume(int volID, double eDep) {
        fEventData.volumeDepositedEnergy[volID] = eDep;
    }

    inline double GetEnergyInVolume(const std::string& volumeName) const {
        auto it = fVolumeIndexMap.find(volumeName);
        if (it != fVolumeIndexMap.end()) return fEventData.volumeDepositedEnergy[it->second];
        return 0.0;
    }

    void AddEnergyInVolumeForParticleForProcess(Double_t energy, const std::string& volumeName,
                                                 const std::string& particleName,
                                                 const std::string& processName);

    void PrintG4Event(int maxTracks = -1, int maxHits = -1) const;
    void PrintEvent() const override { PrintG4Event(); }
    TPad* DrawEvent(const TString& option = "") const override { return nullptr; }

    TRestGeant4Event() = default;
    ~TRestGeant4Event() override = default;
};

#endif
