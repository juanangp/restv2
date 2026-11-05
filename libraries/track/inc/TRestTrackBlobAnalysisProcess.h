#ifndef TRESTTRACKBLOBANALYSISPROCESS_H
#define TRESTTRACKBLOBANALYSISPROCESS_H

#include <string>
#include <vector>

#include "TRestEventProcess.h"

class TRestHits;

/// \class TRestTrackBlobAnalysisProcess
/// \brief Observables of the two ends (blobs) of the main track of a TRestTrackEvent with ordered nodes
/// (after TRestTrackPathMinimizationProcess): their positions, distance and the energy of the nodes inside
/// spheres around them, for the main track or the main track of each projection.
class TRestTrackBlobAnalysisProcess : public TRestEventProcess {
   private:
    /// Observables of the main track of one projection (hit type), or of the most energetic track
    struct BlobObservables {
        std::string name;  ///< Projection, empty for the most energetic track
        int type = 0;      ///< Hit type, 0 for any
        double x1 = 0, y1 = 0, z1 = 0;
        double x2 = 0, y2 = 0, z2 = 0;
        double distance = 0;
        /// One entry per radius
        std::vector<double> q1, q2, qHigh, qLow, qBalance, qRatio;
    };

    std::vector<BlobObservables> fBlobObs;

    /// \brief Fills the observables from the hits of a track (empty if there is no track).
    void FillObservables(const TRestHits* hits, BlobObservables& obs) const;

   public:
    /// Projections (hit types: X, Y, Z, XY, XZ, YZ or XYZ) with the observables of their own main track,
    /// e.g. [XZ, YZ] for strip readouts; empty: the most energetic track, whatever its type
    std::vector<std::string> fProjections = {};

    /// Fraction of the nodes at each end of the track where the blob is searched (most energetic node)
    double fHitsToCheckFraction = 0.2;

    /// Radii of the spheres around the blobs, one entry of the Q observables per radius
    std::vector<TRestWithUnits> fRadii = {};

    TRestTrackBlobAnalysisProcess();
    TRestTrackBlobAnalysisProcess(const std::string& instanceName, const YAML::Node& node);
    TRestTrackBlobAnalysisProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestTrackEvent"; }
    std::string GetOutputEvent() const override { return "TRestTrackEvent"; }

    std::string GetClassName() const override { return "TRestTrackBlobAnalysisProcess"; }
};

#endif
