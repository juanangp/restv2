#include "TRestGeant4Event.h"

// ---------------------------------------------------------------------------
// Self-registration in EventRegistry
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    EventRegistry::Instance().Register("TRestGeant4Event",
                                       []() { return std::make_unique<TRestGeant4Event>(); });
    return true;
}();
}  // namespace

const TRestGeant4Metadata* TRestGeant4Event::GetGeant4Metadata() const {
    if (fMetadata) {
        return fMetadata;
    }

    if (fRestRun) {
        TRestMetadata* baseMeta = fRestRun->GetMetadataClass("TRestGeant4Metadata");
        if (baseMeta) {
            auto* nonConstThis = const_cast<TRestGeant4Event*>(this);
            nonConstThis->fMetadata = dynamic_cast<const TRestGeant4Metadata*>(baseMeta);
        }
    }
    
    return fMetadata;
}

void TRestGeant4Event::CreateBranches(TTree* tree) {
    TRestEvent::CreateBranches(tree);

    tree->Branch("fPrimaryPosition", &fEventData.primaryPosition);
    tree->Branch("fSubEventEnergy", &fEventData.subEventEnergy);
    tree->Branch("fSubEventPosition", &fEventData.subEventPosition);
    tree->Branch("fSubEventDirection", &fEventData.subEventDirection);
    tree->Branch("fTotalDepositedEnergy", &fEventData.totalDepositedEnergy);
    tree->Branch("fSensitiveVolumeEnergy", &fEventData.sensitiveVolumeEnergy);
    tree->Branch("fEventTimeWall", &fEventData.eventTimeWall);
    tree->Branch("fEventTimeWallPrimaryGeneration", &fEventData.eventTimeWallPrimaryGeneration);
    tree->Branch("fNVolumes", &fEventData.nVolumes);

    tree->Branch("fSubEventParticleName", &fEventData.subEventParticleName);
    tree->Branch("fPrimaryParticleNames", &fEventData.primaryParticleNames);
    tree->Branch("fPrimaryEnergies", &fEventData.primaryEnergies);
    tree->Branch("fPrimaryDirections", &fEventData.primaryDirections);
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
    tree->Branch("fTrackGlobalTimestamp", &fEventData.trackGlobalTimestamps);
    tree->Branch("fTrackTimeOffset", &fEventData.trackTimeOffsets);
    tree->Branch("fTrackTimeLength", &fEventData.trackTimeLengths);
    tree->Branch("fTrackLength", &fEventData.trackLengths);
    tree->Branch("fTrackWeight", &fEventData.trackWeights);
    tree->Branch("fTrackSecondariesIDs", &fEventData.trackSecondariesIDs);
    tree->Branch("fTrackSecondariesIndex", &fEventData.trackSecondariesIndices);
    tree->Branch("fTrackSecondariesOffsets", &fEventData.trackSecondariesOffsets);
    tree->Branch("fTrackInitialPosition", &fEventData.trackInitialPositions);

    tree->Branch("fHitX", &fEventData.hitsStorage.x);
    tree->Branch("fHitY", &fEventData.hitsStorage.y);
    tree->Branch("fHitZ", &fEventData.hitsStorage.z);
    tree->Branch("fHitEnergy", &fEventData.hitsStorage.energy);
    tree->Branch("fHitTime", &fEventData.hitsStorage.time);
    tree->Branch("fHitType", &fEventData.hitsStorage.type);
    tree->Branch("fHitProcessID", &fEventData.hitProcessID);
    tree->Branch("fHitVolumeID", &fEventData.hitVolumeID);
    tree->Branch("fHitKineticEnergy", &fEventData.hitKineticEnergy);
    tree->Branch("fHitMomentumDirection", &fEventData.hitMomentumDirection);
    tree->Branch("fHitHadronicTargetIsotopeName", &fEventData.hitHadronicTargetIsotopeName);
    tree->Branch("fHitHadronicTargetIsotopeA", &fEventData.hitHadronicTargetIsotopeA);
    tree->Branch("fHitHadronicTargetIsotopeZ", &fEventData.hitHadronicTargetIsotopeZ);

    tree->Branch("fCrossVolumeNames", &fEventData.crossVolumeNames);
    tree->Branch("fCrossParticleNames", &fEventData.crossParticleNames);
    tree->Branch("fCrossProcessNames", &fEventData.crossProcessNames);
    tree->Branch("fCrossDepositedEnergies", &fEventData.crossDepositedEnergies);

}

