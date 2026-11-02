#ifndef TRESTGEANT4TRACK_H
#define TRESTGEANT4TRACK_H

#include <Math/Vector3D.h>

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
    TRestGeant4Event* fEvent = nullptr;  //!
    std::size_t fTrackIndex = 0;         //!

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

    int GetTrackID() const;
    int GetParentID() const;
    /// \brief Particle (PDG encoding) and creator process IDs, and their names from the tables of the
    /// TRestGeant4Metadata ("" without metadata)
    int GetParticleID() const;
    int GetCreatorProcessID() const;
    std::string GetParticleName() const;
    std::string GetCreatorProcess() const;
    double GetGlobalTime() const;
    double GetTimeOffset() const;
    double GetTimeLength() const;
    double GetInitialKineticEnergy() const;
    double GetInitialEnergy() const { return GetInitialKineticEnergy(); }
    ROOT::Math::XYZVector GetInitialPosition() const;
    double GetWeight() const;
    double GetTotalEnergy() const;
    double GetDepositedEnergy() const;
    double GetLength() const;

    std::string GetInitialVolume() const;
    std::string GetFinalVolume() const;

    std::vector<int> GetSecondaryTrackIDs() const;
    std::vector<TRestGeant4Track> GetSecondaryTracks() const;
    std::vector<TRestGeant4Track> GetChildrenTracks() const { return GetSecondaryTracks(); }
    std::optional<TRestGeant4Track> GetParentTrack() const;

    ROOT::Math::XYZVector GetTrackOrigin() const { return GetInitialPosition(); }
    EColor GetParticleColor() const;

    size_t GetNumberOfHits(int volID = -1) const;
    size_t GetNumberOfPhysicalHits(int volID = -1) const;

    double GetEnergyInVolume(int volID) const;
    ROOT::Math::XYZVector GetMeanPositionInVolume(int volID) const;
    ROOT::Math::XYZVector GetFirstPositionInVolume(int volID) const;
    ROOT::Math::XYZVector GetLastPositionInVolume(int volID) const;

    int GetHitProcess(size_t hit) const;
    int GetHitVolumeID(size_t hit) const;
    std::string GetHitVolumeName(size_t hit) const;
    double GetHitKineticEnergy(size_t hit) const;
    ROOT::Math::XYZVector GetHitMomentumDirection(size_t hit) const;
    ROOT::Math::XYZVector GetHitPosition(size_t hit) const;
    int GetHitHadronicTargetIsotopeA(size_t hit) const;
    int GetHitHadronicTargetIsotopeZ(size_t hit) const;
    double GetHitEnergy(size_t hit) const;

    int GetProcessID(const std::string& processName) const;
    std::string GetProcessName(int id) const;

    void SetHitEnergy(size_t hit, double energy);

    bool GetHadronicOk() const;
    bool ContainsProcessInVolume(int processID, int volumeID = -1) const;
    bool ContainsProcess(int processID) const { return ContainsProcessInVolume(processID, -1); }

    bool ContainsProcessInVolume(const std::string& processName, int volumeID = -1) const;
    bool ContainsProcess(const std::string& processName) const {
        return ContainsProcessInVolume(processName, -1);
    }

    double GetEnergyInVolume(const std::string& volumeName, bool children = false) const;
    std::string GetLastProcessName() const;

    void PrintTrack(int maxHits = -1) const;
    void PrintTrackFilterVolumes(const std::set<std::string>& filterVolumes) const;
    void RemoveHits();

    friend class TRestGeant4Event;
};

#endif
