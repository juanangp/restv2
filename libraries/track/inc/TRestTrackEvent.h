#ifndef TRESTTRACKEVENT_H
#define TRESTTRACKEVENT_H

#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "Math/Vector3Dfwd.h"
#include "TRestEvent.h"
#include "TRestHits.h"

class TRestTrack;

/// \struct TRestTrackData
/// \brief Storage for every track's hits (concatenated back-to-back) plus
/// per-hit spatial uncertainty (volume hits) and per-track identity.
struct TRestTrackData {
    TRestHitsData hits;                         ///< x, y, z, time, energy, type for all tracks
    std::vector<float> sigmaX, sigmaY, sigmaZ;  ///< per-hit volume uncertainty (parallel to `hits`)
    std::vector<int> trackID, parentID;         ///< per-track identity
    std::vector<int> trackNHits;                ///< hits count per track (hits stored contiguously)

    /// \brief Removes all the hits and tracks.
    void clear() {
        hits.clear();
        sigmaX.clear();
        sigmaY.clear();
        sigmaZ.clear();
        trackID.clear();
        parentID.clear();
        trackNHits.clear();
    }
};

/// \class TRestTrackEvent
/// \brief Event holding a collection of particle tracks. Each track is a
/// contiguous run of volume hits (position + energy + time + type + sigma)
/// inside the shared `fData` storage; no per-track object is allocated.
///
/// An event holds the tracks of one processing stage only: a track process writes its tracks to a new
/// event instead of adding them as children of the input tracks. Each track is self-contained (it stores
/// its own representation of the hits, e.g. the reduced nodes), so it does not need the hits or the
/// tracks of the previous stages to be stored. The parent ID is the ID of the track it comes from in the
/// input event of the stage (0 if none), for reference only.
///
/// The tracks are addressed by their index in the event (0 to GetNumberOfTracks() - 1) or by their ID
/// (GetTrackById); the hits of a track by their index inside the track. TRestTrack is a view of one track.
/// The type of a track is the type of its hits (TRestHitsData::REST_HitType, e.g. XZ and YZ for strip
/// readouts, XYZ for 3D readouts); the coordinates a hit does not define are NaN.
///
/// Tree branches: fHitX, fHitY, fHitZ, fHitTime, fHitEnergy, fHitType, fSigmaX, fSigmaY, fSigmaZ (one
/// entry per hit, the tracks back-to-back), fTrackID, fParentID and fTrackNHits (one entry per track).
///
/// Building an event, hit by hit or from a TRestHits:
/// \code
/// int index = event.AddTrack(trackID, parentID);
/// event.AddHitToTrack(position, energy, time, type, sigma);  // repeat for every hit, in order
/// event.AddTrack(trackID2, parentID2, hits, sigmas);
/// \endcode
///
/// Some getters take a projection `option`, kept from legacy for strip readouts, with the same meaning in
/// all of them: "X" selects only the XZ tracks, "Y" only the YZ tracks and "XYZ" only the XYZ (3D) tracks;
/// any other option selects no track. The empty option "" is the XYZ tracks in GetMaxEnergyTrack and the
/// methods based on it, and all the tracks in GetNumberOfTracks and GetEnergy (as without option).
///
/// \author Javier Gracia
/// \author JuanAn García <juanangp@unizar.es>
class TRestTrackEvent : public TRestEvent {
    friend class TRestTrack;

   public:
    using REST_HitType = TRestHitsData::REST_HitType;  ///< Hit type (coordinates the hit defines)
    using XYZVector = ROOT::Math::XYZVector;           ///< 3D vector of positions and sigmas

   private:
    TRestTrackData fData;

    /// Lightweight per-track views over fData.hits (full range). Rebuilt
    /// whenever the underlying storage changes.
    mutable std::vector<TRestHits> fTrackHits;  //!

