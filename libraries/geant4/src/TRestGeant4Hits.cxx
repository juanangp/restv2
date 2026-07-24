#include "TRestGeant4Hits.h"
#include "TRestGeant4Track.h"
#include "TRestGeant4Event.h"
#include <TMath.h>

using namespace std;

ClassImp(TRestGeant4Hits);

TRestGeant4Hits::TRestGeant4Hits() : TRestHits() {}

TRestGeant4Hits::~TRestGeant4Hits() = default;

void TRestGeant4Hits::RemoveG4Hits() {
    RemoveHits();
    fProcessID.clear();
    fVolumeID.clear();
    fKineticEnergy.clear();
    fMomentumDirection.clear();
}

double TRestGeant4Hits::GetEnergyInVolume(int volumeID) const {
    double energy = 0;
    for (size_t n = 0; n < GetNumberOfHits(); n++) {
        if (fVolumeID[n] == volumeID) {
            energy += GetEnergy(n);
        }
    }
    return energy;
}

ROOT::Math::XYZVector TRestGeant4Hits::GetMeanPositionInVolume(int volumeID) const {
    ROOT::Math::XYZVector pos(0.0, 0.0, 0.0);
    double energy = 0;
    
    for (size_t n = 0; n < GetNumberOfHits(); n++) {
        if (fVolumeID[n] == volumeID) {
            // El operador + y * numérico funcionan nativamente en XYZVector
            pos += GetPosition(n) * GetEnergy(n); 
            energy += GetEnergy(n);
        }
    }

    if (energy == 0) {
        double nan = TMath::QuietNaN();
        return {nan, nan, nan};
    }

    return pos * (1. / energy);
}

ROOT::Math::XYZVector TRestGeant4Hits::GetFirstPositionInVolume(int volumeID) const {
    for (size_t n = 0; n < GetNumberOfHits(); n++) {
        if (fVolumeID[n] == volumeID) return GetPosition(n);
    }
    double nan = TMath::QuietNaN();
    return {nan, nan, nan};
}

ROOT::Math::XYZVector TRestGeant4Hits::GetLastPositionInVolume(int volumeID) const {
    for (int n = (int)GetNumberOfHits() - 1; n >= 0; n--) {
        if (fVolumeID[n] == volumeID) return GetPosition(n);
    }
    double nan = TMath::QuietNaN();
    return {nan, nan, nan};
}

size_t TRestGeant4Hits::GetNumberOfHitsInVolume(int volumeID) const {
    size_t result = 0;
    for (size_t n = 0; n < GetNumberOfHits(); n++) {
        if (fVolumeID[n] == volumeID) result++;
    }
    return result;
}

TRestGeant4Metadata* TRestGeant4Hits::GetGeant4Metadata() const {
    const TRestGeant4Event* event = nullptr;
    if (fTrack != nullptr) {
        event = fTrack->GetEvent();
    } else {
        event = fEvent;
    }
    if (event == nullptr) return nullptr;
    
    return const_cast<TRestGeant4Metadata*>(event->GetGeant4Metadata());
}

std::string TRestGeant4Hits::GetProcessName(size_t n) const {
    const auto metadata = GetGeant4Metadata();
    return metadata == nullptr ? "" : metadata->GetGeant4PhysicsInfo().GetProcessName(GetProcessId(n));
}

std::string TRestGeant4Hits::GetVolumeName(size_t n) const {
    const auto metadata = GetGeant4Metadata();
    return metadata == nullptr ? "" : metadata->GetGeant4GeometryInfo().GetVolumeFromID(GetVolumeId(n));
}

