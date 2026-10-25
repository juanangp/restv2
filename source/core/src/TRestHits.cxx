#include "TRestHits.h"

#include <TColor.h>
#include <TGraph.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TH3F.h>
#include <TMath.h>
#include <TPolyLine3D.h>
#include <TPolyMarker3D.h>
#include <TStyle.h>
#include <TVirtualPad.h>

#include <algorithm>
#include <array>
#include <map>
#include <tuple>

#include "TRestEvent.h"
#include "TRestHitsAnalysis.h"


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

std::vector<ROOT::Math::XYZVector> TRestHits::GetPoints() const {
    std::vector<ROOT::Math::XYZVector> points(fNHits);
    for (int n = 0; n < fNHits; n++) {
        const int type = GetType(n);
        points[n].SetXYZ(TRestHitsData::IsDefined(type, 0) ? GetX(n) : 0, TRestHitsData::IsDefined(type, 1) ? GetY(n) : 0,
                         TRestHitsData::IsDefined(type, 2) ? GetZ(n) : 0);
    }
    return points;
}

std::vector<double> TRestHits::GetEnergies() const {
    std::vector<double> energies(fNHits);
    for (int n = 0; n < fNHits; n++) energies[n] = GetEnergy(n);
    return energies;
}

std::vector<double> TRestHits::GetCoordinates(int axis) const {
    std::vector<double> values(fNHits);
    for (int n = 0; n < fNHits; n++) values[n] = GetCoordinate(n, axis);
    return values;
}

namespace {
/// Maximum number of bins of the 3D histogram, bins are merged above it
const double kMax3DBins = 1e6;

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
    TRestEvent::DrawOwned(h3, h3->GetEntries() > 0 ? "box2" : "", TRestHitsData::kAxisTitles[0], TRestHitsData::kAxisTitles[1],
                          TRestHitsData::kAxisTitles[2]);

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
            if (TRestHitsData::IsDefined(hits.GetType(n), a)) {
                nDefined++;
            } else {
                undefined = a;
            }
        }
        if (nDefined != 2) continue;

        double p0[3], p1[3];
        for (int a = 0; a < 3; a++) p0[a] = p1[a] = hits.GetCoordinate(n, a);
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
            const double ca = hits.GetCoordinate(a, common), cb = hits.GetCoordinate(b, common);
            if (std::abs(ca - cb) > 0.05 * (max[common] - min[common])) continue;

            double point[3];
            point[undefinedA] = hits.GetCoordinate(b, undefinedA);
            point[undefinedB] = hits.GetCoordinate(a, undefinedB);
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

/// Z spacing of the samples of a pulse: the median of the distances between consecutive Z values of the
/// hits in the same channel (same type and X/Y position), i.e. the sampling (or integration window) times
/// the drift velocity. 0 if no channel has two different Z values (e.g. simulated hits)
double GetZStep(const TRestHits& hits) {
    std::map<std::tuple<int, double, double>, std::vector<double>> channels;
    for (size_t n = 0; n < hits.GetNumberOfHits(); n++) {
        const int type = hits.GetType(n);
        if (!TRestHitsData::IsDefined(type, 2)) continue;
        const double x = TRestHitsData::IsDefined(type, 0) ? hits.GetX(n) : 0;
        const double y = TRestHitsData::IsDefined(type, 1) ? hits.GetY(n) : 0;
        channels[{type, x, y}].push_back(hits.GetZ(n));
    }

    std::vector<double> steps;
    for (auto& [channel, z] : channels) {
        std::sort(z.begin(), z.end());
        for (size_t i = 1; i < z.size(); i++) {
            if (z[i] > z[i - 1]) steps.push_back(z[i] - z[i - 1]);
        }
    }
    if (steps.empty()) return 0;
    std::nth_element(steps.begin(), steps.begin() + steps.size() / 2, steps.end());
    return steps[steps.size() / 2];
}

/// Number of energy bands of the 3D view with markers, one TPolyMarker3D (one palette color) each
const int kEnergyBands = 50;

/// Energy weighted covariance between the coordinates axis and common of the hits of the groups
double Covariance(const TRestHits& hits, const std::map<int, std::vector<size_t>>& groups, int axis,
                  int common) {
    double sumW = 0, sumA = 0, sumC = 0, sumAC = 0;
    for (const auto& [bin, indices] : groups) {
        for (size_t n : indices) {
            const double w = hits.GetEnergy(n);
            const double a = hits.GetCoordinate(n, axis), c = hits.GetCoordinate(n, common);
            sumW += w;
            sumA += w * a;
            sumC += w * c;
            sumAC += w * a * c;
        }
    }
    return sumW > 0 ? sumAC / sumW - (sumA / sumW) * (sumC / sumW) : 0;
}