    /// Persisted branch pointers (ROOT requires T** for class/vector branches).
    std::vector<float>* fPtrX = nullptr;         //!
    std::vector<float>* fPtrY = nullptr;         //!
    std::vector<float>* fPtrZ = nullptr;         //!
    std::vector<float>* fPtrTime = nullptr;      //!
    std::vector<float>* fPtrEnergy = nullptr;    //!
    std::vector<int>* fPtrType = nullptr;        //!
    std::vector<float>* fPtrSigmaX = nullptr;    //!
    std::vector<float>* fPtrSigmaY = nullptr;    //!
    std::vector<float>* fPtrSigmaZ = nullptr;    //!
    std::vector<int>* fPtrTrackID = nullptr;     //!
    std::vector<int>* fPtrParentID = nullptr;    //!
    std::vector<int>* fPtrTrackNHits = nullptr;  //!

    /// \brief Returns the first global hit index belonging to track n.
    int GetTrackStartIndex(int n) const;

    /// \brief Returns the storage index of the track with the given ID, or -1.
    int FindTrackIndexById(int id) const;

    /// \brief True if track `n` is of the projection `option`: XZ ("X"), YZ ("Y") or XYZ ("" or "XYZ");
    /// false for any other option.
    bool MatchesOption(int n, const std::string& option) const;

    /// \brief Draws the tracks in the pad, see DrawEvent ("tracks").
    void DrawTracks(TVirtualPad* pad) const;

   public:
    std::string GetClassName() const override { return "TRestTrackEvent"; }

    /// \brief Resets the event header and removes all the tracks.
    void Initialize() override {
        TRestEvent::Initialize();
        fData.clear();
        RebuildTrackViews();
    }

    /// \brief Creates the branches of the event in `tree` (see the class description).
    void CreateBranches(TTree* tree) override {
        TRestEvent::CreateBranches(tree);
        tree->Branch("fHitX", &fData.hits.x);
        tree->Branch("fHitY", &fData.hits.y);
        tree->Branch("fHitZ", &fData.hits.z);
        tree->Branch("fHitTime", &fData.hits.time);
        tree->Branch("fHitEnergy", &fData.hits.energy);
        tree->Branch("fHitType", &fData.hits.type);
        tree->Branch("fSigmaX", &fData.sigmaX);
        tree->Branch("fSigmaY", &fData.sigmaY);
        tree->Branch("fSigmaZ", &fData.sigmaZ);
        tree->Branch("fTrackID", &fData.trackID);
        tree->Branch("fParentID", &fData.parentID);
        tree->Branch("fTrackNHits", &fData.trackNHits);
    }

    /// \brief Binds the branches of `tree` to the storage of the event, to read it.
    void SetBranchAddresses(TTree* tree) override {
        TRestEvent::SetBranchAddresses(tree);

        fPtrX = &fData.hits.x;
        fPtrY = &fData.hits.y;
        fPtrZ = &fData.hits.z;
        fPtrTime = &fData.hits.time;
        fPtrEnergy = &fData.hits.energy;
        fPtrType = &fData.hits.type;
        fPtrSigmaX = &fData.sigmaX;
        fPtrSigmaY = &fData.sigmaY;
        fPtrSigmaZ = &fData.sigmaZ;
        fPtrTrackID = &fData.trackID;
        fPtrParentID = &fData.parentID;
        fPtrTrackNHits = &fData.trackNHits;

        BindBranch(tree, "fHitX", &fPtrX);
        BindBranch(tree, "fHitY", &fPtrY);
        BindBranch(tree, "fHitZ", &fPtrZ);
        BindBranch(tree, "fHitTime", &fPtrTime);
        BindBranch(tree, "fHitEnergy", &fPtrEnergy);
        BindBranch(tree, "fHitType", &fPtrType);
        BindBranch(tree, "fSigmaX", &fPtrSigmaX);
        BindBranch(tree, "fSigmaY", &fPtrSigmaY);
        BindBranch(tree, "fSigmaZ", &fPtrSigmaZ);
        BindBranch(tree, "fTrackID", &fPtrTrackID);
        BindBranch(tree, "fParentID", &fPtrParentID);
        BindBranch(tree, "fTrackNHits", &fPtrTrackNHits);
    }

