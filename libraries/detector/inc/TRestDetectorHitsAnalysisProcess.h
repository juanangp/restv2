#ifndef TRESTDETECTORHITSANALYSISPROCESS_H
#define TRESTDETECTORHITSANALYSISPROCESS_H

#include <array>
#include <string>
#include <vector>

#include "TRestEventProcess.h"

class TRestHits;

/// \class TRestDetectorHitsAnalysisProcess
/// \brief Computes observables of the hits of a TRestDetectorHitsEvent, for any readout: energy, number
/// of hits, energy weighted mean, sigma and skewness, and per projection (hit type) with balances.
class TRestDetectorHitsAnalysisProcess : public TRestEventProcess {
   private:
    /// Hits of one projection (hit type)
    struct ProjectionObservables {
        std::string name;
        int type = 0;
        int nHits = 0;
        double energy = 0;
        std::array<double, 3> sigma = {0, 0, 0};  ///< Sigma of each coordinate, hits of this type
    };

    /// Balances (a - b) / (a + b) between two projections
    struct BalanceObservables {
        size_t first = 0, second = 0;  ///< Indices in fProjectionObs
        /// Coordinate only each projection defines (e.g. X of XZ and Y of YZ), -1 if not exactly one
        int firstAxis = -1, secondAxis = -1;
        double nHits = 0;
        double energy = 0;
        double sigma = 0;  ///< Of firstAxis and secondAxis
    };

    int fNHits = 0;
    double fEnergy = 0;
    double fMaxHitEnergy = 0, fMinHitEnergy = 0, fMeanHitEnergy = 0;
    double fMeanX = 0, fMeanY = 0, fMeanZ = 0;
    double fSigmaX = 0, fSigmaY = 0, fSigmaZ = 0;
    double fSkewXY = 0, fSkewZ = 0;

    std::vector<ProjectionObservables> fProjectionObs;
    std::vector<BalanceObservables> fBalances;

   public:
    /// Projections (hit types: X, Y, Z, XY, XZ, YZ, XYZ) with their number of hits and energy, e.g. [XZ, YZ]
    /// for strip readouts
    std::vector<std::string> fProjections = {};

    TRestDetectorHitsAnalysisProcess();
    TRestDetectorHitsAnalysisProcess(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorHitsAnalysisProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }

    std::string GetClassName() const override { return "TRestDetectorHitsAnalysisProcess"; }
};

#endif
