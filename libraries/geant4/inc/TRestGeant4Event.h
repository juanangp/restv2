#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "TRestEvent.h"
#include "TRestRun.h"
#include "TRestGeant4Track.h"
#include "TRestGeant4Hits.h"

class TTree;
class TRestGeant4Event;
class TRestGeant4Metadata;
class G4Event;
class G4Track;
class G4Step;

// ============================================================================
//  TRestGeant4EventData (Unified flat AOD with TRestHitsData)
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
    std::vector<int> trackStartIndices;
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
        
        clear_all(primaryParticleNames, primaryEnergies, primaryDirections, volumeStored, volumeStoredNames,
                  volumeDepositedEnergy, trackIDs, parentIDs, trackParticleNames, trackCreatorProcesses,
                  trackDepositedEnergy, trackInitialEnergies, trackStartIndices, trackNHits, 
                  trackGlobalTimestamps, trackTimeOffsets, trackTimeLengths, trackLengths, trackWeights,
                  trackSecondariesIDs, trackSecondariesOffsets, trackSecondariesIndices, trackInitialPositions,
                  crossVolumeNames, crossParticleNames, crossProcessNames, crossDepositedEnergies,
                  hitProcessID, hitVolumeID, hitKineticEnergy, hitMomentumDirection,
                  hitHadronicTargetIsotopeName, hitHadronicTargetIsotopeA, hitHadronicTargetIsotopeZ);

        hitsStorage.clear();
        subEventParticleName.clear();
        primaryPosition = {}; subEventPosition = {}; subEventDirection = {};
        subEventEnergy = totalDepositedEnergy = sensitiveVolumeEnergy = eventTimeWall = eventTimeWallPrimaryGeneration = 0.0;
        nVolumes = 0;
    }
};

// ============================================================================
//  TRestGeant4TrackView
// ============================================================================
class TRestGeant4TrackView : public TRestGeant4Hits {
    public:
     TRestGeant4EventData* fEventData = nullptr;
     int fTrackIdx = -1;
 
     TRestGeant4TrackView(TRestGeant4EventData* data, TRestGeant4Event* event, int idx)
        : TRestGeant4Hits(&(data->hitsStorage), 
                           &(data->hitProcessID), 
                           &(data->hitVolumeID), 
                           &(data->hitKineticEnergy), 
                           &(data->hitMomentumDirection),
                           &(data->hitHadronicTargetIsotopeName),
                           &(data->hitHadronicTargetIsotopeA),
                           &(data->hitHadronicTargetIsotopeZ),
                           data->trackStartIndices[idx], 
                           data->trackNHits[idx]),
          fEventData(data),
          fTrackIdx(idx) {
        SetEvent(event);
    }

    int GetTrackID() const { return fEventData->trackIDs[fTrackIdx]; }
    int GetParentID() const { return fEventData->parentIDs[fTrackIdx]; }
    
    double GetLength() const { return fEventData->trackLengths[fTrackIdx]; }
    double GetTimeLength() const { return fEventData->trackTimeLengths[fTrackIdx]; }
    double GetWeight() const { return fEventData->trackWeights[fTrackIdx]; }
    
    ROOT::Math::XYZVector GetInitialPosition() const { 
        return fEventData->trackInitialPositions[fTrackIdx]; 
    }

    std::string GetInitialVolume() const { return GetVolumeName(0); }

    std::string GetParticleName() const { return fEventData->trackParticleNames[fTrackIdx]; }
    std::string GetCreatorProcess() const { return fEventData->trackCreatorProcesses[fTrackIdx]; }
    double GetInitialEnergy() const { return fEventData->trackInitialEnergies[fTrackIdx]; }

    std::vector<int> GetSecondaryTrackIDs() const {
    std::vector<int> secondaries;
        
    int start = fEventData->trackSecondariesIndices[fTrackIdx];
    int offset = fEventData->trackSecondariesOffsets[fTrackIdx];
        
      secondaries.reserve(offset);
      for (int i = 0; i < offset; ++i) {
          secondaries.push_back(fEventData->trackSecondariesIDs[start + i]);
      }
        
     return secondaries;
   }
   
};

/// \class TRestGeant4Event
/// \brief Event container for Geant4 output with flat AOD views and track synchronization.
class TRestGeant4Event : public TRestEvent {
   private:
    mutable std::unordered_map<std::string, double> fVolumeEnergyCache;
    std::unordered_map<std::string, size_t> fCrossIndexMap;
    std::unordered_map<std::string, size_t> fTradVolumeIndexMap;

