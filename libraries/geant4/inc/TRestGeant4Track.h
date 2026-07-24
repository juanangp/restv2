#pragma once

#include <Math/Vector3D.h>
#include <TColor.h>

#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "TRestGeant4Hits.h"

class TRestGeant4Event;
class TRestGeant4Metadata;
class G4Track;
class G4Step;

class TRestGeant4Track {
   protected:
    Int_t fTrackID;
    Int_t fParentID;

    std::string fParticleName;

    TRestGeant4Hits fHits;

    std::string fCreatorProcess;

    std::vector<Int_t> fSecondaryTrackIDs;

    Double_t fGlobalTimestamp;
    Double_t fTimeOffset = 0;
    Double_t fTimeLength;

    Double_t fInitialKineticEnergy;
    Double_t fLength;

    ROOT::Math::XYZVector fInitialPosition;

    Double_t fWeight = 1;

    TRestGeant4Event* fEvent = nullptr;  //!

   public:
    inline const TRestGeant4Hits& GetHits() const { return fHits; }
    inline TRestGeant4Hits* GetHitsPointer() { return &fHits; }
    inline const TRestGeant4Event* GetEvent() const { return fEvent; }
    const TRestGeant4Metadata* GetGeant4Metadata() const;

    inline void SetEvent(TRestGeant4Event* event) { fEvent = event; }
    inline void SetHits(const TRestGeant4Hits& hits) {
        fHits = hits;
        fHits.SetTrack(this);
    }

    inline void SetTimeOffset(const double tOffset) { fTimeOffset = tOffset; }

    inline std::string GetCreatorProcess() const { return fCreatorProcess; }

    inline void AddSecondaryTrackID(Int_t trackID) { fSecondaryTrackIDs.push_back(trackID); }

    size_t GetNumberOfHits(Int_t volID = -1) const;
    size_t GetNumberOfPhysicalHits(Int_t volID = -1) const;

    inline Int_t GetTrackID() const { return fTrackID; }
    inline Int_t GetParentID() const { return fParentID; }
    inline std::string GetParticleName() const { return fParticleName; }
    inline Double_t GetGlobalTime() const { return fGlobalTimestamp; }
    inline Double_t GetTimeOffset() const { return fTimeOffset; }
    inline Double_t GetTimeLength() const { return fTimeLength; }
    inline Double_t GetInitialKineticEnergy() const { return fInitialKineticEnergy; }
    inline ROOT::Math::XYZVector GetInitialPosition() const { return fInitialPosition; }
    inline Double_t GetWeight() const { return fWeight; }
    inline Double_t GetTotalEnergy() const { return fHits.GetTotalEnergy(); }
    inline Double_t GetLength() const { return fLength; }
    std::string GetInitialVolume() const;
    std::string GetFinalVolume() const;

    inline std::vector<Int_t> GetSecondaryTrackIDs() const { return fSecondaryTrackIDs; }
    std::vector<const TRestGeant4Track*> GetSecondaryTracks() const;
    inline std::vector<const TRestGeant4Track*> GetChildrenTracks() const { return GetSecondaryTracks(); }

    TRestGeant4Track* GetParentTrack() const;

    inline ROOT::Math::XYZVector GetTrackOrigin() const {
        return GetInitialPosition();
    }  // Migrado a XYZVector

    EColor GetParticleColor() const;

    inline Double_t GetEnergyInVolume(Int_t volID) const { return fHits.GetEnergyInVolume(volID); }
    inline ROOT::Math::XYZVector GetMeanPositionInVolume(Int_t volID) const {  // Migrado a XYZVector
        return fHits.GetMeanPositionInVolume(volID);
    }
    inline ROOT::Math::XYZVector GetFirstPositionInVolume(Int_t volID) const {  // Migrado a XYZVector
        return fHits.GetFirstPositionInVolume(volID);
    }
    inline ROOT::Math::XYZVector GetLastPositionInVolume(Int_t volID) const {  // Migrado a XYZVector
        return fHits.GetLastPositionInVolume(volID);
    }

    Int_t GetProcessID(const std::string& processName) const;  // Migrado a std::string
    std::string GetProcessName(Int_t id) const;                // Migrado a std::string

    Bool_t ContainsProcessInVolume(Int_t processID, Int_t volumeID = -1) const;
    inline Bool_t ContainsProcess(Int_t processID) const { return ContainsProcessInVolume(processID, -1); }

    Bool_t ContainsProcessInVolume(const std::string& processName,
                                   Int_t volumeID = -1) const;             // Migrado a std::string
    inline Bool_t ContainsProcess(const std::string& processName) const {  // Migrado a std::string
        return ContainsProcessInVolume(processName, -1);
    }

    Double_t GetEnergyInVolume(const std::string& volumeName,
                               bool children = false) const;  // Migrado a std::string

    std::string GetLastProcessName() const;  // Migrado a std::string

    /// Prints the track information. N number of hits to print, 0 = all
    void PrintTrack(size_t maxHits = 0) const;
    void PrintTrackFilterVolumes(const std::set<std::string>& filterVolumes) const;

    inline void RemoveHits() { fHits.RemoveHits(); }

    // Constructor
    TRestGeant4Track();

    // Destructor
    virtual ~TRestGeant4Track();

    friend class TRestGeant4Event;  // allows TRestGeant4Event to access private members

    // restG4
   public:
    explicit TRestGeant4Track(const G4Track*);  //!
    void UpdateTrack(const G4Track*);           //!
    void InsertStep(const G4Step*);             //!
};
