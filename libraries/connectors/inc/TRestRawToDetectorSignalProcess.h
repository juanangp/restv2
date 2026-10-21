#ifndef TRESTRAWTODETECTORSIGNALPROCESS_H
#define TRESTRAWTODETECTORSIGNALPROCESS_H

#include <string>
#include <utility>

#include "TRestEventProcess.h"

/// \class TRestRawToDetectorSignalProcess
/// \brief Transforms a TRestRawSignalEvent (ADC samples per bin) into a TRestDetectorSignalEvent
/// (time, amplitude points), optionally applying zero suppression.
class TRestRawToDetectorSignalProcess : public TRestEventProcess {
   private:
    /// Number of signals of the event without any point in the output
    int fNSignalsRejected = 0;

   public:
    /// Sampling time used to transform the bin number into time
    TRestWithUnits fSampling = 0.02;

    /// Time of the first bin of the raw signal
    double fTriggerStarts = 0;

    /// Factor applied to the amplitude of the output points
    double fGain = 1;

    /// Minimum raw value of a point to be transferred (only without zero suppression)
    double fThreshold = 0.1;

    /// If true, only the points over threshold (see TRestRawSignalAnalysisProcess) are transferred,
    /// with the baseline subtracted
    bool fZeroSuppression = false;

    /// Bin range used to calculate the baseline and its fluctuation (zero suppression)
    std::pair<int, int> fBaselineRange = {5, 55};

    /// Baseline calculation option (zero suppression), see TRestPulseShapeAnalysis
    std::string fBaselineOption = "";

    /// Bin range where the points over threshold are searched (zero suppression)
    std::pair<int, int> fIntegralRange = {100, 500};

    /// Number of baseline sigmas for a point to be over threshold (zero suppression)
    double fPointsThreshold = 3;

    /// Number of baseline sigmas for a pulse to be accepted (zero suppression)
    double fSignalThreshold = 5;

    /// Number of consecutive points over threshold to accept a pulse (zero suppression)
    int fPointsOverThreshold = 5;

    /// Number of flat points that end a pulse over threshold (zero suppression)
    int fNPointsFlat = 512;

    TRestRawToDetectorSignalProcess();
    TRestRawToDetectorSignalProcess(const std::string& instanceName, const YAML::Node& node);
    TRestRawToDetectorSignalProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestDetectorSignalEvent"; }

    std::string GetClassName() const override { return "TRestRawToDetectorSignalProcess"; }
};

#endif
