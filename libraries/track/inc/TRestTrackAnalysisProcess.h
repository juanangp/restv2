#ifndef TRESTTRACKANALYSISPROCESS_H
#define TRESTTRACKANALYSISPROCESS_H

#include <string>
#include <utility>
#include <vector>

#include "TRestEventProcess.h"

class TRestHits;

/// \class TRestTrackAnalysisProcess
/// \brief Computes observables of the tracks of a TRestTrackEvent, for any readout: scalars for the main
/// track (the most energetic one or, with projections, the main tracks of all of them together), vectors
/// for the other tracks and, for the given projections (hit types, e.g. XZ and YZ for strips), the main
/// track of each one and balances. Optionally rejects events by number of tracks.
class TRestTrackAnalysisProcess : public TRestEventProcess {
   private:
    /// Observables of one track (or set of hits). Coordinates it does not define are NaN
    struct TrackObservables {
        double energy = 0;
        int type = 0;
        int nHits = 0;
        double length = 0, volume = 0;
        double meanX = 0, meanY = 0, meanZ = 0;
        double sigmaX = 0, sigmaY = 0, sigmaZ = 0;
        double gaussSigmaX = 0, gaussSigmaY = 0, gaussSigmaZ = 0;
        double twist = 0, twistWeighted = 0, twistLow = 0, twistHigh = 0;
    };

    /// The same observables for the other tracks, one entry per track
    struct TracksObservables {
        std::vector<double> energy;
        std::vector<int> type;
        std::vector<int> nHits;
        std::vector<double> length, volume;
        std::vector<double> meanX, meanY, meanZ;
        std::vector<double> sigmaX, sigmaY, sigmaZ;
        std::vector<double> gaussSigmaX, gaussSigmaY, gaussSigmaZ;
        std::vector<double> twist, twistWeighted, twistLow, twistHigh;

        void clear();
        void push_back(const TrackObservables& track);
    };

    /// Tracks of one projection (hit type)
    struct ProjectionObservables {
        std::string name;
        int type = 0;
        int nTracks = 0;
        double energy = 0;                ///< Energy of the tracks of this type
        double maxTrackEnergyRatio = 0;  ///< (energy - maxTrack.energy) / energy
        TrackObservables maxTrack;
        int maxTrackIndex = -1;  ///< Track index in the event, -1 if none
    };

    /// Balances (a - b) / (a + b) between the main tracks of two projections
    struct BalanceObservables {
        size_t first = 0, second = 0;  ///< Indices in fProjectionObs
        /// Coordinate only each projection defines (e.g. X of XZ and Y of YZ), -1 if not exactly one
        int firstAxis = -1, secondAxis = -1;
        double energy = 0;
        double sigma = 0;  ///< Of firstAxis and secondAxis
    };

    int fNTracks = 0;
    double fTotalEnergy = 0;
    double fMaxTrackEnergyRatio = 0;
    double fMaxTrackSkewXY = 0, fMaxTrackSkewZ = 0;
    /// Most energetic track or, with projections, the main tracks of all of them together
    TrackObservables fMaxTrack;
    TracksObservables fTracks;

    std::vector<ProjectionObservables> fProjectionObs;
    std::vector<BalanceObservables> fBalances;

    /// \brief Observables of a set of hits (a track or the main tracks of the projections together).
    TrackObservables GetHitsObservables(const TRestHits& hits) const;
    /// \brief Registers prefix + name of each observable, with Mean, Sigma and GaussSigma only along the
    /// coordinates `type` defines.
    void RegisterTrackObservables(const std::string& prefix, TrackObservables& obs, int type);
    /// \brief Registers Mean, Sigma and GaussSigma of prefix + coordinate, for the coordinates `type`
    /// defines.
    void RegisterCoordinateObservables(const std::string& prefix, TrackObservables& obs, int type);

   public:
    /// Projections (hit types: X, Y, Z, XY, XZ, YZ, XYZ) with observables of their own main track, e.g.
    /// [XZ, YZ] for strip readouts
    std::vector<std::string> fProjections = {};

    /// If true, the gaussian sigmas are computed (a fit per coordinate and track); otherwise they are 0
    bool fGaussFit = false;

    /// Percentage of the hits at each end of a track used for TwistLow and TwistHigh
    double fTwistTailPercentage = 20;

    /// If true, events with a number of tracks of any projection (or in total, without projections) out of
    /// nTracksCut are rejected
    bool fCutsEnabled = false;

    /// Accepted range of the number of tracks per projection
    std::pair<int, int> fNTracksCut = {1, 10};

    TRestTrackAnalysisProcess();
    TRestTrackAnalysisProcess(const std::string& instanceName, const YAML::Node& node);
    TRestTrackAnalysisProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestTrackEvent"; }
    std::string GetOutputEvent() const override { return "TRestTrackEvent"; }

    std::string GetClassName() const override { return "TRestTrackAnalysisProcess"; }
};

#endif
