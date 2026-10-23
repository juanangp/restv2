#ifndef TRESTHITSANALYSIS_H
#define TRESTHITSANALYSIS_H

#include <array>
#include <cstddef>
#include <vector>

#include "Math/Vector3D.h"

/// This namespace defines generic functions to analyse sets of hits (or track nodes), given as positions
/// and, when needed, their energies (same order), so they can be used with any hits container (e.g.
/// TRestHits::GetPoints and TRestHits::GetEnergies).
///
/// 2D (or 1D) hits: the distances are computed with the three coordinates, so the undefined ones must
/// have the same value in all the points (TRestHits::GetPoints sets them to 0), which gives the distance
/// in the projection the hits define. FindClusters instead takes the coordinates to use.
namespace TRestHitsAnalysis {

using XYZVector = ROOT::Math::XYZVector;

/// \brief Groups the points in clusters: two points closer than `distance` belong to the same cluster,
/// and so do the points connected through a chain of such pairs. The distance only uses the coordinates
/// enabled in `coordinates` (e.g. {true, false, true} for XZ hits), the others are ignored (they can be
/// NaN, as in the hit positions). The points are placed in cells of side `distance`, so each point is only compared with those
/// of its cell and the adjacent ones (time growing with the number of points, not with its square).
/// \return The clusters as indices of `points`, each one in increasing order, ordered by their first
/// index. With distance <= 0 every point is a cluster.
std::vector<std::vector<size_t>> FindClusters(const std::vector<XYZVector>& points, double distance,
                                              const std::array<bool, 3>& coordinates = {true, true, true});

/// \brief A point with energy, e.g. a node of a reduced track.
struct Node {
    XYZVector position;
    double energy = 0;
};

/// \brief Reduces the points to `nodes` nodes with k-means: each node is the energy weighted mean of
/// the points closest to it, and its energy their sum. The nodes start equally spaced along the points
/// order. With `fixBoundaries`, the first and last nodes stay at the first and last points, with no
/// points nor energy.
/// \return The nodes in order, without the empty ones (except the fixed boundaries). Empty if nodes < 2
/// or there are no points.
std::vector<Node> KMeansClustering(const std::vector<XYZVector>& points, const std::vector<double>& energies,
                                   int nodes, int maxIt = 100, bool fixBoundaries = false);

/// \brief Minimum distance between the different values of one coordinate (the readout pitch for strip
/// coordinates), as legacy TRestHits::GetBoundaries. 0 if there are less than two different values.
double GetMinimumDistance(std::vector<double> values);

/// \brief Boundaries and bins of one coordinate with the given bin size (if <= 0, a tenth of the extent).
/// The range leaves a margin of 25% of the extent of the values, and at least 10 bins, on each side,
/// with the values at bin centres. No values give [-1, 1] with one bin.
void GetBoundaries(const std::vector<double>& values, double binSize, double& max, double& min, int& nBins);

/// \brief Origin and end of a track given by its points in order: the end is the extreme (first or last
/// point) closest to the most energetic point (the Bragg peak), the origin the other one.
void GetTrackEnds(const std::vector<XYZVector>& points, const std::vector<double>& energies, XYZVector& origin,
                  XYZVector& end);

/// \brief Size of the distribution of one coordinate of the points (`values`) from a gaussian fit of
/// the energy against the coordinate, as legacy TRestHits::GetGaussSigmaX. With less than `nHitsMin`
/// points, two points with no energy (and error `error`) are added 0.5 beyond the extremes, so the fit
/// of narrow distributions converges.
/// \return The fitted sigma, 0 without points, -1 if the fit fails or diverges (sigma larger than the
/// extent of the points).
double GetGaussSigma(const std::vector<double>& values, const std::vector<double>& energies,
                     double error = 150, int nHitsMin = 100000);

/// \brief Mean twist of the points [n, m) in order (m = 0: up to the last one), as legacy
/// TRestHits::GetHitsTwist: (1 - cos(angle)) / 2 at each inner point, from the directions to the next
/// point. 0 with less than 2 points, -1 with no inner points.
double GetTwist(const std::vector<XYZVector>& points, size_t n = 0, size_t m = 0);

/// \brief GetTwist weighted at each inner point with the energy differences with its neighbours, as
/// legacy TRestHits::GetHitsTwistWeighted.
double GetTwistWeighted(const std::vector<XYZVector>& points, const std::vector<double>& energies, size_t n = 0,
                        size_t m = 0);

}  // namespace TRestHitsAnalysis

#endif
