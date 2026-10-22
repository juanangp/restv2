#include "TRestHits.h"

#include <TColor.h>
#include <TGraph.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TH3F.h>
#include <TMath.h>
#include <TPolyLine3D.h>
#include <TPolyMarker3D.h>
#include <TVirtualPad.h>

#include "TRestEvent.h"


using REST_HitType = TRestHitsData::REST_HitType;

bool TRestHits::areXY() const {
    if (fNHits == 0) return false;
    for (int i = 0; i < fNHits; ++i) {
        const auto type = static_cast<TRestHitsData::REST_HitType>(fData->type[GetGlobalIdx(i)]);
        if (type != TRestHitsData::XY) return false;
    }
    return true;
}

bool TRestHits::areXZ() const {
    if (fNHits == 0) return false;
    for (int i = 0; i < fNHits; ++i) {
        const auto type = static_cast<TRestHitsData::REST_HitType>(fData->type[GetGlobalIdx(i)]);
        if (type != TRestHitsData::XZ) return false;
    }
    return true;
}

bool TRestHits::areYZ() const {
    if (fNHits == 0) return false;
    for (int i = 0; i < fNHits; ++i) {
        const auto type = static_cast<TRestHitsData::REST_HitType>(fData->type[GetGlobalIdx(i)]);
        if (type != TRestHitsData::YZ) return false;
    }
    return true;
}

bool TRestHits::areXYZ() const {
    if (fNHits == 0) return false;
    for (int i = 0; i < fNHits; ++i) {
         const auto type = static_cast<TRestHitsData::REST_HitType>(fData->type[GetGlobalIdx(i)]);
        if (type != TRestHitsData::XYZ) return false;
    }
    return true;
}

void TRestHits::AddHit(ROOT::Math::XYZVector position, double energy, double time, REST_HitType type) {
    if (!fData) return;
    fData->x.push_back((float)position.X());
    fData->y.push_back((float)position.Y());
    fData->z.push_back((float)position.Z());
    fData->energy.push_back((float)energy);
    fData->time.push_back((float)time);
    fData->type.push_back(static_cast<int>(type));

    fNHits++;
}

void TRestHits::AddHits(const TRestHits& hits) {
    for (size_t i = 0; i < hits.GetNumberOfHits(); ++i) {
        AddHit(hits.GetPosition(i), hits.GetEnergy(i), hits.GetTime(i), hits.GetType(i));
    }
}

bool TRestHits::isNaN(int n) const {
    int idx = GetGlobalIdx(n);
    return (TMath::IsNaN(fData->x[idx]) || TMath::IsNaN(fData->y[idx]) || TMath::IsNaN(fData->z[idx]));
}

void TRestHits::Translate(int n, double x, double y, double z) {
    int idx = GetGlobalIdx(n);
    fData->x[idx] += x;
    fData->y[idx] += y;
    fData->z[idx] += z;
}

void TRestHits::RotateIn3D(int n, double alpha, double beta, double gamma, ROOT::Math::XYZVector vMean) {
    auto vHit = GetPosition(n) - vMean;

    vHit = ROOT::Math::VectorUtil::RotateZ(vHit, gamma);
    vHit = ROOT::Math::VectorUtil::RotateY(vHit, beta);
    vHit = ROOT::Math::VectorUtil::RotateX(vHit, alpha);

    int idx = GetGlobalIdx(n);
    fData->x[idx] = vHit.X() + vMean.X();
    fData->y[idx] = vHit.Y() + vMean.Y();
    fData->z[idx] = vHit.Z() + vMean.Z();
}

double TRestHits::GetMaximumHitEnergy() const {
    if (fNHits == 0) return 0;
    float maxE = fData->energy[fStartIdx];
    for (int i = 1; i < fNHits; ++i) {
        maxE = std::max(maxE, fData->energy[GetGlobalIdx(i)]);
    }
    return maxE;
}

double TRestHits::GetMinimumHitEnergy() const {
    if (fNHits == 0) return 0;
    float minE = fData->energy[fStartIdx];
    for (int i = 1; i < fNHits; ++i) {
        minE = std::min(minE, fData->energy[GetGlobalIdx(i)]);
    }
    return minE;
}

double TRestHits::GetTotalEnergy() const {
    double total = 0;
    for (int i = 0; i < fNHits; ++i) total += fData->energy[GetGlobalIdx(i)];
    return total;
}

