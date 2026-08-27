#include "TRestGeant4Hits.h"

#include <TMath.h>

#include "TRestGeant4Event.h"
#include "TRestGeant4Track.h"

TRestGeant4Hits::TRestGeant4Hits() : TRestHits(&fStorage), fIsView(false) {}

TRestGeant4Hits::TRestGeant4Hits(TRestHitsData* parentBaseData, 
                                 std::vector<int>* proc, 
                                 std::vector<int>* vol, 
                                 std::vector<float>* kin, 
                                 std::vector<ROOT::Math::XYZVector>* mom,
                                 std::vector<std::string>* hadName,
                                 std::vector<int>* hadA,
                                 std::vector<int>* hadZ,
                                 size_t start, 
                                 size_t size)
    : TRestHits(parentBaseData, start, size) {
    fData = parentBaseData;
    fStartIdx = start;
    fNHits = size;
    fIsView = true;

    // Sincronizamos las direcciones de memoria
    fMappedProcessID = proc;
    fMappedVolumeID = vol;
    fMappedKineticEnergy = kin;
    fMappedMomentumDirection = mom;
    fMappedHadronicTargetIsotopeName = hadName;
    fMappedHadronicTargetIsotopeA = hadA;
    fMappedHadronicTargetIsotopeZ = hadZ;
}

TRestGeant4Hits::TRestGeant4Hits(const TRestGeant4Hits& other) : TRestHits(other) {
    fIsView = false; // El nuevo objeto clonado tendrá sus propios datos locales independientes
    fMappedProcessID = nullptr;
    fMappedVolumeID = nullptr;
    fMappedKineticEnergy = nullptr;
    fMappedMomentumDirection = nullptr;
    fMappedHadronicTargetIsotopeName = nullptr;
    fMappedHadronicTargetIsotopeA = nullptr;
    fMappedHadronicTargetIsotopeZ = nullptr;
    fTrack = other.fTrack;
    fEvent = other.fEvent;
    fMetadata = other.fMetadata;

    this->fStorage = other.fStorage;
    fData = &fStorage;
    fStartIdx = 0;
    fNHits = fStorage.x.size();
    this->fProcessID = other.GetVec(other.fProcessID, other.fMappedProcessID);
    this->fVolumeID = other.GetVec(other.fVolumeID, other.fMappedVolumeID);
    this->fKineticEnergy = other.GetVec(other.fKineticEnergy, other.fMappedKineticEnergy);
    this->fMomentumDirection = other.GetVec(other.fMomentumDirection, other.fMappedMomentumDirection);
    this->fHadronicTargetIsotopeName = other.GetVec(other.fHadronicTargetIsotopeName, other.fMappedHadronicTargetIsotopeName);
    this->fHadronicTargetIsotopeA = other.GetVec(other.fHadronicTargetIsotopeA, other.fMappedHadronicTargetIsotopeA);
    this->fHadronicTargetIsotopeZ = other.GetVec(other.fHadronicTargetIsotopeZ, other.fMappedHadronicTargetIsotopeZ);
}

TRestGeant4Hits& TRestGeant4Hits::operator=(const TRestGeant4Hits& other) {
    if (this != &other) {
        TRestHits::operator=(other);
        fIsView = false; // Al asignarle datos, se convierte en un contenedor local con datos propios
        fMappedProcessID = nullptr;
        fMappedVolumeID = nullptr;
        fMappedKineticEnergy = nullptr;
        fMappedMomentumDirection = nullptr;
        fMappedHadronicTargetIsotopeName = nullptr;
        fMappedHadronicTargetIsotopeA = nullptr;
        fMappedHadronicTargetIsotopeZ = nullptr;
        fTrack = other.fTrack;
        fEvent = other.fEvent;
        fMetadata = other.fMetadata;

        this->fStorage = other.fStorage;
        fData = &fStorage;
        fStartIdx = 0;
        fNHits = fStorage.x.size();
        this->fProcessID = other.GetVec(other.fProcessID, other.fMappedProcessID);
        this->fVolumeID = other.GetVec(other.fVolumeID, other.fMappedVolumeID);
        this->fKineticEnergy = other.GetVec(other.fKineticEnergy, other.fMappedKineticEnergy);
        this->fMomentumDirection = other.GetVec(other.fMomentumDirection, other.fMappedMomentumDirection);
        this->fHadronicTargetIsotopeName = other.GetVec(other.fHadronicTargetIsotopeName, other.fMappedHadronicTargetIsotopeName);
        this->fHadronicTargetIsotopeA = other.GetVec(other.fHadronicTargetIsotopeA, other.fMappedHadronicTargetIsotopeA);
        this->fHadronicTargetIsotopeZ = other.GetVec(other.fHadronicTargetIsotopeZ, other.fMappedHadronicTargetIsotopeZ);
    }
    return *this;
}