    protected:
    std::string* fPtrSubEventParticleName = nullptr;
    std::vector<std::string>* fPtrPrimaryParticleNames = nullptr;
    std::vector<double>* fPtrPrimaryEnergies = nullptr;
    std::vector<ROOT::Math::XYZVector>* fPtrPrimaryDirections = nullptr;
    ROOT::Math::XYZVector* fPtrPrimaryPosition = nullptr;  //!
    ROOT::Math::XYZVector* fPtrSubEventPosition = nullptr; //!
    ROOT::Math::XYZVector* fPtrSubEventDirection = nullptr; //!
    std::vector<int>* fPtrVolumeStored = nullptr;
    std::vector<std::string>* fPtrVolumeStoredNames = nullptr;
    std::vector<double>* fPtrVolumeDepositedEnergy = nullptr;
    std::vector<int>* fPtrTrackIDs = nullptr;
    std::vector<int>* fPtrParentIDs = nullptr;
    std::vector<std::string>* fPtrTrackParticleNames = nullptr;
    std::vector<std::string>* fPtrTrackCreatorProcesses = nullptr;
    std::vector<double>* fPtrTrackDepositedEnergy = nullptr;
    std::vector<double>* fPtrTrackInitialEnergies = nullptr;
    std::vector<int>* fPtrTrackStartIndices = nullptr;
    std::vector<int>* fPtrTrackNHits = nullptr;
    std::vector<double>* fPtrTrackGlobalTimestamp = nullptr;
    std::vector<double>* fPtrTrackTimeOffset = nullptr;
    std::vector<double>* fPtrTrackTimeLength = nullptr;
    std::vector<double>* fPtrTrackLength = nullptr;
    std::vector<double>* fPtrTrackWeight = nullptr;
    std::vector<int>* fPtrTrackSecondariesIDs = nullptr;
    std::vector<int>* fPtrTrackSecondariesIndex = nullptr;
    std::vector<int>* fPtrTrackSecondariesOffsets = nullptr;
    std::vector<ROOT::Math::XYZVector>* fPtrTrackInitialPosition = nullptr;
    
    std::vector<float>* fPtrHitX = nullptr;
    std::vector<float>* fPtrHitY = nullptr;
    std::vector<float>* fPtrHitZ = nullptr;
    std::vector<float>* fPtrHitEnergy = nullptr;
    std::vector<float>* fPtrHitTime = nullptr;
    std::vector<int>* fPtrHitType = nullptr;
    std::vector<int>* fPtrHitVolumeID = nullptr;
    std::vector<int>* fPtrHitProcessID = nullptr;

    std::vector<std::string>* fPtrCrossVolumeNames = nullptr;
    std::vector<std::string>* fPtrCrossParticleNames = nullptr;
    std::vector<std::string>* fPtrCrossProcessNames = nullptr;
    std::vector<double>* fPtrCrossDepositedEnergies = nullptr;
    std::vector<float>* fPtrHitKineticEnergy = nullptr;
    std::vector<ROOT::Math::XYZVector>* fPtrHitMomentumDirection = nullptr;
    std::vector<std::string>* fPtrHitHadronicTargetIsotopeName = nullptr;
    std::vector<int>* fPtrHitHadronicTargetIsotopeA = nullptr;
    std::vector<int>* fPtrHitHadronicTargetIsotopeZ = nullptr;

   public:
    using XYZVector = ROOT::Math::XYZVector;

    TRestGeant4EventData fEventData;
    mutable const TRestGeant4Metadata* fMetadata = nullptr;
    TRestRun *fRestRun = nullptr;
    mutable std::vector<TRestGeant4TrackView> fTracksViews;

    std::vector<TRestGeant4Track*> fTracks;
    std::map<int, int> fTrackIDToTrackIndex;

    /// \brief Builds the event from one Geant4 event payload.
    TRestGeant4Event(const G4Event* event);
    /// \brief Inserts a new track if its id is not yet registered.
    bool InsertTrack(const G4Track* track);
    /// \brief Updates an existing track with the latest Geant4 state.
    void UpdateTrack(const G4Track* track);
    /// \brief Appends one Geant4 step into the corresponding track hit sequence.
    void InsertStep(const G4Step* step);
    /// \brief Synchronizes primary vertex data after Geant4 primary generation.
    void UpdatePrimaryData(const G4Event* event);
    std::string GetClassName() const override { return "TRestGeant4Event"; }