double TRestHits::GetMeanHitEnergy() const { return fNHits > 0 ? GetTotalEnergy() / fNHits : 0; }

void TRestHits::MergeHits(int n, int m) {
    int idxN = GetGlobalIdx(n);
    int idxM = GetGlobalIdx(m);

    double enN = fData->energy[idxN];
    double enM = fData->energy[idxM];
    double totalEnergy = enN + enM;

    if (totalEnergy > 0) {
        fData->x[idxN] = (fData->x[idxN] * enN + fData->x[idxM] * enM) / totalEnergy;
        fData->y[idxN] = (fData->y[idxN] * enN + fData->y[idxM] * enM) / totalEnergy;
        fData->z[idxN] = (fData->z[idxN] * enN + fData->z[idxM] * enM) / totalEnergy;
        fData->time[idxN] = (fData->time[idxN] * enN + fData->time[idxM] * enM) / totalEnergy;
        fData->energy[idxN] = (float)totalEnergy;
    }
    RemoveHit(m);
}

void TRestHits::SwapHits(int i, int j) {
    int idxI = GetGlobalIdx(i);
    int idxJ = GetGlobalIdx(j);
    std::swap(fData->x[idxI], fData->x[idxJ]);
    std::swap(fData->y[idxI], fData->y[idxJ]);
    std::swap(fData->z[idxI], fData->z[idxJ]);
    std::swap(fData->energy[idxI], fData->energy[idxJ]);
    std::swap(fData->time[idxI], fData->time[idxJ]);
    std::swap(fData->type[idxI], fData->type[idxJ]);
}

void TRestHits::RemoveHit(int n) {
    int idx = GetGlobalIdx(n);
    fData->x.erase(fData->x.begin() + idx);
    fData->y.erase(fData->y.begin() + idx);
    fData->z.erase(fData->z.begin() + idx);
    fData->energy.erase(fData->energy.begin() + idx);
    fData->time.erase(fData->time.begin() + idx);
    fData->type.erase(fData->type.begin() + idx);
    fNHits--;
}

void TRestHits::Shuffle(int NLoop) {
    if (!fData || fNHits < 2) return;

    auto order = TRestHitsUtils::BuildShuffleOrder(fNHits, NLoop);

    TRestHitsUtils::ApplyPermutation(order, fStartIdx, fData->x, fData->y, fData->z, fData->time,
                                     fData->energy, fData->type);
}

void TRestHits::Sort(std::function<bool(int, int)> compareCondition) {
    if (!fData || fNHits < 2) return;

    auto order = TRestHitsUtils::BuildSortOrder(
        fNHits, compareCondition, [this](int a, int b) { return GetEnergy(a) < GetEnergy(b); });

    TRestHitsUtils::ApplyPermutation(order, fStartIdx, fData->x, fData->y, fData->z, fData->time,
                                     fData->energy, fData->type);
}

ROOT::Math::XYZVector TRestHits::GetPosition(int n) const {
    int idx = GetGlobalIdx(n);
    const auto type = static_cast<TRestHitsData::REST_HitType>(fData->type[idx]);
    double x = fData->x[idx];
    double y = fData->y[idx];
    double z = fData->z[idx];

    if (type == TRestHitsData::XY) return {x, y, 0};
    if (type == TRestHitsData::XZ) return {x, 0, z};
    if (type == TRestHitsData::YZ) return {0, y, z};
    return {x, y, z};
}

int TRestHits::GetNumberOfHitsByType(const REST_HitType type) const {
    int nHits = 0;
    for (int i = 0; i < fNHits; ++i) {
        if (fData->type[GetGlobalIdx(i)] % type == 0) nHits++;
    }
    return nHits;
}

double TRestHits::GetEnergyByType(const REST_HitType type) const {
    double totalEnergy = 0;
    for (int i = 0; i < fNHits; ++i) {
        int idx = GetGlobalIdx(i);
        if (fData->type[idx] % type == 0) totalEnergy += fData->energy[idx];
    }
    return totalEnergy;
}

