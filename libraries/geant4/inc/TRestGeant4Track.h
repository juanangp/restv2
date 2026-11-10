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

/// \class TRestGeant4Track
/// \brief A light view of one track of a TRestGeant4Event (the event and the track index): the data is in
/// the TRestGeant4EventData of the event, so a track is valid only while the event is not modified.
///
/// The hit indices of the methods are relative to the track (0 to GetNumberOfHits() - 1). Units: keV, mm,
/// us. The names need the TRestGeant4Metadata of the event (GetGeant4Metadata).
///
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
class TRestGeant4Track {
   protected:
    /// The event of the track
    TRestGeant4Event* fEvent = nullptr;  //!
    /// Index of the track in the track vectors of the event
    std::size_t fTrackIndex = 0;  //!

    /// \brief True if the event exists, the index is valid and the track vectors are consistent
    bool IsValid() const;
    /// \brief Index of the first hit of the track in the hit vectors of the event (0 if not valid)
    std::size_t GetHitStart() const;
    /// \brief Number of hits of the track (0 if not valid)
    std::size_t GetHitCount() const;

   public:
    /// \brief Track not attached to an event.
    TRestGeant4Track() = default;
    /// \brief View of the track at `index` of `event`.
    TRestGeant4Track(TRestGeant4Event* event, std::size_t index);
    ~TRestGeant4Track() = default;

    const TRestGeant4Event* GetEvent() const { return fEvent; }  ///< The event of the track
    /// \brief The metadata of the event (nullptr without event or metadata)
    const TRestGeant4Metadata* GetGeant4Metadata() const;
    /// \brief Attaches the view to another event (same index).
    void SetEvent(TRestGeant4Event* event);

    /// \brief The hits of the track, a view of the hits of the event (empty if not valid)
    TRestHits GetHits() const;

    int GetTrackID() const;   ///< Geant4 track ID
    int GetParentID() const;  ///< Track ID of the parent (0 for a primary)
    /// \brief Particle (PDG encoding) and creator process IDs, and their names from the tables of the
    /// TRestGeant4Metadata ("" without metadata)
    int GetParticleID() const;
    int GetCreatorProcessID() const;
    std::string GetParticleName() const;
    std::string GetCreatorProcess() const;   ///< Name of the creator process ("" without metadata)
    double GetGlobalTime() const;            ///< Global time at the creation of the track (us)
    double GetTimeOffset() const;            ///< Global time offset after a reset of the global time (us)
    double GetTimeLength() const;            ///< Local (proper) time of the track (us)
    double GetInitialKineticEnergy() const;  ///< Initial kinetic energy (keV)
    double GetInitialEnergy() const { return GetInitialKineticEnergy(); }  ///< As GetInitialKineticEnergy
    ROOT::Math::XYZVector GetInitialPosition() const;  ///< Position where the track was created (mm)
    double GetWeight() const;                          ///< Geant4 weight of the track
    double GetTotalEnergy() const;                     ///< Sum of the energies of its hits (keV)
    double GetDepositedEnergy() const;                 ///< Energy deposited by the track (keV)
    double GetLength() const;                          ///< Length of the track (mm)

    /// \brief Volume name of the first hit ("" without metadata or hits)
    std::string GetInitialVolume() const;
    /// \brief Volume name of the last hit ("" without metadata or hits)
    std::string GetFinalVolume() const;

    /// \brief Track IDs of the children (secondaries) of the track
    std::vector<int> GetSecondaryTrackIDs() const;
    /// \brief Views of the stored children of the track
    std::vector<TRestGeant4Track> GetSecondaryTracks() const;
    /// \brief As GetSecondaryTracks
    std::vector<TRestGeant4Track> GetChildrenTracks() const { return GetSecondaryTracks(); }
    /// \brief View of the parent track (empty for a primary or if it is not stored)
    std::optional<TRestGeant4Track> GetParentTrack() const;

