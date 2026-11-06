#ifndef TRESTDETECTORHITSEVENT_H
#define TRESTDETECTORHITSEVENT_H

#include <vector>

#include "Math/Vector3Dfwd.h"
#include "TRestEvent.h"
#include "TRestHits.h"

/// \class TRestDetectorHitsEvent
/// \brief Event container holding a flat list of 3D hits (position + energy + time + type).
class TRestDetectorHitsEvent : public TRestEvent {
   public:
    using REST_HitType = TRestHitsData::REST_HitType;
    using XYZVector = ROOT::Math::XYZVector;

   private:
    /// Persisted hit storage (x, y, z, time, energy, type).
    TRestHitsData fHitsData;

    /// Lightweight view over fHitsData (full range). Rebuilt whenever the
    /// underlying storage changes (see RefreshViews).
    mutable TRestHits fHits{&fHitsData, 0, 0};

    /// Auxiliary (non-persisted) storage/views for filtered projections.
    TRestHitsData fXZData;                    //!
    TRestHitsData fYZData;                    //!
    TRestHitsData fXYZData;                   //!
    TRestHits fXZHitsView{&fXZData, 0, 0};    //!
    TRestHits fYZHitsView{&fYZData, 0, 0};    //!
    TRestHits fXYZHitsView{&fXYZData, 0, 0};  //!

    /// Persisted branch pointers (ROOT requires T** for class/vector branches).
    std::vector<float>* fPtrX = nullptr;       //!
    std::vector<float>* fPtrY = nullptr;       //!
    std::vector<float>* fPtrZ = nullptr;       //!
    std::vector<float>* fPtrTime = nullptr;    //!
    std::vector<float>* fPtrEnergy = nullptr;  //!
    std::vector<int>* fPtrType = nullptr;      //!

   public:
    std::string GetClassName() const override { return "TRestDetectorHitsEvent"; }

    void Initialize() override {
        TRestEvent::Initialize();
        fHitsData.clear();
        RebuildHitsView();
    }

    void CreateBranches(TTree* tree) override {
        TRestEvent::CreateBranches(tree);
        tree->Branch("fHitX", &fHitsData.x);
        tree->Branch("fHitY", &fHitsData.y);
        tree->Branch("fHitZ", &fHitsData.z);
        tree->Branch("fHitTime", &fHitsData.time);
        tree->Branch("fHitEnergy", &fHitsData.energy);
        tree->Branch("fHitType", &fHitsData.type);
    }

    void SetBranchAddresses(TTree* tree) override {
        TRestEvent::SetBranchAddresses(tree);

        fPtrX = &fHitsData.x;
        fPtrY = &fHitsData.y;
        fPtrZ = &fHitsData.z;
        fPtrTime = &fHitsData.time;
        fPtrEnergy = &fHitsData.energy;
        fPtrType = &fHitsData.type;

        BindBranch(tree, "fHitX", &fPtrX);
        BindBranch(tree, "fHitY", &fPtrY);
        BindBranch(tree, "fHitZ", &fPtrZ);
        BindBranch(tree, "fHitTime", &fPtrTime);
        BindBranch(tree, "fHitEnergy", &fPtrEnergy);
        BindBranch(tree, "fHitType", &fPtrType);
    }

    /// \brief Rebuilds the fHits view to span the current size of fHitsData.
    /// Called by TRestRun after reading an entry, and internally after any
    /// mutation of fHitsData (AddHit, Sort, Shuffle, Initialize, CopyFrom).
    void RebuildHitsView() { fHits = TRestHits(&fHitsData, 0, (int)fHitsData.x.size()); }
    void RefreshViews() const override { const_cast<TRestDetectorHitsEvent*>(this)->RebuildHitsView(); }

    void CopyFrom(const TRestEvent* other) override {
        TRestEvent::CopyFrom(other);
        auto* source = dynamic_cast<const TRestDetectorHitsEvent*>(other);
        if (source) {
            fHitsData = source->fHitsData;
            RebuildHitsView();
        }
    }

