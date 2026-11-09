/// Unit tests of TRestHitsAnalysis (GoogleTest, BUILD_TESTING): clusters, shortest path, node reduction
/// (k-means and merging), line fits, track ends, gaussian sigma and twist, on small sets of points built in
/// the tests.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "TRestHitsAnalysis.h"

namespace HA = TRestHitsAnalysis;
using HA::XYZVector;

namespace {

/// Length of the open path through the points in the given order
double PathLength(const std::vector<XYZVector>& points, const std::vector<size_t>& order) {
    double length = 0;
    for (size_t i = 1; i < order.size(); i++) length += (points[order[i]] - points[order[i - 1]]).R();
    return length;
}

/// Points along x at the given positions (y = z = 0)
std::vector<XYZVector> PointsAlongX(const std::vector<double>& xs) {
    std::vector<XYZVector> points;
    for (double x : xs) points.emplace_back(x, 0, 0);
    return points;
}

void ExpectVectorNear(const XYZVector& actual, const XYZVector& expected, double tolerance) {
    EXPECT_NEAR(actual.X(), expected.X(), tolerance);
    EXPECT_NEAR(actual.Y(), expected.Y(), tolerance);
    EXPECT_NEAR(actual.Z(), expected.Z(), tolerance);
}

}  // namespace

TEST(HitsAnalysis, FindClusters) {
    // Two groups, interleaved in the input: {0, 2, 4} around x = 0 and {1, 3} around x = 10
    const auto points = PointsAlongX({0, 10, 1, 11, 2});
    const auto clusters = HA::FindClusters(points, 1.5);
    ASSERT_EQ(clusters.size(), 2u);
    EXPECT_EQ(clusters[0], (std::vector<size_t>{0, 2, 4}));
    EXPECT_EQ(clusters[1], (std::vector<size_t>{1, 3}));

    // Pairs at exactly the distance are not joined
    EXPECT_EQ(HA::FindClusters(points, 1).size(), 5u);
    // A distance that is not positive: every point is a cluster
    const auto single = HA::FindClusters(points, 0);
    ASSERT_EQ(single.size(), 5u);
    for (size_t i = 0; i < single.size(); i++) EXPECT_EQ(single[i], std::vector<size_t>{i});
    EXPECT_TRUE(HA::FindClusters({}, 1).empty());
}

TEST(HitsAnalysis, FindClustersWithSomeCoordinates) {
    // XZ hits: y is undefined (NaN) and ignored
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const std::vector<XYZVector> points = {{0, nan, 0}, {0.5, nan, 0.5}, {5, nan, 5}};
    const auto clusters = HA::FindClusters(points, 1, {true, false, true});
    ASSERT_EQ(clusters.size(), 2u);
    EXPECT_EQ(clusters[0], (std::vector<size_t>{0, 1}));
    EXPECT_EQ(clusters[1], (std::vector<size_t>{2}));

    // Only x: far in y, but the same cluster
    const std::vector<XYZVector> separated = {{0, 0, 0}, {0.5, 100, 0}};
    EXPECT_EQ(HA::FindClusters(separated, 1, {true, false, false}).size(), 1u);
    EXPECT_EQ(HA::FindClusters(separated, 1).size(), 2u);
}

TEST(HitsAnalysis, ShortestPathExact) {
    // Points of a line in a shuffled order: the path follows the line
    const auto points = PointsAlongX({3, 0, 4, 1, 2});
    const auto path = HA::GetShortestPath(points);
    ASSERT_EQ(path.size(), points.size());
    EXPECT_NEAR(PathLength(points, path), 4, 1e-12);
    const std::vector<size_t> forward = {1, 3, 4, 0, 2};
    const std::vector<size_t> backward(forward.rbegin(), forward.rend());
    EXPECT_TRUE(path == forward || path == backward);

    // Less than 3 points: the input order
    EXPECT_EQ(HA::GetShortestPath(PointsAlongX({5, 1})), (std::vector<size_t>{0, 1}));
}

TEST(HitsAnalysis, ShortestPathHeuristic) {
    // More points than maxExactPoints: nearest neighbour and 2-opt, exact for points of a line
    std::vector<double> xs;
    for (int i = 0; i < 30; i++) xs.push_back((i * 7) % 30);  // a permutation of 0..29
    const auto points = PointsAlongX(xs);
    const auto path = HA::GetShortestPath(points, false, 12);
    ASSERT_EQ(path.size(), points.size());
    std::vector<size_t> sorted = path;
    std::sort(sorted.begin(), sorted.end());
    for (size_t i = 0; i < sorted.size(); i++) EXPECT_EQ(sorted[i], i);
    EXPECT_NEAR(PathLength(points, path), 29, 1e-9);
}