void TRestGeant4Event::SetBranchAddresses(TTree* tree) {
    TRestEvent::SetBranchAddresses(tree);

    tree->SetBranchAddress("fSubEventEnergy", &fEventData.subEventEnergy);
    tree->SetBranchAddress("fTotalDepositedEnergy", &fEventData.totalDepositedEnergy);
    tree->SetBranchAddress("fSensitiveVolumeEnergy", &fEventData.sensitiveVolumeEnergy);
    tree->SetBranchAddress("fEventTimeWall", &fEventData.eventTimeWall);
    tree->SetBranchAddress("fEventTimeWallPrimaryGeneration", &fEventData.eventTimeWallPrimaryGeneration);
    tree->SetBranchAddress("fNVolumes", &fEventData.nVolumes);

    fPtrPrimaryPosition       = &fEventData.primaryPosition;
    fPtrSubEventPosition      = &fEventData.subEventPosition;
    fPtrSubEventDirection     = &fEventData.subEventDirection;
    fPtrSubEventParticleName  = &fEventData.subEventParticleName;
    fPtrPrimaryParticleNames  = &fEventData.primaryParticleNames;
    fPtrPrimaryEnergies       = &fEventData.primaryEnergies;
    fPtrPrimaryDirections     = &fEventData.primaryDirections;
    fPtrVolumeStored          = &fEventData.volumeStored;
    fPtrVolumeStoredNames     = &fEventData.volumeStoredNames;
    fPtrVolumeDepositedEnergy = &fEventData.volumeDepositedEnergy;
    fPtrTrackIDs              = &fEventData.trackIDs;
    fPtrParentIDs             = &fEventData.parentIDs;
    fPtrTrackParticleNames    = &fEventData.trackParticleNames;
    fPtrTrackCreatorProcesses = &fEventData.trackCreatorProcesses;
    fPtrTrackDepositedEnergy  = &fEventData.trackDepositedEnergy;
    fPtrTrackInitialEnergies  = &fEventData.trackInitialEnergies;
    fPtrTrackStartIndices     = &fEventData.trackStartIndices;
    fPtrTrackNHits            = &fEventData.trackNHits;
    fPtrTrackGlobalTimestamp   = &fEventData.trackGlobalTimestamps;
    fPtrTrackTimeOffset        = &fEventData.trackTimeOffsets;
    fPtrTrackTimeLength        = &fEventData.trackTimeLengths;
    fPtrTrackLength            = &fEventData.trackLengths;
    fPtrTrackWeight            = &fEventData.trackWeights;
    fPtrTrackSecondariesIDs     = &fEventData.trackSecondariesIDs;
    fPtrTrackSecondariesIndex   = &fEventData.trackSecondariesIndices;
    fPtrTrackSecondariesOffsets = &fEventData.trackSecondariesOffsets;
    fPtrTrackInitialPosition   = &fEventData.trackInitialPositions;
    fPtrHitX                  = &fEventData.hitsStorage.x;
    fPtrHitY                  = &fEventData.hitsStorage.y;
    fPtrHitZ                  = &fEventData.hitsStorage.z;
    fPtrHitEnergy             = &fEventData.hitsStorage.energy;
    fPtrHitTime               = &fEventData.hitsStorage.time;
    fPtrHitType               = &fEventData.hitsStorage.type;
    fPtrHitProcessID          = &fEventData.hitProcessID;
    fPtrHitVolumeID           = &fEventData.hitVolumeID;
    fPtrHitKineticEnergy      = &fEventData.hitKineticEnergy;
    fPtrHitMomentumDirection  = &fEventData.hitMomentumDirection;
    fPtrHitHadronicTargetIsotopeName = &fEventData.hitHadronicTargetIsotopeName;
    fPtrHitHadronicTargetIsotopeA    = &fEventData.hitHadronicTargetIsotopeA;
    fPtrHitHadronicTargetIsotopeZ    = &fEventData.hitHadronicTargetIsotopeZ;

    fPtrCrossVolumeNames      = &fEventData.crossVolumeNames;
    fPtrCrossParticleNames    = &fEventData.crossParticleNames;
    fPtrCrossProcessNames     = &fEventData.crossProcessNames;
    fPtrCrossDepositedEnergies = &fEventData.crossDepositedEnergies;

    tree->SetBranchAddress("fPrimaryPosition", &fPtrPrimaryPosition);
    tree->SetBranchAddress("fSubEventPosition", &fPtrSubEventPosition);
    tree->SetBranchAddress("fSubEventDirection", &fPtrSubEventDirection);
    tree->SetBranchAddress("fSubEventParticleName", &fPtrSubEventParticleName);
    tree->SetBranchAddress("fPrimaryParticleNames", &fPtrPrimaryParticleNames);
    tree->SetBranchAddress("fPrimaryEnergies", &fPtrPrimaryEnergies);
    tree->SetBranchAddress("fPrimaryDirections", &fPtrPrimaryDirections);
    tree->SetBranchAddress("fVolumeStored", &fPtrVolumeStored);
    tree->SetBranchAddress("fVolumeStoredNames", &fPtrVolumeStoredNames);
    tree->SetBranchAddress("fVolumeDepositedEnergy", &fPtrVolumeDepositedEnergy);
    tree->SetBranchAddress("fTrackIDs", &fPtrTrackIDs);
    tree->SetBranchAddress("fTrackParentIDs", &fPtrParentIDs);
    tree->SetBranchAddress("fTrackParticleNames", &fPtrTrackParticleNames);
    tree->SetBranchAddress("fTrackCreatorProcesses", &fPtrTrackCreatorProcesses);
    tree->SetBranchAddress("fTrackDepositedEnergy", &fPtrTrackDepositedEnergy);
    tree->SetBranchAddress("fTrackInitialEnergies", &fPtrTrackInitialEnergies);
    tree->SetBranchAddress("fTrackStartIndices", &fPtrTrackStartIndices);
    tree->SetBranchAddress("fTrackNHits", &fPtrTrackNHits);
    tree->SetBranchAddress("fTrackGlobalTimestamp", &fPtrTrackGlobalTimestamp);
    tree->SetBranchAddress("fTrackTimeOffset", &fPtrTrackTimeOffset);
    tree->SetBranchAddress("fTrackTimeLength", &fPtrTrackTimeLength);
    tree->SetBranchAddress("fTrackLength", &fPtrTrackLength);
    tree->SetBranchAddress("fTrackWeight", &fPtrTrackWeight);
    tree->SetBranchAddress("fTrackSecondariesIDs", &fPtrTrackSecondariesIDs);
    tree->SetBranchAddress("fTrackSecondariesIndex", &fPtrTrackSecondariesIndex);
    tree->SetBranchAddress("fTrackSecondariesOffsets", &fPtrTrackSecondariesOffsets);
    tree->SetBranchAddress("fTrackInitialPosition", &fPtrTrackInitialPosition);

    tree->SetBranchAddress("fHitX", &fPtrHitX);
    tree->SetBranchAddress("fHitY", &fPtrHitY);
    tree->SetBranchAddress("fHitZ", &fPtrHitZ);
    tree->SetBranchAddress("fHitEnergy", &fPtrHitEnergy);
    tree->SetBranchAddress("fHitTime", &fPtrHitTime);
    tree->SetBranchAddress("fHitType", &fPtrHitType);
    tree->SetBranchAddress("fHitProcessID", &fPtrHitProcessID);
    tree->SetBranchAddress("fHitVolumeID", &fPtrHitVolumeID);
    
    tree->SetBranchAddress("fHitKineticEnergy", &fPtrHitKineticEnergy);
    tree->SetBranchAddress("fHitMomentumDirection", &fPtrHitMomentumDirection);
    tree->SetBranchAddress("fHitHadronicTargetIsotopeName", &fPtrHitHadronicTargetIsotopeName);
    tree->SetBranchAddress("fHitHadronicTargetIsotopeA", &fPtrHitHadronicTargetIsotopeA);
    tree->SetBranchAddress("fHitHadronicTargetIsotopeZ", &fPtrHitHadronicTargetIsotopeZ);

    tree->SetBranchAddress("fCrossVolumeNames", &fPtrCrossVolumeNames);
    tree->SetBranchAddress("fCrossParticleNames", &fPtrCrossParticleNames);
    tree->SetBranchAddress("fCrossProcessNames", &fPtrCrossProcessNames);
    tree->SetBranchAddress("fCrossDepositedEnergies", &fPtrCrossDepositedEnergies);
}

