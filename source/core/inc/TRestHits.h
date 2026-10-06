#ifndef TRESTHITS_H
#define TRESTHITS_H

#include <cmath>
#include <functional>
#include <iostream>
#include <numeric>
#include <limits>
#include <vector>

#include <TRandom.h>

#include "Math/Vector3D.h"
#include "Math/GenVector/VectorUtil.h"

namespace TRestHitsUtils {

/// \brief Reorders each of the given parallel arrays, in place, over the range
/// [start, start+order.size()), according to `order` (order[i] = index,
/// relative to `start`, of the element that ends up at position i).
template <typename... Arrays>
void ApplyPermutation(const std::vector<int>& order, int start, Arrays&... arrays) {
    const int n = (int)order.size();
    auto permuteOne = [&order, n, start](auto& values) {
        using T = typename std::decay_t<decltype(values)>::value_type;
        std::vector<T> sorted(n);
        for (int i = 0; i < n; ++i) sorted[i] = values[start + order[i]];
        for (int i = 0; i < n; ++i) values[start + i] = sorted[i];
    };
    (permuteOne(arrays), ...);
}

/// \brief Builds an identity-then-sorted index order over [0, n), using
/// `compareCondition` if given, or `defaultCompare` otherwise.
inline std::vector<int> BuildSortOrder(int n, std::function<bool(int, int)> compareCondition,
                                       std::function<bool(int, int)> defaultCompare) {
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), compareCondition ? compareCondition : defaultCompare);
    return order;
}

/// \brief Builds an index order obtained by applying NLoop random swaps to
/// the identity order over [0, n).
inline std::vector<int> BuildShuffleOrder(int n, int NLoop) {
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    for (int k = 0; k < NLoop; ++k) {
        int i = (int)(n * gRandom->Uniform(0, 1));
        int j = (int)(n * gRandom->Uniform(0, 1));
        std::swap(order[i], order[j]);
    }
    return order;
}

}  // namespace TRestHitsUtils


/// \struct TRestHitsData
/// \brief Shared storage container for hit coordinates, time and energy arrays.
struct TRestHitsData {
    /// Hit dimensionality/type encoding.
    enum REST_HitType : int { unknown = -1, X = 2, Y = 3, Z = 5, XY = 6, XZ = 10, YZ = 15, XYZ = 30 };

    std::vector<float> x, y, z, time, energy;
    std::vector<int> type;

    /// \brief Clears all hit arrays.
    void clear() {
        x.clear();
        y.clear();
        z.clear();
        time.clear();
        energy.clear();
        type.clear();
    }
};

/// \class TRestHits
/// \brief Lightweight view and utility API over `TRestHitsData`.
///
/// The class references external storage and exposes geometric/statistical
/// operations for subsets of hits.
class TRestHits {
   protected:
    TRestHitsData* fData = nullptr;
    int fStartIdx = 0;
    int fNHits = 0;

    /// \brief Converts local index to global storage index.
    /// \param n Local hit index.
    /// \return Global index into shared storage.
    inline int GetGlobalIdx(int n) const { return fStartIdx + n; }

   public:
    /// \brief Constructs a hit-view over shared data.
    /// \param data Shared hit storage.
    /// \param start First hit index in the storage.
    /// \param n Number of hits in the view.
    TRestHits(TRestHitsData* data, int start = 0, int n = 0) : fData(data), fStartIdx(start), fNHits(n) {}

    /// \brief Translates one hit in 3D.
    void Translate(int n, double x, double y, double z);

    /// \brief Rotates one hit around a center using Euler angles.
    void RotateIn3D(int n, double alpha, double beta, double gamma, ROOT::Math::XYZVector center);

    inline void RemoveHits() {
        if (!fData || fNHits <= 0) {
            fNHits = 0;
            return;
        }

        auto startIt = fData->x.begin() + fStartIdx;
        auto endIt = startIt + fNHits;

        fData->x.erase(startIt, endIt);
        fData->y.erase(fData->y.begin() + fStartIdx, fData->y.begin() + fStartIdx + fNHits);
        fData->z.erase(fData->z.begin() + fStartIdx, fData->z.begin() + fStartIdx + fNHits);
        fData->time.erase(fData->time.begin() + fStartIdx, fData->time.begin() + fStartIdx + fNHits);
        fData->energy.erase(fData->energy.begin() + fStartIdx, fData->energy.begin() + fStartIdx + fNHits);
        fData->type.erase(fData->type.begin() + fStartIdx, fData->type.begin() + fStartIdx + fNHits);

        fNHits = 0;
    }

