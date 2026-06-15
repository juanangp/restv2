#include "TRestHits.h"
#include <TMath.h>
#include <algorithm>
#include "Math/GenVector/VectorUtil.h"
#include "TRestLogManager.h"

using REST_HitType = TRestHitsData::REST_HitType;

bool TRestHits::areXY() const {
    if (fNHits == 0) return false;
    for (int i = 0; i < fNHits; ++i) {
        if (fData->type[GetGlobalIdx(i)] != TRestHitsData::XY) return false;
    }
    return true;
}

bool TRestHits::areXZ() const {
    if (fNHits == 0) return false;
    for (int i = 0; i < fNHits; ++i) {
        if (fData->type[GetGlobalIdx(i)] != TRestHitsData::XZ) return false;
    }
    return true;
}

bool TRestHits::areYZ() const {
    if (fNHits == 0) return false;
    for (int i = 0; i < fNHits; ++i) {
        if (fData->type[GetGlobalIdx(i)] != TRestHitsData::YZ) return false;
    }
    return true;
}

bool TRestHits::areXYZ() const {
    if (fNHits == 0) return false;
    for (int i = 0; i < fNHits; ++i) {
        if (fData->type[GetGlobalIdx(i)] != TRestHitsData::XYZ) return false;
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
    fData->type.push_back(type);

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

double TRestHits::GetMeanHitEnergy() const { 
    return fNHits > 0 ? GetTotalEnergy() / fNHits : 0; 
}

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

ROOT::Math::XYZVector TRestHits::GetPosition(int n) const {
    int idx = GetGlobalIdx(n);
    REST_HitType type = fData->type[idx];
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
        if (fData->type[idx] % TRestHitsData::X == 0) {
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
        if (fData->type[idx] % TRestHitsData::Y == 0) {
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
        if (fData->type[idx] % TRestHitsData::Z == 0) {
            mean += fData->z[idx] * fData->energy[idx];
            totalEnergy += fData->energy[idx];
        }
    }
    return totalEnergy > 0 ? mean / totalEnergy : 0;
}