TRestGeant4Hits::~TRestGeant4Hits() = default;

void TRestGeant4Hits::RemoveG4Hits() {
    RemoveHits();
    if (!fIsView) {
        fProcessID.clear();
        fVolumeID.clear();
        fKineticEnergy.clear();
        fMomentumDirection.clear();
        fHadronicTargetIsotopeName.clear();
        fHadronicTargetIsotopeA.clear();
        fHadronicTargetIsotopeZ.clear();
    }
}

double TRestGeant4Hits::GetEnergyInVolume(int volumeID) const {
    double energy = 0;
    for (size_t n = 0; n < GetNumberOfHits(); n++) {
        if (GetVolumeId(n) == volumeID) {
            energy += GetEnergy(n);
        }
    }
    return energy;
}

ROOT::Math::XYZVector TRestGeant4Hits::GetMeanPositionInVolume(int volumeID) const {
    ROOT::Math::XYZVector pos(0.0, 0.0, 0.0);
    double energy = 0;

    for (size_t n = 0; n < GetNumberOfHits(); n++) {
        if (GetVolumeId(n) == volumeID) {
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
        if (GetVolumeId(n) == volumeID) return GetPosition(n);
    }
    double nan = TMath::QuietNaN();
    return {nan, nan, nan};
}

ROOT::Math::XYZVector TRestGeant4Hits::GetLastPositionInVolume(int volumeID) const {
    for (int n = (int)GetNumberOfHits() - 1; n >= 0; n--) {
        if (GetVolumeId(n) == volumeID) return GetPosition(n);
    }
    double nan = TMath::QuietNaN();
    return {nan, nan, nan};
}

size_t TRestGeant4Hits::GetNumberOfHitsInVolume(int volumeID) const {
    size_t result = 0;
    for (size_t n = 0; n < GetNumberOfHits(); n++) {
        if (GetVolumeId(n) == volumeID) result++;
    }
    return result;
}

TRestGeant4Metadata* TRestGeant4Hits::GetGeant4Metadata() const {
    if (fMetadata != nullptr) return fMetadata;

    const TRestGeant4Event* event = nullptr;
    if (fTrack != nullptr) {
        event = fTrack->GetEvent();
    } else {
        event = fEvent;
    }
    if (event == nullptr) return nullptr;

    fMetadata = const_cast<TRestGeant4Metadata*>(event->GetGeant4Metadata());
    return fMetadata;
}

std::string TRestGeant4Hits::GetProcessName(size_t n) const {
    const auto metadata = GetGeant4Metadata();
    return metadata == nullptr ? "" : metadata->GetGeant4PhysicsInfo().GetProcessName(GetProcessId(n));
}

std::string TRestGeant4Hits::GetVolumeName(size_t n) const {
    const auto metadata = GetGeant4Metadata();
    return metadata == nullptr ? "" : metadata->GetGeant4GeometryInfo().GetVolumeFromID(GetVolumeId(n));
}

void TRestGeant4Hits::PrintHits(int nHits) const {
    int N = nHits;

    if (N < 0) N = GetNumberOfHits();
    if (N > (int)GetNumberOfHits()) N = GetNumberOfHits();

    std::cout << "\tNumber of hits to print " << N << "/" << GetNumberOfHits() << std::endl;

    for (int n = 0; n < N; n++) {
        ROOT::Math::XYZVector position(GetX(n), GetY(n), GetZ(n));

        std::cout << "\t  - Hit " << n 
                  << " - Energy: " << REST_Units::FormatAs(GetEnergy(n), REST_Units::Energy)
                  << " - Process: " << GetProcessName(n) 
                  << " - Volume: " << GetVolumeName(n) 
                  << " - Position: " << REST_Units::FormatAs(position, REST_Units::Length)
                  << " - T: " << REST_Units::FormatAs(GetTime(n), REST_Units::Time)
                  << " - KE: " << REST_Units::FormatAs(GetKineticEnergy(n), REST_Units::Energy)
                  << std::endl;
    }
}