    inline void ClearHits() {
        if (fData) fData->clear();
        fStartIdx = 0;
        fNHits = 0;
    }

    /// \brief Adds one hit.
    void AddHit(ROOT::Math::XYZVector position, double energy, double time, TRestHitsData::REST_HitType type);

    /// \brief Appends hits from another view.
    void AddHits(const TRestHits& hits);

    /// \brief Returns index of most energetic hit in a range.
    int GetMostEnergeticHitInRange(int n, int m) const;

    double GetMaximumHitDistance() const;
    double GetMaximumHitDistance2() const;

    virtual void MergeHits(int n, int m);
    virtual void SwapHits(int i, int j);
    virtual void RemoveHit(int n);

    /// \brief Reorders the hits in-place, by default ascending in energy.
    /// Usage: hits.Sort([&hits](int a, int b) { return hits.GetEnergy(a) > hits.GetEnergy(b); });
    virtual void Sort(std::function<bool(int, int)> compareCondition = nullptr);

    /// \brief Randomly permutes the hits in-place.
    virtual void Shuffle(int NLoop);

    virtual bool areXY() const;
    virtual bool areXZ() const;
    virtual bool areYZ() const;
    virtual bool areXYZ() const;

    bool isNaN(int n) const;

    double GetDistanceToNode(int n);

    bool isSortedByEnergy() const;

    /// \brief Returns number of hits in current view.
    inline size_t GetNumberOfHits() const { return (size_t)fNHits; }
    inline double GetX(int n) const { return fData->x[GetGlobalIdx(n)]; }
    inline double GetY(int n) const { return fData->y[GetGlobalIdx(n)]; }
    inline double GetZ(int n) const { return fData->z[GetGlobalIdx(n)]; }
    inline double GetTime(int n) const { return fData->time[GetGlobalIdx(n)]; }
    inline double GetEnergy(int n) const { return fData->energy[GetGlobalIdx(n)]; }
    inline TRestHitsData::REST_HitType GetType(int n) const {
        return static_cast<TRestHitsData::REST_HitType>(fData->type[GetGlobalIdx(n)]);
    }

    ROOT::Math::XYZVector GetPosition(int n) const;
    inline ROOT::Math::XYZVector GetVector(int i, int j) { return GetPosition(i) - GetPosition(j); }

    int GetNumberOfHitsByType(const TRestHitsData::REST_HitType type) const;
    inline int GetNumberOfHitsX() const { return GetNumberOfHitsByType(TRestHitsData::REST_HitType::X); }
    inline int GetNumberOfHitsY() const { return GetNumberOfHitsByType(TRestHitsData::REST_HitType::Y); }

    double GetMeanPositionX() const;
    double GetMeanPositionY() const;
    double GetMeanPositionZ() const;
    inline ROOT::Math::XYZVector GetMeanPosition() const {
        return {GetMeanPositionX(), GetMeanPositionY(), GetMeanPositionZ()};
    };

    double GetSigmaXY2() const;
    double GetSigmaX2() const;
    double GetSigmaY2() const;
    inline double GetSigmaX() const { return std::sqrt(GetSigmaX2()); }
    inline double GetSigmaY() const { return std::sqrt(GetSigmaY2()); }
    inline double GetSigmaZ() const { return std::sqrt(GetSigmaZ2()); }
    double GetSigmaZ2() const;
    double GetSkewXY() const;
    double GetSkewZ() const;

    double GetEnergyByType(const TRestHitsData::REST_HitType type) const;
    double GetEnergyX() const { return GetEnergyByType(TRestHitsData::REST_HitType::X); }
    double GetEnergyY() const { return GetEnergyByType(TRestHitsData::REST_HitType::Y); }

    double GetMaximumHitEnergy() const;
    double GetMinimumHitEnergy() const;
    double GetMeanHitEnergy() const;

    double GetTotalEnergy() const;
    double GetDistance2(int n, int m) const;
    inline double GetDistance(int N, int M) const { return std::sqrt(GetDistance2(N, M)); }
    double GetTotalDistance() const;
    double GetHitsPathLength(int n = 0, int m = 0) const;

    int GetClosestHit(ROOT::Math::XYZVector position);

    std::pair<double, double> GetProjection(int n, int m, ROOT::Math::XYZVector position);

    double GetTransversalProjection(ROOT::Math::XYZVector p0, ROOT::Math::XYZVector direction,
                                    ROOT::Math::XYZVector position) const;

    virtual void PrintHits(int nHits = -1) const;

    TRestHits() = default;
    virtual ~TRestHits() = default;
};

#endif
