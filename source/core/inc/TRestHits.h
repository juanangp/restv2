#ifndef TRESTHITS_H
#define TRESTHITS_H

#include <TRandom.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "Math/GenVector/VectorUtil.h"
#include "Math/Vector3D.h"

class TVirtualPad;

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
        // Integer(n) gives [0, n): (int)(n * Uniform(0, 1)) could give n if the generator returns 1
        int i = (int)gRandom->Integer(n);
        int j = (int)gRandom->Integer(n);
        std::swap(order[i], order[j]);
    }
    return order;
}

}  // namespace TRestHitsUtils

/// \struct TRestHitsData
/// \brief Shared storage container for hit coordinates, time and energy arrays.
///
/// Parallel arrays, one element per hit; the coordinates a hit does not define (by type) are NaN.
struct TRestHitsData {
    /// Hit dimensionality/type encoding: the product of the primes of its defined coordinates (X = 2,
    /// Y = 3, Z = 5), so a type defines a coordinate if it is divisible by its prime.
    enum REST_HitType : int { unknown = -1, X = 2, Y = 3, Z = 5, XY = 6, XZ = 10, YZ = 15, XYZ = 30 };

    std::vector<float> x, y, z, time, energy;  ///< Positions (mm), time and energy of the hits.
    std::vector<int> type;                     ///< REST_HitType of each hit.

    /// Coordinates as indices 0, 1, 2 (X, Y, Z): factor in the hit type and axis title
    static constexpr REST_HitType kAxisTypes[3] = {X, Y, Z};
    /// Axis titles of X, Y, Z
    static constexpr const char* kAxisTitles[3] = {"X-axis (mm)", "Y-axis (mm)", "Z-axis (mm)"};
    /// Names of the hit types (projections of the analysis processes), for TRestTools::NormalizeOption
    inline static const std::vector<std::string> kTypeNames = {"X", "Y", "Z", "XY", "XZ", "YZ", "XYZ"};
    /// Projection planes XZ, YZ and XY (as legacy), as pairs of coordinates
    static constexpr int kPlanes[3][2] = {{0, 2}, {1, 2}, {0, 1}};

    /// \brief Whether a hit of the given type defines the coordinate (0, 1, 2 = X, Y, Z).
    static bool IsDefined(int hitType, int axis) { return hitType % kAxisTypes[axis] == 0; }

    /// \brief Appends a hit. The coordinates its type does not define are stored as NaN, whatever the value
    /// given (e.g. the 0 of a projected position, TRestHits::GetPosition, when the hits are copied); all
    /// of them are kept for the unknown type.
    void push_back(double hitX, double hitY, double hitZ, double hitTime, double hitEnergy, int hitType) {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const bool known = hitType > 0;
        x.push_back(known && !IsDefined(hitType, 0) ? nan : (float)hitX);
        y.push_back(known && !IsDefined(hitType, 1) ? nan : (float)hitY);
        z.push_back(known && !IsDefined(hitType, 2) ? nan : (float)hitZ);
        time.push_back((float)hitTime);
        energy.push_back((float)hitEnergy);
        type.push_back(hitType);
    }

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
/// operations for subsets of hits. A view covers the hits [fStartIdx, fStartIdx + fNHits) of the
/// storage (e.g. one track of a track event sharing the storage of all its tracks). Hits are added at the
/// end of the storage and removed from it, so the methods that add or remove hits (AddHit, AddHits,
/// RemoveHit, RemoveHits, MergeHits) are only allowed on the last view of the storage (the one ending at
/// its end, e.g. a single view over the whole storage): on any other view they throw std::logic_error, as
/// the hits would fall outside the view and the indices of the following views would shift.
///
/// The energy-weighted means, sigmas and skewness of a coordinate only use the hits defining it. The
/// distances use the plane of the hits if all are XY, XZ or YZ, otherwise the three coordinates.
class TRestHits {
   protected:
    TRestHitsData* fData = nullptr;  ///< Shared storage (not owned).
    int fStartIdx = 0;               ///< First hit of the view in the storage.
    int fNHits = 0;                  ///< Number of hits of the view.

    /// \brief Converts local index to global storage index.
    /// \param n Local hit index.
    /// \return Global index into shared storage.
    inline int GetGlobalIdx(int n) const { return fStartIdx + n; }

    /// \brief Throws std::logic_error if this view does not end at the end of the storage (it is not the
    /// last view), as adding or removing hits would then break this and the following views.
    /// \param method Name of the calling method, for the message.
    void CheckIsLastView(const char* method) const;