void TRestGeant4Event::RefreshViews() const {
    fTracksViews.clear();
    fTracksViews.reserve(fEventData.trackIDs.size());
    for (size_t i = 0; i < fEventData.trackIDs.size(); ++i) {
        fTracksViews.emplace_back(const_cast<TRestGeant4EventData*>(&fEventData),
                                  const_cast<TRestGeant4Event*>(this), static_cast<int>(i));
    }
}

void TRestGeant4Event::MoveFrom(TRestGeant4Event&& source) {
    TRestEvent::CopyFrom(&source);
    fEventData = std::move(source.fEventData);
    fMetadata = source.fMetadata;
    fRestRun = source.fRestRun;
    fTracks.clear();
    fTrackIDToTrackIndex.clear();
    fTracksViews.clear();
    fVolumeEnergyCache.clear();
    fCrossIndexMap.clear();
    fTradVolumeIndexMap.clear();
    RefreshViews();
}

void TRestGeant4Event::AddTrack(int trackID, int parentID, const std::string& pName, const std::string& process,
                                double initialEnergy, const TRestGeant4Hits& hits,
                                double globalTimestamp, double timeOffset, double timeLength,
                                double length, double weight, const ROOT::Math::XYZVector& initialPos,
                                const std::vector<int>& secondaryIDs) {

    fEventData.trackStartIndices.push_back((int)fEventData.hitsStorage.x.size());
    fEventData.trackNHits.push_back((int)hits.GetNumberOfHits());

    fEventData.trackIDs.push_back(trackID);
    fEventData.parentIDs.push_back(parentID);
    fEventData.trackParticleNames.push_back(pName);
    fEventData.trackCreatorProcesses.push_back(process);
    fEventData.trackInitialEnergies.push_back(initialEnergy);
    fEventData.trackDepositedEnergy.push_back(hits.GetTotalEnergy());

    fEventData.trackGlobalTimestamps.push_back(globalTimestamp);
    fEventData.trackTimeOffsets.push_back(timeOffset);
    fEventData.trackTimeLengths.push_back(timeLength);
    fEventData.trackLengths.push_back(length);
    fEventData.trackWeights.push_back(weight);
    fEventData.trackInitialPositions.push_back(initialPos);

    int currentSecondaryStartIndex = static_cast<int>(fEventData.trackSecondariesIDs.size());
    fEventData.trackSecondariesIndices.push_back(currentSecondaryStartIndex);
    fEventData.trackSecondariesOffsets.push_back(static_cast<int>(secondaryIDs.size()));

    for (int secID : secondaryIDs) {
        fEventData.trackSecondariesIDs.push_back(secID);
    }

    for (size_t i = 0; i < hits.GetNumberOfHits(); ++i) {
        fEventData.hitsStorage.x.push_back(static_cast<float>(hits.GetX(i)));
        fEventData.hitsStorage.y.push_back(static_cast<float>(hits.GetY(i)));
        fEventData.hitsStorage.z.push_back(static_cast<float>(hits.GetZ(i)));
        fEventData.hitsStorage.energy.push_back(static_cast<float>(hits.GetEnergy(i)));
        fEventData.hitsStorage.time.push_back(static_cast<float>(hits.GetTime(i)));
        fEventData.hitsStorage.type.push_back(static_cast<int>(hits.GetType(i))); 

        fEventData.hitProcessID.push_back(static_cast<int>(hits.GetProcessId(i)));
        fEventData.hitVolumeID.push_back(static_cast<int>(hits.GetVolumeId(i)));
        fEventData.hitKineticEnergy.push_back(static_cast<float>(hits.GetKineticEnergy(i)));
        fEventData.hitMomentumDirection.push_back(hits.GetMomentumDirection(i));
        
        fEventData.hitHadronicTargetIsotopeName.push_back(hits.GetHadronicTargetIsotopeName(i));
        fEventData.hitHadronicTargetIsotopeA.push_back(hits.GetHadronicTargetIsotopeA(i));
        fEventData.hitHadronicTargetIsotopeZ.push_back(hits.GetHadronicTargetIsotopeZ(i));
    }

    fTracksViews.clear();
}

