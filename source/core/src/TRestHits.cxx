#include "TRestHits.h"

#include <TMath.h>


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