   private:
    /// Coordinates of the distances: all the hits XY, XZ or YZ (the plane), otherwise XYZ
    enum class DistanceView { XYZ, XY, XZ, YZ };
    /// \brief View of the distances of these hits (areXY/areXZ/areYZ, O(N)): computed once per method
    DistanceView GetDistanceView() const;
    /// \brief Squared distance between hits n and m in the given view
    double GetDistance2(int n, int m, DistanceView view) const;

   public:
    /// \brief Constructs a hit-view over shared data.
    /// \param data Shared hit storage.
    /// \param start First hit index in the storage.
    /// \param n Number of hits in the view.
    TRestHits(TRestHitsData* data, int start = 0, int n = 0) : fData(data), fStartIdx(start), fNHits(n) {}

    /// \brief Translates one hit in 3D.
    /// \param n Hit index.
    /// \param x Shift in X.
    /// \param y Shift in Y.
    /// \param z Shift in Z.
    void Translate(int n, double x, double y, double z);

    /// \brief Rotates one hit around a center: by gamma around Z, then beta around Y, then alpha around X.
    /// \param n Hit index.
    /// \param alpha Angle around X (rad).
    /// \param beta Angle around Y (rad).
    /// \param gamma Angle around Z (rad).
    /// \param center Center of the rotation.
    void RotateIn3D(int n, double alpha, double beta, double gamma, ROOT::Math::XYZVector center);

