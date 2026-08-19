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


void TRestGeant4Event::SetBranchAddresses(TTree* tree) {
    TRestEvent::SetBranchAddresses(tree);

    tree->SetBranchAddress("fPrimaryPosition", &fEventData.primaryPosition);
    tree->SetBranchAddress("fSubEventEnergy", &fEventData.subEventEnergy);
    tree->SetBranchAddress("fSubEventPosition", &fEventData.subEventPosition);
    tree->SetBranchAddress("fSubEventDirection", &fEventData.subEventDirection);
    tree->SetBranchAddress("fTotalDepositedEnergy", &fEventData.totalDepositedEnergy);
    tree->SetBranchAddress("fSensitiveVolumeEnergy", &fEventData.sensitiveVolumeEnergy);
    tree->SetBranchAddress("fEventTimeWall", &fEventData.eventTimeWall);
    tree->SetBranchAddress("fEventTimeWallPrimaryGeneration", &fEventData.eventTimeWallPrimaryGeneration);
    tree->SetBranchAddress("fNVolumes", &fEventData.nVolumes);

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
    fPtrHitX                  = &fEventData.hitsStorage.x;
    fPtrHitY                  = &fEventData.hitsStorage.y;
    fPtrHitZ                  = &fEventData.hitsStorage.z;
    fPtrHitEnergy             = &fEventData.hitsStorage.energy;
    fPtrHitTime               = &fEventData.hitsStorage.time;
    fPtrHitType               = &fEventData.hitsStorage.type;
    fPtrCrossVolumeNames      = &fEventData.crossVolumeNames;
    fPtrCrossParticleNames    = &fEventData.crossParticleNames;
    fPtrCrossProcessNames     = &fEventData.crossProcessNames;
    fPtrCrossDepositedEnergies = &fEventData.crossDepositedEnergies;

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
    tree->SetBranchAddress("fHitX", &fPtrHitX);
    tree->SetBranchAddress("fHitY", &fPtrHitY);
    tree->SetBranchAddress("fHitZ", &fPtrHitZ);
    tree->SetBranchAddress("fHitEnergy", &fPtrHitEnergy);
    tree->SetBranchAddress("fHitTime", &fPtrHitTime);
    tree->SetBranchAddress("fHitType", &fPtrHitType);
    tree->SetBranchAddress("fCrossVolumeNames", &fPtrCrossVolumeNames);
    tree->SetBranchAddress("fCrossParticleNames", &fPtrCrossParticleNames);
    tree->SetBranchAddress("fCrossProcessNames", &fPtrCrossProcessNames);
    tree->SetBranchAddress("fCrossDepositedEnergies", &fPtrCrossDepositedEnergies);
}



void TRestGeant4Event::RefreshViews() const {
    fTracksViews.clear();
    for (size_t i = 0; i < fEventData.trackIDs.size(); ++i) {
        fTracksViews.emplace_back(const_cast<TRestGeant4EventData*>(&fEventData), static_cast<int>(i));
    }
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
    double preservedTotalEnergy = fEventData.totalDepositedEnergy;
    double preservedSensitiveEnergy = fEventData.sensitiveVolumeEnergy;
    double preservedTimeWall = fEventData.eventTimeWall;
    double preservedTimeWallPrimary = fEventData.eventTimeWallPrimaryGeneration;
    int preservedNVolumes = fEventData.nVolumes;

    std::vector<int> preservedVolStored = fEventData.volumeStored;
    std::vector<std::string> preservedVolNames = fEventData.volumeStoredNames;
    std::vector<double> preservedVolEnergy = fEventData.volumeDepositedEnergy;
    std::vector<std::string> preservedCrossVol = fEventData.crossVolumeNames;
    std::vector<std::string> preservedCrossPart = fEventData.crossParticleNames;
    std::vector<std::string> preservedCrossProc = fEventData.crossProcessNames;
    std::vector<double> preservedCrossEnergy = fEventData.crossDepositedEnergies;

    fEventData.clear();

    fEventData.totalDepositedEnergy = preservedTotalEnergy;
    fEventData.sensitiveVolumeEnergy = preservedSensitiveEnergy;
    fEventData.eventTimeWall = preservedTimeWall;
    fEventData.eventTimeWallPrimaryGeneration = preservedTimeWallPrimary;
    fEventData.nVolumes = preservedNVolumes;
    fEventData.volumeStored = preservedVolStored;
    fEventData.volumeStoredNames = preservedVolNames;
    fEventData.volumeDepositedEnergy = preservedVolEnergy;
    fEventData.crossVolumeNames = preservedCrossVol;
    fEventData.crossParticleNames = preservedCrossPart;
    fEventData.crossProcessNames = preservedCrossProc;
    fEventData.crossDepositedEnergies = preservedCrossEnergy;

    int currentHitStartIndex = 0;
    for (const auto& track : fTracks) {
        if (track == nullptr) continue;

        fEventData.trackStartIndices.push_back(currentHitStartIndex);
        const auto& hits = track->GetHits();

        fEventData.trackIDs.push_back(track->GetTrackID());
        fEventData.parentIDs.push_back(track->GetParentID());
        fEventData.trackParticleNames.push_back(track->GetParticleName());
        fEventData.trackCreatorProcesses.push_back(track->GetCreatorProcess());
        fEventData.trackInitialEnergies.push_back(track->GetInitialKineticEnergy());
        fEventData.trackDepositedEnergy.push_back(hits.GetTotalEnergy());
        fEventData.trackNHits.push_back(static_cast<int>(hits.GetNumberOfHits()));

        for (size_t i = 0; i < hits.GetNumberOfHits(); ++i) {
            fEventData.hitsStorage.x.push_back(static_cast<float>(hits.GetX(i)));
            fEventData.hitsStorage.y.push_back(static_cast<float>(hits.GetY(i)));
            fEventData.hitsStorage.z.push_back(static_cast<float>(hits.GetZ(i)));
            fEventData.hitsStorage.energy.push_back(static_cast<float>(hits.GetEnergy(i)));
            fEventData.hitsStorage.time.push_back(static_cast<float>(hits.GetTime(i)));
            fEventData.hitsStorage.type.push_back(static_cast<int>(hits.GetType(i)));
        }
        currentHitStartIndex += static_cast<int>(hits.GetNumberOfHits());
    }

    RefreshViews();
}
