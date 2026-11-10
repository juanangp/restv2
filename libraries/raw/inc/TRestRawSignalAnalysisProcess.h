#ifndef TRESTRAWSIGNALANALYSISPROCESS_H
#define TRESTRAWSIGNALANALYSISPROCESS_H

#include <vector>

#include "TRestEventProcess.h"
class TRestRawSignalEvent;

/// \class TRestRawSignalAnalysisProcess
/// \brief Computes pulse-shape observables for each signal inside a raw signal event.
///
/// A signal is "good" (counted in NGoodSignals and used for the event-level sums, extrema and
/// averages) when it has at least 2 points over threshold (legacy criterion). Signals without points
/// over threshold get 0 peak time and amplitude; a maximum found outside the signal is also set to 0.
/// Saturated uses the legacy flat-top criterion (TRestRawSignal::IsADCSaturation with 3 flat bins).
/// Events without good signals get 0 in MinPeakAmplitude, MinEventValue and MinPeakTime, and ratios
/// with a null denominator (RateOfChangeAvg, IntegralBalance) are set to 0.
///
/// For each signal, the baseline and its sigma are computed in `baselineRange` (with `baselineOption`)
/// and subtracted. The points over threshold are searched in `integralRange`: pulses of at least
/// `pointsOverThreshold` consecutive points above `pointsThreshold` sigmas whose standard deviation is
/// above `signalThreshold` sigmas (a flat top longer than `nPointsFlat` ends the pulse). The peak amplitude
/// and time come from those points with `maxOption`.
///
/// Example:
/// \code
/// rawAna:
///   class: "TRestRawSignalAnalysisProcess"
///   baselineRange: [5, 55]
///   baselineOption: "robust"       # "", robust or outliers
///   integralRange: [10, 500]
///   pointsThreshold: 2
///   signalThreshold: 5
///   pointsOverThreshold: 5
///   nPointsFlat: 512
///   maxOption: "tripleMaxAverage"  # onlyMax, tripleMax, tripleMaxAverage, gaussFit, landauFit, agetFit
///   observables: ["all"]
/// \endcode
///
/// Observables, one entry per signal (vectors):
/// - **SignalsID**: signal ID.
/// - **Baseline**, **BaselineSigma**: baseline and its sigma.
/// - **AmpSgnMaxMethod**: peak amplitude (maxOption) over the baseline.
/// - **AmpSgnIntMethod**: sum of the points over threshold.
/// - **PeakTime**: bin of the peak.
/// - **RiseTime**, **RiseSlope**: rise time (bins) and slope of the points over threshold.
/// - **Width**: full width at half maximum of the baseline-subtracted signal, in bins.
/// - **PointsOverThr**: number of points over threshold.
/// - **Saturated**: ADC saturation flat top (TRestRawSignal::IsADCSaturation).
///
/// Observables of the event:
/// - **NSignals**, **NGoodSignals**: signals and good signals.
/// - **TimeBinLength**: number of points of the first signal.
/// - **BaselineAvg**, **BaselineSigmaAvg**: averages over all the signals.
/// - **FullIntegralSum**: integral in integralRange of the good signals; **ThresholdIntegralSum**: sum of
///   their AmpSgnIntMethod.
/// - **MaxIntegral** (= **PeakAmplitudeIntegral**): sum of the peak amplitudes of the good signals.
/// - **MaxPeakAmplitude**, **MinPeakAmplitude**: largest and smallest peak amplitude of the good signals.
/// - **MinEventValue**: minimum of the points over threshold of the good signals.
/// - **MaxPeakTime**, **MinPeakTime**, **MaxPeakTimeDelay** (max - min), **AveragePeakTime**.
/// - **RiseSlopeAvg**, **RiseTimeAvg**, **AmplitudeAvg**, **IntegralAvg**, **WidthAvg**,
///   **PointsOverThrAvg**: averages over the good signals.
/// - **SlopeIntegral**: sum over the good signals of their points over threshold up to the first decrease.
/// - **RateOfChangeAvg**: RiseSlopeAvg / SlopeIntegral.
/// - **IntegralBalance**: (FullIntegralSum - ThresholdIntegralSum) / (FullIntegralSum +
///   ThresholdIntegralSum).
/// - **AmplitudeIntegralRatio**: ThresholdIntegralSum / MaxIntegral; **AmplitudeRatio**: MaxIntegral /
///   MaxPeakAmplitude.
///
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
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
        std::vector<double> Width;
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
        double AmplitudeAvg = 0.0;
        double IntegralAvg = 0.0;
        double WidthAvg = 0.0;
        double PointsOverThrAvg = 0.0;

        void clear() {
            auto clear_vectors = [](auto&... vecs) { (vecs.clear(), ...); };
            clear_vectors(SignalsID, Baseline, BaselineSigma, AmpSgnMaxMethod, AmpSgnIntMethod, RiseTime,
                          RiseSlope, PeakTime, Width, PointsOverThr, Saturated);

            BaselineAvg = BaselineSigmaAvg = 0.0;
            TimeBinLength = NSignals = NGoodSignals = 0;
            FullIntegralSum = ThresholdIntegralSum = RiseSlopeAvg = SlopeIntegral = 0.0;
            RateOfChangeAvg = RiseTimeAvg = MaxIntegral = IntegralBalance = 0.0;
            AmplitudeIntegralRatio = MinPeakAmplitude = MaxPeakAmplitude = 0.0;
            PeakAmplitudeIntegral = MinEventValue = AmplitudeRatio = 0.0;
            MaxPeakTime = MinPeakTime = MaxPeakTimeDelay = AveragePeakTime = 0.0;
            AmplitudeAvg = IntegralAvg = WidthAvg = PointsOverThrAvg = 0.0;
        }
    };

    SignalObservables fObs;

   public:
    /// The range where the baseline range will be calculated
    std::pair<int, int> fBaselineRange = {5, 55};

    /// The range where the observables will be calculated
    std::pair<int, int> fIntegralRange = {10, 500};

    /// Option for calculation of baseline parameters: "" (standard deviation), "robust" or "outliers"
    std::string fBaselineOption = "";

    /// Method to find the peak amplitude and time (TRestPulseShapeAnalysis::kMaxOptions)
    std::string fMaxOption = "tripleMaxAverage";

    /// The number of sigmas over baseline fluctuations to identify a point over threshold
    double fPointsThreshold = 2;

    /// A parameter to define a minimum signal fluctuation. Measured in sigmas.
    double fSignalThreshold = 5;

    /// The minimum number of consecutive points over threshold to identify a pulse
    int fPointsOverThreshold = 5;

    /// Number of flat points over threshold that artificially end a pulse
    int fNPointsFlat = 512;

    /// \brief Default constructor (no configuration).
    TRestRawSignalAnalysisProcess();
    /// \brief Constructor from the YAML node of the process section.
    TRestRawSignalAnalysisProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructor from a YAML file and the name of the process section.
    TRestRawSignalAnalysisProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads analysis parameters (ranges, thresholds and options) from YAML.
    virtual void LoadConfig() override;
    /// \brief Registers the observables.
    void InitProcess() override;
    /// \brief Analyzes one input event and fills signal/event-level observables.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Nothing to finalize.
    void EndProcess() override;

    std::string GetInputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }

    std::string GetClassName() const override { return "TRestRawSignalAnalysisProcess"; }
    // void PrintMetadata() override;
};

#endif
