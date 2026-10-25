#include "TRestHitsAnalysis.h"

#include <TF1.h>
#include <TFitResult.h>
#include <TGraphErrors.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <unordered_map>

using TRestHitsAnalysis::XYZVector;

namespace {
using Cell = std::array<long long, 3>;

struct CellHash {
    size_t operator()(const Cell& c) const {
        return std::hash<long long>()(c[0] * 73856093LL ^ c[1] * 19349663LL ^ c[2] * 83492791LL);
    }
};

double GetCoordinate(const XYZVector& p, int axis) { return axis == 0 ? p.X() : axis == 1 ? p.Y() : p.Z(); }

// Line that best fits the points: energy weighted centroid and principal axis of the energy weighted
// covariance (unit vector, null for a single point). False if there are no points or no energy
bool FitLine(const std::vector<XYZVector>& points, const std::vector<double>& energies, XYZVector& centroid,
             XYZVector& axis) {
    if (points.empty() || points.size() != energies.size()) return false;

    double totalEnergy = 0;
    centroid = XYZVector();
    for (size_t i = 0; i < points.size(); i++) {
        const double w = std::max(energies[i], 0.);
        totalEnergy += w;
        centroid += w * points[i];
    }
    if (totalEnergy <= 0) return false;
    centroid /= totalEnergy;

    double covariance[3][3] = {};
    for (size_t i = 0; i < points.size(); i++) {
        const double w = std::max(energies[i], 0.) / totalEnergy;
        const XYZVector d = points[i] - centroid;
        const double c[3] = {d.X(), d.Y(), d.Z()};
        for (int a = 0; a < 3; a++) {
            for (int b = 0; b < 3; b++) covariance[a][b] += w * c[a] * c[b];
        }
    }
    auto multiply = [&covariance](const XYZVector& v) {
        const double c[3] = {v.X(), v.Y(), v.Z()};
        double r[3] = {};
        for (int a = 0; a < 3; a++) {
            for (int b = 0; b < 3; b++) r[a] += covariance[a][b] * c[b];
        }
        return XYZVector(r[0], r[1], r[2]);
    };

    // Direction of largest spread (principal eigenvector, power iteration), starting from the column of
    // the covariance with the largest norm, which is not orthogonal to it. A single point gives no
    // direction
    axis = XYZVector();
    for (int a = 0; a < 3; a++) {
        const XYZVector column(covariance[0][a], covariance[1][a], covariance[2][a]);
        if (column.R() > axis.R()) axis = column;
    }
    if (axis.R() > 0) {
        axis = axis.Unit();
        for (int it = 0; it < 100; it++) {
            const XYZVector next = multiply(axis);
            if (next.R() == 0) break;
            const XYZVector unit = next.Unit();
            const bool converged = (unit - axis).R() < 1e-12;
            axis = unit;
            if (converged) break;
        }
    }
    return true;
}
}  // namespace