    /// \brief Rebuilds the per-track hit views from the current fData layout.
    void RebuildTrackViews() {
        fTrackHits.clear();
        fTrackHits.reserve(fData.trackNHits.size());
        int start = 0;
        for (int n : fData.trackNHits) {
            fTrackHits.emplace_back(&fData.hits, start, n);
            start += n;
        }
    }
    /// \brief Rebuilds the track views after the storage was filled from a tree.
    void RefreshViews() const override { const_cast<TRestTrackEvent*>(this)->RebuildTrackViews(); }

    /// \brief Copies the header and, if `other` is a TRestTrackEvent, its tracks.
    void CopyFrom(const TRestEvent* other) override {
        TRestEvent::CopyFrom(other);
        auto* source = dynamic_cast<const TRestTrackEvent*>(other);
        if (source) {
            fData = source->fData;
            RebuildTrackViews();
        }
    }

    // Low-level: int idx = event.AddTrack(trackID, parentID);
    //            event.AddHitToTrack(x, y, z, en, t, type);  // repeat for every hit, in order
    /// \brief Appends an empty track; its hits are added with AddHitToTrack.
    /// \return Index of the new track.
    int AddTrack(int trackID, int parentID);
    /// \brief Appends a hit to the last track added (call AddTrack first).
    /// \param sigma Spatial uncertainty of the hit along X, Y and Z.
    void AddHitToTrack(const XYZVector& position, double energy, double time, REST_HitType type,
                       const XYZVector& sigma = XYZVector(0, 0, 0));

    // Adds a full track in one call, taking an already built hits container as
    // the track representation.
    /// \brief Appends a track with a copy of `hits` (position, energy, time and type of each hit).
    /// \param sigmas Sigma of each hit; the hits without an entry (e.g. if it is empty) get sigma 0.
    /// \return Index of the new track.
    int AddTrack(int trackID, int parentID, const TRestHits& hits, const std::vector<XYZVector>& sigmas = {});

    /// \brief Removes the track of index `n` and its hits.
    void RemoveTrack(int n);
    /// \brief Removes all the tracks.
    inline void RemoveTracks() {
        fData.clear();
        RebuildTrackViews();
    }

    /// \brief Number of tracks of the event (all the types).
    inline int GetNumberOfTracks() const { return (int)fTrackHits.size(); }
    /// \brief Number of XZ ("X"), YZ ("Y") or XYZ ("XYZ") tracks, all of them for "" (as
    /// GetNumberOfTracks()); 0 for any other option.
    int GetNumberOfTracks(const std::string& option) const;

    /// \brief Track of index `n` (not checked: the view is invalid if there is no such track).
    TRestTrack GetTrack(std::size_t n);
    /// \copydoc GetTrack(std::size_t)
    TRestTrack GetTrack(std::size_t n) const;
    /// \brief Track with the given ID, an invalid track (no event) if there is none.
    TRestTrack GetTrackById(int id);
    /// \copydoc GetTrackById(int)
    TRestTrack GetTrackById(int id) const;

    /// \brief Most energetic XZ track, an invalid track (no event) if there is none.
    TRestTrack GetMaxEnergyTrackInX();
    /// \brief Most energetic YZ track, an invalid track (no event) if there is none.
    TRestTrack GetMaxEnergyTrackInY();
    /// \brief Most energetic track among the XZ ("X"), YZ ("Y") or XYZ ("" or "XYZ") tracks, whatever its
    /// energy (also <= 0); an invalid track (no event) if there is none or for any other option. For any
    /// track type use TRestTrackAnalysisProcess.
    TRestTrack GetMaxEnergyTrack(const std::string& option = "");
    /// \brief Most energetic track with an ID different from GetMaxEnergyTrack(option), among the tracks
    /// of the same projection (XZ for "X", YZ for "Y", XYZ for "" or "XYZ"); an invalid track if there is
    /// none.
    TRestTrack GetSecondMaxEnergyTrack(const std::string& option = "");

    /// \brief Volume (TRestTrack::GetVolume) of GetMaxEnergyTrack(option), 0 if there is none.
    double GetMaxEnergyTrackVolume(const std::string& option = "");
    /// \brief Length (TRestTrack::GetLength) of GetMaxEnergyTrack(option), 0 if there is none.
    double GetMaxEnergyTrackLength(const std::string& option = "");
    /// \brief Energy of all the tracks.
    double GetEnergy() const;
    /// \brief Energy of the XZ ("X"), YZ ("Y") or XYZ ("XYZ") tracks, of all of them for "" (as
    /// GetEnergy()); 0 for any other option.
    double GetEnergy(const std::string& option) const;