void TRestGeant4Event::ClearTracks() {
    for (auto* track : fTracks) {
      if (track) delete track;
    }
    fTracks.clear();
    fTrackIDToTrackIndex.clear();
    
    fEventData.trackIDs.clear();
    fEventData.parentIDs.clear();
    fEventData.trackParticleNames.clear();
    fEventData.trackCreatorProcesses.clear();
    fEventData.trackDepositedEnergy.clear();
    fEventData.trackInitialEnergies.clear();
    fEventData.trackStartIndices.clear();
    fEventData.trackNHits.clear();

    fEventData.trackGlobalTimestamps.clear();
    fEventData.trackTimeOffsets.clear();
    fEventData.trackTimeLengths.clear();
    fEventData.trackLengths.clear();
    fEventData.trackWeights.clear();
    fEventData.trackInitialPositions.clear();

    fEventData.trackSecondariesIDs.clear();
    fEventData.trackSecondariesIndices.clear();
    fEventData.trackSecondariesOffsets.clear();

    fEventData.hitsStorage.clear();
    fVolumeEnergyCache.clear();
    fCrossIndexMap.clear();
    fTradVolumeIndexMap.clear();
    
    RefreshViews();
}

void TRestGeant4Event::AddEnergyInVolumeForParticleForProcess(Double_t energy, const std::string& volumeName,
                                                              const std::string& particleName,
                                                              const std::string& processName) {
    if (energy <= 0) return;
    fEventData.totalDepositedEnergy += energy;

    std::string crossKey = volumeName + "\0" + particleName + "\0" + processName;
    auto itCross = fCrossIndexMap.find(crossKey);
    if (itCross != fCrossIndexMap.end()) {
        fEventData.crossDepositedEnergies[itCross->second] += energy;
    } else {
        size_t newIdx = fEventData.crossVolumeNames.size();
        fEventData.crossVolumeNames.push_back(volumeName);
        fEventData.crossParticleNames.push_back(particleName);
        fEventData.crossProcessNames.push_back(processName);
        fEventData.crossDepositedEnergies.push_back(energy);
        fCrossIndexMap[crossKey] = newIdx;
    }

    auto itTrad = fTradVolumeIndexMap.find(volumeName);
    if (itTrad != fTradVolumeIndexMap.end()) {
        fEventData.volumeDepositedEnergy[itTrad->second] += energy;
    } else {
        size_t newVolIdx = fEventData.volumeStoredNames.size();
        fEventData.volumeStoredNames.push_back(volumeName);
        fEventData.volumeDepositedEnergy.push_back(energy);
        fEventData.volumeStored.push_back(fEventData.nVolumes);
        fTradVolumeIndexMap[volumeName] = newVolIdx;
        fEventData.nVolumes++;
    }
}

