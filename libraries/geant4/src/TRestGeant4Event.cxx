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

void TRestGeant4Event::AddEnergyInVolumeForParticleForProcess(Double_t energy, const std::string& volumeName,
                                                              const std::string& particleName,
                                                              const std::string& processName) {
    if (energy <= 0) return;

    fEventData.totalDepositedEnergy += energy;

    // --- 1. Decomposed AOD optimization ---
    // Create a compact unique key by concatenating the strings.
    std::string crossKey = volumeName + "\0" + particleName + "\0" + processName;

    auto itCross = fCrossIndexMap.find(crossKey);
    if (itCross != fCrossIndexMap.end()) {
        // If it already exists, update it directly by index in O(1).
        fEventData.crossDepositedEnergies[itCross->second] += energy;
    } else {
        // If it is new, append it and register its position.
        size_t newIdx = fEventData.crossVolumeNames.size();
        fEventData.crossVolumeNames.push_back(volumeName);
        fEventData.crossParticleNames.push_back(particleName);
        fEventData.crossProcessNames.push_back(processName);
        fEventData.crossDepositedEnergies.push_back(energy);
        fCrossIndexMap[crossKey] = newIdx;
    }

    // --- 2. Traditional REST optimization ---
    auto itTrad = fTradVolumeIndexMap.find(volumeName);
    if (itTrad != fTradVolumeIndexMap.end()) {
        // Direct O(1) access by index.
        fEventData.volumeDepositedEnergy[itTrad->second] += energy;
    } else {
        // Register a new volume entry.
        size_t newVolIdx = fEventData.volumeStoredNames.size();
        fEventData.volumeStoredNames.push_back(volumeName);
        fEventData.volumeDepositedEnergy.push_back(energy);
        fEventData.volumeStored.push_back(fEventData.nVolumes);
        fTradVolumeIndexMap[volumeName] = newVolIdx;
        fEventData.nVolumes++;
    }
}

void TRestGeant4Event::SyncTracksToEventData() {
    for (int i = 0; i < fEventData.nVolumes; ++i) {
        this->SetEnergyDepositedInVolume(i, this->GetEnergyInVolume(fEventData.volumeStoredNames[i]));
    }

    fEventData.trackIDs.clear();
    fEventData.parentIDs.clear();
    fEventData.trackParticleNames.clear();
    fEventData.trackCreatorProcesses.clear();
    fEventData.trackInitialEnergies.clear();
    fEventData.trackStartIndices.clear();
    fEventData.trackNHits.clear();

    int currentHitStartIndex = 0;

    for (const auto& track : fTracks) {
        fEventData.trackIDs.push_back(track.GetTrackID());
        fEventData.parentIDs.push_back(track.GetParentID());
        fEventData.trackParticleNames.push_back(track.GetParticleName());
        fEventData.trackCreatorProcesses.push_back(track.GetCreatorProcess());

        fEventData.trackInitialEnergies.push_back(track.GetTotalEnergy());

        fEventData.trackStartIndices.push_back(currentHitStartIndex);
        fEventData.trackNHits.push_back(track.GetNumberOfHits());

        currentHitStartIndex += track.GetNumberOfHits();
    }

    this->RefreshViews();
}