    /// \brief ID of the track of index `n`.
    inline int GetTrackID(int n) const { return fData.trackID[n]; }
    /// \brief Parent ID of the track of index `n` (0 if none).
    inline int GetParentID(int n) const { return fData.parentID[n]; }
    /// \brief Sum of the hit energies of track `n`.
    inline double GetTrackEnergy(int n) const { return fTrackHits[n].GetTotalEnergy(); }
    /// \brief Sum of the distances between consecutive hits of track `n`.
    inline double GetTrackLength(int n) const { return fTrackHits[n].GetTotalDistance(); }
    /// \brief Squared distance of the two farthest hits of track `n`.
    inline double GetTrackVolume(int n) const { return fTrackHits[n].GetMaximumHitDistance2(); }
    /// \brief Mean position of the hits of track `n` (TRestHits::GetMeanPosition).
    inline XYZVector GetTrackMeanPosition(int n) const { return fTrackHits[n].GetMeanPosition(); }
    /// \brief Number of hits of track `n`.
    inline int GetNumberOfHits(int n) const { return (int)fTrackHits[n].GetNumberOfHits(); }

    /// \brief True if all the hits of track `n` are XY (false for a track without hits).
    inline bool isXY(int n) const { return fTrackHits[n].areXY(); }
    /// \brief True if all the hits of track `n` are XZ (false for a track without hits).
    inline bool isXZ(int n) const { return fTrackHits[n].areXZ(); }
    /// \brief True if all the hits of track `n` are YZ (false for a track without hits).
    inline bool isYZ(int n) const { return fTrackHits[n].areYZ(); }
    /// \brief True if all the hits of track `n` are XYZ (TRestHits::areXYZ).
    inline bool isXYZ(int n) const { return fTrackHits[n].areXYZ(); }
    /// \brief True if every track of the event is XYZ (also for an event without tracks).
    bool isXYZ() const;

    /// \brief Sigma along X of the hit `hit` (index inside the track) of track `n`.
    inline double GetSigmaX(int n, int hit) const { return fData.sigmaX[GetTrackStartIndex(n) + hit]; }
    /// \brief Sigma along Y of the hit `hit` of track `n`.
    inline double GetSigmaY(int n, int hit) const { return fData.sigmaY[GetTrackStartIndex(n) + hit]; }
    /// \brief Sigma along Z of the hit `hit` of track `n`.
    inline double GetSigmaZ(int n, int hit) const { return fData.sigmaZ[GetTrackStartIndex(n) + hit]; }
    /// \brief Sigmas along X, Y and Z of the hit `hit` of track `n`.
    inline XYZVector GetSigma(int n, int hit) const {
        return {GetSigmaX(n, hit), GetSigmaY(n, hit), GetSigmaZ(n, hit)};
    }
    /// \brief sqrt(sigmaX² + sigmaY² + sigmaZ²) of the hit `hit` of track `n`.
    inline double GetClusterSize(int n, int hit) const {
        return std::sqrt(GetSigmaX(n, hit) * GetSigmaX(n, hit) + GetSigmaY(n, hit) * GetSigmaY(n, hit) +
                         GetSigmaZ(n, hit) * GetSigmaZ(n, hit));
    }
    /// \brief sqrt(sigmaX² + sigmaY²) of the hit `hit` of track `n`.
    inline double GetXYSize(int n, int hit) const {
        return std::sqrt(GetSigmaX(n, hit) * GetSigmaX(n, hit) + GetSigmaY(n, hit) * GetSigmaY(n, hit));
    }

    /// \brief Number of hits of all the tracks.
    int GetTotalHits() const;

