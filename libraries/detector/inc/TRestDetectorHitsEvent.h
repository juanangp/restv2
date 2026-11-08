#ifndef TRESTDETECTORHITSEVENT_H
#define TRESTDETECTORHITSEVENT_H

#include <cstdint>
#include <vector>

#include "Math/Vector3Dfwd.h"
#include "TRestEvent.h"
#include "TRestHits.h"

/// \class TRestDetectorHitsEvent
/// \brief Event container holding a flat list of 3D hits (position + energy + time + type).
///
/// The hits are stored in a TRestHitsData (one vector per quantity, stored as float) and accessed through a
/// TRestHits view (GetHits), which provides the hit analysis methods (mean position, sigma, skewness...).
/// The type of each hit (TRestHitsData::REST_HitType: X, Y, XY, XZ, YZ, XYZ...) tells which coordinates it
/// defines (e.g. XZ and YZ for the hits of strip readouts; the others are NaN).
///
/// Branches of the event tree: fHitX, fHitY, fHitZ, fHitTime, fHitEnergy and fHitType (plus those of
/// TRestEvent). It is the input and output of the hits processes of this library (drift, avalanche,
/// smearing, transformation, light attenuation, analysis) and the output of TRestDetectorSignalToHitsProcess
/// and of TRestGeant4ToDetectorHitsProcess (connectors library).
class TRestDetectorHitsEvent : public TRestEvent {
   public:
    /// Type of a hit: the coordinates it defines (TRestHitsData::REST_HitType)
    using REST_HitType = TRestHitsData::REST_HitType;
    /// 3D vector type of positions (mm)
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
    /// \brief Returns the class name, "TRestDetectorHitsEvent".
    std::string GetClassName() const override { return "TRestDetectorHitsEvent"; }

    /// \brief Seed of the random generator of a process for one event: the seed of the process (non 0)
    /// mixed with the event and subevent IDs (splitmix64). The processes re-seed with it at the start of
    /// each event, so a fixed seed gives the same result whatever the number of threads. 32 bits (as
    /// TRandom3 uses) and never 0 (0 is a random seed for TRandom3).
    static unsigned long EventSeed(unsigned long seed, const TRestEvent& event) {
        auto mix = [](uint64_t x) {
            x += 0x9e3779b97f4a7c15ULL;
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
            return x ^ (x >> 31);
        };
        uint64_t h = mix(seed);
        h = mix(h ^ static_cast<uint32_t>(event.GetID()));
        h = mix(h ^ static_cast<uint32_t>(event.GetSubID()));
        const unsigned long folded = (h ^ (h >> 32)) & 0xffffffffULL;
        return folded != 0 ? folded : 1;
    }

    /// \brief Resets the event (TRestEvent::Initialize) and removes all the hits.
    void Initialize() override {
        TRestEvent::Initialize();
        fHitsData.clear();
        RebuildHitsView();
    }

    /// \brief Creates the event branches in a tree: those of TRestEvent and fHitX, fHitY, fHitZ, fHitTime,
    /// fHitEnergy and fHitType.
    /// \param tree Output event tree.
    void CreateBranches(TTree* tree) override {
        TRestEvent::CreateBranches(tree);
        tree->Branch("fHitX", &fHitsData.x);
        tree->Branch("fHitY", &fHitsData.y);
        tree->Branch("fHitZ", &fHitsData.z);
        tree->Branch("fHitTime", &fHitsData.time);
        tree->Branch("fHitEnergy", &fHitsData.energy);
        tree->Branch("fHitType", &fHitsData.type);
    }

    /// \brief Binds the hit branches of an input tree (written by CreateBranches) to the hit storage.
    /// \param tree Input event tree.
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
    /// \brief Rebuilds the hits view after the storage is filled from a tree (RebuildHitsView).
    void RefreshViews() const override { const_cast<TRestDetectorHitsEvent*>(this)->RebuildHitsView(); }

    /// \brief Copies the event header (TRestEvent::CopyFrom) and, if other is a TRestDetectorHitsEvent,
    /// its hits.
    void CopyFrom(const TRestEvent* other) override {
        TRestEvent::CopyFrom(other);
        auto* source = dynamic_cast<const TRestDetectorHitsEvent*>(other);
        if (source) {
            fHitsData = source->fHitsData;
            RebuildHitsView();
        }
    }

    /// \brief Appends a hit (stored as float).
    /// \param x, y, z Position (mm); the coordinates the type does not define are usually NaN.
    /// \param en Energy (keV).
    /// \param t Time (us).
    /// \param type Coordinates defined by the hit (XYZ by default).
    void AddHit(double x, double y, double z, double en, double t = 0, REST_HitType type = REST_HitType::XYZ);
    /// \brief Appends a hit from a position vector (see the other overload).
    void AddHit(const XYZVector& position, double energy, double time, REST_HitType type = REST_HitType::XYZ);

    /// \brief Sorts the hits in place with a comparison of hit indices (TRestHits::Sort), e.g.
    /// `event.Sort([&event](int a, int b) { return event.GetEnergy(a) > event.GetEnergy(b); });`
    /// \param compareCondition Strict ordering of two hit indices; nullptr uses the TRestHits default.
    void Sort(std::function<bool(int, int)> compareCondition = nullptr);
    /// \brief Randomly permutes the hits in place with NLoop random swaps (TRestHits::Shuffle).
    void Shuffle(int NLoop);

