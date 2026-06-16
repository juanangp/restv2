#pragma once

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

#include "Math/GenVector/DisplacementVector3D.h"
#include "Math/Vector3Dfwd.h"

// ============================================================
// TRestHitsData
// ============================================================
struct TRestHitsData {
    enum REST_HitType : int { 
        unknown = -1, X = 2, Y = 3, Z = 5, XY = 6, 
        XZ = 10, YZ = 15, XYZ = 30
    };

    std::vector<float> x, y, z, time, energy;
    std::vector<REST_HitType> type;

    void clear() {
        x.clear(); y.clear(); z.clear(); time.clear(); energy.clear(); type.clear();
    }
};

class TRestHits {
   protected:
    TRestHitsData* fData = nullptr;
    int fStartIdx = 0;
    int fNHits = 0;

    inline int GetGlobalIdx(int n) const { return fStartIdx + n; }

   public:
    TRestHits(TRestHitsData* data, int start = 0, int n = 0) 
        : fData(data), fStartIdx(start), fNHits(n) {}

    void Translate(int n, double x, double y, double z);
    void RotateIn3D(int n, double alpha, double beta, double gamma, ROOT::Math::XYZVector center);
    // void Rotate(int n, double alpha, ROOT::Math::XYZVector vAxis, ROOT::Math::XYZVector vMean);

    inline void RemoveHits();

    void AddHit(ROOT::Math::XYZVector position, double energy, double time, TRestHitsData::REST_HitType type);
    void AddHits(const TRestHits& hits);

    int GetMostEnergeticHitInRange(int n, int m) const;

    double GetMaximumHitDistance() const;
    double GetMaximumHitDistance2() const;

    virtual void MergeHits(int n, int m);
    virtual void SwapHits(int i, int j);
    virtual void RemoveHit(int n);

    virtual bool areXY() const;
    virtual bool areXZ() const;
    virtual bool areYZ() const;
    virtual bool areXYZ() const;

    bool isNaN(int n) const;

    double GetDistanceToNode(int n);

    bool isSortedByEnergy() const;

    // Getters
    inline size_t GetNumberOfHits() const { return (size_t)fNHits; }
    inline double GetX(int n) const { return fData->x[GetGlobalIdx(n)]; }
    inline double GetY(int n) const { return fData->y[GetGlobalIdx(n)]; }
    inline double GetZ(int n) const { return fData->z[GetGlobalIdx(n)]; }
    inline double GetTime(int n) const { return fData->time[GetGlobalIdx(n)]; }
    inline double GetEnergy(int n) const { return fData->energy[GetGlobalIdx(n)]; }
    inline TRestHitsData::REST_HitType GetType(int n) const { return static_cast<TRestHitsData::REST_HitType>(fData->type[GetGlobalIdx(n)]); }

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
    double GetSigmaZ2() const;
    double GetSkewXY() const;
    double GetSkewZ() const;

    // double GetGaussSigmaX(double error = 150.0, int nHitsMin = 100000);
    // double GetGaussSigmaY(double error = 150.0, int nHitsMin = 100000);
    // double GetGaussSigmaZ(double error = 150.0, int nHitsMin = 100000);

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

    virtual void PrintHits(int nHits = -1) const {}

    TRestHits() = default;
    ~TRestHits() = default;
};