std::vector<std::vector<size_t>> TRestHitsAnalysis::FindClusters(const std::vector<XYZVector>& points, double distance,
                                                                 const std::array<bool, 3>& coordinates) {
    const double maxDistance2 = distance * distance;
    auto distance2 = [&](size_t a, size_t b) {
        double d2 = 0;
        for (int axis = 0; axis < 3; axis++) {
            if (!coordinates[axis]) continue;
            const double d = GetCoordinate(points[a], axis) - GetCoordinate(points[b], axis);
            d2 += d * d;
        }
        return d2;
    };

    // Cells of side distance along the enabled coordinates (0 along the others): the points closer than
    // distance to a point are in its cell or in the adjacent ones. With distance <= 0 no points are
    // joined, whatever the cell size
    const double cellSize = distance > 0 ? distance : 1.;
    auto getCell = [&](size_t n) {
        Cell cell = {0, 0, 0};
        for (int axis = 0; axis < 3; axis++) {
            if (coordinates[axis]) cell[axis] = (long long)std::floor(GetCoordinate(points[n], axis) / cellSize);
        }
        return cell;
    };

    std::unordered_map<Cell, std::vector<size_t>, CellHash> cells;
    for (size_t n = 0; n < points.size(); n++) cells[getCell(n)].push_back(n);

    std::vector<Cell> offsets = {{0, 0, 0}};
    for (int axis = 0; axis < 3; axis++) {
        if (!coordinates[axis]) continue;
        std::vector<Cell> expanded;
        for (const auto& offset : offsets) {
            for (int d = -1; d <= 1; d++) {
                Cell neighbour = offset;
                neighbour[axis] = d;
                expanded.push_back(neighbour);
            }
        }
        offsets = std::move(expanded);
    }

    std::vector<std::vector<size_t>> clusters;
    std::vector<bool> assigned(points.size(), false);
    for (size_t seed = 0; seed < points.size(); seed++) {
        if (assigned[seed]) continue;

        // Breadth-first search of the points closer than distance to any point already in the cluster
        std::vector<size_t> cluster = {seed};
        assigned[seed] = true;
        for (size_t q = 0; q < cluster.size(); q++) {
            const size_t point = cluster[q];
            const Cell cell = getCell(point);
            for (const auto& offset : offsets) {
                auto it = cells.find({cell[0] + offset[0], cell[1] + offset[1], cell[2] + offset[2]});
                if (it == cells.end()) continue;

                // Assigned points leave the cell, so they are not compared again
                auto& members = it->second;
                for (size_t m = 0; m < members.size();) {
                    const size_t j = members[m];
                    if (!assigned[j]) {
                        if (distance2(point, j) >= maxDistance2) {
                            m++;
                            continue;
                        }
                        assigned[j] = true;
                        cluster.push_back(j);
                    }
                    members[m] = members.back();
                    members.pop_back();
                }
            }
        }
        std::sort(cluster.begin(), cluster.end());
        clusters.push_back(std::move(cluster));
    }
    return clusters;
}

std::vector<TRestHitsAnalysis::Node> TRestHitsAnalysis::KMeansClustering(const std::vector<XYZVector>& points,
                                                                         const std::vector<double>& energies,
                                                                         const std::vector<XYZVector>& centroids,
                                                                         int maxIt, bool fixBoundaries) {
    const int nPoints = (int)points.size();
    const int nodes = (int)centroids.size();
    if (nodes < 2 || nPoints == 0) return {};

    auto isFixed = [&](int n) { return fixBoundaries && (n == 0 || n == nodes - 1); };

    std::vector<XYZVector> centroid = centroids, centroidOld(nodes);

    std::vector<std::vector<int>> clusterPoints(nodes);
    for (int it = 0; it < maxIt; it++) {
        for (auto& cluster : clusterPoints) cluster.clear();

        for (int i = 0; i < nPoints; i++) {
            double minDistance = std::numeric_limits<double>::max();
            int closest = -1;
            for (int n = 0; n < nodes; n++) {
                if (isFixed(n)) continue;
                const double d = (centroid[n] - points[i]).R();
                if (d < minDistance) {
                    minDistance = d;
                    closest = n;
                }
            }
            if (closest >= 0) clusterPoints[closest].push_back(i);
        }

        bool converge = true;
        for (int n = 0; n < nodes; n++) {
            if (isFixed(n)) continue;
            XYZVector mean;
            double totalEnergy = 0;
            for (int i : clusterPoints[n]) {
                mean += energies[i] * points[i];
                totalEnergy += energies[i];
            }
            if (totalEnergy > 0) centroid[n] = mean / totalEnergy;
            converge &= (centroid[n] - centroidOld[n]).R() < 1e-9;
            centroidOld[n] = centroid[n];
        }
        if (converge) break;
    }

    std::vector<Node> result;
    for (int n = 0; n < nodes; n++) {
        if (isFixed(n)) {
            result.push_back({centroid[n], 0, XYZVector()});
        } else if (!clusterPoints[n].empty()) {
            double energy = 0;
            double variance[3] = {};
            for (int i : clusterPoints[n]) {
                energy += energies[i];
                const XYZVector d = points[i] - centroid[n];
                variance[0] += energies[i] * d.X() * d.X();
                variance[1] += energies[i] * d.Y() * d.Y();
                variance[2] += energies[i] * d.Z() * d.Z();
            }
            auto sigma = [&](int a) { return energy > 0 ? std::sqrt(std::max(variance[a] / energy, 0.)) : 0; };
            result.push_back({centroid[n], energy, XYZVector(sigma(0), sigma(1), sigma(2))});
        }
    }
    return result;
}