    /// \brief Number of hits of the event.
    inline size_t GetNumberOfHits() const { return fHits.GetNumberOfHits(); }
    /// \brief View over all the hits, with the analysis methods of TRestHits.
    inline TRestHits* GetHits() { return &fHits; }
    /// \brief View over all the hits (const).
    inline const TRestHits* GetHits() const { return &fHits; }

    /// \brief X coordinate of hit n (mm).
    inline double GetX(int n) const { return fHits.GetX(n); }
    /// \brief Y coordinate of hit n (mm).
    inline double GetY(int n) const { return fHits.GetY(n); }
    /// \brief Z coordinate of hit n (mm).
    inline double GetZ(int n) const { return fHits.GetZ(n); }
    /// \brief Type (defined coordinates) of hit n.
    inline REST_HitType GetType(int n) const { return fHits.GetType(n); }
    /// \brief Squared distance between hits n and m (TRestHits::GetDistance2).
    inline double GetDistance2(int n, int m) const { return fHits.GetDistance2(n, m); }
    /// \brief Energy of hit n (keV).
    inline double GetEnergy(int n) const { return fHits.GetEnergy(n); }
    /// \brief Time of hit n.
    inline double GetTime(int n) const { return fHits.GetTime(n); }

    /// \brief Copy of the hits of type XZ (e.g. X strips), as a view valid until the next call.
    TRestHits* GetXZHits();
    /// \brief Copy of the hits of type YZ (e.g. Y strips), as a view valid until the next call.
    TRestHits* GetYZHits();
    /// \brief Copy of the hits of type XYZ, as a view valid until the next call.
    TRestHits* GetXYZHits();

    /// \brief Prints the event header and all its hits.
    inline void PrintEvent() const override { PrintDetectorHitsEvent(-1); }
    /// \brief Prints the event header and its hits.
    /// \param nHits Maximum number of hits printed (-1: all).
    void PrintDetectorHitsEvent(int nHits) const;

    /// \brief Position of hit n.
    inline XYZVector GetPosition(int n) const { return fHits.GetPosition(n); }
    /// \brief Energy weighted mean position of the hits (TRestHits::GetMeanPosition).
    inline XYZVector GetMeanPosition() const { return fHits.GetMeanPosition(); }

    /// \brief Number of hits of type X.
    inline int GetNumberOfHitsX() const { return fHits.GetNumberOfHitsX(); }
    /// \brief Number of hits of type Y.
    inline int GetNumberOfHitsY() const { return fHits.GetNumberOfHitsY(); }

    /// \brief Energy weighted mean X of the hits defining X (TRestHits).
    inline double GetMeanPositionX() const { return fHits.GetMeanPositionX(); }
    /// \brief Energy weighted mean Y of the hits defining Y (TRestHits).
    inline double GetMeanPositionY() const { return fHits.GetMeanPositionY(); }
    /// \brief Energy weighted mean Z of the hits defining Z (TRestHits).
    inline double GetMeanPositionZ() const { return fHits.GetMeanPositionZ(); }
    /// \brief Energy weighted variance in XY (TRestHits::GetSigmaXY2).
    inline double GetSigmaXY2() const { return fHits.GetSigmaXY2(); }
    /// \brief Energy weighted standard deviation in X (TRestHits::GetSigmaX).
    inline double GetSigmaX() const { return fHits.GetSigmaX(); }
    /// \brief Energy weighted standard deviation in Y (TRestHits::GetSigmaY).
    inline double GetSigmaY() const { return fHits.GetSigmaY(); }
    /// \brief Energy weighted variance in Z (TRestHits::GetSigmaZ2).
    inline double GetSigmaZ2() const { return fHits.GetSigmaZ2(); }
    /// \brief Skewness in XY (TRestHits::GetSkewXY).
    inline double GetSkewXY() const { return fHits.GetSkewXY(); }
    /// \brief Skewness in Z (TRestHits::GetSkewZ).
    inline double GetSkewZ() const { return fHits.GetSkewZ(); }

    /// \brief Largest energy of a hit.
    inline double GetMaximumHitEnergy() const { return fHits.GetMaximumHitEnergy(); }
    /// \brief Smallest energy of a hit.
    inline double GetMinimumHitEnergy() const { return fHits.GetMinimumHitEnergy(); }
    /// \brief Mean energy per hit.
    inline double GetMeanHitEnergy() const { return fHits.GetMeanHitEnergy(); }

    /// \brief Energy of the hits of type X.
    inline double GetEnergyX() const { return fHits.GetEnergyX(); }
    /// \brief Energy of the hits of type Y.
    inline double GetEnergyY() const { return fHits.GetEnergyY(); }
    /// \brief Sum of the energies of all the hits.
    inline double GetTotalEnergy() const { return fHits.GetTotalEnergy(); }

    /// \brief Index of the hit closest to a position (TRestHits::GetClosestHit).
    inline int GetClosestHit(const XYZVector& position) { return fHits.GetClosestHit(position); }

    // NOTE: geometry methods (cylinder/prism) pending confirmation/porting.

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

    /// \brief Builds an empty event.
    TRestDetectorHitsEvent() = default;
    ~TRestDetectorHitsEvent() override = default;
};

#endif