void TRestGeant4Event::SyncTracksToEventData() {
    size_t nTracks = fTracks.size();

    fEventData.trackIDs.clear();
    fEventData.parentIDs.clear();
    fEventData.trackParticleNames.clear();
    fEventData.trackCreatorProcesses.clear();
    fEventData.trackDepositedEnergy.clear();
    fEventData.trackInitialEnergies.clear();
    fEventData.trackStartIndices.clear();
    fEventData.trackNHits.clear();

    fEventData.trackGlobalTimestamps.clear();
    fEventData.trackTimeOffsets.clear();
    fEventData.trackTimeLengths.clear();
    fEventData.trackLengths.clear();
    fEventData.trackWeights.clear();
    fEventData.trackSecondariesIDs.clear();
    fEventData.trackSecondariesOffsets.clear();
    fEventData.trackSecondariesIndices.clear();
    fEventData.trackInitialPositions.clear();

    fEventData.hitsStorage.clear(); 
    fEventData.hitProcessID.clear();
    fEventData.hitVolumeID.clear();
    fEventData.hitKineticEnergy.clear();
    fEventData.hitMomentumDirection.clear();
    fEventData.hitHadronicTargetIsotopeName.clear();
    fEventData.hitHadronicTargetIsotopeA.clear();
    fEventData.hitHadronicTargetIsotopeZ.clear();

    if (nTracks == 0) {
        RefreshViews();
        return;
    }

    fEventData.trackGlobalTimestamps.reserve(nTracks);
    fEventData.trackTimeOffsets.reserve(nTracks);
    fEventData.trackTimeLengths.reserve(nTracks);
    fEventData.trackLengths.reserve(nTracks);
    fEventData.trackWeights.reserve(nTracks);
    fEventData.trackInitialPositions.reserve(nTracks);

    size_t totalHits = 0;
    for (const auto* track : fTracks) {
        if (track) totalHits += track->GetHits().GetNumberOfHits();
    }
    
    fEventData.hitsStorage.x.reserve(totalHits);
    fEventData.hitsStorage.y.reserve(totalHits);
    fEventData.hitsStorage.z.reserve(totalHits);
    fEventData.hitsStorage.energy.reserve(totalHits);
    fEventData.hitsStorage.time.reserve(totalHits);
    fEventData.hitsStorage.type.reserve(totalHits);
    
    fEventData.hitProcessID.reserve(totalHits);
    fEventData.hitVolumeID.reserve(totalHits);
    fEventData.hitKineticEnergy.reserve(totalHits);
    fEventData.hitMomentumDirection.reserve(totalHits);
    fEventData.hitHadronicTargetIsotopeName.reserve(totalHits);
    fEventData.hitHadronicTargetIsotopeA.reserve(totalHits);
    fEventData.hitHadronicTargetIsotopeZ.reserve(totalHits);

    int currentHitStartIndex = 0;
    size_t currentSecondaryStartIndex = 0;
    for (const auto& track : fTracks) {
        if (track == nullptr) continue;

        fEventData.trackStartIndices.push_back(currentHitStartIndex);
        const auto& hits = track->GetHits();
        const size_t nHits = hits.GetNumberOfHits();

        fEventData.trackIDs.push_back(track->GetTrackID());
        fEventData.parentIDs.push_back(track->GetParentID());
        fEventData.trackParticleNames.push_back(track->GetParticleName());
        fEventData.trackCreatorProcesses.push_back(track->GetCreatorProcess());
        fEventData.trackInitialEnergies.push_back(track->GetInitialKineticEnergy());
        fEventData.trackNHits.push_back(static_cast<int>(nHits));

        fEventData.trackGlobalTimestamps.push_back(track->GetGlobalTime());
        fEventData.trackTimeOffsets.push_back(track->GetTimeOffset());
        fEventData.trackTimeLengths.push_back(track->GetTimeLength());
        fEventData.trackLengths.push_back(track->GetLength());
        fEventData.trackWeights.push_back(track->GetWeight());
        fEventData.trackInitialPositions.push_back(track->GetInitialPosition());

        const std::vector<int>& secondaries = track->GetSecondaryTrackIDs();
        const size_t nSecondaries = secondaries.size();

        fEventData.trackSecondariesIndices.push_back(currentSecondaryStartIndex);
        fEventData.trackSecondariesOffsets.push_back(static_cast<int>(nSecondaries));

        for (int secID : secondaries) {
            fEventData.trackSecondariesIDs.push_back(secID);
        }

        double trackDepositedEnergy = 0;

        for (size_t i = 0; i < nHits; ++i) {
            trackDepositedEnergy += hits.GetEnergy(i);
            
            fEventData.hitsStorage.x.push_back(static_cast<float>(hits.GetX(i)));
            fEventData.hitsStorage.y.push_back(static_cast<float>(hits.GetY(i)));
            fEventData.hitsStorage.z.push_back(static_cast<float>(hits.GetZ(i)));
            fEventData.hitsStorage.energy.push_back(static_cast<float>(hits.GetEnergy(i)));
            fEventData.hitsStorage.time.push_back(static_cast<float>(hits.GetTime(i)));
            fEventData.hitsStorage.type.push_back(static_cast<int>(hits.GetType(i)));
            
            fEventData.hitProcessID.push_back(static_cast<int>(hits.GetProcessId(i)));
            fEventData.hitVolumeID.push_back(static_cast<int>(hits.GetVolumeId(i)));
            fEventData.hitKineticEnergy.push_back(static_cast<float>(hits.GetKineticEnergy(i)));
            fEventData.hitMomentumDirection.push_back(hits.GetMomentumDirection(i));
            fEventData.hitHadronicTargetIsotopeName.push_back(hits.GetHadronicTargetIsotopeName(i));
            fEventData.hitHadronicTargetIsotopeA.push_back(hits.GetHadronicTargetIsotopeA(i));
            fEventData.hitHadronicTargetIsotopeZ.push_back(hits.GetHadronicTargetIsotopeZ(i));
        }
        fEventData.trackDepositedEnergy.push_back(trackDepositedEnergy);
        currentHitStartIndex += static_cast<int>(nHits);
        currentSecondaryStartIndex += static_cast<int>(nSecondaries);
    }

    RefreshViews();
}

