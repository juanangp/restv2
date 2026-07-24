#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "TRestEvent.h"
#include "TRestGeant4Metadata.h"
#include "TRestGeant4Track.h"
#include "TRestHits.h"

class TTree;
class TRestRun;
class G4Event;
class G4Track;
class G4Step;
class G4Event;
class G4Track;
class G4Step;

struct TRestVolumeEnergyDeposit {
    std::string volumeName;
    std::string particleName;
    std::string creatorProcess;
    double depositedEnergy = 0.0;
};

struct TRestGeant4TrackData {
    int trackID = 0;
    int parentID = 0;
    std::string particleName;
    std::string creatorProcess;
    double initialEnergy = 0.0;
    int startIndex = 0;
    int nHits = 0;
};

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
    std::vector<std::string> trackDepositedEnergy;
    std::vector<double> trackInitialEnergies;

    std::vector<int> trackStartIndices;
    std::vector<int> trackNHits;

    std::vector<std::string> crossVolumeNames;
    std::vector<std::string> crossParticleNames;
    std::vector<std::string> crossProcessNames;
    std::vector<double> crossDepositedEnergies;

    TRestHitsData hitsStorage;

    void clear() {
        auto clear_all = [](auto&... vecs) { (vecs.clear(), ...); };
        clear_all(primaryParticleNames, primaryEnergies, primaryDirections, volumeStored, volumeStoredNames,
                  volumeDepositedEnergy, trackIDs, parentIDs, trackParticleNames, trackCreatorProcesses,
                  trackDepositedEnergy, trackInitialEnergies, trackStartIndices, trackNHits, crossVolumeNames,
                  crossParticleNames, crossProcessNames, crossDepositedEnergies);

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

// ============================================================================
//  TRestGeant4TrackView
// ============================================================================
class TRestGeant4TrackView : public TRestHits {
   public:
    TRestGeant4EventData* fEventData = nullptr;
    int fTrackIdx = -1;

   public:
    TRestGeant4TrackView(TRestGeant4EventData* data, int idx)
        : TRestHits(&(data->hitsStorage), data->trackStartIndices[idx], data->trackNHits[idx]),
          fEventData(data),
          fTrackIdx(idx) {}

    int GetTrackID() const { return fEventData->trackIDs[fTrackIdx]; }
    int GetParentID() const { return fEventData->parentIDs[fTrackIdx]; }
    std::string GetParticleName() const { return fEventData->trackParticleNames[fTrackIdx]; }
    std::string GetCreatorProcess() const { return fEventData->trackCreatorProcesses[fTrackIdx]; }
    double GetInitialEnergy() const { return fEventData->trackInitialEnergies[fTrackIdx]; }
};

// ============================================================================
//  TRestGeant4Event
// ============================================================================
class TRestGeant4Event : public TRestEvent {
   private:
    mutable std::unordered_map<std::string, double> fVolumeEnergyCache;
    std::unordered_map<std::string, size_t> fCrossIndexMap;
    std::unordered_map<std::string, size_t> fTradVolumeIndexMap;

   public:
    using XYZVector = ROOT::Math::XYZVector;

    TRestGeant4EventData fEventData;
    TRestGeant4Metadata* fMetadata = nullptr;
    mutable std::vector<TRestGeant4TrackView> fTracksViews;

    std::vector<std::string>* fPtrPrimaryNames = nullptr;
    std::vector<double>* fPtrPrimaryEnergies = nullptr;
    std::vector<XYZVector>* fPtrPrimaryDirections = nullptr;
    std::vector<int>* fPtrTrackIDs = nullptr;
    std::vector<int>* fPtrTrackStartIndices = nullptr;

    std::vector<float>* fPtrHitX = nullptr;
    std::vector<float>* fPtrHitY = nullptr;
    std::vector<float>* fPtrHitZ = nullptr;
    std::vector<float>* fPtrHitEnergy = nullptr;
    std::vector<float>* fPtrHitTime = nullptr;

    std::vector<TRestGeant4Track> fTracks;
    std::map<int, int> fTrackIDToTrackIndex;

    TRestGeant4Event(const G4Event* event);
    bool InsertTrack(const G4Track* track);
    void UpdateTrack(const G4Track* track);
    void InsertStep(const G4Step* step);
    std::string GetClassName() const override { return "TRestGeant4Event"; }

    void Initialize() override {
        fEventData.clear();
        fTracksViews.clear();
        fVolumeEnergyCache.clear();
        fCrossIndexMap.clear();
        fTradVolumeIndexMap.clear();
    }

    bool IsSubEvent() const { return GetSubID() > 0; }

    TRestGeant4Hits fInitialStep;

    std::vector<TRestGeant4Track>& GetTracks() { return fTracks; }
    const std::vector<TRestGeant4Track>& GetTracks() const { return fTracks; }
    std::map<int, int>& GetTrackIDToTrackIndex() { return fTrackIDToTrackIndex; }
    const std::map<int, int>& GetTrackIDToTrackIndex() const { return fTrackIDToTrackIndex; }

    void ClearTracks() {
        fTracks.clear();
        fTrackIDToTrackIndex.clear();
        fEventData.trackIDs.clear();
        fEventData.parentIDs.clear();
        fEventData.trackParticleNames.clear();
        fEventData.trackCreatorProcesses.clear();
        fEventData.trackDepositedEnergy.clear();  // Keep vectors in sync when clearing track state.
        fEventData.trackInitialEnergies.clear();
        fEventData.trackStartIndices.clear();
        fEventData.trackNHits.clear();
        fEventData.hitsStorage.clear();
        fVolumeEnergyCache.clear();
        fCrossIndexMap.clear();
        fTradVolumeIndexMap.clear();
        RefreshViews();
    }

    void SyncTracksToEventData();
    void InitializeOnDetectorConstruction(const std::string&, const G4VPhysicalVolume*) {}
    void PopulateFromGeant4World(const G4VPhysicalVolume* world);

    void CreateBranches(TTree* tree) override {
        TRestEvent::CreateBranches(tree);

        tree->Branch("fPrimaryPosition", &fEventData.primaryPosition);
        tree->Branch("fPrimaryParticleNames", &fEventData.primaryParticleNames);
        tree->Branch("fPrimaryEnergies", &fEventData.primaryEnergies);
        tree->Branch("fPrimaryDirections", &fEventData.primaryDirections);

        tree->Branch("fSubEventParticleName", &fEventData.subEventParticleName);
        tree->Branch("fSubEventEnergy", &fEventData.subEventEnergy);
        tree->Branch("fSubEventPosition", &fEventData.subEventPosition);
        tree->Branch("fSubEventDirection", &fEventData.subEventDirection);

        tree->Branch("fTotalDepositedEnergy", &fEventData.totalDepositedEnergy);
        tree->Branch("fSensitiveVolumeEnergy", &fEventData.sensitiveVolumeEnergy);
        tree->Branch("fEventTimeWall", &fEventData.eventTimeWall);
        tree->Branch("fEventTimeWallPrimaryGeneration", &fEventData.eventTimeWallPrimaryGeneration);

        tree->Branch("fNVolumes", &fEventData.nVolumes);
        tree->Branch("fVolumeStored", &fEventData.volumeStored);
        tree->Branch("fVolumeStoredNames", &fEventData.volumeStoredNames);
        tree->Branch("fVolumeDepositedEnergy", &fEventData.volumeDepositedEnergy);

        tree->Branch("fTrackIDs", &fEventData.trackIDs);
        tree->Branch("fTrackParentIDs", &fEventData.parentIDs);
        tree->Branch("fTrackParticleNames", &fEventData.trackParticleNames);
        tree->Branch("fTrackCreatorProcesses", &fEventData.trackCreatorProcesses);
        tree->Branch("fTrackDepositedEnergy", &fEventData.trackDepositedEnergy);
        tree->Branch("fTrackInitialEnergies", &fEventData.trackInitialEnergies);
        tree->Branch("fTrackStartIndices", &fEventData.trackStartIndices);
        tree->Branch("fTrackNHits", &fEventData.trackNHits);

        tree->Branch("fHitX", &fEventData.hitsStorage.x);
        tree->Branch("fHitY", &fEventData.hitsStorage.y);
        tree->Branch("fHitZ", &fEventData.hitsStorage.z);
        tree->Branch("fHitEnergy", &fEventData.hitsStorage.energy);
        tree->Branch("fHitTime", &fEventData.hitsStorage.time);
        tree->Branch("fHitType", &fEventData.hitsStorage.type);

        tree->Branch("fCrossVolumeNames", &fEventData.crossVolumeNames);
        tree->Branch("fCrossParticleNames", &fEventData.crossParticleNames);
        tree->Branch("fCrossProcessNames", &fEventData.crossProcessNames);
        tree->Branch("fCrossDepositedEnergies", &fEventData.crossDepositedEnergies);
    }

    void SetBranchAddresses(TTree* tree) override {
        TRestEvent::SetBranchAddresses(tree);

        fPtrPrimaryNames = &fEventData.primaryParticleNames;
        fPtrPrimaryEnergies = &fEventData.primaryEnergies;
        fPtrPrimaryDirections = &fEventData.primaryDirections;
        fPtrTrackIDs = &fEventData.trackIDs;
        fPtrTrackStartIndices = &fEventData.trackStartIndices;

        fPtrHitX = &fEventData.hitsStorage.x;
        fPtrHitY = &fEventData.hitsStorage.y;
        fPtrHitZ = &fEventData.hitsStorage.z;
        fPtrHitEnergy = &fEventData.hitsStorage.energy;
        fPtrHitTime = &fEventData.hitsStorage.time;

        tree->SetBranchAddress("fPrimaryParticleNames", &fPtrPrimaryNames);
        tree->SetBranchAddress("fPrimaryEnergies", &fPtrPrimaryEnergies);
        tree->SetBranchAddress("fPrimaryDirections", &fPtrPrimaryDirections);

        tree->SetBranchAddress("fSubEventParticleName", &fEventData.subEventParticleName);
        tree->SetBranchAddress("fSubEventEnergy", &fEventData.subEventEnergy);
        tree->SetBranchAddress("fSubEventPosition", &fEventData.subEventPosition);
        tree->SetBranchAddress("fSubEventDirection", &fEventData.subEventDirection);

        tree->SetBranchAddress("fTotalDepositedEnergy", &fEventData.totalDepositedEnergy);
        tree->SetBranchAddress("fSensitiveVolumeEnergy", &fEventData.sensitiveVolumeEnergy);
        tree->SetBranchAddress("fEventTimeWall", &fEventData.eventTimeWall);
        tree->SetBranchAddress("fEventTimeWallPrimaryGeneration", &fEventData.eventTimeWallPrimaryGeneration);

        tree->SetBranchAddress("fNVolumes", &fEventData.nVolumes);
        tree->SetBranchAddress("fVolumeStored", &fEventData.volumeStored);
        tree->SetBranchAddress("fVolumeStoredNames", &fEventData.volumeStoredNames);
        tree->SetBranchAddress("fVolumeDepositedEnergy", &fEventData.volumeDepositedEnergy);

        tree->SetBranchAddress("fTrackIDs", &fPtrTrackIDs);
        tree->SetBranchAddress("fTrackParentIDs", &fEventData.parentIDs);
        tree->SetBranchAddress("fTrackParticleNames", &fEventData.trackParticleNames);
        tree->SetBranchAddress("fTrackCreatorProcesses", &fEventData.trackCreatorProcesses);
        tree->SetBranchAddress("fTrackDepositedEnergy", &fEventData.trackDepositedEnergy);
        tree->SetBranchAddress("fTrackInitialEnergies", &fEventData.trackInitialEnergies);
        tree->SetBranchAddress("fTrackStartIndices", &fPtrTrackStartIndices);
        tree->SetBranchAddress("fTrackNHits", &fEventData.trackNHits);

        tree->SetBranchAddress("fHitX", &fPtrHitX);
        tree->SetBranchAddress("fHitY", &fPtrHitY);
        tree->SetBranchAddress("fHitZ", &fPtrHitZ);
        tree->SetBranchAddress("fHitEnergy", &fPtrHitEnergy);
        tree->SetBranchAddress("fHitTime", &fPtrHitTime);
        tree->SetBranchAddress("fHitType", &fEventData.hitsStorage.type);

        tree->SetBranchAddress("fCrossVolumeNames", &fEventData.crossVolumeNames);
        tree->SetBranchAddress("fCrossParticleNames", &fEventData.crossParticleNames);
        tree->SetBranchAddress("fCrossProcessNames", &fEventData.crossProcessNames);
        tree->SetBranchAddress("fCrossDepositedEnergies", &fEventData.crossDepositedEnergies);
    }

    void RefreshViews() const override {
        fTracksViews.clear();
        for (size_t i = 0; i < fEventData.trackIDs.size(); ++i) {
            fTracksViews.emplace_back(const_cast<TRestGeant4EventData*>(&fEventData), static_cast<int>(i));
        }
    }

    void CopyFrom(const TRestEvent* other) override {
        TRestEvent::CopyFrom(other);
        auto source = dynamic_cast<const TRestGeant4Event*>(other);
        if (source) {
            this->fEventData = source->fEventData;
            this->RefreshViews();
        }
    }

    void AddTrack(int trackID, int parentID, const std::string& pName, const std::string& process,
                  double initialEnergy, const TRestHits& hits) {
        fEventData.trackStartIndices.push_back((int)fEventData.hitsStorage.x.size());
        fEventData.trackNHits.push_back((int)hits.GetNumberOfHits());

        fEventData.trackIDs.push_back(trackID);
        fEventData.parentIDs.push_back(parentID);
        fEventData.trackParticleNames.push_back(pName);
        fEventData.trackCreatorProcesses.push_back(process);
        fEventData.trackInitialEnergies.push_back(initialEnergy);

        for (size_t i = 0; i < hits.GetNumberOfHits(); ++i) {
            fEventData.hitsStorage.x.push_back(static_cast<float>(hits.GetX(i)));
            fEventData.hitsStorage.y.push_back(static_cast<float>(hits.GetY(i)));
            fEventData.hitsStorage.z.push_back(static_cast<float>(hits.GetZ(i)));
            fEventData.hitsStorage.energy.push_back(static_cast<float>(hits.GetEnergy(i)));
            fEventData.hitsStorage.time.push_back(static_cast<float>(hits.GetTime(i)));
            fEventData.hitsStorage.type.push_back(hits.GetType(i));
        }
        fTracksViews.clear();
    }
    TRestGeant4Metadata* GetGeant4Metadata() const { return fMetadata; }
    void SetGeant4Metadata(TRestGeant4Metadata* metadata) { fMetadata = metadata; }

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
        if (it == fEventData.trackIDs.end()) throw std::runtime_error("Track ID no encontrado");
        return GetTrack(std::distance(fEventData.trackIDs.begin(), it));
    }

    TRestHits GetEventHits() const {
        return TRestHits(const_cast<TRestHitsData*>(&fEventData.hitsStorage), 0,
                         (int)fEventData.hitsStorage.x.size());
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

    void PrintEvent() const override {};
    TPad* DrawEvent(const TString& option = "") const override { return nullptr; };

    TRestGeant4Event() = default;
    ~TRestGeant4Event() = default;
};
