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

/// \class TRestGeant4Track
/// \brief Holds one Geant4 particle track and its associated step-level hit history.
class TRestGeant4Track {
   protected:
    int fTrackID;
    int fParentID;

    std::string fParticleName; 

    TRestGeant4Hits fHits;

    std::string fCreatorProcess;

    std::vector<int> fSecondaryTrackIDs;

    double fGlobalTimestamp;
    double fTimeOffset = 0;
    double fTimeLength;

    double fInitialKineticEnergy;
    double fLength;

    ROOT::Math::XYZVector fInitialPosition;

    double fWeight = 1;

    TRestGeant4Event* fEvent = nullptr;  //!

   public:
    inline const TRestGeant4Hits& GetHits() const { return fHits; }
    inline TRestGeant4Hits* GetHitsPointer() { return &fHits; }
    inline const TRestGeant4Event* GetEvent() const { return fEvent; }
    /// \brief Returns Geant4 metadata associated with the parent event.
    const TRestGeant4Metadata* GetGeant4Metadata() const;

    inline void SetEvent(TRestGeant4Event* event) {
        fEvent = event;
        fHits.SetEvent(event);
    }
    inline void SetHits(const TRestGeant4Hits& hits) {
        fHits = hits;
        fHits.SetTrack(this);
    }

    inline void SetTimeOffset(const double tOffset) { fTimeOffset = tOffset; }

    inline std::string GetCreatorProcess() const { return fCreatorProcess; }

    inline void AddSecondaryTrackID(Int_t trackID) { fSecondaryTrackIDs.push_back(trackID); }

    /// \brief Returns total hits, optionally filtered by volume id.
    size_t GetNumberOfHits(Int_t volID = -1) const;
    /// \brief Returns non-zero energy hits, optionally filtered by volume id.
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
    /// \brief Returns the volume name where the track starts.
    std::string GetInitialVolume() const;
    /// \brief Returns the volume name where the track ends.
    std::string GetFinalVolume() const;

    inline std::vector<Int_t> GetSecondaryTrackIDs() const { return fSecondaryTrackIDs; }
    std::vector<const TRestGeant4Track*> GetSecondaryTracks() const;
    inline std::vector<const TRestGeant4Track*> GetChildrenTracks() const { return GetSecondaryTracks(); }

    /// \brief Resolves and returns the parent track from the owning event.
    TRestGeant4Track* GetParentTrack() const;

    inline ROOT::Math::XYZVector GetTrackOrigin() const {
        return GetInitialPosition();
    }  // Migrated to XYZVector

    EColor GetParticleColor() const;

    inline Double_t GetEnergyInVolume(Int_t volID) const { return fHits.GetEnergyInVolume(volID); }
    inline ROOT::Math::XYZVector GetMeanPositionInVolume(Int_t volID) const {  // Migrated to XYZVector
        return fHits.GetMeanPositionInVolume(volID);
    }
    inline ROOT::Math::XYZVector GetFirstPositionInVolume(Int_t volID) const {  // Migrated to XYZVector
        return fHits.GetFirstPositionInVolume(volID);
    }
    inline ROOT::Math::XYZVector GetLastPositionInVolume(Int_t volID) const {  // Migrated to XYZVector
        return fHits.GetLastPositionInVolume(volID);
    }

    /// \brief Resolves process id from process name.
    Int_t GetProcessID(const std::string& processName) const;
    /// \brief Resolves process name from process id.
    std::string GetProcessName(Int_t id) const;

    Bool_t ContainsProcessInVolume(Int_t processID, Int_t volumeID = -1) const;
    inline Bool_t ContainsProcess(Int_t processID) const { return ContainsProcessInVolume(processID, -1); }

    /// \brief Returns true if a named process appears in the selected volume.
    Bool_t ContainsProcessInVolume(const std::string& processName, Int_t volumeID = -1) const;
    inline Bool_t ContainsProcess(const std::string& processName) const {
        return ContainsProcessInVolume(processName, -1);
    }

    /// \brief Returns deposited energy in a named volume (optionally including daughters).
    Double_t GetEnergyInVolume(const std::string& volumeName, bool children = false) const;

    /// \brief Returns the process name recorded in the last hit of this track.
    std::string GetLastProcessName() const;

    /// Prints the track information. N number of hits to print, 0 = all
    void PrintTrack(size_t maxHits = 0) const;
    /// \brief Prints track information restricted to the provided volume name set.
    void PrintTrackFilterVolumes(const std::set<std::string>& filterVolumes) const;

    inline void RemoveHits() { fHits.RemoveHits(); }

    // Constructor
    TRestGeant4Track();

    // Destructor
    virtual ~TRestGeant4Track();

    friend class TRestGeant4Event;  // allows TRestGeant4Event to access private members

    // restG4
   public:
    /// \brief Constructs a REST track from a Geant4 track object.
    explicit TRestGeant4Track(const G4Track*);  //!
    /// \brief Updates scalar track properties from Geant4 runtime state.
    void UpdateTrack(const G4Track*);  //!
    /// \brief Adds one Geant4 step to the internal hit collection.
    void InsertStep(const G4Step*);  //!
};