bool TRestHitsAnalysis::GetLine(const std::vector<XYZVector>& points, const std::vector<double>& energies,
                                XYZVector& first, XYZVector& last) {
    XYZVector centroid, axis;
    if (!FitLine(points, energies, centroid, axis)) return false;

    double tMin = 0, tMax = 0;
    for (size_t i = 0; i < points.size(); i++) {
        const double t = (points[i] - centroid).Dot(axis);
        if (i == 0 || t < tMin) tMin = t;
        if (i == 0 || t > tMax) tMax = t;
    }
    first = centroid + tMin * axis;
    last = centroid + tMax * axis;
    return true;
}

std::vector<XYZVector> TRestHitsAnalysis::GetLineNodes(const std::vector<XYZVector>& points,
                                                       const std::vector<double>& energies, int nodes) {
    XYZVector first, last;
    if (nodes < 2 || !GetLine(points, energies, first, last)) return {};

    std::vector<XYZVector> result(nodes);
    for (int n = 0; n < nodes; n++) result[n] = first + (double)n / (nodes - 1) * (last - first);
    return result;
}

double TRestHitsAnalysis::GetLineResidual(const std::vector<XYZVector>& points, const std::vector<double>& energies) {
    XYZVector centroid, axis;
    if (!FitLine(points, energies, centroid, axis)) return -1;

    double sum = 0, totalEnergy = 0;
    for (size_t i = 0; i < points.size(); i++) {
        const double w = std::max(energies[i], 0.);
        const XYZVector d = points[i] - centroid;
        const XYZVector perpendicular = d - d.Dot(axis) * axis;
        sum += w * perpendicular.Mag2();
        totalEnergy += w;
    }
    return std::sqrt(sum / totalEnergy);
}

double TRestHitsAnalysis::GetMinimumDistance(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    double minDiff = 0;
    for (size_t i = 1; i < values.size(); i++) {
        const double diff = values[i] - values[i - 1];
        if (diff > 0 && (minDiff == 0 || diff < minDiff)) minDiff = diff;
    }
    return minDiff;
}

void TRestHitsAnalysis::GetBoundaries(const std::vector<double>& values, double binSize, double& max, double& min,
                                      int& nBins) {
    if (values.empty()) {
        min = -1;
        max = 1;
        nBins = 1;
        return;
    }
    min = *std::min_element(values.begin(), values.end());
    max = *std::max_element(values.begin(), values.end());
    if (binSize <= 0) binSize = max > min ? (max - min) / 10. : 1;

    // Margin in bins; the edges keep the values at bin centres (strips at multiples of the pitch)
    const int marginBins = std::max(10, (int)std::ceil(0.25 * (max - min) / binSize));
    nBins = (int)std::ceil((max - min) / binSize - 1e-6) + 1 + 2 * marginBins;
    min -= (marginBins + 0.5) * binSize;
    max = min + nBins * binSize;
}