double TRestHits::GetMeanPositionX() const {
    double mean = 0, totalEnergy = 0;
    for (int i = 0; i < fNHits; ++i) {
        int idx = GetGlobalIdx(i);
        const auto type = static_cast<TRestHitsData::REST_HitType>(fData->type[idx]);
        if (type % TRestHitsData::X == 0) {
            mean += fData->x[idx] * fData->energy[idx];
            totalEnergy += fData->energy[idx];
        }
    }
    return totalEnergy > 0 ? mean / totalEnergy : 0;
}

double TRestHits::GetMeanPositionY() const {
    double mean = 0, totalEnergy = 0;
    for (int i = 0; i < fNHits; ++i) {
        int idx = GetGlobalIdx(i);
        const auto type = static_cast<TRestHitsData::REST_HitType>(fData->type[idx]);
        if (type % TRestHitsData::Y == 0) {
            mean += fData->y[idx] * fData->energy[idx];
            totalEnergy += fData->energy[idx];
        }
    }
    return totalEnergy > 0 ? mean / totalEnergy : 0;
}

double TRestHits::GetMeanPositionZ() const {
    double mean = 0, totalEnergy = 0;
    for (int i = 0; i < fNHits; ++i) {
        int idx = GetGlobalIdx(i);
        const auto type = static_cast<TRestHitsData::REST_HitType>(fData->type[idx]);
        if (type % TRestHitsData::Z == 0) {
            mean += fData->z[idx] * fData->energy[idx];
            totalEnergy += fData->energy[idx];
        }
    }
    return totalEnergy > 0 ? mean / totalEnergy : 0;
}

// --- Sigma / skewness ---

double TRestHits::GetSigmaX2() const {
    const double meanX = GetMeanPositionX();
    double sigma2 = 0, totalEnergy = 0;
    for (int i = 0; i < fNHits; ++i) {
        int idx = GetGlobalIdx(i);
        if (fData->type[idx] % TRestHitsData::X == 0) {
            sigma2 += fData->energy[idx] * (meanX - fData->x[idx]) * (meanX - fData->x[idx]);
            totalEnergy += fData->energy[idx];
        }
    }
    return totalEnergy > 0 ? sigma2 / totalEnergy : 0;
}

double TRestHits::GetSigmaY2() const {
    const double meanY = GetMeanPositionY();
    double sigma2 = 0, totalEnergy = 0;
    for (int i = 0; i < fNHits; ++i) {
        int idx = GetGlobalIdx(i);
        if (fData->type[idx] % TRestHitsData::Y == 0) {
            sigma2 += fData->energy[idx] * (meanY - fData->y[idx]) * (meanY - fData->y[idx]);
            totalEnergy += fData->energy[idx];
        }
    }
    return totalEnergy > 0 ? sigma2 / totalEnergy : 0;
}

double TRestHits::GetSigmaZ2() const {
    const double meanZ = GetMeanPositionZ();
    double sigma2 = 0, totalEnergy = 0;
    for (int i = 0; i < fNHits; ++i) {
        int idx = GetGlobalIdx(i);
        if (fData->type[idx] % TRestHitsData::Z == 0) {
            sigma2 += fData->energy[idx] * (meanZ - fData->z[idx]) * (meanZ - fData->z[idx]);
            totalEnergy += fData->energy[idx];
        }
    }
    return totalEnergy > 0 ? sigma2 / totalEnergy : 0;
}

double TRestHits::GetSigmaXY2() const { return GetSigmaX2() + GetSigmaY2(); }

double TRestHits::GetSkewXY() const {
    const double totalEnergy = GetTotalEnergy();
    const double sigmaXY = std::sqrt(GetSigmaXY2());
    if (totalEnergy <= 0 || sigmaXY <= 0) return 0;

    const double meanX = GetMeanPositionX();
    const double meanY = GetMeanPositionY();
    double skew = 0;
    for (int i = 0; i < fNHits; ++i) {
        int idx = GetGlobalIdx(i);
        if (fData->type[idx] % TRestHitsData::X == 0)
            skew += fData->energy[idx] * std::pow(meanX - fData->x[idx], 3);
        if (fData->type[idx] % TRestHitsData::Y == 0)
            skew += fData->energy[idx] * std::pow(meanY - fData->y[idx], 3);
    }
    return skew / (totalEnergy * sigmaXY * sigmaXY * sigmaXY);
}

