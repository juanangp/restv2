#ifndef TRESTDETECTORSIGNALPEAKSFINDERPROCESS_H
#define TRESTDETECTORSIGNALPEAKSFINDERPROCESS_H

#include <string>
#include <vector>

#include "TRestEventProcess.h"

/// \class TRestDetectorSignalPeaksFinderProcess
/// \brief Finds all the peaks of the signals of a TRestDetectorSignalEvent and groups them in time
/// coincidence windows. The event is not modified. See the class documentation in
/// TRestDetectorSignalPeaksFinderProcess.cxx (observables and example).
///
/// \author Luis Obis <luis.antonio.obis@gmail.com>
/// \author JuanAn García <juanangp@unizar.es>
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
    TRestWithUnits fDistance = 2;  // us

    /// Half of the number of points of the moving average used to find the peaks
    int fHalfWindow = 5;

    /// Maximum number of points of the moving average window greater or equal than a peak
    int fMaxGreaterEqual = 3;

    /// Moves the peak to the highest smoothed point within distance after it (false for legacy vetoes)
    bool fRefineMaximum = true;

    /// The peak amplitude is the average of the points within this number of points of the peak
    int fAmplitudeHalfWindow = 1;

    /// A coincidence window starts at the earliest peak not yet in a window and contains all the peaks up to
    /// window after it
    TRestWithUnits fWindow = 1;  // us

    /// \brief Default constructor (no configuration).
    TRestDetectorSignalPeaksFinderProcess();
    /// \brief Builds the process from an in-memory YAML section (LoadConfig).
    TRestDetectorSignalPeaksFinderProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds the process from the section sectionName of a YAML file (LoadConfig).
    TRestDetectorSignalPeaksFinderProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the parameters from the YAML node.
    void LoadConfig() override;
    /// \brief Checks the parameters and registers the observables.
    void InitProcess() override;
    /// \brief Finds the peaks of the signals and fills the observables; the event is copied unchanged.
    /// \return Always true.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Nothing to do at the end of the processing.
    void EndProcess() override {}

    /// \brief Input event type, "TRestDetectorSignalEvent".
    std::string GetInputEvent() const override { return "TRestDetectorSignalEvent"; }
    /// \brief Output event type, "TRestDetectorSignalEvent".
    std::string GetOutputEvent() const override { return "TRestDetectorSignalEvent"; }

    /// \brief Returns the class name, "TRestDetectorSignalPeaksFinderProcess".
    std::string GetClassName() const override { return "TRestDetectorSignalPeaksFinderProcess"; }
};

#endif
