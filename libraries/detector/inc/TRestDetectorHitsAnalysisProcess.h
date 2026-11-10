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
///
/// The parameters (YAML keys), the observables and an example are described in the class documentation
/// (TRestDetectorHitsAnalysisProcess.cxx).
///
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
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
    double fGaussSigmaX = 0, fGaussSigmaY = 0, fGaussSigmaZ = 0;
    double fFiducialEnergy = 0;

    std::vector<ProjectionObservables> fProjectionObs;
    std::vector<BalanceObservables> fBalances;

   public:
    /// Projections (hit types: X, Y, Z, XY, XZ, YZ, XYZ) with their number of hits and energy, e.g. [XZ, YZ]
    /// for strip readouts
    std::vector<std::string> fProjections = {};

    /// GaussSigmaX/Y/Z from gaussian fits (slow), otherwise 0
    bool fGaussFit = false;

    /// Fiducial cylinder for FiducialEnergy: center, axis direction, radius (0: none) and length along the
    /// axis, centered on the center (0: infinite)
    std::array<TRestWithUnits, 3> fFiducialCenter = {0.0, 0.0, 0.0};
    /// Direction of the axis of the fiducial cylinder (any length)
    std::array<double, 3> fFiducialAxis = {0, 0, 1};
    /// Radius of the fiducial cylinder (0: no FiducialEnergy)
    TRestWithUnits fFiducialRadius = 0;
    /// Length of the fiducial cylinder along its axis, centred on fiducialCenter (0: infinite)
    TRestWithUnits fFiducialLength = 0;

    /// \brief Default constructor (no configuration).
    TRestDetectorHitsAnalysisProcess();
    /// \brief Builds the process from an in-memory YAML section (LoadConfig).
    TRestDetectorHitsAnalysisProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds the process from the section sectionName of a YAML file (LoadConfig).
    TRestDetectorHitsAnalysisProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the parameters from the YAML node.
    void LoadConfig() override;
    /// \brief Builds the projections and their pairs (throws for an unknown projection) and registers the
    /// observables.
    void InitProcess() override;
    /// \brief Computes the observables of the event, which is copied unchanged.
    /// \return false if the event has no hits.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Nothing to do at the end of the processing.
    void EndProcess() override {}

    /// \brief Input event type, "TRestDetectorHitsEvent".
    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    /// \brief Output event type, "TRestDetectorHitsEvent".
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }

    /// \brief Returns the class name, "TRestDetectorHitsAnalysisProcess".
    std::string GetClassName() const override { return "TRestDetectorHitsAnalysisProcess"; }
};

#endif
