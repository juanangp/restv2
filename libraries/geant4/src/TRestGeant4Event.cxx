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

void TRestGeant4Event::AddEnergyInVolumeForParticleForProcess(Double_t energy, 
                                                              const std::string& volumeName,
                                                              const std::string& particleName, 
                                                              const std::string& processName) {
    if (energy <= 0) return;

    fEventData.totalDepositedEnergy += energy;

    // --- 1. OPTIMIZACIÓN AOD DESGLOSADO ---
    // Creamos una clave única compacta uniendo los strings
    std::string crossKey = volumeName + "\0" + particleName + "\0" + processName;
    
    auto itCross = fCrossIndexMap.find(crossKey);
    if (itCross != fCrossIndexMap.end()) {
        // Si ya existe, accedemos directamente por índice en O(1) sin bucles
        fEventData.crossDepositedEnergies[itCross->second] += energy;
    } else {
        // Si es nuevo, insertamos al final y registramos su posición
        size_t newIdx = fEventData.crossVolumeNames.size();
        fEventData.crossVolumeNames.push_back(volumeName);
        fEventData.crossParticleNames.push_back(particleName);
        fEventData.crossProcessNames.push_back(processName);
        fEventData.crossDepositedEnergies.push_back(energy);
        fCrossIndexMap[crossKey] = newIdx;
    }

    // --- 2. OPTIMIZACIÓN TRADICIONAL REST ---
    auto itTrad = fTradVolumeIndexMap.find(volumeName);
    if (itTrad != fTradVolumeIndexMap.end()) {
        // Acceso directo por índice en O(1)
        fEventData.volumeDepositedEnergy[itTrad->second] += energy;
    } else {
        // Registro de nuevo volumen
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


