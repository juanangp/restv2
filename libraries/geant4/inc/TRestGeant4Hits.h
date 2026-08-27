#pragma once

#include <TRestHits.h>

#include <iostream>
#include <string>
#include <vector>

// Modern ROOT vector header adopted by REST-for-Physics
#include <Math/Vector3D.h>

#include "TRestGeant4Metadata.h"

class G4Step;
class TRestGeant4Track;
class TRestGeant4Event;

/// \class TRestGeant4Hits
/// \brief Stores Geant4 hit-level information and provides volume/process-based accessors.
class TRestGeant4Hits : public TRestHits {
   protected:
    TRestHitsData fStorage; // Almacenamiento base (x, y, z, time, energy, type)

    std::vector<int> fProcessID = {};
    std::vector<int> fVolumeID = {};
    std::vector<float> fKineticEnergy = {};
    std::vector<ROOT::Math::XYZVector> fMomentumDirection = {};

    std::vector<std::string> fHadronicTargetIsotopeName = {};
    std::vector<int> fHadronicTargetIsotopeA = {};
    std::vector<int> fHadronicTargetIsotopeZ = {};

    std::vector<int>* fMappedProcessID = nullptr;
    std::vector<int>* fMappedVolumeID = nullptr;
    std::vector<float>* fMappedKineticEnergy = nullptr;
    std::vector<ROOT::Math::XYZVector>* fMappedMomentumDirection = nullptr;
    std::vector<std::string>* fMappedHadronicTargetIsotopeName = nullptr;
    std::vector<int>* fMappedHadronicTargetIsotopeA = nullptr;
    std::vector<int>* fMappedHadronicTargetIsotopeZ = nullptr;

    bool fIsView = false;

    TRestGeant4Track* fTrack = nullptr;  //!
    TRestGeant4Event* fEvent = nullptr;  //!
    mutable TRestGeant4Metadata* fMetadata = nullptr;  //!

    template<typename T>
    inline const std::vector<T>& GetVec(const std::vector<T>& local, const std::vector<T>* mapped) const {
        return fIsView && mapped ? *mapped : local;
    }

   public:
    /// \brief Returns associated Geant4 metadata through the owning event/track context.
    TRestGeant4Metadata* GetGeant4Metadata() const;

    inline const TRestGeant4Track* GetTrack() const { return fTrack; }
    inline void SetTrack(TRestGeant4Track* track) {
        fTrack = track;
        fMetadata = nullptr;
    }

    inline const TRestGeant4Event* GetEvent() const { return fEvent; }
    inline void SetEvent(TRestGeant4Event* event) {
        fEvent = event;
        fMetadata = nullptr;
    }

    inline ROOT::Math::XYZVector GetMomentumDirection(size_t n) const { 
      const auto& vec = fIsView && fMappedMomentumDirection ? *fMappedMomentumDirection : fMomentumDirection;
      size_t idx = fIsView ? (fStartIdx + n) : n;
      return idx < vec.size() ? vec[idx] : ROOT::Math::XYZVector(0,0,0);
    }

    inline int GetProcessId(size_t n) const { 
      const auto& vec = fIsView && fMappedProcessID ? *fMappedProcessID : fProcessID;
      size_t idx = fIsView ? (fStartIdx + n) : n;
      return idx < vec.size() ? vec[idx] : 0; // 0 = Init / ID seguro por defecto
    }
    inline int GetProcess(size_t n) const { return GetProcessId(n); }
    inline int GetHitProcess(size_t n) const { return GetProcessId(n); }
    std::string GetProcessName(size_t n) const;

    inline int GetVolumeId(size_t n) const { 
      const auto& vec = fIsView && fMappedVolumeID ? *fMappedVolumeID : fVolumeID;
      size_t idx = fIsView ? (fStartIdx + n) : n;
      return idx < vec.size() ? vec[idx] : 0; 
    }
    inline int GetHitVolume(size_t n) const { return GetVolumeId(n); }
    std::string GetVolumeName(size_t n) const;

    inline bool GetHadronicOk() const { 
      return (fIsView && fMappedHadronicTargetIsotopeName ? fMappedHadronicTargetIsotopeName->size() : fHadronicTargetIsotopeName.size()) > 0; 
    }

    inline std::string GetHadronicTargetIsotopeName(size_t n) const { 
      const auto& vec = fIsView && fMappedHadronicTargetIsotopeName ? *fMappedHadronicTargetIsotopeName : fHadronicTargetIsotopeName;
      size_t idx = fIsView ? (fStartIdx + n) : n;
      return idx < vec.size() ? vec[idx] : "";
    }

    inline int GetHadronicTargetIsotopeA(size_t n) const { 
      const auto& vec = fIsView && fMappedHadronicTargetIsotopeA ? *fMappedHadronicTargetIsotopeA : fHadronicTargetIsotopeA;
      size_t idx = fIsView ? (fStartIdx + n) : n;
      return idx < vec.size() ? vec[idx] : 0;
    }

    inline int GetHadronicTargetIsotopeZ(size_t n) const { 
      const auto& vec = fIsView && fMappedHadronicTargetIsotopeZ ? *fMappedHadronicTargetIsotopeZ : fHadronicTargetIsotopeZ;
      size_t idx = fIsView ? (fStartIdx + n) : n;
      return idx < vec.size() ? vec[idx] : 0;
    }

    /// \brief Clears Geant4-specific hit attributes while preserving base hit container semantics.
    void RemoveG4Hits();

    inline Double_t GetKineticEnergy(size_t n) const { 
      const auto& vec = fIsView && fMappedKineticEnergy ? *fMappedKineticEnergy : fKineticEnergy;
      size_t idx = fIsView ? (fStartIdx + n) : n;
      return idx < vec.size() ? vec[idx] : 0.0;
    }

    /// \brief Accumulates deposited energy for all hits matching a given volume id.
    Double_t GetEnergyInVolume(Int_t volumeID) const;

    /// \brief Returns energy-weighted centroid of hits in a given volume.
    ROOT::Math::XYZVector GetMeanPositionInVolume(Int_t volumeID) const;
    /// \brief Returns first hit position registered in the selected volume.
    ROOT::Math::XYZVector GetFirstPositionInVolume(Int_t volumeID) const;
    /// \brief Returns last hit position registered in the selected volume.
    ROOT::Math::XYZVector GetLastPositionInVolume(Int_t volumeID) const;

    /// \brief Counts hits recorded in a given volume id.
    size_t GetNumberOfHitsInVolume(Int_t volumeID) const;

    // non-const methods (should only be used on the analysis, carefully)
    // REST v3 Field Registry: points to centralized storage.
    std::vector<float>& GetEnergyRef() { return fData->energy; }

    // Constructor
    TRestGeant4Hits();
    TRestGeant4Hits(const TRestGeant4Hits& other);
    TRestGeant4Hits& operator=(const TRestGeant4Hits& other);
    TRestGeant4Hits(TRestHitsData* parentBaseData, 
                std::vector<int>* proc, 
                std::vector<int>* vol, 
                std::vector<float>* kin, 
                std::vector<ROOT::Math::XYZVector>* mom,
                std::vector<std::string>* hadName,
                std::vector<int>* hadA,
                std::vector<int>* hadZ,
                size_t start, 
                size_t size);
    
    virtual void PrintHits(int nHits = -1) const override;
    // Destructor
    virtual ~TRestGeant4Hits();

    // restG4
   public:
    /// \brief Inserts one Geant4 step as a hit entry in the current hit container.
    void InsertStep(const G4Step*);
};