    void Initialize() override {
        fEventData.clear();
        fTracksViews.clear();
        fVolumeEnergyCache.clear();
        fCrossIndexMap.clear();
        fTradVolumeIndexMap.clear();
    }

    /// \brief Returns true when this entry represents a delayed sub-event fragment.
    bool IsSubEvent() const { return GetSubID() > 0; }

    TRestGeant4Hits fInitialStep;

    std::vector<TRestGeant4Track*>& GetTracks() { return fTracks; }
    const std::vector<TRestGeant4Track*>& GetTracks() const { return fTracks; }
    std::map<int, int>& GetTrackIDToTrackIndex() { return fTrackIDToTrackIndex; }
    const std::map<int, int>& GetTrackIDToTrackIndex() const { return fTrackIDToTrackIndex; }

    /// \brief Clears track caches and flat storage vectors while keeping event object reusable.
    void ClearTracks();

    /// \brief Synchronizes object-based tracks into flat AOD vectors and hit storage.
    void SyncTracksToEventData();
    void MoveFrom(TRestGeant4Event&& source);
    void InitializeOnDetectorConstruction(const std::string&, const G4VPhysicalVolume*) {}
    /// \brief Populates detector-volume bookkeeping using the Geant4 world hierarchy.
    void PopulateFromGeant4World(const G4VPhysicalVolume* world);

    void CreateBranches(TTree* tree) override;
    void SetBranchAddresses(TTree* tree) override;
    void RefreshViews() const override;

    void CopyFrom(const TRestEvent* other) override {
        TRestEvent::CopyFrom(other);
        auto source = dynamic_cast<const TRestGeant4Event*>(other);
        if (source) {
            this->fEventData = source->fEventData;
            this->RefreshViews();
        }
    }

    void AddTrack(int trackID, int parentID, const std::string& pName, const std::string& process,
              double initialEnergy, const TRestGeant4Hits& hits,
              double globalTimestamp = 0.0, double timeOffset = 0.0, double timeLength = 0.0,
              double length = 0.0, double weight = 1.0, 
              const ROOT::Math::XYZVector& initialPos = ROOT::Math::XYZVector(),
              const std::vector<int>& secondaryIDs = std::vector<int>());

    const TRestGeant4Metadata* GetGeant4Metadata() const;
    inline void SetGeant4Metadata(const TRestGeant4Metadata* metadata) { fMetadata = metadata; }
    inline void SetRestRun(TRestRun* run) { fRestRun = run; }

    size_t GetNumberOfTracks() const { return fEventData.trackIDs.size(); }
    size_t GetNumberOfPrimaries() const { return fEventData.primaryParticleNames.size(); }

    inline TRestGeant4TrackView& GetTrack(int n) {
        if (fTracksViews.size() != fEventData.trackIDs.size()) RefreshViews();
        return fTracksViews.at(n);
    }

    inline const TRestGeant4TrackView& GetTrack(int n) const {
        if (fTracksViews.size() != fEventData.trackIDs.size()) RefreshViews();
        return fTracksViews.at(n);
    }

    inline TRestGeant4TrackView& GetTrackByID(int id) {
        auto it = std::find(fEventData.trackIDs.begin(), fEventData.trackIDs.end(), id);
        if (it == fEventData.trackIDs.end()) throw std::runtime_error("Track ID not found");
        return GetTrack(std::distance(fEventData.trackIDs.begin(), it));
    }

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
        if (fVolumeEnergyCache.empty() && !fEventData.crossVolumeNames.empty()) {
            for (size_t i = 0; i < fEventData.crossVolumeNames.size(); ++i) {
                fVolumeEnergyCache[fEventData.crossVolumeNames[i]] += fEventData.crossDepositedEnergies[i];
            }
        }

        auto it = fVolumeEnergyCache.find(volumeName);
        if (it != fVolumeEnergyCache.end()) {
            return it->second;
        }
        return 0.0;
    }

    void AddEnergyInVolumeForParticleForProcess(Double_t energy, const std::string& volumeName,
                                                const std::string& particleName,
                                                const std::string& processName);

    void PrintG4Event(int maxTracks = -1, int maxHits = -1) const;
    void PrintEvent() const override {PrintG4Event();}
    TPad* DrawEvent(const TString& option = "") const override { return nullptr; };

    TRestGeant4Event() = default;
    ~TRestGeant4Event() = default;
};