/// 3D view in the current pad as markers colored by energy, see TRestHits::DrawHistograms
void Draw3DMarkers(const TRestHits& hits, const double min[3], const double max[3], const int nBins[3]) {
    std::vector<std::array<double, 4>> points;  // x, y, z, energy
    for (size_t n = 0; n < hits.GetNumberOfHits(); n++) {
        if (hits.GetType(n) % REST_HitType::XYZ == 0)
            points.push_back({hits.GetX(n), hits.GetY(n), hits.GetZ(n), hits.GetEnergy(n)});
    }

    // Hits with one undefined coordinate (e.g. XZ and YZ strips): those undefined along different
    // coordinates are combined in each bin of their common coordinate (Z for XZ and YZ). Instead of every
    // pair (a rectangle of ghosts when a bin has several strips of each projection), both groups are sorted
    // by their defined coordinate and paired monotonically by their accumulated energy fraction, so each bin
    // gives a segment of the track holding the mean of the energies of both projections. The direction of
    // the pairing (X-Y correlation) is the sign of the product of the slopes of both projections along the
    // common coordinate
    std::map<int, std::vector<size_t>> groups[3][3];  // [undefined][common]: bin of common -> hits
    for (size_t n = 0; n < hits.GetNumberOfHits(); n++) {
        if (hits.GetEnergy(n) <= 0) continue;
        int undefined = -1, nDefined = 0;
        for (int a = 0; a < 3; a++) {
            if (TRestHitsData::IsDefined(hits.GetType(n), a)) {
                nDefined++;
            } else {
                undefined = a;
            }
        }
        if (nDefined != 2) continue;
        for (int common = 0; common < 3; common++) {
            if (common == undefined) continue;
            const double binSize = (max[common] - min[common]) / nBins[common];
            const double offset = hits.GetCoordinate(n, common) - min[common];
            const int bin = binSize > 0 ? (int)std::floor(offset / binSize) : 0;
            groups[undefined][common][bin].push_back(n);
        }
    }
    for (int ua = 0; ua < 3; ua++) {
        for (int ub = ua + 1; ub < 3; ub++) {
            const int common = 3 - ua - ub;
            if (groups[ua][common].empty() || groups[ub][common].empty()) continue;
            // Group A (undefined ua) defines ub, group B (undefined ub) defines ua
            const bool ascending = Covariance(hits, groups[ua][common], ub, common) *
                                       Covariance(hits, groups[ub][common], ua, common) >=
                                   0;
            for (const auto& [bin, indicesA] : groups[ua][common]) {
                const auto it = groups[ub][common].find(bin);
                if (it == groups[ub][common].end()) continue;
                std::vector<size_t> groupA = indicesA, groupB = it->second;
                std::sort(groupA.begin(), groupA.end(), [&](size_t i, size_t j) {
                    return hits.GetCoordinate(i, ub) < hits.GetCoordinate(j, ub);
                });
                std::sort(groupB.begin(), groupB.end(), [&](size_t i, size_t j) {
                    return ascending ? hits.GetCoordinate(i, ua) < hits.GetCoordinate(j, ua)
                                     : hits.GetCoordinate(i, ua) > hits.GetCoordinate(j, ua);
                });
                double energyA = 0, energyB = 0;
                for (size_t i : groupA) energyA += hits.GetEnergy(i);
                for (size_t j : groupB) energyB += hits.GetEnergy(j);
                const double meanEnergy = 0.5 * (energyA + energyB);

                size_t a = 0, b = 0;
                double restA = hits.GetEnergy(groupA[0]) / energyA;
                double restB = hits.GetEnergy(groupB[0]) / energyB;
                while (a < groupA.size() && b < groupB.size()) {
                    const double fraction = std::min(restA, restB);
                    const size_t i = groupA[a], j = groupB[b];
                    double point[3];
                    point[ua] = hits.GetCoordinate(j, ua);
                    point[ub] = hits.GetCoordinate(i, ub);
                    point[common] = 0.5 * (hits.GetCoordinate(i, common) + hits.GetCoordinate(j, common));
                    if (fraction > 0) points.push_back({point[0], point[1], point[2], fraction * meanEnergy});
                    restA -= fraction;
                    restB -= fraction;
                    if (restA <= 1e-9 && ++a < groupA.size()) restA = hits.GetEnergy(groupA[a]) / energyA;
                    if (restB <= 1e-9 && ++b < groupB.size()) restB = hits.GetEnergy(groupB[b]) / energyB;
                }
            }
        }
    }

    // Empty TH3 only for the frame and the axes, the points drawn on top as markers colored by energy with
    // the palette, one TPolyMarker3D per energy band
    auto* frame = new TH3F("XYZ", "", 1, min[0], max[0], 1, min[1], max[1], 1, min[2], max[2]);
    TRestEvent::DrawOwned(frame, "", TRestHitsData::kAxisTitles[0], TRestHitsData::kAxisTitles[1],
                          TRestHitsData::kAxisTitles[2]);
    if (points.empty()) return;

    double minEnergy = points[0][3], maxEnergy = points[0][3];
    for (const auto& p : points) {
        minEnergy = std::min(minEnergy, p[3]);
        maxEnergy = std::max(maxEnergy, p[3]);
    }
    std::vector<TPolyMarker3D*> bands(kEnergyBands, nullptr);
    const double range = maxEnergy - minEnergy;
    for (const auto& p : points) {
        const int band =
            range > 0 ? std::min(kEnergyBands - 1, (int)((p[3] - minEnergy) / range * kEnergyBands))
                      : kEnergyBands - 1;
        if (!bands[band]) {
            bands[band] = new TPolyMarker3D();
            bands[band]->SetMarkerStyle(kFullCircle);
            bands[band]->SetMarkerSize(0.6);
            const int nColors = gStyle->GetNumberOfColors();
            bands[band]->SetMarkerColor(gStyle->GetColorPalette((band * nColors) / kEnergyBands));
        }
        bands[band]->SetNextPoint(p[0], p[1], p[2]);
    }
    for (auto* marker : bands) {
        if (!marker) continue;
        marker->SetBit(TObject::kCanDelete);
        marker->Draw();
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
        const int a = TRestHitsData::kPlanes[p][0], b = TRestHitsData::kPlanes[p][1];
        GraphData graph{{}, {}, TRestHitsData::kAxisTitles[a], TRestHitsData::kAxisTitles[b], planeColors[p]};
        for (size_t n = 0; n < GetNumberOfHits(); n++) {
            if (!TRestHitsData::IsDefined(GetType(n), a) || !TRestHitsData::IsDefined(GetType(n), b)) continue;
            graph.u.push_back(GetCoordinate(n, a));
            graph.v.push_back(GetCoordinate(n, b));
        }
        if (!graph.u.empty()) graphs.push_back(std::move(graph));
    }

    // 1D hits: energy against each coordinate
    if (graphs.empty()) {
        for (int a = 0; a < 3; a++) {
            GraphData graph{{}, {}, TRestHitsData::kAxisTitles[a], "Energy", kBlack};
            for (size_t n = 0; n < GetNumberOfHits(); n++) {
                if (!TRestHitsData::IsDefined(GetType(n), a)) continue;
                graph.u.push_back(GetCoordinate(n, a));
                graph.v.push_back(GetEnergy(n));
            }
            if (!graph.u.empty()) graphs.push_back(std::move(graph));
        }
    }
    if (graphs.empty()) return;

    pad->Divide(graphs.size(), 1);
    for (size_t i = 0; i < graphs.size(); i++) {
        pad->cd(i + 1);
        TRestEvent::SetPadMargins(gPad);
        auto* graph = new TGraph(graphs[i].u.size(), graphs[i].u.data(), graphs[i].v.data());
        graph->SetTitle(title.c_str());
        graph->SetMarkerColor(graphs[i].color);
        graph->SetMarkerSize(0.3);
        graph->SetMarkerStyle(20);
        TRestEvent::DrawOwned(graph, "AP", graphs[i].xTitle, graphs[i].yTitle);
    }
}

