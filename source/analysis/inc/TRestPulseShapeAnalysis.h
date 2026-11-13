#ifndef TRESTPULSESHAPEANALYSIS_H
#define TRESTPULSESHAPEANALYSIS_H

#include <TGraph.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class TF1;

/// \brief Generic functions to calculate signal parameters (baseline, maxima, integrals, rise time, peaks,
/// shaping and noise) on signals given as std::vector (one value per bin) or TGraph (time, value), so they
/// can be used with any signal container (raw, DAQ or detector signals).
///
/// \author Javier Galán <javier.galan@unizar.es>
/// \author Konrad Altenmüller
/// \author JuanAn García <juanangp@unizar.es>
namespace TRestPulseShapeAnalysis {

/// Options of CalculateBaselineAndSigma ("": mean and standard deviation, robust: median and IQR, outliers:
/// excluding the outliers), for TRestTools::NormalizeOption
inline const std::vector<std::string> kBaselineOptions = {"", "robust", "outliers"};

/// Options of GetMax, for TRestTools::NormalizeOption
inline const std::vector<std::string> kMaxOptions = {"onlyMax",  "tripleMax", "tripleMaxAverage",
                                                     "gaussFit", "landauFit", "agetFit"};

/// Types of ShapingFunction, ShapingSupport and BuildResponse, for TRestTools::NormalizeOption
inline const std::vector<std::string> kShapingTypes = {"gaus", "exponential", "shaper", "shaperSin"};

/// \brief AGET response shape u^3·exp(-3u), optionally times sin(u). Not clamped:
/// callers decide what happens for u < 0.
inline double AgetShape(double u, bool withSine = true) {
    const double s = u * u * u * std::exp(-3.0 * u);
    return withSine ? s * std::sin(u) : s;
}

/// \brief Shaping response h(u) of a type, with u = t / shapingTime: gaus (exp(-u^2 / 2), centred),
/// exponential (exp(-u)), shaper and shaperSin (AgetShape). The causal ones are 0 for u < 0. Throws for an
/// unknown type.
double ShapingFunction(const std::string& type, double u);

/// \brief Range of u where the response of a type is computed: [-3.5, 3.5] for gaus, [0, 5] otherwise
std::pair<double, double> ShapingSupport(const std::string& type);

/// \brief Response of a type sampled in bins (shapingTime in bins), normalized so that its samples add up to
/// gain. center is the index of the sample aligned with the input sample (0 for causal responses, half the
/// length for gaus). Returns an empty vector if the type is unknown or the response would be empty/null.
std::vector<double> BuildResponse(const std::string& type, double shapingTime, double gain, int& center);

/// \brief Convolves in with a sampled response (output of the same length as in). With ignoreNegative the
/// negative samples of in are ignored.
std::vector<double> Convolve(const std::vector<double>& in, const std::vector<double>& response, int center,
                             bool ignoreNegative);

/// \brief Baseline and its sigma of the bins [startBin, endBin) of the signal (clamped to it) with an option
/// of kBaselineOptions. An empty range gives 0 and 0 (all the options); returns true.
template <typename T>
bool GetBaselineSigma(const std::vector<T>& signal, int startBin, int endBin, double& baseLine,
                      double& baseLineSigma, std::string option = "");

/// \brief Baseline and sigma (GetBaselineSigma with `option`) and the signal with the baseline subtracted;
/// empty if they cannot be computed
template <typename T>
std::vector<float> CalculateBaselineAndSigma(const std::vector<T>& signal, int startBin, int endBin,
                                             double& baseLine, double& baseLineSigma,
                                             std::string option = "");

/// \brief Baseline as the mean and sigma as the standard deviation of the bins [startBin, endBin), and
/// the signal with the baseline subtracted
template <typename T>
std::vector<float> CalculateBaselineAndSigmaSD(const std::vector<T>& signal, int startBin, int endBin,
                                               double& baseLine, double& baseLineSigma);

/// \brief Baseline as the median and sigma as IQR / 1.349 (robust to outliers) of the bins [startBin,
/// endBin), and the signal with the baseline subtracted
template <typename T>
std::vector<float> CalculateBaselineAndSigmaIQR(const std::vector<T>& signal, int startBin, int endBin,
                                                double& baseLine, double& baseLineSigma);

/// \brief Baseline as the median and sigma as the standard deviation of the values of the bins [startBin,
/// endBin) between the first and third quartiles, and the signal with the baseline subtracted
template <typename T>
std::vector<float> CalculateBaselineAndSigmaExcludeOutliers(const std::vector<T>& signal, int startBin,
                                                            int endBin, double& baseLine,
                                                            double& baseLineSigma);

/// \brief Mean of the bins [startBin, endBin) inside the signal, 0 if there are none
template <typename T>
double GetAverage(const std::vector<T>& signal, int startBin, int endBin);

/// \brief Moving average of 2 * halfWindow + 1 points, shrunk at the edges to the existing points
template <typename T>
std::vector<double> GetMovingAverage(const std::vector<T>& values, int halfWindow);

/// \brief Moving average of averagingPoints (made odd) points; the points closer than half of them to the
/// edges take the average of the closest full window
template <typename T>
std::vector<float> GetSignalSmoothed(const std::vector<T>& signal, int averagingPoints = 3);

/// \brief GetSignalSmoothed after replacing the outliers (more than 3 sigma from the baseline) by the
/// baseline; baseLine and baseLineSigma are computed with CalculateBaselineAndSigmaIQR in the bins
/// [5, size - 5)
template <typename T>
std::vector<float> GetSignalSmoothed_ExcludeOutliers(const std::vector<T>& signal, int averagingPoints,
                                                     double& baseLine, double& baseLineSigma);

/// \brief Differences of consecutive bins, signal[i + 1] - signal[i] (one element less than the signal)
template <typename T>
std::vector<float> GetDerivative(const std::vector<T>& signal);

/// \brief Points (bin, value) of the pulses over threshold of a baseline subtracted signal.
/// \param range Bins [first, second) searched; first < 0 becomes 0 and second <= 0 or beyond the signal
/// becomes its size (the range is updated)
/// \param thrPar (pointThreshold, signalThreshold) in units of baseLineSigma: a pulse is a group of
/// consecutive points over pointThreshold * baseLineSigma, kept if its standard deviation is over
/// signalThreshold * baseLineSigma
/// \param nPointsOver Minimum number of points of a pulse
/// \param nPointsFlat A pulse ends after this number of consecutive points whose difference with the
/// previous one is below the threshold (flat tails over threshold, e.g. overshoots)
/// \param baseLineSigma Baseline fluctuation computed beforehand
template <typename T>
std::vector<std::pair<int, float>> GetPointsOverThreshold(const std::vector<T>& signal,
                                                          std::pair<int, int>& range,
                                                          const std::pair<double, double>& thrPar,
                                                          int nPointsOver, int nPointsFlat,
                                                          double baseLineSigma);

/// \brief Bin of the maximum in [startBin, endBin) (endBin <= 0: up to the end; the first one on ties)
template <typename T>
int GetMaxBin(const std::vector<T>& signal, int startBin = 0, int endBin = 0) {
    if (endBin <= 0 || endBin > (int)signal.size()) endBin = signal.size();
    if (startBin < 0) startBin = 0;

    return std::distance(signal.begin(),
                         std::max_element(signal.begin() + startBin, signal.begin() + endBin));
}

/// \brief Bin of the minimum in [startBin, endBin) (endBin <= 0: up to the end; the first one on ties)
template <typename T>
int GetMinBin(const std::vector<T>& signal, int startBin = 0, int endBin = 0) {
    if (endBin <= 0 || endBin > (int)signal.size()) endBin = signal.size();
    if (startBin < 0) startBin = 0;

    return std::distance(signal.begin(),
                         std::min_element(signal.begin() + startBin, signal.begin() + endBin));
}

/// \brief Sum of the bins [startBin, endBin) inside the signal
template <typename T>
double GetIntegral(const std::vector<T>& signal, int startBin, int endBin);

/// \brief Width in bins of the highest peak: distance between the first bins at each side of the maximum
/// that are not above half of it (or the edges); 0 for an empty signal
template <typename T>
double GetMaxPeakWidth(const std::vector<T>& signal);

/// \brief Finds all the peaks of a baseline subtracted signal given as (time, value) points sorted in time,
/// e.g. TRestDetectorSignal::GetPointsVector. A point of the signal smoothed with a moving average of
/// 2 * halfWindow + 1 consecutive points is a peak candidate if it is over threshold and at most
/// maxGreaterEqual of the other points of that window are greater or equal. With refineMaximum the peak is
/// the highest smoothed point within `distance` (time units) after the candidate (on flat tops the candidate
/// is at the left of the maximum), otherwise the candidate itself; the next peak must be at least `distance`
/// after it. Returns (time, amplitude) pairs, with the amplitude as the average of the (not smoothed) points
/// within amplitudeHalfWindow of the peak (0: the peak point alone). The legacy TRestRawPeaksFinderProcess
/// used the defaults for TPC signals (TRestRawSignal::GetPeaks) and halfWindow = 2, maxGreaterEqual = 0,
/// refineMaximum = false, amplitudeHalfWindow = 0 for veto signals (TRestRawSignal::GetPeaksVeto).
std::vector<std::pair<double, double>> GetPeaks(const std::vector<std::pair<double, double>>& points,
                                                double threshold, double distance, int halfWindow = 5,
                                                int maxGreaterEqual = 3, bool refineMaximum = true,
                                                int amplitudeHalfWindow = 1);

/// \brief Sum of the values of the first rise of a pulse (e.g. GetPointsOverThreshold), from the first
/// point until the values decrease
template <typename T>
double GetSlopeIntegral(const std::vector<std::pair<T, float>>& signal);
/// \brief Slope of the rise of a pulse (e.g. GetPointsOverThreshold): (maximum - first value) / (time of
/// the maximum - first time); 0 with less than 2 points or if the first point is the maximum
template <typename T>
double GetRiseSlope(const std::vector<std::pair<T, float>>& signal);
/// \brief Rise time of a pulse (e.g. GetPointsOverThreshold): time of the maximum - first time; 0 with less
/// than 2 points
template <typename T>
double GetRiseTime(const std::vector<std::pair<T, float>>& signal);

/// \brief Graph of (x, y) points with a title (empty graph without points)
template <typename T, typename U>
TGraph GetGraphPair(const std::vector<std::pair<T, U>>& points, const std::string& title = "");

/// \brief Graph of a signal, (bin, value), with a title (empty graph for an empty signal)
template <typename T>
TGraph GetGraph(const std::vector<T>& signal, const std::string& title = "");

/// \brief Fills `noise` (its size kept) with gaussian noise of sigma `amplitude` (gRandom)
template <typename T>
void GetWhiteNoise(std::vector<T>& noise, double amplitude);

/// \brief Fills `noise` (its size kept) with a sine of `period` bins (not necessarily an integer) and `phase`
/// whose amplitude is modulated by a slower random sine (period 4 to 8 times longer), at most maxAmplitude
/// (gRandom); zeros if period or maxAmplitude are not positive
template <typename T>
void GetSinNoise(std::vector<T>& noise, double maxAmplitude, double period, double phase = 0);

/// \brief Points grouped in time windows [k * intWindow, (k + 1) * intWindow) (any integer k): (center of
/// the window, mean value of its points), in increasing time. Throws std::invalid_argument if intWindow is
/// not positive.
std::vector<std::pair<double, double>> GetIntWindow(TGraph& signal, double intWindow);
/// \brief (time, value) of the maximum point and of its two neighbours (the maximum repeated at the
/// edges); the three (-1, -1) for an empty graph
std::array<std::pair<double, double>, 3> GetTripleMax(TGraph& signal);
/// \brief (time, value) of the minimum point; (-1, -1) for an empty graph
std::pair<double, double> GetMin(TGraph& signal);
/// \brief (time, amplitude) of the maximum with an option of kMaxOptions: onlyMax (GetMaxAmplitude,
/// also for an unknown option), tripleMax (GetTripleMaxIntegral), tripleMaxAverage (GetTripleMaxAverage),
/// gaussFit (GetMaxGauss), landauFit (GetMaxLandau) or agetFit (GetMaxAget); (-1, -1) for an empty graph or
/// a failed agetFit
std::pair<double, double> GetMax(TGraph& signal, std::string option);
/// \brief (time, value) of the maximum point; (-1, -1) for an empty graph
std::pair<double, double> GetMaxAmplitude(TGraph& signal);
/// \brief Triple max (GetTripleMax): (value weighted mean time, mean value) of the three points; (0, 0) if
/// their values add up to 0, (-1, -1) for an empty graph
std::pair<double, double> GetTripleMaxAverage(TGraph& signal);
/// \brief Triple max (GetTripleMax): (value weighted mean time, sum of the values) of the three points;
/// (0, 0) if their values add up to 0, (-1, -1) for an empty graph
std::pair<double, double> GetTripleMaxIntegral(TGraph& signal);
/// \brief Gaussian fit of the top of the pulse (the points over half the maximum around it and one more on
/// each side), with initial values from the pulse: (mean, amplitude); the maximum point if the top has less
/// than 4 points or the fit fails, (-1, -1) for an empty graph
std::pair<double, double> GetMaxGauss(TGraph& signal);
/// \brief Landau fit of the top of the pulse (as GetMaxGauss): (most probable value, constant); the maximum
/// point if the top has less than 4 points or the fit fails, (-1, -1) for an empty graph
std::pair<double, double> GetMaxLandau(TGraph& signal);
/// \brief AGET response fit (FitAget, with sine, baseline fixed to 0) to the points in [0.75, 1.35] times
/// the time of the maximum point: (time of the maximum, amplitude); (-1, -1) for an empty graph or a failed
/// fit
std::pair<double, double> GetMaxAget(TGraph& signal);

/// \brief Fits the AGET response to all the points of the graph (to fit a different range or to exclude
/// points, pass a reduced graph). The model is baseline + amplitude * S((x - time) / width + peakU), where S
/// is AgetShape normalised to 1 at its maximum and zero for u <= 0. If fixedBaseline is given it is not
/// fitted. Returns the fitted function with parameters (0) Baseline, (1) Amplitude of the maximum above
/// the baseline, (2) Time of the maximum, (3) Width; or nullptr if the fit fails.
std::unique_ptr<TF1> FitAget(TGraph& signal, bool withSine = true, std::optional<double> fixedBaseline = {},
                             bool verbose = false);

}  // namespace TRestPulseShapeAnalysis

#endif
