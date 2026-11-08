#ifndef TRESTTRACK_H
#define TRESTTRACK_H

#include <Math/Vector3D.h>
#include <Rtypes.h>

#include <vector>

#include "TRestHits.h"

class TRestTrackEvent;

/// \class TRestTrack
/// \brief Lightweight view of one track of a TRestTrackEvent: the event and the index of the track in it.
///
/// It stores no hits: every getter reads the shared storage of the event, so a TRestTrack is valid only
/// while the event exists and its tracks are not added or removed. A default constructed track (or one
/// returned when no track matches, e.g. TRestTrackEvent::GetMaxEnergyTrack) has no event: test it with
/// `GetEvent() != nullptr`. On an invalid track the getters return empty values (no hits, 0 energy, ID
/// -1; the hit index of GetSigmaX/Y/Z(localHitIdx) is not checked).
///
/// \code
/// TRestTrack track = event->GetMaxEnergyTrack();
/// if (track.GetEvent()) std::cout << track.GetEnergy() << " " << track.GetLength() << std::endl;
/// \endcode
class TRestTrack {
   protected:
    TRestTrackEvent* fEvent = nullptr;  ///< Event holding the track, nullptr for an invalid track
    std::size_t fTrackIndex = 0;        ///< Index of the track in fEvent

    /// \brief True if there is an event and the index is one of its tracks.
    bool IsValid() const;
    /// \brief Index in the hit storage of the event of the first hit of the track (0 if invalid).
    std::size_t GetHitStart() const;
    /// \brief Number of hits of the track (0 if invalid).
    std::size_t GetHitCount() const;

   public:
    using XYZVector = ROOT::Math::XYZVector;

    /// \brief Invalid track (no event).
    TRestTrack() = default;
    /// \brief View of the track `index` of `event`.
    TRestTrack(TRestTrackEvent* event, std::size_t index);
    ~TRestTrack() = default;

    /// \brief Event holding the track, nullptr for an invalid track.
    const TRestTrackEvent* GetEvent() const { return fEvent; }
    /// \brief Sets the event of the view (the index is kept).
    void SetEvent(TRestTrackEvent* event) { fEvent = event; }
    /// \brief Index of the track in its event.
    std::size_t GetIndex() const { return fTrackIndex; }

    /// \brief Hits of the track: a TRestHits view over the storage of the event (empty if invalid).
    const TRestHits GetHits() const;
    /// \brief Track ID, -1 if invalid.
    int GetTrackID() const;
    /// \brief ID of the track of the previous stage it comes from (0 if none), -1 if invalid.
    int GetParentID() const;
    /// \brief Number of hits, 0 if invalid.
    int GetNumberOfHits() const;
    /// \brief Sum of the hit energies, 0 if invalid.
    double GetEnergy() const;
    /// \brief Sum of the distances between consecutive hits (TRestHits::GetTotalDistance), 0 if invalid.
    double GetLength() const;
    /// \brief Squared distance of the two farthest hits (TRestHits::GetMaximumHitDistance2), 0 if
    /// invalid.
    double GetVolume() const;

    /// \brief Origin and end of the track (TRestHitsAnalysis::GetTrackEnds).
    void GetBoundaries(XYZVector& orig, XYZVector& end) const;

    /// \brief Spatial uncertainty (sigma) along X of each hit of the track (empty if invalid).
    std::vector<float> GetSigmaX() const;
    /// \brief Spatial uncertainty (sigma) along Y of each hit of the track (empty if invalid).
    std::vector<float> GetSigmaY() const;
    /// \brief Spatial uncertainty (sigma) along Z of each hit of the track (empty if invalid).
    std::vector<float> GetSigmaZ() const;
    /// \brief Sigma along X of the hit `localHitIdx` of the track (index inside the track, not checked), 0
    /// if the track is invalid.
    float GetSigmaX(std::size_t localHitIdx) const;
    /// \brief Sigma along Y of the hit `localHitIdx` of the track (index inside the track, not checked), 0
    /// if the track is invalid.
    float GetSigmaY(std::size_t localHitIdx) const;
    /// \brief Sigma along Z of the hit `localHitIdx` of the track (index inside the track, not checked), 0
    /// if the track is invalid.
    float GetSigmaZ(std::size_t localHitIdx) const;

    friend class TRestTrackEvent;
};

#endif