void TRestGeant4Event::PrintG4Event(int maxTracks, int maxHits) const {
    std::cout << "=========================================================" << std::endl;
    std::cout << " TRestGeant4Event - PrintEvent" << std::endl;
    std::cout << "=========================================================" << std::endl;

    TRestEvent::PrintEvent();

    int totalTracksAvailable = static_cast<int>(GetNumberOfTracks());
    
    if (maxTracks < 0) {
        maxTracks = totalTracksAvailable;
    }
    int nTracks = std::min(maxTracks, totalTracksAvailable);

    std::cout << "- Total deposited energy: " 
              << REST_Units::FormatAs(fEventData.totalDepositedEnergy, REST_Units::Energy) << std::endl;
    std::cout << "- Sensitive detectors total energy: " 
              << REST_Units::FormatAs(fEventData.sensitiveVolumeEnergy, REST_Units::Energy) << std::endl;
    std::cout << "- Event Wall Time: " 
              << REST_Units::FormatAs(fEventData.eventTimeWall, REST_Units::Time) << std::endl;
    
    std::cout << "- Primary source position: " 
              << REST_Units::FormatAs(fEventData.primaryPosition, REST_Units::Length) << std::endl;

    std::cout << "- Primary Particles (" << fEventData.primaryParticleNames.size() << "):" << std::endl;
    for (size_t i = 0; i < fEventData.primaryParticleNames.size(); ++i) {
        std::string sourceNumberString = " [" + std::to_string(i) + "]";
        std::cout << "   - Source" << sourceNumberString << ": " << fEventData.primaryParticleNames[i] << std::endl;
        
        std::cout << "     Direction: (" 
                  << fEventData.primaryDirections[i].X() << ", " 
                  << fEventData.primaryDirections[i].Y() << ", " 
                  << fEventData.primaryDirections[i].Z() << ")" << std::endl;
                  
        std::cout << "     Energy: " 
                  << REST_Units::FormatAs(fEventData.primaryEnergies[i], REST_Units::Energy) << std::endl;
    }

    std::cout << "- Number of tracks to print: " << nTracks << " (Total in event: " << totalTracksAvailable << ")" << std::endl;

    for (int i = 0; i < nTracks; ++i) {
        const auto &trackView = GetTrack(i);
        
        std::cout << "   -----------------------------------------------------" << std::endl;
        std::cout << " * TrackID: " << trackView.GetTrackID() 
                  << " - Particle: " << trackView.GetParticleName() 
                  << " - ParentID: " << trackView.GetParentID()
                  << " - Created by '" << trackView.GetCreatorProcess() << "'"
                  << " in volume '" << trackView.GetInitialVolume() << "'" 
                  << " with initial KE of " << REST_Units::FormatAs(trackView.GetInitialEnergy(), REST_Units::Energy) << std::endl;
                  
        std::cout << "      Length: " << REST_Units::FormatAs(trackView.GetLength(), REST_Units::Length)
                  << " | Time Length: " << REST_Units::FormatAs(trackView.GetTimeLength(), REST_Units::Time)
                  << " | Weight: " << trackView.GetWeight() << std::endl;
        
        std::cout << "      Initial Position: " 
                  << REST_Units::FormatAs(trackView.GetInitialPosition(), REST_Units::Length) << std::endl;

        trackView.PrintHits(maxHits);
    }
    std::cout << "=========================================================" << std::endl;
}
