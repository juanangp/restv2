#ifndef TRESTDETECTORSIGNALPEAKSFINDERPROCESS_H
#define TRESTDETECTORSIGNALPEAKSFINDERPROCESS_H

#include <string>
#include <vector>

#include "TRestEventProcess.h"

/// \class TRestDetectorSignalPeaksFinderProcess
/// \brief Finds all the peaks of the signals of a TRestDetectorSignalEvent and groups them in time
/// coincidence windows. The event is not modified.
class TRestDetectorSignalPeaksFinderProcess : public TRestEventProcess {
   private:
    struct PeaksObservables {
        std::vector<int> PeaksSignalID;
        std::vector<double> PeaksTime;
        std::vector<double> PeaksAmplitude;
        std::vector<int> PeaksWindowIndex;

        std::vector<double> WindowTime;
        std::vector<int> WindowMultiplicity;
        std::vector<double> WindowAmplitude;

        int NPeaks = 0;
        int NPeaksSignals = 0;
        int NWindows = 0;
        double PeaksAmplitudeSum = 0.0;

        void clear() {
            auto clear_vectors = [](auto&... vecs) { (vecs.clear(), ...); };
            clear_vectors(PeaksSignalID, PeaksTime, PeaksAmplitude, PeaksWindowIndex, WindowTime,
                          WindowMultiplicity, WindowAmplitude);
            NPeaks = NPeaksSignals = NWindows = 0;
            PeaksAmplitudeSum = 0.0;
        }
    };

    PeaksObservables fObs;

   public:
    /// Minimum smoothed amplitude of a peak (signal amplitude units, the baseline is already subtracted)
    double fThreshold = 100;

    /// Minimum time between two peaks of the same signal
    TRestWithUnits fDistance = 2; //us

    /// Half of the number of points of the moving average used to find the peaks
    int fHalfWindow = 5;

    /// Maximum number of points of the moving average window greater or equal than a peak
    int fMaxGreaterEqual = 3;

    /// A coincidence window starts at the earliest peak not yet in a window and contains all the peaks up to
    TRestWithUnits fWindow = 1; //us

    TRestDetectorSignalPeaksFinderProcess();
    TRestDetectorSignalPeaksFinderProcess(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorSignalPeaksFinderProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestDetectorSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestDetectorSignalEvent"; }

    std::string GetClassName() const override { return "TRestDetectorSignalPeaksFinderProcess"; }
};

#endif