    /// \brief Removes the hits of the view from the storage (the view becomes empty).
    /// Only on the last view of the storage (std::logic_error otherwise, see the class description).
    inline void RemoveHits() {
        if (!fData || fNHits <= 0) {
            fNHits = 0;
            return;
        }
        CheckIsLastView("RemoveHits");

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

    /// \brief Clears the whole storage (all the views) and empties this view.
    inline void ClearHits() {
        if (fData) fData->clear();
        fStartIdx = 0;
        fNHits = 0;
    }

    /// \brief Adds one hit at the end of the storage (nothing without storage). Only on the last view of
    /// the storage (std::logic_error otherwise, see the class description).
    /// \param position Hit position.
    /// \param energy Hit energy.
    /// \param time Hit time.
    /// \param type Coordinates defined by the hit.
    void AddHit(ROOT::Math::XYZVector position, double energy, double time, TRestHitsData::REST_HitType type);

    /// \brief Appends hits from another view (AddHit: only on the last view of the storage).
    void AddHits(const TRestHits& hits);

    /// \brief Returns index of most energetic hit in a range.
    /// \param n First hit (included).
    /// \param m Last hit (excluded).
    /// \return Hit index, -1 if no hit has a positive energy.
    int GetMostEnergeticHitInRange(int n, int m) const;

    /// \brief Maximum distance between two hits (see GetMaximumHitDistance2).
    double GetMaximumHitDistance() const;
    /// \brief Maximum squared distance between two hits: exact up to 500 hits, otherwise a farthest-point
    /// approximation (O(N)).
    double GetMaximumHitDistance2() const;

    /// \brief Merges hit m into hit n: energy-weighted position and time, summed energy; removes hit m.
    /// Only on the last view of the storage (std::logic_error otherwise).
    virtual void MergeHits(int n, int m);
    /// \brief Swaps two hits.
    virtual void SwapHits(int i, int j);
    /// \brief Removes one hit from the storage. Only on the last view of the storage (std::logic_error
    /// otherwise, see the class description).
    virtual void RemoveHit(int n);

    /// \brief Reorders the hits in-place, by default descending in energy (most energetic hit first, as
    /// legacy REST SortByEnergy, so that isSortedByEnergy() is true afterwards).
    /// Usage with another order: hits.Sort([&hits](int a, int b) { return hits.GetZ(a) < hits.GetZ(b); });
    virtual void Sort(std::function<bool(int, int)> compareCondition = nullptr);

    /// \brief Randomly permutes the hits in-place.
    /// \param NLoop Number of random swaps (gRandom).
    virtual void Shuffle(int NLoop);

    virtual bool areXY() const;   ///< True if there are hits and all of them are XY.
    virtual bool areXZ() const;   ///< True if there are hits and all of them are XZ.
    virtual bool areYZ() const;   ///< True if there are hits and all of them are YZ.
    virtual bool areXYZ() const;  ///< True if there are hits and all of them are XYZ.

    /// \brief True if any coordinate of the hit is NaN (e.g. an undefined coordinate).
    bool isNaN(int n) const;

    /// \brief Path length along the hits from the first one to hit n (projected positions, GetPosition).
    double GetDistanceToNode(int n);

    /// \brief True if the energies are in descending order (the default order of Sort()).
    bool isSortedByEnergy() const;

    /// \brief Returns number of hits in current view.
    inline size_t GetNumberOfHits() const { return (size_t)fNHits; }
    inline double GetX(int n) const { return fData->x[GetGlobalIdx(n)]; }  ///< X of a hit (NaN if undefined).
    inline double GetY(int n) const { return fData->y[GetGlobalIdx(n)]; }  ///< Y of a hit (NaN if undefined).
    inline double GetZ(int n) const { return fData->z[GetGlobalIdx(n)]; }  ///< Z of a hit (NaN if undefined).
    /// \brief Coordinate 0, 1, 2 (X, Y, Z) of a hit.
    inline double GetCoordinate(int n, int axis) const {
        return axis == 0 ? GetX(n) : axis == 1 ? GetY(n) : GetZ(n);
    }
    inline double GetTime(int n) const { return fData->time[GetGlobalIdx(n)]; }      ///< Time of a hit.
    inline double GetEnergy(int n) const { return fData->energy[GetGlobalIdx(n)]; }  ///< Energy of a hit.
    /// \brief Type (defined coordinates) of a hit.
    inline TRestHitsData::REST_HitType GetType(int n) const {
        return static_cast<TRestHitsData::REST_HitType>(fData->type[GetGlobalIdx(n)]);
    }

    /// \brief Position of a hit; for XY, XZ and YZ hits the undefined coordinate is 0.
    ROOT::Math::XYZVector GetPosition(int n) const;
    /// \brief Vector from hit j to hit i (GetPosition(i) - GetPosition(j)).
    inline ROOT::Math::XYZVector GetVector(int i, int j) { return GetPosition(i) - GetPosition(j); }

    /// \brief Number of hits defining the coordinates of `type` (hit type divisible by it).
    int GetNumberOfHitsByType(const TRestHitsData::REST_HitType type) const;
    /// \brief Number of hits defining X.
    inline int GetNumberOfHitsX() const { return GetNumberOfHitsByType(TRestHitsData::REST_HitType::X); }
    /// \brief Number of hits defining Y.
    inline int GetNumberOfHitsY() const { return GetNumberOfHitsByType(TRestHitsData::REST_HitType::Y); }

    double GetMeanPositionX() const;  ///< Energy-weighted mean X of the hits defining X (0 if none).
    double GetMeanPositionY() const;  ///< Energy-weighted mean Y of the hits defining Y (0 if none).
    double GetMeanPositionZ() const;  ///< Energy-weighted mean Z of the hits defining Z (0 if none).
    /// \brief Energy-weighted mean position (GetMeanPositionX/Y/Z).
    inline ROOT::Math::XYZVector GetMeanPosition() const {
        return {GetMeanPositionX(), GetMeanPositionY(), GetMeanPositionZ()};
    };

    double GetSigmaXY2() const;  ///< GetSigmaX2() + GetSigmaY2().
    double GetSigmaX2() const;   ///< Energy-weighted variance of X (hits defining X).
    double GetSigmaY2() const;   ///< Energy-weighted variance of Y (hits defining Y).
    inline double GetSigmaX() const { return std::sqrt(GetSigmaX2()); }  ///< Standard deviation of X.
    inline double GetSigmaY() const { return std::sqrt(GetSigmaY2()); }  ///< Standard deviation of Y.
    inline double GetSigmaZ() const { return std::sqrt(GetSigmaZ2()); }  ///< Standard deviation of Z.
    double GetSigmaZ2() const;  ///< Energy-weighted variance of Z (hits defining Z).
    double GetSkewXY() const;   ///< Energy-weighted skewness in XY (0 without energy or spread).
    double GetSkewZ() const;    ///< Energy-weighted skewness in Z (0 without energy or spread).

    /// \brief Total energy of the hits defining the coordinates of `type`.
    double GetEnergyByType(const TRestHitsData::REST_HitType type) const;
    /// \brief Energy of the hits defining X.
    double GetEnergyX() const { return GetEnergyByType(TRestHitsData::REST_HitType::X); }
    /// \brief Energy of the hits defining Y.
    double GetEnergyY() const { return GetEnergyByType(TRestHitsData::REST_HitType::Y); }

    double GetMaximumHitEnergy() const;  ///< Maximum hit energy (0 without hits).
    double GetMinimumHitEnergy() const;  ///< Minimum hit energy (0 without hits).
    double GetMeanHitEnergy() const;     ///< Mean hit energy (0 without hits).

    double GetTotalEnergy() const;  ///< Sum of the hit energies.
    /// \brief Squared distance between two hits (in their plane if all the hits are XY, XZ or YZ).
    double GetDistance2(int n, int m) const;
    /// \brief Distance between two hits (see GetDistance2).
    inline double GetDistance(int N, int M) const { return std::sqrt(GetDistance2(N, M)); }
    /// \brief Sum of the distances between consecutive hits.
    double GetTotalDistance() const;
    /// \brief Sum of the distances between consecutive hits from hit n to hit m (clamped to the view).
    double GetHitsPathLength(int n = 0, int m = 0) const;

    /// \brief Index of the hit closest to a position (GetPosition), 0 without hits.
    int GetClosestHit(ROOT::Math::XYZVector position);

    /// \brief Longitudinal and transversal components of `position` relative to hit m, along the segment
    /// from hit m to hit n.
    /// \return Pair (longitudinal, transversal).
    std::pair<double, double> GetProjection(int n, int m, ROOT::Math::XYZVector position);

    /// \brief Distance from `position` to the line through p0 with the given direction.
    double GetTransversalProjection(ROOT::Math::XYZVector p0, ROOT::Math::XYZVector direction,
                                    ROOT::Math::XYZVector position) const;

    /// \brief Prints position, energy and time of the first nHits hits (-1: all).
    virtual void PrintHits(int nHits = -1) const;

    /// \brief Hit positions and energies, the input of the TRestHitsAnalysis functions. The coordinates a
    /// hit does not define (by type, NaN in the position) are 0, so the distances between hits of the same
    /// type are those in their projection.
    std::vector<ROOT::Math::XYZVector> GetPoints() const;
    /// \brief Energy of every hit.
    std::vector<double> GetEnergies() const;
    /// \brief Coordinate 0, 1, 2 (X, Y, Z) of every hit (same order as GetEnergies).
    std::vector<double> GetCoordinates(int axis) const;

    /// Bins of a coordinate to draw it (GetBinning)
    struct Binning {
        double min = -1;  ///< Lower edge.
        double max = 1;   ///< Upper edge.
        int nBins = 1;    ///< Number of bins.
    };
    /// \brief Minimum distance between the different values of one coordinate (the readout pitch for strip
    /// coordinates), as legacy. 0 if there are less than two different values.
    static double GetMinimumDistance(std::vector<double> values);
    /// \brief Bins of one coordinate to draw it with the given bin size; if it is <= 0, the minimum distance
    /// between the values (GetMinimumDistance) or, if there is none, a tenth of their extent. The range
    /// leaves a margin of 25% of the extent of the values, and at least 10 bins, on each side, with the
    /// values at bin centres. No values give [-1, 1] with one bin.
    static Binning GetBinning(const std::vector<double>& values, double binSize = 0);

    /// \brief Draws the hits as TGraph markers in the pad (divided in one row): one graph per projection
    /// (XZ, YZ, XY) with hits defining both coordinates or, if there are none (1D hits), the energy
    /// against each defined coordinate.
    void DrawGraphs(TVirtualPad* pad, const std::string& title = "") const;

    /// \brief Draws the hits as histograms weighted by energy in the pad: a row with the projections
    /// (XZ, YZ, XY) with hits defining both coordinates, plus a 3D view when the hits define the three
    /// coordinates, and a row with the X, Y and Z distributions. Empty histograms are not drawn, so 1D,
    /// 2D and 3D hits only get the pads that apply. The bin size is the pitch or, if pitch <= 0, the
    /// minimum distance between hits in X and Y (the readout pitch), also used for Z. If it gives more than
    /// 200 bins over the hits range, the coordinate is taken as continuous (e.g. simulated hits): bins of
    /// 1 mm without pitch, and at most 200 bins per axis.
    /// The 3D view shows a TH3 weighted by energy with the hits defining the three coordinates, and the
    /// hits with one undefined coordinate (e.g. strips) as lines along it, coloured by energy, with the
    /// crossings of lines along different coordinates (the reconstructed positions, ghosts included).
    /// With markers3D (track view), the Z bin size is the spacing of the samples of the pulses (median
    /// distance between consecutive Z values of the same channel, if any channel has several), and the 3D
    /// view shows instead markers (TPolyMarker3D) colored by energy with the
    /// palette, on an empty TH3 for the axes: the hits defining the three coordinates and the hits with one
    /// undefined coordinate combined in each bin of their common coordinate, both projections sorted by
    /// their defined coordinate and paired monotonically by their accumulated energy fraction (no ghost
    /// combinations), the direction given by the sign of the slopes of both projections along the common
    /// coordinate (e.g. alpha tracks with XZ and YZ strips).
    void DrawHistograms(TVirtualPad* pad, const std::string& histOption = "", double pitch = 0,
                        bool markers3D = false) const;

    TRestHits() = default;           ///< Empty view without storage.
    virtual ~TRestHits() = default;  ///< Destructor (the storage is not owned).
};

#endif