    void AddHit(double x, double y, double z, double en, double t = 0, REST_HitType type = REST_HitType::XYZ);
    void AddHit(const XYZVector& position, double energy, double time, REST_HitType type = REST_HitType::XYZ);

    // Usage event.Sort([&event](int a, int b) { return event.GetEnergy(a) > event.GetEnergy(b); });
    void Sort(std::function<bool(int, int)> compareCondition = nullptr);
    void Shuffle(int NLoop);

    inline size_t GetNumberOfHits() const { return fHits.GetNumberOfHits(); }
    inline TRestHits* GetHits() { return &fHits; }
    inline const TRestHits* GetHits() const { return &fHits; }

    inline double GetX(int n) const { return fHits.GetX(n); }
    inline double GetY(int n) const { return fHits.GetY(n); }
    inline double GetZ(int n) const { return fHits.GetZ(n); }
    inline REST_HitType GetType(int n) const { return fHits.GetType(n); }
    inline double GetDistance2(int n, int m) const { return fHits.GetDistance2(n, m); }
    inline double GetEnergy(int n) const { return fHits.GetEnergy(n); }
    inline double GetTime(int n) const { return fHits.GetTime(n); }

    TRestHits* GetXZHits();
    TRestHits* GetYZHits();
    TRestHits* GetXYZHits();

    inline void PrintEvent() const override { PrintDetectorHitsEvent(-1); }
    void PrintDetectorHitsEvent(int nHits) const;

    inline XYZVector GetPosition(int n) const { return fHits.GetPosition(n); }
    inline XYZVector GetMeanPosition() const { return fHits.GetMeanPosition(); }

    inline int GetNumberOfHitsX() const { return fHits.GetNumberOfHitsX(); }
    inline int GetNumberOfHitsY() const { return fHits.GetNumberOfHitsY(); }

    inline double GetMeanPositionX() const { return fHits.GetMeanPositionX(); }
    inline double GetMeanPositionY() const { return fHits.GetMeanPositionY(); }
    inline double GetMeanPositionZ() const { return fHits.GetMeanPositionZ(); }
    inline double GetSigmaXY2() const { return fHits.GetSigmaXY2(); }
    inline double GetSigmaX() const { return fHits.GetSigmaX(); }
    inline double GetSigmaY() const { return fHits.GetSigmaY(); }
    inline double GetSigmaZ2() const { return fHits.GetSigmaZ2(); }
    inline double GetSkewXY() const { return fHits.GetSkewXY(); }
    inline double GetSkewZ() const { return fHits.GetSkewZ(); }

    inline double GetMaximumHitEnergy() const { return fHits.GetMaximumHitEnergy(); }
    inline double GetMinimumHitEnergy() const { return fHits.GetMinimumHitEnergy(); }
    inline double GetMeanHitEnergy() const { return fHits.GetMeanHitEnergy(); }

    inline double GetEnergyX() const { return fHits.GetEnergyX(); }
    inline double GetEnergyY() const { return fHits.GetEnergyY(); }
    inline double GetTotalEnergy() const { return fHits.GetTotalEnergy(); }

    inline int GetClosestHit(const XYZVector& position) { return fHits.GetClosestHit(position); }

    // NOTA: métodos de geometría (cilindro/prisma) pendientes de confirmar/portar.

    /// \brief Draws the hits, one row of pads per option. Options separated by ':':
    /// - graph: hits as TGraph markers (TRestHits::DrawGraphs);
    /// - hist(rootOption)[pitch]: histograms weighted by energy (TRestHits::DrawHistograms): projections
    ///   and 3D view, and X, Y and Z distributions, only those the hits define (1D, 2D or 3D events);
    /// - markers(rootOption)[pitch]: as hist, with the Z bin size of the spacing of the pulse samples and
    ///   the 3D view as markers colored by energy, the strips paired without ghosts
    ///   (TRestHits::DrawHistograms with markers3D);
    /// - print: PrintEvent.
    /// Default: "hist(Cont1,col)".
    TPad* DrawEvent(const std::string& option = "") const override;

    TRestDetectorHitsEvent() = default;
    ~TRestDetectorHitsEvent() override = default;
};

#endif
