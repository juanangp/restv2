#ifndef TRESTDETECTORHITSEVENT_H
#define TRESTDETECTORHITSEVENT_H

#include <TPad.h>

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
    TRestHitsData fXZData;   //!
    TRestHitsData fYZData;   //!
    TRestHitsData fXYZData;  //!
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

        tree->SetBranchAddress("fHitX", &fPtrX);
        tree->SetBranchAddress("fHitY", &fPtrY);
        tree->SetBranchAddress("fHitZ", &fPtrZ);
        tree->SetBranchAddress("fHitTime", &fPtrTime);
        tree->SetBranchAddress("fHitEnergy", &fPtrEnergy);
        tree->SetBranchAddress("fHitType", &fPtrType);
    }

    /// \brief Rebuilds the fHits view to span the current size of fHitsData.
    /// Called by TRestRun after reading an entry, and internally after any
    /// mutation of fHitsData (AddHit, Sort, Shuffle, Initialize, CopyFrom).
    void RebuildHitsView() { fHits = TRestHits(&fHitsData, 0, (int)fHitsData.x.size()); }
    void RefreshViews() const override {
      const_cast<TRestDetectorHitsEvent*>(this)->RebuildHitsView();
    }

    void CopyFrom(const TRestEvent* other) override {
        TRestEvent::CopyFrom(other);
        auto* source = dynamic_cast<const TRestDetectorHitsEvent*>(other);
        if (source) {
            fHitsData = source->fHitsData;
            RebuildHitsView();
        }
    }

    void AddHit(Double_t x, Double_t y, Double_t z, Double_t en, Double_t t = 0,
                REST_HitType type = REST_HitType::XYZ);
    void AddHit(const XYZVector& position, Double_t energy, Double_t time,
                REST_HitType type = REST_HitType::XYZ);

    // Usage event.Sort([&event](int a, int b) { return event.GetEnergy(a) > event.GetEnergy(b); });
    void Sort(std::function<bool(int, int)> compareCondition = nullptr);
    void Shuffle(int NLoop);

    inline size_t GetNumberOfHits() const { return fHits.GetNumberOfHits(); }
    inline TRestHits* GetHits() { return &fHits; }
    inline const TRestHits* GetHits() const { return &fHits; }

    inline Double_t GetX(int n) const { return fHits.GetX(n); }
    inline Double_t GetY(int n) const { return fHits.GetY(n); }
    inline Double_t GetZ(int n) const { return fHits.GetZ(n); }
    inline REST_HitType GetType(int n) const { return fHits.GetType(n); }
    inline Double_t GetDistance2(int n, int m) const { return fHits.GetDistance2(n, m); }
    inline Double_t GetEnergy(int n) const { return fHits.GetEnergy(n); }
    inline Double_t GetTime(int n) const { return fHits.GetTime(n); }

    TRestHits* GetXZHits();
    TRestHits* GetYZHits();
    TRestHits* GetXYZHits();

    inline void PrintEvent() const override { PrintDetectorHitsEvent(-1); }
    void PrintDetectorHitsEvent(Int_t nHits) const;

    inline XYZVector GetPosition(int n) const { return fHits.GetPosition(n); }
    inline XYZVector GetMeanPosition() const { return fHits.GetMeanPosition(); }

    inline Int_t GetNumberOfHitsX() const { return fHits.GetNumberOfHitsX(); }
    inline Int_t GetNumberOfHitsY() const { return fHits.GetNumberOfHitsY(); }

    inline Double_t GetMeanPositionX() const { return fHits.GetMeanPositionX(); }
    inline Double_t GetMeanPositionY() const { return fHits.GetMeanPositionY(); }
    inline Double_t GetMeanPositionZ() const { return fHits.GetMeanPositionZ(); }
    inline Double_t GetSigmaXY2() const { return fHits.GetSigmaXY2(); }
    inline Double_t GetSigmaX() const { return fHits.GetSigmaX(); }
    inline Double_t GetSigmaY() const { return fHits.GetSigmaY(); }
    inline Double_t GetSigmaZ2() const { return fHits.GetSigmaZ2(); }
    inline Double_t GetSkewXY() const { return fHits.GetSkewXY(); }
    inline Double_t GetSkewZ() const { return fHits.GetSkewZ(); }

    inline Double_t GetMaximumHitEnergy() const { return fHits.GetMaximumHitEnergy(); }
    inline Double_t GetMinimumHitEnergy() const { return fHits.GetMinimumHitEnergy(); }
    inline Double_t GetMeanHitEnergy() const { return fHits.GetMeanHitEnergy(); }

    inline Double_t GetEnergyX() const { return fHits.GetEnergyX(); }
    inline Double_t GetEnergyY() const { return fHits.GetEnergyY(); }
    inline Double_t GetTotalEnergy() const { return fHits.GetTotalEnergy(); }

    inline Int_t GetClosestHit(const XYZVector& position) { return fHits.GetClosestHit(position); }

    // NOTA: métodos de geometría (cilindro/prisma) pendientes de confirmar/portar.

    TPad* DrawEvent(const TString& option = "") const override { return nullptr; }

    TRestDetectorHitsEvent() = default;
    ~TRestDetectorHitsEvent() override = default;
};

#endif