double TRestHits::GetSkewZ() const {
    const double totalEnergy = GetTotalEnergy();
    const double sigmaZ = std::sqrt(GetSigmaZ2());
    if (totalEnergy <= 0 || sigmaZ <= 0) return 0;

    const double meanZ = GetMeanPositionZ();
    double skew = 0;
    for (int i = 0; i < fNHits; ++i) {
        int idx = GetGlobalIdx(i);
        if (fData->type[idx] % TRestHitsData::Z == 0)
            skew += fData->energy[idx] * std::pow(meanZ - fData->z[idx], 3);
    }
    return skew / (totalEnergy * sigmaZ * sigmaZ * sigmaZ);
}

// --- Distance / geometry ---

double TRestHits::GetDistance2(int n, int m) const {
    double dx = GetX(n) - GetX(m);
    double dy = GetY(n) - GetY(m);
    double dz = GetZ(n) - GetZ(m);
    if (areXY()) return dx * dx + dy * dy;
    if (areXZ()) return dx * dx + dz * dz;
    if (areYZ()) return dy * dy + dz * dz;
    return dx * dx + dy * dy + dz * dz;
}

double TRestHits::GetTotalDistance() const {
    double distance = 0;
    for (int i = 0; i + 1 < fNHits; ++i) distance += std::sqrt(GetDistance2(i, i + 1));
    return distance;
}

double TRestHits::GetHitsPathLength(int n, int m) const {
    if (n < 0) n = 0;
    if (m > fNHits - 1) m = fNHits - 1;

    double distance = 0;
    for (int i = n; i < m; ++i) distance += std::sqrt(GetDistance2(i, i + 1));
    return distance;
}

double TRestHits::GetDistanceToNode(int n) {
    if (n > fNHits - 1) n = fNHits - 1;

    double distance = 0;
    for (int hit = 0; hit < n; ++hit) distance += GetVector(hit + 1, hit).R();
    return distance;
}

double TRestHits::GetMaximumHitDistance2() const {
    if (fNHits < 2) return 0;


    constexpr int kExactThreshold = 500;
    if (fNHits <= kExactThreshold) {
        double maxDistance = 0;
        for (int n = 0; n < fNHits; ++n) {
            for (int m = n + 1; m < fNHits; ++m) {
                double d = GetDistance2(n, m);
                if (d > maxDistance) maxDistance = d;
            }
        }
        return maxDistance;
    }

    // Heuristic "farthest-point" O(n)
    auto farthestFrom = [this](int from) {
        int best = from;
        double bestDist = 0;
        for (int i = 0; i < fNHits; ++i) {
            double d = GetDistance2(from, i);
            if (d > bestDist) { bestDist = d; best = i; }
        }
        return std::pair<int, double>{best, bestDist};
    };

    auto [a, _] = farthestFrom(0);
    auto [b, maxDistance] = farthestFrom(a);

    return maxDistance;
}

double TRestHits::GetMaximumHitDistance() const { return std::sqrt(GetMaximumHitDistance2()); }

int TRestHits::GetMostEnergeticHitInRange(int n, int m) const {
    double maxEnergy = 0;
    int hit = -1;
    for (int i = n; i < m; ++i) {
        if (GetEnergy(i) > maxEnergy) {
            maxEnergy = GetEnergy(i);
            hit = i;
        }
    }
    return hit;
}

int TRestHits::GetClosestHit(ROOT::Math::XYZVector position) {
    int closestHit = 0;
    double minDistance = 1.e30;
    for (int n = 0; n < fNHits; ++n) {
        double distance = (position - GetPosition(n)).Mag2();
        if (distance < minDistance) {
            closestHit = n;
            minDistance = distance;
        }
    }
    return closestHit;
}

std::pair<double, double> TRestHits::GetProjection(int n, int m, ROOT::Math::XYZVector position) {
    auto nodesSegment = GetVector(n, m);
    auto origin = position - GetPosition(m);

    if (origin.Mag2() == 0) return {0, 0};

    double segmentMag = std::sqrt(nodesSegment.Mag2());
    double longitudinal = (segmentMag > 0) ? (nodesSegment.Dot(origin) / segmentMag) : 0;

    if (origin == nodesSegment) return {longitudinal, 0};

    double transversal = std::sqrt(origin.Mag2() - longitudinal * longitudinal);
    return {longitudinal, transversal};
}

