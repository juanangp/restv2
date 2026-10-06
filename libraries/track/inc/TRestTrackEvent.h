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

    Int_t fLevels = -1;  ///< Maximum parent-child depth found in the event.

    /// Persisted branch pointers (ROOT requires T** for class/vector branches).
    std::vector<float>* fPtrX = nullptr;        //!
    std::vector<float>* fPtrY = nullptr;        //!
    std::vector<float>* fPtrZ = nullptr;        //!
    std::vector<float>* fPtrTime = nullptr;     //!
    std::vector<float>* fPtrEnergy = nullptr;   //!
    std::vector<int>* fPtrType = nullptr;       //!
    std::vector<float>* fPtrSigmaX = nullptr;   //!
    std::vector<float>* fPtrSigmaY = nullptr;   //!
    std::vector<float>* fPtrSigmaZ = nullptr;   //!
    std::vector<int>* fPtrTrackID = nullptr;    //!
    std::vector<int>* fPtrParentID = nullptr;   //!
    std::vector<int>* fPtrTrackNHits = nullptr;  //!

    /// \brief Returns the first global hit index belonging to track n.
    int GetTrackStartIndex(int n) const;

    /// \brief Returns the storage index of the track with the given ID, or -1.
    int FindTrackIndexById(Int_t id) const;

   public:
    std::string GetClassName() const override { return "TRestTrackEvent"; }

    void Initialize() override {
        TRestEvent::Initialize();
        fData.clear();
        fLevels = -1;
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

        tree->SetBranchAddress("fHitX", &fPtrX);
        tree->SetBranchAddress("fHitY", &fPtrY);
        tree->SetBranchAddress("fHitZ", &fPtrZ);
        tree->SetBranchAddress("fHitTime", &fPtrTime);
        tree->SetBranchAddress("fHitEnergy", &fPtrEnergy);
        tree->SetBranchAddress("fHitType", &fPtrType);
        tree->SetBranchAddress("fSigmaX", &fPtrSigmaX);
        tree->SetBranchAddress("fSigmaY", &fPtrSigmaY);
        tree->SetBranchAddress("fSigmaZ", &fPtrSigmaZ);
        tree->SetBranchAddress("fTrackID", &fPtrTrackID);
        tree->SetBranchAddress("fParentID", &fPtrParentID);
        tree->SetBranchAddress("fTrackNHits", &fPtrTrackNHits);
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
    void RefreshViews() const override {
        const_cast<TRestTrackEvent*>(this)->RebuildTrackViews();
    }

    void CopyFrom(const TRestEvent* other) override {
        TRestEvent::CopyFrom(other);
        auto* source = dynamic_cast<const TRestTrackEvent*>(other);
        if (source) {
            fData = source->fData;
            fLevels = source->fLevels;
            RebuildTrackViews();
        }
    }

    // Low-level: int idx = event.AddTrack(trackID, parentID);
    //            event.AddHitToTrack(x, y, z, en, t, type);  // repeat for every hit, in order
    Int_t AddTrack(Int_t trackID, Int_t parentID);
    void AddHitToTrack(const XYZVector& position, Double_t energy, Double_t time,
                       REST_HitType type, const XYZVector& sigma = XYZVector(0,0,0));

    // Adds a full track in one call, taking an already built hits container as
    // the track representation. If `sigmas` is empty, each hit's spatial
    // uncertainty is estimated from the spacing to its neighbouring hits.
    Int_t AddTrack(Int_t trackID, Int_t parentID, const TRestHits& hits,
                   const std::vector<XYZVector>& sigmas = {});

    void RemoveTrack(Int_t n);
    inline void RemoveTracks() {
        fData.clear();
        fLevels = -1;
        RebuildTrackViews();
    }

    Int_t GetNumberOfTracks(const TString& option = "") const;

    TRestTrack GetTrack(std::size_t n);
    TRestTrack GetTrack(std::size_t n) const;
    TRestTrack GetTrackById(Int_t id);
    TRestTrack GetTrackById(Int_t id) const;

    TRestTrack GetMaxEnergyTrackInX();
    TRestTrack GetMaxEnergyTrackInY();
    TRestTrack GetMaxEnergyTrack(const TString& option = "");
    TRestTrack GetSecondMaxEnergyTrack(const TString& option = "");

    TRestTrack GetOriginTrack(Int_t tck);
    TRestTrack GetOriginTrackById(Int_t tckId);

    Double_t GetMaxEnergyTrackVolume(const TString& option = "");
    Double_t GetMaxEnergyTrackLength(const TString& option = "");
    Double_t GetEnergy(const TString& option = "");

    inline Int_t GetTrackID(Int_t n) const { return fData.trackID[n]; }
    inline Int_t GetParentID(Int_t n) const { return fData.parentID[n]; }
    inline Double_t GetTrackEnergy(Int_t n) const { return fTrackHits[n].GetTotalEnergy(); }
    inline Double_t GetTrackLength(Int_t n) const { return fTrackHits[n].GetTotalDistance(); }
    inline Double_t GetTrackVolume(Int_t n) const { return fTrackHits[n].GetMaximumHitDistance2(); }
    inline XYZVector GetTrackMeanPosition(Int_t n) const { return fTrackHits[n].GetMeanPosition(); }
    inline Int_t GetNumberOfHits(Int_t n) const { return (Int_t)fTrackHits[n].GetNumberOfHits(); }

    inline Bool_t isXY(Int_t n) const { return fTrackHits[n].areXY(); }
    inline Bool_t isXZ(Int_t n) const { return fTrackHits[n].areXZ(); }
    inline Bool_t isYZ(Int_t n) const { return fTrackHits[n].areYZ(); }
    inline Bool_t isXYZ(Int_t n) const { return fTrackHits[n].areXYZ(); }
    Bool_t isXYZ() const;

    inline Double_t GetSigmaX(Int_t n, Int_t hit) const { return fData.sigmaX[GetTrackStartIndex(n) + hit]; }
    inline Double_t GetSigmaY(Int_t n, Int_t hit) const { return fData.sigmaY[GetTrackStartIndex(n) + hit]; }
    inline Double_t GetSigmaZ(Int_t n, Int_t hit) const { return fData.sigmaZ[GetTrackStartIndex(n) + hit]; }
    inline XYZVector GetSigma(Int_t n, Int_t hit) const {
        return {GetSigmaX(n, hit), GetSigmaY(n, hit), GetSigmaZ(n, hit)};
    }
    inline Double_t GetClusterSize(Int_t n, Int_t hit) const {
        return std::sqrt(GetSigmaX(n, hit) * GetSigmaX(n, hit) + GetSigmaY(n, hit) * GetSigmaY(n, hit) +
                         GetSigmaZ(n, hit) * GetSigmaZ(n, hit));
    }
    inline Double_t GetXYSize(Int_t n, Int_t hit) const {
        return std::sqrt(GetSigmaX(n, hit) * GetSigmaX(n, hit) + GetSigmaY(n, hit) * GetSigmaY(n, hit));
    }

    Int_t GetLevel(Int_t tck) const;
    void SetLevels();
    inline Int_t GetLevels() const { return fLevels; }
    Bool_t isTopLevel(Int_t tck) const;
    Int_t GetOriginTrackID(Int_t tck) const;

    Int_t GetTotalHits() const;

    void SwapTrackHits(Int_t n, int i, int j);
    void SortTrackHits(Int_t n, std::function<bool(int, int)> compareCondition = nullptr);
    void ShuffleTrackHits(Int_t n, int NLoop);
    void RemoveTrackHit(Int_t n, int hitIdx);
    void MergeTrackHits(Int_t n, int i, int j);

    Double_t GetMaxTrackRelativeZ();
    void GetMaxTrackBoundaries(XYZVector& orig, XYZVector& end);

    /// \brief Reduces the hits of track `trackIndex` into `nodes` clusters using k-means.
    /// Appends and returns the index of the newly created clustered track.
    Int_t KMeansClustering(Int_t trackIndex, Int_t nodes, Int_t maxIt = 100, Bool_t fixBoundaries = false);

    void PrintOnlyTracks() const;
    void PrintTrackEvent(Bool_t fullInfo) const;
    void PrintTrack(Int_t n, Bool_t fullInfo = true) const;
    inline void PrintEvent() const override { PrintTrackEvent(false); }

    // NOTA: métodos de dibujo (TGraph/TPad: DrawEvent, DrawHits, GetOriginEnd) pendientes de portar.
    TPad* DrawEvent(const TString& option = "") const override { return nullptr; }

    TRestTrackEvent() = default;
    ~TRestTrackEvent() override = default;
};

#endif