double TRestHitsAnalysis::GetTrackEnds(const std::vector<XYZVector>& points, const std::vector<double>& energies,
                                       XYZVector& origin, XYZVector& end) {
    if (points.empty()) return 0;
    const XYZVector& first = points.front();
    const XYZVector& last = points.back();
    const XYZVector segment = last - first;
    const double length2 = segment.Mag2();

    // Energy of the halves closer to the first and to the last point
    double firstEnergy = 0, lastEnergy = 0;
    for (size_t i = 0; i < points.size(); i++) {
        const double fraction = length2 > 0 ? (points[i] - first).Dot(segment) / length2
                                            : (points.size() > 1 ? (double)i / (points.size() - 1) : 0);
        if (fraction < 0.5) {
            firstEnergy += energies[i];
        } else if (fraction > 0.5) {
            lastEnergy += energies[i];
        } else {
            firstEnergy += energies[i] / 2.;
            lastEnergy += energies[i] / 2.;
        }
    }

    if (firstEnergy > lastEnergy) {
        end = first;
        origin = last;
    } else {
        origin = first;
        end = last;
    }
    const double total = firstEnergy + lastEnergy;
    return total > 0 ? std::abs(lastEnergy - firstEnergy) / total : 0;
}

double TRestHitsAnalysis::GetGaussSigma(const std::vector<double>& values, const std::vector<double>& energies,
                                        double error, int nHitsMin) {
    const int nHits = (int)values.size();
    if (nHits == 0) return 0;

    const bool addPoints = nHits <= nHitsMin;
    const int nAdd = addPoints ? 2 : 0;
    const int nElems = nHits + nAdd;
    std::vector<double> x(nElems), y(nElems), ex(nElems, 0), ey(nElems);

    double xMin = std::numeric_limits<double>::max();
    double xMax = std::numeric_limits<double>::lowest();
    for (int n = 0, k = nAdd / 2; n < nHits; n++, k++) {
        x[k] = values[n];
        y[k] = energies[n];
        ey[k] = y[k] != 0 ? 10 * std::sqrt(std::abs(y[k])) : 0;
        xMin = std::min(xMin, x[k]);
        xMax = std::max(xMax, x[k]);
    }
    if (addPoints) {
        x.front() = xMin - 0.5;
        x.back() = xMax + 0.5;
        y.front() = y.back() = 0;
        ey.front() = ey.back() = error;
    }

    const auto maxIt = std::max_element(y.begin(), y.end());
    TGraphErrors graph(nElems, x.data(), y.data(), ex.data(), ey.data());
    TF1 fit("", "gaus");
    fit.SetParameter(0, *maxIt);
    fit.SetParameter(1, x[maxIt - y.begin()]);
    fit.SetParameter(2, (x.back() - x.front()) / 2.0);

    // Q: quiet; N: no plot; B: given start parameters; S: return the fit result
    TFitResultPtr fitResult = graph.Fit(&fit, "QNBS");
    if (!fitResult->IsValid()) return -1;
    // A sigma beyond the extent of the points is a diverged fit (a flat gaussian)
    const double sigma = std::abs(fit.GetParameter(2));
    return sigma <= x.back() - x.front() ? sigma : -1;
}

double TRestHitsAnalysis::GetTwist(const std::vector<XYZVector>& points, size_t n, size_t m) {
    if (m == 0) m = points.size();
    if (m < n + 2) return 0;

    double sum = 0;
    int count = 0;
    for (size_t N = n + 1; N + 1 < m; N++) {
        // Unit: null vector if the points coincide (as TVector3::Unit in legacy)
        const XYZVector a = (points[N - 1] - points[N + 1]).Unit();
        const XYZVector b = (points[N] - points[N + 1]).Unit();
        sum += (1 - a.Dot(b)) / 2.;
        count++;
    }
    return count == 0 ? -1 : sum / count;
}

double TRestHitsAnalysis::GetTwistWeighted(const std::vector<XYZVector>& points, const std::vector<double>& energies,
                                           size_t n, size_t m) {
    if (m == 0) m = points.size();
    if (m < n + 2) return 0;

    double sum = 0;
    int count = 0;
    for (size_t N = n + 1; N + 1 < m; N++) {
        const XYZVector a = (points[N - 1] - points[N]).Unit();
        const XYZVector b = (points[N] - points[N + 1]).Unit();
        const double weight = std::abs(energies[N - 1] - energies[N]) + std::abs(energies[N] - energies[N + 1]);
        sum += weight * (1 - a.Dot(b)) / 2.;
        count++;
    }
    return count == 0 ? -1 : sum / count;
}