double TRestHits::GetTransversalProjection(ROOT::Math::XYZVector p0, ROOT::Math::XYZVector direction,
                                           ROOT::Math::XYZVector position) const {
    auto oX = position - p0;
    if (oX.Mag2() == 0) return 0;

    double dirMag = std::sqrt(direction.Mag2());
    double longitudinal = (dirMag > 0) ? (direction.Dot(oX) / dirMag) : 0;

    return std::sqrt(oX.Mag2() - longitudinal * longitudinal);
}

// --- Misc ---

bool TRestHits::isSortedByEnergy() const {
    for (int i = 0; i + 1 < fNHits; ++i) {
        if (GetEnergy(i + 1) > GetEnergy(i)) return false;
    }
    return true;
}

void TRestHits::PrintHits(Int_t nHits) const {
    int N = nHits;

    if (N == -1) N = GetNumberOfHits();
    if (N > (int)GetNumberOfHits()) N = GetNumberOfHits();

    for (int n = 0; n < N; n++) {
        std::cout << "Hit " << n << " X: " << GetX(n) << " Y: " << GetY(n) << " Z: " << GetZ(n)
             << " Energy: " << GetEnergy(n) << " T: " << GetTime(n);
        std::cout << std::endl;
    }
}

double TRestHits::GetMinimumDistance(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    double minDiff = 0;
    for (size_t i = 1; i < values.size(); i++) {
        const double diff = values[i] - values[i - 1];
        if (diff > 0 && (minDiff == 0 || diff < minDiff)) minDiff = diff;
    }
    return minDiff;
}

