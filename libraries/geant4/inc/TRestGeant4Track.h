#ifndef TRESTGEANT4TRACK_H
#define TRESTGEANT4TRACK_H

#include <Math/Vector3D.h>
#include <TColor.h>

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "TRestHits.h"

class TRestGeant4Event;
class TRestGeant4Metadata;

class TRestGeant4Track {
   protected:
    TRestGeant4Event* fEvent = nullptr; //!
    std::size_t fTrackIndex = 0;        //!

    bool IsValid() const;
    std::size_t GetHitStart() const;
    std::size_t GetHitCount() const;

   public:
    TRestGeant4Track() = default;
    TRestGeant4Track(TRestGeant4Event* event, std::size_t index);
    ~TRestGeant4Track() = default;

    const TRestGeant4Event* GetEvent() const { return fEvent; }
    const TRestGeant4Metadata* GetGeant4Metadata() const;
    void SetEvent(TRestGeant4Event* event);

    TRestHits GetHits() const;

    Int_t GetTrackID() const;
    Int_t GetParentID() const;
    const std::string& GetParticleName() const;
    const std::string& GetCreatorProcess() const;
    Double_t GetGlobalTime() const;
    Double_t GetTimeOffset() const;
    Double_t GetTimeLength() const;
    Double_t GetInitialKineticEnergy() const;
    Double_t GetInitialEnergy() const { return GetInitialKineticEnergy(); }
    ROOT::Math::XYZVector GetInitialPosition() const;
    Double_t GetWeight() const;
    Double_t GetTotalEnergy() const;
    Double_t GetDepositedEnergy() const;
    Double_t GetLength() const;

    std::string GetInitialVolume() const;
    std::string GetFinalVolume() const;

    std::vector<Int_t> GetSecondaryTrackIDs() const;
    std::vector<TRestGeant4Track> GetSecondaryTracks() const;
    std::vector<TRestGeant4Track> GetChildrenTracks() const { return GetSecondaryTracks(); }
    std::optional<TRestGeant4Track> GetParentTrack() const;

    ROOT::Math::XYZVector GetTrackOrigin() const { return GetInitialPosition(); }
    EColor GetParticleColor() const;

    size_t GetNumberOfHits(Int_t volID = -1) const;
    size_t GetNumberOfPhysicalHits(Int_t volID = -1) const;

    Double_t GetEnergyInVolume(Int_t volID) const;
    ROOT::Math::XYZVector GetMeanPositionInVolume(Int_t volID) const;
    ROOT::Math::XYZVector GetFirstPositionInVolume(Int_t volID) const;
    ROOT::Math::XYZVector GetLastPositionInVolume(Int_t volID) const;

    Int_t GetHitProcess(size_t hit) const;
    Int_t GetHitVolumeID(size_t hit) const;
    std::string GetHitVolumeName(size_t hit) const;
    Double_t GetHitKineticEnergy(size_t hit) const;
    ROOT::Math::XYZVector GetHitMomentumDirection(size_t hit) const;
    ROOT::Math::XYZVector GetHitPosition(size_t hit) const;
    std::string GetHitHadronicTargetIsotopeName(size_t hit) const;
    Int_t GetHitHadronicTargetIsotopeA(size_t hit) const;
    Int_t GetHitHadronicTargetIsotopeZ(size_t hit) const;
    Double_t GetHitEnergy(size_t hit) const;

    Int_t GetProcessID(const std::string& processName) const;
    std::string GetProcessName(Int_t id) const;

    void SetHitEnergy(size_t hit, Double_t energy);

    Bool_t GetHadronicOk() const;
    Bool_t ContainsProcessInVolume(Int_t processID, Int_t volumeID = -1) const;
    Bool_t ContainsProcess(Int_t processID) const {
        return ContainsProcessInVolume(processID, -1);
    }

    Bool_t ContainsProcessInVolume(const std::string& processName, Int_t volumeID = -1) const;
    Bool_t ContainsProcess(const std::string& processName) const {
        return ContainsProcessInVolume(processName, -1);
    }

    Double_t GetEnergyInVolume(const std::string& volumeName, bool children = false) const;
    std::string GetLastProcessName() const;

    void PrintTrack(int maxHits = -1) const;
    void PrintTrackFilterVolumes(const std::set<std::string>& filterVolumes) const;
    void RemoveHits();

    friend class TRestGeant4Event;
};

#endif