TEST(HitsAnalysis, ShortestPathCyclic) {
    // The corners of a square: the closed path is its perimeter (no diagonals)
    const std::vector<XYZVector> points = {{0, 0, 0}, {1, 1, 0}, {1, 0, 0}, {0, 1, 0}};
    const auto path = HA::GetShortestPath(points, true);
    ASSERT_EQ(path.size(), 4u);
    const double closed = PathLength(points, path) + (points[path.back()] - points[path.front()]).R();
    EXPECT_NEAR(closed, 4, 1e-12);
}

TEST(HitsAnalysis, KMeansClustering) {
    // Two groups with the same energies: the nodes at their means
    const auto points = PointsAlongX({-1, 0, 1, 9, 10, 11});
    const std::vector<double> energies(points.size(), 2);
    const auto nodes = HA::KMeansClustering(points, energies, {{2, 0, 0}, {8, 0, 0}});
    ASSERT_EQ(nodes.size(), 2u);
    ExpectVectorNear(nodes[0].position, {0, 0, 0}, 1e-9);
    ExpectVectorNear(nodes[1].position, {10, 0, 0}, 1e-9);
    EXPECT_DOUBLE_EQ(nodes[0].energy, 6);
    EXPECT_DOUBLE_EQ(nodes[1].energy, 6);
    EXPECT_NEAR(nodes[0].sigma.X(), std::sqrt(2. / 3), 1e-9);
    EXPECT_NEAR(nodes[0].sigma.Y(), 0, 1e-12);

    // A node without points is removed
    const auto withEmpty = HA::KMeansClustering(points, energies, {{0, 0, 0}, {10, 0, 0}, {100, 0, 0}});
    EXPECT_EQ(withEmpty.size(), 2u);

    // Less than two centroids or no points
    EXPECT_TRUE(HA::KMeansClustering(points, energies, {{0, 0, 0}}).empty());
    EXPECT_TRUE(HA::KMeansClustering({}, {}, {{0, 0, 0}, {1, 0, 0}}).empty());
}

TEST(HitsAnalysis, KMeansClusteringFixedBoundaries) {
    const auto points = PointsAlongX({4, 5, 6});
    const std::vector<double> energies(points.size(), 1);
    const auto nodes = HA::KMeansClustering(points, energies, {{0, 0, 0}, {3, 0, 0}, {10, 0, 0}}, 100, true);
    // The first and last stay, without energy; the middle one takes all the points
    ASSERT_EQ(nodes.size(), 3u);
    ExpectVectorNear(nodes[0].position, {0, 0, 0}, 1e-12);
    EXPECT_EQ(nodes[0].energy, 0);
    ExpectVectorNear(nodes[1].position, {5, 0, 0}, 1e-9);
    EXPECT_DOUBLE_EQ(nodes[1].energy, 3);
    ExpectVectorNear(nodes[2].position, {10, 0, 0}, 1e-12);
    EXPECT_EQ(nodes[2].energy, 0);
}

TEST(HitsAnalysis, MergePoints) {
    const auto points = PointsAlongX({0, 0.5, 10, 10.5});
    const std::vector<double> energies(points.size(), 1);

    // Distance 1 merges the close pairs, then it grows to 2 (minimumDistance) and nothing else is merged
    const auto nodes = HA::MergePoints(points, energies, 1, 2, 2, 10);
    ASSERT_EQ(nodes.size(), 2u);
    ExpectVectorNear(nodes[0].position, {0.25, 0, 0}, 1e-12);
    ExpectVectorNear(nodes[1].position, {10.25, 0, 0}, 1e-12);
    EXPECT_DOUBLE_EQ(nodes[0].energy, 2);
    EXPECT_NEAR(nodes[0].sigma.X(), 0.25, 1e-12);

    // A single node: the distance grows until everything is merged
    const auto single = HA::MergePoints(points, energies, 1, 2, 2, 1);
    ASSERT_EQ(single.size(), 1u);
    ExpectVectorNear(single[0].position, {5.25, 0, 0}, 1e-12);
    EXPECT_DOUBLE_EQ(single[0].energy, 4);

    // The sigmas of the points are added in quadrature
    const std::vector<XYZVector> sigmas(points.size(), {0, 1, 0});
    const auto withSigmas = HA::MergePoints(points, energies, 1, 2, 2, 10, sigmas);
    ASSERT_EQ(withSigmas.size(), 2u);
    EXPECT_NEAR(withSigmas[0].sigma.Y(), 1, 1e-12);

    EXPECT_TRUE(HA::MergePoints({}, {}, 1, 2, 2, 10).empty());
    EXPECT_THROW(HA::MergePoints(points, energies, 0, 2, 2, 10), std::invalid_argument);
    EXPECT_THROW(HA::MergePoints(points, energies, 1, 2, 1, 10), std::invalid_argument);
}

