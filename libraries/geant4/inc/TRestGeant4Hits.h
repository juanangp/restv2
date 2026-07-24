#pragma once

#include <TRestHits.h>
#include <iostream>
#include <vector>
#include <string>

// Modern ROOT vector header adopted by REST-for-Physics
#include <Math/Vector3D.h>

#include "TRestGeant4Metadata.h"

class G4Step;
class TRestGeant4Track;
class TRestGeant4Event;

class TRestGeant4Hits : public TRestHits {
   protected:
    std::vector<int> fProcessID = {};
    std::vector<int> fVolumeID = {};
    std::vector<float> fKineticEnergy = {};
    std::vector<ROOT::Math::XYZVector> fMomentumDirection = {}; 

    std::vector<std::string> fHadronicTargetIsotopeName = {};
    std::vector<int> fHadronicTargetIsotopeA = {};
    std::vector<int> fHadronicTargetIsotopeZ = {};

    TRestGeant4Track* fTrack = nullptr;  //!
    TRestGeant4Event* fEvent = nullptr;  //!

   public:
    TRestGeant4Metadata* GetGeant4Metadata() const;

    inline const TRestGeant4Track* GetTrack() const { return fTrack; }
    inline void SetTrack(TRestGeant4Track* track) { fTrack = track; }

    inline const TRestGeant4Event* GetEvent() const { return fEvent; }
    inline void SetEvent(TRestGeant4Event* event) { fEvent = event; }

    inline ROOT::Math::XYZVector GetMomentumDirection(size_t n) const { return fMomentumDirection[n]; }

    inline int GetProcessId(size_t n) const { return fProcessID[n]; }
    inline int GetProcess(size_t n) const { return GetProcessId(n); }
    inline int GetHitProcess(size_t n) const { return GetProcessId(n); }
    std::string GetProcessName(size_t n) const; 

    inline int GetVolumeId(size_t n) const { return fVolumeID[n]; }
    inline int GetHitVolume(size_t n) const { return GetVolumeId(n); }
    std::string GetVolumeName(size_t n) const;
    inline bool GetHadronicOk() const { return fHadronicTargetIsotopeName.size() > 0; }
    inline std::string GetHadronicTargetIsotopeName(size_t n) const { return fHadronicTargetIsotopeName[n]; }
    inline int GetHadronicTargetIsotopeA(size_t n) const { return fHadronicTargetIsotopeA[n]; }
    inline int GetHadronicTargetIsotopeZ(size_t n) const { return fHadronicTargetIsotopeZ[n]; }

    void RemoveG4Hits();

    inline Double_t GetKineticEnergy(size_t n) const { return fKineticEnergy[n]; }

    Double_t GetEnergyInVolume(Int_t volumeID) const;

    ROOT::Math::XYZVector GetMeanPositionInVolume(Int_t volumeID) const; 
    ROOT::Math::XYZVector GetFirstPositionInVolume(Int_t volumeID) const; 
    ROOT::Math::XYZVector GetLastPositionInVolume(Int_t volumeID) const; 

    size_t GetNumberOfHitsInVolume(Int_t volumeID) const;

    // non-const methods (should only be used on the analysis, carefully)
    // Corrección para REST v3 Field Registry: apunta al almacenamiento centralizado
    std::vector<float>& GetEnergyRef() { return fData->energy; }

    // Constructor
    TRestGeant4Hits();
    // Destructor
    virtual ~TRestGeant4Hits();

    ClassDef(TRestGeant4Hits, 9);  

    // restG4
   public:
    void InsertStep(const G4Step*);
};