void TRestHits::DrawHistograms(TVirtualPad* pad, const std::string& histOption, double pitch,
                               bool markers3D) const {
    std::vector<double> values[3];
    for (size_t n = 0; n < GetNumberOfHits(); n++) {
        for (int a = 0; a < 3; a++) {
            if (TRestHitsData::IsDefined(GetType(n), a)) values[a].push_back(GetCoordinate(n, a));
        }
    }

    // Without a pitch, X and Y use the minimum distance between their hits (the readout pitch). Z has no
    // natural pitch (the drift gives almost continuous values), so it uses the X/Y bin size
    double binSize[3];
    binSize[0] = pitch > 0 ? pitch : TRestHitsAnalysis::GetMinimumDistance(values[0]);
    binSize[1] = pitch > 0 ? pitch : TRestHitsAnalysis::GetMinimumDistance(values[1]);
    if (binSize[0] <= 0) binSize[0] = binSize[1];
    if (binSize[1] <= 0) binSize[1] = binSize[0];
    binSize[2] = pitch > 0 ? pitch : std::min(binSize[0], binSize[1]);
    if (binSize[2] <= 0) binSize[2] = TRestHitsAnalysis::GetMinimumDistance(values[2]);
    // Track view: Z uses the spacing of the samples of the pulses, so there are no empty bins inside a
    // pulse (pulses drawn cut) and the strips are paired sample by sample
    if (markers3D) {
        const double zStep = GetZStep(*this);
        if (zStep > 0) binSize[2] = zStep;
    }

    double min[3], max[3];
    int nBins[3];
    for (int a = 0; a < 3; a++) {
        TRestHitsAnalysis::GetBoundaries(values[a], binSize[a], max[a], min[a], nBins[a]);
    }

    struct Item {
        TH1* hist;
        const char* xTitle;
        const char* yTitle;
    };
    std::vector<Item> projections, distributions;

    const char* planeNames[3] = {"XZ", "YZ", "XY"};
    for (int p = 0; p < 3; p++) {
        const int a = TRestHitsData::kPlanes[p][0], b = TRestHitsData::kPlanes[p][1];
        auto* h = new TH2F(planeNames[p], "", nBins[a], min[a], max[a], nBins[b], min[b], max[b]);
        h->SetDirectory(nullptr);
        for (size_t n = 0; n < GetNumberOfHits(); n++) {
            if (TRestHitsData::IsDefined(GetType(n), a) && TRestHitsData::IsDefined(GetType(n), b))
                h->Fill(GetCoordinate(n, a), GetCoordinate(n, b), GetEnergy(n));
        }
        // Histograms not drawn are not owned by any pad
        if (h->GetEntries() == 0) {
            delete h;
            continue;
        }
        projections.push_back({h, TRestHitsData::kAxisTitles[a], TRestHitsData::kAxisTitles[b]});
    }

    const char* axisNames[3] = {"X", "Y", "Z"};
    for (int a = 0; a < 3; a++) {
        if (values[a].empty()) continue;
        auto* h = new TH1F(axisNames[a], "", nBins[a], min[a], max[a]);
        h->SetDirectory(nullptr);
        for (size_t n = 0; n < GetNumberOfHits(); n++) {
            if (TRestHitsData::IsDefined(GetType(n), a)) h->Fill(GetCoordinate(n, a), GetEnergy(n));
        }
        // The weighted fill stores the errors, ROOT would draw them as error bars
        h->Sumw2(false);
        distributions.push_back({h, TRestHitsData::kAxisTitles[a], "Energy"});
    }

    // Only with hits defining at least two coordinates, so it shows points or lines
    const bool draw3D = !projections.empty() && !values[0].empty() && !values[1].empty() && !values[2].empty();

    const int nFirstRow = projections.size() + (draw3D ? 1 : 0);
    const int nColumns = std::max<int>(nFirstRow, distributions.size());
    if (nColumns == 0) return;
    pad->Divide(nColumns, (nFirstRow > 0) + !distributions.empty());

    auto drawAt = [&](int padIndex, const Item& item) {
        pad->cd(padIndex);
        TRestEvent::SetPadMargins(gPad);
        TRestEvent::DrawOwned(item.hist, histOption, item.xTitle, item.yTitle);
    };
    for (size_t i = 0; i < projections.size(); i++) drawAt(i + 1, projections[i]);
    if (draw3D) {
        pad->cd(projections.size() + 1);
        if (markers3D) {
            Draw3DMarkers(*this, min, max, nBins);
        } else {
            Draw3D(*this, min, max, nBins);
        }
    }
    const int firstDistributionPad = nFirstRow > 0 ? nColumns + 1 : 1;
    for (size_t i = 0; i < distributions.size(); i++) drawAt(firstDistributionPad + i, distributions[i]);
}