TEST(HitsAnalysis, LineFit) {
    // Points of the diagonal x = y, from (0, 0) to (4, 4)
    std::vector<XYZVector> points;
    for (int i = 0; i <= 4; i++) points.emplace_back(i, i, 0);
    const std::vector<double> energies(points.size(), 1);

    XYZVector first, last;
    ASSERT_TRUE(HA::GetLine(points, energies, first, last));
    EXPECT_NEAR((last - first).R(), 4 * std::sqrt(2.), 1e-9);
    const XYZVector low = first.X() < last.X() ? first : last;
    const XYZVector high = first.X() < last.X() ? last : first;
    ExpectVectorNear(low, {0, 0, 0}, 1e-9);
    ExpectVectorNear(high, {4, 4, 0}, 1e-9);
    EXPECT_NEAR(HA::GetLineResidual(points, energies), 0, 1e-9);

    // Equally spaced nodes along the line
    const auto nodes = HA::GetLineNodes(points, energies, 5);
    ASSERT_EQ(nodes.size(), 5u);
    for (size_t i = 1; i < nodes.size(); i++) EXPECT_NEAR((nodes[i] - nodes[i - 1]).R(), std::sqrt(2.), 1e-9);
    EXPECT_TRUE(HA::GetLineNodes(points, energies, 1).empty());

    // No energy: no line
    const std::vector<double> noEnergy(points.size(), 0);
    EXPECT_FALSE(HA::GetLine(points, noEnergy, first, last));
    EXPECT_EQ(HA::GetLineResidual(points, noEnergy), -1);
    EXPECT_EQ(HA::GetLineResidual({}, {}), -1);
}

TEST(HitsAnalysis, LineResidual) {
    // Two rows at y = +-1 along x: the line is y = 0, at distance 1 of every point
    const std::vector<XYZVector> points = {{0, 1, 0}, {0, -1, 0}, {10, 1, 0}, {10, -1, 0}};
    const std::vector<double> energies(points.size(), 1);
    EXPECT_NEAR(HA::GetLineResidual(points, energies), 1, 1e-9);
}

TEST(HitsAnalysis, TrackEnds) {
    const auto points = PointsAlongX({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
    // More energy at the end (Bragg peak)
    std::vector<double> energies = {1, 1, 1, 1, 1, 1, 1, 1, 1, 6};
    XYZVector origin, end;
    const double asymmetry = HA::GetTrackEnds(points, energies, origin, end);
    ExpectVectorNear(origin, {0, 0, 0}, 1e-12);
    ExpectVectorNear(end, {9, 0, 0}, 1e-12);
    // First half 5, second half 10
    EXPECT_NEAR(asymmetry, 5. / 15, 1e-12);

    // Reversed
    std::reverse(energies.begin(), energies.end());
    HA::GetTrackEnds(points, energies, origin, end);
    ExpectVectorNear(origin, {9, 0, 0}, 1e-12);
    ExpectVectorNear(end, {0, 0, 0}, 1e-12);

    // No energy
    EXPECT_EQ(HA::GetTrackEnds(points, std::vector<double>(points.size(), 0), origin, end), 0);
}

TEST(HitsAnalysis, GaussSigma) {
    // Energy of a gaussian of sigma 3 against the coordinate
    std::vector<double> values, energies;
    for (int x = -10; x <= 10; x++) {
        values.push_back(x);
        energies.push_back(100 * std::exp(-x * x / 18.));
    }
    EXPECT_NEAR(HA::GetGaussSigma(values, energies), 3, 0.2);
    EXPECT_EQ(HA::GetGaussSigma({}, {}), 0);
}

TEST(HitsAnalysis, Twist) {
    // A straight line has no twist
    const auto line = PointsAlongX({0, 1, 2, 3});
    EXPECT_NEAR(HA::GetTwist(line), 0, 1e-12);
    EXPECT_NEAR(HA::GetTwistWeighted(line, {1, 2, 3, 4}), 0, 1e-12);

    // A right angle: (1 - cos 45) / 2 between the directions to the next point (GetTwist), and the angle of
    // the two segments, weighted by the energy differences (2), in GetTwistWeighted
    const std::vector<XYZVector> corner = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}};
    EXPECT_NEAR(HA::GetTwist(corner), (1 - std::sqrt(0.5)) / 2, 1e-12);
    EXPECT_NEAR(HA::GetTwistWeighted(corner, {1, 2, 1}), 1, 1e-12);

    // Two points: no inner points; one point: 0
    EXPECT_EQ(HA::GetTwist(PointsAlongX({0, 1})), -1);
    EXPECT_EQ(HA::GetTwist(PointsAlongX({0})), 0);
}