    /// \brief As GetInitialPosition
    ROOT::Math::XYZVector GetTrackOrigin() const { return GetInitialPosition(); }
    /// \brief Colour of the particle in the event drawings: e- red, e+ blue, alpha orange, mu- violet,
    /// gamma green, other gray
    EColor GetParticleColor() const;

    /// \brief Number of hits of the track, in the volume `volID` if not negative
    size_t GetNumberOfHits(int volID = -1) const;
    /// \brief Number of hits with energy, in the volume `volID` if not negative
    size_t GetNumberOfPhysicalHits(int volID = -1) const;

    /// \brief Energy of the hits in the volume (keV)
    double GetEnergyInVolume(int volID) const;
    /// \brief Energy weighted mean position of the hits in the volume (mm, NaN without energy)
    ROOT::Math::XYZVector GetMeanPositionInVolume(int volID) const;
    /// \brief Position of the first hit in the volume (mm, NaN if none)
    ROOT::Math::XYZVector GetFirstPositionInVolume(int volID) const;
    /// \brief Position of the last hit in the volume (mm, NaN if none)
    ROOT::Math::XYZVector GetLastPositionInVolume(int volID) const;

    int GetHitProcess(size_t hit) const;   ///< Process ID of a hit
    int GetHitVolumeID(size_t hit) const;  ///< Volume ID of a hit
    /// \brief Physical volume name of a hit ("" and a warning without metadata)
    std::string GetHitVolumeName(size_t hit) const;
    double GetHitKineticEnergy(size_t hit) const;  ///< Kinetic energy of the particle at a hit (keV)
    ROOT::Math::XYZVector GetHitMomentumDirection(size_t hit) const;  ///< Momentum direction at a hit
    ROOT::Math::XYZVector GetHitPosition(size_t hit) const;           ///< Position of a hit (mm)
    /// \brief A of the hadronic target nucleus of a hit (0 if none; needs storeHadronicTargetInfo)
    int GetHitHadronicTargetIsotopeA(size_t hit) const;
    /// \brief Z of the hadronic target nucleus of a hit (0 if none; needs storeHadronicTargetInfo)
    int GetHitHadronicTargetIsotopeZ(size_t hit) const;
    double GetHitEnergy(size_t hit) const;  ///< Energy deposited at a hit (keV)

    /// \brief Process ID of a process name (-1 if not found or without metadata)
    int GetProcessID(const std::string& processName) const;
    /// \brief Process name of a process ID ("" and a warning if not found)
    std::string GetProcessName(int id) const;

    /// \brief Sets the energy of a hit (keV), e.g. after quenching
    void SetHitEnergy(size_t hit, double energy);

    /// \brief True if the event has the hadronic target information (storeHadronicTargetInfo)
    bool GetHadronicOk() const;
    /// \brief True if a hit of the track has the process, in the volume `volumeID` if not negative
    bool ContainsProcessInVolume(int processID, int volumeID = -1) const;
    /// \brief True if a hit of the track has the process
    bool ContainsProcess(int processID) const { return ContainsProcessInVolume(processID, -1); }

    /// \brief True if a hit of the track has the process (by name, false without metadata), in the volume
    /// `volumeID` if not negative
    bool ContainsProcessInVolume(const std::string& processName, int volumeID = -1) const;
    /// \brief True if a hit of the track has the process (by name)
    bool ContainsProcess(const std::string& processName) const {
        return ContainsProcessInVolume(processName, -1);
    }

    /// \brief Energy of the hits in a volume by its physical name (keV, 0 without metadata)
    /// \param volumeName Physical volume name.
    /// \param children If true, the energy of all the stored descendants is added.
    double GetEnergyInVolume(const std::string& volumeName, bool children = false) const;
    /// \brief Process name of the last hit ("" without metadata or hits)
    std::string GetLastProcessName() const;

    /// \brief Prints the track and its hits (maxHits: maximum number of hits printed, -1 all)
    void PrintTrack(int maxHits = -1) const;
    /// \brief Prints the track if it has a hit in one of the volumes (physical names)
    void PrintTrackFilterVolumes(const std::set<std::string>& filterVolumes) const;

    friend class TRestGeant4Event;
};

#endif
