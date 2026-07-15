#pragma once

#include "TRestEventProcess.h"
#include "TRestRawSignalEvent.h"
#include "TRestRun.h"
#include <vector>
#include <TFile.h>
#include <TTree.h>
#include <limits>

///
/// Read data from the root file output of femdaq into a TRestRawSignalEvent
///
class TRestRawSignalAnalysisProcess : public TRestEventProcess {
   private:
      struct SignalObservables {
        std::vector<int> SignalsID;
        std::vector<double> Baseline;
        std::vector<double> BaselineSigma;
        std::vector<double> AmpSgnMaxMethod;
        std::vector<double> AmpSgnIntMethod;
        std::vector<double> RiseTime;
        std::vector<double> RiseSlope;
        std::vector<double> PeakTime;
        std::vector<int> PointsOverThr;
        std::vector<bool> Saturated;
        

        double BaselineAvg = 0.0;
        double BaselineSigmaAvg = 0.0;
        int TimeBinLength = 0.0;
        int NSignals = 0;
        int NGoodSignals = 0;
        double FullIntegralSum = 0.0;
        double ThresholdIntegralSum = 0.0;
        
        double RiseSlopeAvg = 0.0;
        double SlopeIntegral = 0.0;
        double RateOfChangeAvg = 0.0;
        double RiseTimeAvg = 0.0;
        double MaxIntegral = 0.0;
        double IntegralBalance = 0.0;
        double AmplitudeIntegralRatio = 0.0;
        double MinPeakAmplitude = 0.0;
        double MaxPeakAmplitude = 0.0;
        double PeakAmplitudeIntegral = 0.0;
        double MinEventValue = 0.0;
        double AmplitudeRatio = 0.0;
        double MaxPeakTime = 0.0;
        double MinPeakTime = 0.0;
        double MaxPeakTimeDelay = 0.0;
        double AveragePeakTime = 0.0;
    };

    SignalObservables fObs;

   public:

    /// Just a flag to quickly determine if we have to apply the range filter
    bool fRangeEnabled = false;

    /// The range where the baseline range will be calculated
    std::pair<int, int> fBaselineRange = {5, 55};

    /// The range where the observables will be calculated
    std::pair<int, int> fIntegralRange = {10, 500};

    /// Option for calculation of baseline parameters, can be set to "ROBUST"
    std::string fBaselineOption ="";

    std::string fMaxOption = "tripleMaxAverage";

    /// The number of sigmas over baseline fluctuations to identify a point over threshold
    double fPointsThreshold = 2;

    /// A parameter to define a minimum signal fluctuation. Measured in sigmas.
    double fSignalThreshold = 5;

    /// The minimum number of points over threshold to identify a signal as such
    int fPointsOverThreshold = 5;

    int fNPointsFlat = 512;

    /// It defines the signals id range where analysis is applied
    std::pair<int, int> fSignalsRange = {-1, -1};  //<

    TRestRawSignalAnalysisProcess();
    TRestRawSignalAnalysisProcess(const std::string& instanceName, const YAML::Node& node);
    TRestRawSignalAnalysisProcess(const std::string& fileName, const std::string& sectionName);

    virtual void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override;

    std::string GetInputEvent() const override { return "TRestRawSignalEvent";}
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent";}

    std::string GetClassName() const override { return "TRestRawSignalAnalysisProcess"; }
    //void PrintMetadata() override;
};