void TRestHits::GetBoundaries(const std::vector<double>& values, double binSize, double& max, double& min,
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

namespace {
/// Coordinates as indices 0, 1, 2 (X, Y, Z): factor in the hit type and axis title
const REST_HitType kAxisTypes[3] = {REST_HitType::X, REST_HitType::Y, REST_HitType::Z};
const char* kAxisTitles[3] = {"X-axis (mm)", "Y-axis (mm)", "Z-axis (mm)"};
/// Projection planes XZ, YZ and XY (as legacy), as pairs of coordinates
const int kPlanes[3][2] = {{0, 2}, {1, 2}, {0, 1}};
/// Maximum number of bins of the 3D histogram, bins are merged above it
const double kMax3DBins = 1e6;

bool IsDefined(int type, int axis) { return type % kAxisTypes[axis] == 0; }

double GetCoordinate(const TRestHits& hits, int n, int axis) {
    return axis == 0 ? hits.GetX(n) : axis == 1 ? hits.GetY(n) : hits.GetZ(n);
}

/// Margins so the axis titles are not clipped
void SetPadMargins(TVirtualPad* pad) {
    pad->SetBottomMargin(0.13);
    pad->SetLeftMargin(0.15);
}

/// 3D view in the current pad, see TRestHits::DrawHistograms
void Draw3D(const TRestHits& hits, const double min[3], const double max[3], const int nBins[3]) {
    const double totalBins = (double)nBins[0] * nBins[1] * nBins[2];
    const int merge = totalBins > kMax3DBins ? (int)std::ceil(std::cbrt(totalBins / kMax3DBins)) : 1;

    // Also the frame of the lines when there are no hits with the three coordinates
    auto* h3 = new TH3F("XYZ", "", std::max(1, nBins[0] / merge), min[0], max[0],
                        std::max(1, nBins[1] / merge), min[1], max[1], std::max(1, nBins[2] / merge), min[2],
                        max[2]);
    for (size_t n = 0; n < hits.GetNumberOfHits(); n++) {
        if (hits.GetType(n) % REST_HitType::XYZ == 0)
            h3->Fill(hits.GetX(n), hits.GetY(n), hits.GetZ(n), hits.GetEnergy(n));
    }
    TRestEvent::DrawOwned(h3, h3->GetEntries() > 0 ? "box2" : "", kAxisTitles[0], kAxisTitles[1],
                          kAxisTitles[2]);

    const double eMin = hits.GetMinimumHitEnergy(), eMax = hits.GetMaximumHitEnergy();
    const int nColors = TColor::GetNumberOfColors();
    auto energyColor = [&](double e) {
        const int ci = eMax > eMin ? (int)((e - eMin) / (eMax - eMin) * (nColors - 1)) : 0;
        return TColor::GetColorPalette(ci);
    };

    // Hits with one undefined coordinate: a line along it. Hits with only one defined coordinate (planes)
    // are not drawn
    std::vector<std::pair<int, int>> lines;  // {hit, undefined coordinate}
    for (size_t n = 0; n < hits.GetNumberOfHits(); n++) {
        int undefined = -1, nDefined = 0;
        for (int a = 0; a < 3; a++) {
            if (IsDefined(hits.GetType(n), a)) {
                nDefined++;
            } else {
                undefined = a;
            }
        }
        if (nDefined != 2) continue;

        double p0[3], p1[3];
        for (int a = 0; a < 3; a++) p0[a] = p1[a] = GetCoordinate(hits, n, a);
        p0[undefined] = min[undefined];
        p1[undefined] = max[undefined];

        auto* line = new TPolyLine3D(2);
        line->SetPoint(0, p0[0], p0[1], p0[2]);
        line->SetPoint(1, p1[0], p1[1], p1[2]);
        line->SetLineColor(energyColor(hits.GetEnergy(n)));
        line->SetBit(TObject::kCanDelete);
        line->Draw();
        lines.emplace_back(n, undefined);
    }

    // Lines along different coordinates cross where their common coordinate matches (as legacy for
    // XZ and YZ strips, within 5% of the range)
    for (size_t i = 0; i < lines.size(); i++) {
        for (size_t j = i + 1; j < lines.size(); j++) {
            const auto [a, undefinedA] = lines[i];
            const auto [b, undefinedB] = lines[j];
            if (undefinedA == undefinedB) continue;
            const int common = 3 - undefinedA - undefinedB;
            const double ca = GetCoordinate(hits, a, common), cb = GetCoordinate(hits, b, common);
            if (std::abs(ca - cb) > 0.05 * (max[common] - min[common])) continue;

            double point[3];
            point[undefinedA] = GetCoordinate(hits, b, undefinedA);
            point[undefinedB] = GetCoordinate(hits, a, undefinedB);
            point[common] = 0.5 * (ca + cb);

            auto* marker = new TPolyMarker3D(1);
            marker->SetPoint(0, point[0], point[1], point[2]);
            marker->SetMarkerStyle(20);
            marker->SetMarkerSize(1.2);
            marker->SetMarkerColor(energyColor(0.5 * (hits.GetEnergy(a) + hits.GetEnergy(b))));
            marker->SetBit(TObject::kCanDelete);
            marker->Draw();
        }
    }
}
}  // namespace

void TRestHits::DrawGraphs(TVirtualPad* pad, const std::string& title) const {
    struct GraphData {
        std::vector<double> u, v;
        const char* xTitle;
        const char* yTitle;
        Color_t color;
    };
    std::vector<GraphData> graphs;

    const Color_t planeColors[3] = {kBlue, kRed, kBlack};
    for (int p = 0; p < 3; p++) {
        const int a = kPlanes[p][0], b = kPlanes[p][1];
        GraphData graph{{}, {}, kAxisTitles[a], kAxisTitles[b], planeColors[p]};
        for (size_t n = 0; n < GetNumberOfHits(); n++) {
            if (!IsDefined(GetType(n), a) || !IsDefined(GetType(n), b)) continue;
            graph.u.push_back(GetCoordinate(*this, n, a));
            graph.v.push_back(GetCoordinate(*this, n, b));
        }
        if (!graph.u.empty()) graphs.push_back(std::move(graph));
    }

    // 1D hits: energy against each coordinate
    if (graphs.empty()) {
        for (int a = 0; a < 3; a++) {
            GraphData graph{{}, {}, kAxisTitles[a], "Energy", kBlack};
            for (size_t n = 0; n < GetNumberOfHits(); n++) {
                if (!IsDefined(GetType(n), a)) continue;
                graph.u.push_back(GetCoordinate(*this, n, a));
                graph.v.push_back(GetEnergy(n));
            }
            if (!graph.u.empty()) graphs.push_back(std::move(graph));
        }
    }
    if (graphs.empty()) return;

    pad->Divide(graphs.size(), 1);
    for (size_t i = 0; i < graphs.size(); i++) {
        pad->cd(i + 1);
        SetPadMargins(gPad);
        auto* graph = new TGraph(graphs[i].u.size(), graphs[i].u.data(), graphs[i].v.data());
        graph->SetTitle(title.c_str());
        graph->SetMarkerColor(graphs[i].color);
        graph->SetMarkerSize(0.3);
        graph->SetMarkerStyle(20);
        TRestEvent::DrawOwned(graph, "AP", graphs[i].xTitle, graphs[i].yTitle);
    }
}

void TRestHits::DrawHistograms(TVirtualPad* pad, const std::string& histOption, double pitch) const {
    std::vector<double> values[3];
    for (size_t n = 0; n < GetNumberOfHits(); n++) {
        for (int a = 0; a < 3; a++) {
            if (IsDefined(GetType(n), a)) values[a].push_back(GetCoordinate(*this, n, a));
        }
    }

    // Without a pitch, X and Y use the minimum distance between their hits (the readout pitch). Z has no
    // natural pitch (the drift gives almost continuous values), so it uses the X/Y bin size
    double binSize[3];
    binSize[0] = pitch > 0 ? pitch : GetMinimumDistance(values[0]);
    binSize[1] = pitch > 0 ? pitch : GetMinimumDistance(values[1]);
    if (binSize[0] <= 0) binSize[0] = binSize[1];
    if (binSize[1] <= 0) binSize[1] = binSize[0];
    binSize[2] = pitch > 0 ? pitch : std::min(binSize[0], binSize[1]);
    if (binSize[2] <= 0) binSize[2] = GetMinimumDistance(values[2]);

    double min[3], max[3];
    int nBins[3];
    for (int a = 0; a < 3; a++) GetBoundaries(values[a], binSize[a], max[a], min[a], nBins[a]);

    struct Item {
        TH1* hist;
        const char* xTitle;
        const char* yTitle;
    };
    std::vector<Item> projections, distributions;

    const char* planeNames[3] = {"XZ", "YZ", "XY"};
    for (int p = 0; p < 3; p++) {
        const int a = kPlanes[p][0], b = kPlanes[p][1];
        auto* h = new TH2F(planeNames[p], "", nBins[a], min[a], max[a], nBins[b], min[b], max[b]);
        h->SetDirectory(nullptr);
        for (size_t n = 0; n < GetNumberOfHits(); n++) {
            if (IsDefined(GetType(n), a) && IsDefined(GetType(n), b))
                h->Fill(GetCoordinate(*this, n, a), GetCoordinate(*this, n, b), GetEnergy(n));
        }
        // Histograms not drawn are not owned by any pad
        if (h->GetEntries() == 0) {
            delete h;
            continue;
        }
        projections.push_back({h, kAxisTitles[a], kAxisTitles[b]});
    }

    const char* axisNames[3] = {"X", "Y", "Z"};
    for (int a = 0; a < 3; a++) {
        if (values[a].empty()) continue;
        auto* h = new TH1F(axisNames[a], "", nBins[a], min[a], max[a]);
        h->SetDirectory(nullptr);
        for (size_t n = 0; n < GetNumberOfHits(); n++) {
            if (IsDefined(GetType(n), a)) h->Fill(GetCoordinate(*this, n, a), GetEnergy(n));
        }
        // The weighted fill stores the errors, ROOT would draw them as error bars
        h->Sumw2(false);
        distributions.push_back({h, kAxisTitles[a], "Energy"});
    }

    // Only with hits defining at least two coordinates, so it shows points or lines
    const bool draw3D = !projections.empty() && !values[0].empty() && !values[1].empty() && !values[2].empty();

    const int nFirstRow = projections.size() + (draw3D ? 1 : 0);
    const int nColumns = std::max<int>(nFirstRow, distributions.size());
    if (nColumns == 0) return;
    pad->Divide(nColumns, (nFirstRow > 0) + !distributions.empty());

    auto drawAt = [&](int padIndex, const Item& item) {
        pad->cd(padIndex);
        SetPadMargins(gPad);
        TRestEvent::DrawOwned(item.hist, histOption, item.xTitle, item.yTitle);
    };
    for (size_t i = 0; i < projections.size(); i++) drawAt(i + 1, projections[i]);
    if (draw3D) {
        pad->cd(projections.size() + 1);
        Draw3D(*this, min, max, nBins);
    }
    const int firstDistributionPad = nFirstRow > 0 ? nColumns + 1 : 1;
    for (size_t i = 0; i < distributions.size(); i++) drawAt(firstDistributionPad + i, distributions[i]);
}