    /// \brief Swaps the hits `i` and `j` (indices inside the track) of track `n`.
    void SwapTrackHits(int n, int i, int j);
    /// \brief Sorts the hits of track `n`.
    /// \param compareCondition Comparison of two hit indices inside the track (true if the first goes
    /// before); by default, increasing Z.
    void SortTrackHits(int n, std::function<bool(int, int)> compareCondition = nullptr);
    /// \brief Shuffles the hits of track `n` with `NLoop` random swaps.
    void ShuffleTrackHits(int n, int NLoop);
    /// \brief Removes the hit `hitIdx` (index inside the track) of track `n`.
    void RemoveTrackHit(int n, int hitIdx);
    /// \brief Merges the hit `j` of track `n` into the hit `i`: energy weighted mean of position, time and
    /// sigmas, summed energy; then the hit `j` is removed (also if both energies sum 0, without merging).
    void MergeTrackHits(int n, int i, int j);

    /// \brief Relative Z position of the half energy integral of the most energetic XZ and YZ tracks
    /// together (legacy, strip readouts), to tell upwards from downwards tracks: (Zfirst - Zhalf) /
    /// (Zfirst - Zlast) with the hits sorted by Z, 0 for no Z extent.
    /// \return -1 if there is no XZ or no YZ track.
    double GetMaxTrackRelativeZ();
    /// \brief Origin and end of the most energetic XZ and YZ tracks together (legacy, strip readouts): X
    /// from the XZ track, Y from the YZ track and Z the mean of both, each from TRestTrack::GetBoundaries.
    /// `orig` and `end` are not modified if there is no XZ or no YZ track.
    void GetMaxTrackBoundaries(XYZVector& orig, XYZVector& end);

    /// \brief Reduces the hits of track `trackIndex` into `nodes` clusters using k-means. The clustered
    /// track is added to `output` (the event of the next stage), with this track as parent.
    /// The initial nodes are equally spaced along the hits order; the new track ID is the number of tracks of
    /// `output` + 1, and the nodes get the hit type of the first hit and no time.
    /// \param maxIt Maximum k-means iterations.
    /// \param fixBoundaries If true, the first and last nodes stay at their initial positions (the first
    /// and last hits), with no energy.
    /// \return Index of the clustered track in `output`, or -1 (nodes < 2, no hits or no result).
    int KMeansClustering(int trackIndex, int nodes, TRestTrackEvent& output, int maxIt = 100,
                         bool fixBoundaries = false) const;

    /// \brief Prints the index, ID and parent ID of each track.
    void PrintOnlyTracks() const;
    /// \brief Prints the header, the total number of tracks, the number of XYZ, XZ and YZ tracks and each
    /// track (PrintTrack).
    void PrintTrackEvent(bool fullInfo) const;
    /// \brief Prints the ID, type, energy, length, mean position and number of hits of track `n` and, with
    /// fullInfo, its hits.
    void PrintTrack(int n, bool fullInfo = true) const;
    /// \brief PrintTrackEvent without the hits.
    inline void PrintEvent() const override { PrintTrackEvent(false); }

    /// \brief Draws the tracks, one row of pads per option. Options separated by ':':
    /// - tracks: hits as markers in the projections (XZ, YZ, XY) the hits define, plus a 3D view if
    ///   there are XYZ hits (1D hits: energy against each coordinate). Each track gets a colour, a
    ///   marker size growing with the hit energy and, below 100 hits, a line joining its hits, drawn
    ///   over the hits of every track;
    /// - hits(rootOption)[pitch]: hits of all the tracks as histograms weighted by energy
    ///   (TRestHits::DrawHistograms);
    /// - print: PrintEvent.
    /// Default: "tracks".
    TPad* DrawEvent(const std::string& option = "") const override;

    TRestTrackEvent() = default;
    /// \brief Copies the header and the tracks, with the views over the storage of the copy (the implicit
    /// copy would leave them over the storage of `other`).
    TRestTrackEvent(const TRestTrackEvent& other) : TRestEvent(other), fData(other.fData) {
        RebuildTrackViews();
    }
    /// \brief Copies the header and the tracks (see the copy constructor).
    TRestTrackEvent& operator=(const TRestTrackEvent& other) {
        if (this != &other) {
            TRestEvent::operator=(other);
            fData = other.fData;
            RebuildTrackViews();
        }
        return *this;
    }
    ~TRestTrackEvent() override = default;
};

#endif
