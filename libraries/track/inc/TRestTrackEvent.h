#ifndef TRESTTRACKEVENT_H
#define TRESTTRACKEVENT_H

#include <cmath>
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
class TRestTrackEvent : public TRestEvent {
    friend class TRestTrack;

   public:
    using REST_HitType = TRestHitsData::REST_HitType;
    using XYZVector = ROOT::Math::XYZVector;

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

    /// \brief Draws the tracks in the pad, see DrawEvent ("tracks").
    void DrawTracks(TVirtualPad* pad) const;

   public:
    std::string GetClassName() const override { return "TRestTrackEvent"; }

    void Initialize() override {
        TRestEvent::Initialize();
        fData.clear();
        RebuildTrackViews();
    }

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
    void RefreshViews() const override { const_cast<TRestTrackEvent*>(this)->RebuildTrackViews(); }

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
    int AddTrack(int trackID, int parentID);
    void AddHitToTrack(const XYZVector& position, double energy, double time, REST_HitType type,
                       const XYZVector& sigma = XYZVector(0, 0, 0));

    // Adds a full track in one call, taking an already built hits container as
    // the track representation. If `sigmas` is empty, each hit's spatial
    // uncertainty is estimated from the spacing to its neighbouring hits.
    int AddTrack(int trackID, int parentID, const TRestHits& hits, const std::vector<XYZVector>& sigmas = {});

    void RemoveTrack(int n);
    inline void RemoveTracks() {
        fData.clear();
        RebuildTrackViews();
    }

    int GetNumberOfTracks(const std::string& option = "") const;

    TRestTrack GetTrack(std::size_t n);
    TRestTrack GetTrack(std::size_t n) const;
    TRestTrack GetTrackById(int id);
    TRestTrack GetTrackById(int id) const;

    TRestTrack GetMaxEnergyTrackInX();
    TRestTrack GetMaxEnergyTrackInY();
    TRestTrack GetMaxEnergyTrack(const std::string& option = "");
    TRestTrack GetSecondMaxEnergyTrack(const std::string& option = "");

    double GetMaxEnergyTrackVolume(const std::string& option = "");
    double GetMaxEnergyTrackLength(const std::string& option = "");
    double GetEnergy(const std::string& option = "");

    inline int GetTrackID(int n) const { return fData.trackID[n]; }
    inline int GetParentID(int n) const { return fData.parentID[n]; }
    inline double GetTrackEnergy(int n) const { return fTrackHits[n].GetTotalEnergy(); }
    inline double GetTrackLength(int n) const { return fTrackHits[n].GetTotalDistance(); }
    inline double GetTrackVolume(int n) const { return fTrackHits[n].GetMaximumHitDistance2(); }
    inline XYZVector GetTrackMeanPosition(int n) const { return fTrackHits[n].GetMeanPosition(); }
    inline int GetNumberOfHits(int n) const { return (int)fTrackHits[n].GetNumberOfHits(); }

    inline bool isXY(int n) const { return fTrackHits[n].areXY(); }
    inline bool isXZ(int n) const { return fTrackHits[n].areXZ(); }
    inline bool isYZ(int n) const { return fTrackHits[n].areYZ(); }
    inline bool isXYZ(int n) const { return fTrackHits[n].areXYZ(); }
    bool isXYZ() const;

    inline double GetSigmaX(int n, int hit) const { return fData.sigmaX[GetTrackStartIndex(n) + hit]; }
    inline double GetSigmaY(int n, int hit) const { return fData.sigmaY[GetTrackStartIndex(n) + hit]; }
    inline double GetSigmaZ(int n, int hit) const { return fData.sigmaZ[GetTrackStartIndex(n) + hit]; }
    inline XYZVector GetSigma(int n, int hit) const {
        return {GetSigmaX(n, hit), GetSigmaY(n, hit), GetSigmaZ(n, hit)};
    }
    inline double GetClusterSize(int n, int hit) const {
        return std::sqrt(GetSigmaX(n, hit) * GetSigmaX(n, hit) + GetSigmaY(n, hit) * GetSigmaY(n, hit) +
                         GetSigmaZ(n, hit) * GetSigmaZ(n, hit));
    }
    inline double GetXYSize(int n, int hit) const {
        return std::sqrt(GetSigmaX(n, hit) * GetSigmaX(n, hit) + GetSigmaY(n, hit) * GetSigmaY(n, hit));
    }

    int GetTotalHits() const;

    void SwapTrackHits(int n, int i, int j);
    void SortTrackHits(int n, std::function<bool(int, int)> compareCondition = nullptr);
    void ShuffleTrackHits(int n, int NLoop);
    void RemoveTrackHit(int n, int hitIdx);
    void MergeTrackHits(int n, int i, int j);

    double GetMaxTrackRelativeZ();
    void GetMaxTrackBoundaries(XYZVector& orig, XYZVector& end);

    /// \brief Reduces the hits of track `trackIndex` into `nodes` clusters using k-means. The clustered
    /// track is added to `output` (the event of the next stage), with this track as parent.
    /// \return Index of the clustered track in `output`, or -1.
    int KMeansClustering(int trackIndex, int nodes, TRestTrackEvent& output, int maxIt = 100,
                         bool fixBoundaries = false) const;

    void PrintOnlyTracks() const;
    void PrintTrackEvent(bool fullInfo) const;
    void PrintTrack(int n, bool fullInfo = true) const;
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
    ~TRestTrackEvent() override = default;
};

#endif
