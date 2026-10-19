#ifndef TRESTPULSESHAPEANALYSIS_H
#define TRESTPULSESHAPEANALYSIS_H

#include <TGraph.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <numeric>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class TF1;

/// This namespace define generic functions to calculate different signal parameters
namespace TRestPulseShapeAnalysis {

/// \brief AGET response shape u^3·exp(-3u), optionally times sin(u). Not clamped:
/// callers decide what happens for u < 0.
inline double AgetShape(double u, bool withSine = true) {
    const double s = u * u * u * std::exp(-3.0 * u);
    return withSine ? s * std::sin(u) : s;
}

template <typename T>
bool GetBaselineSigma(const std::vector<T>& signal, int startBin, int endBin, double& baseLine,
                      double& baseLineSigma, std::string option = "");

template <typename T>
std::vector<float> CalculateBaselineAndSigma(const std::vector<T>& signal, int startBin, int endBin,
                                             double& baseLine, double& baseLineSigma,
                                             std::string option = "");

template <typename T>
std::vector<float> CalculateBaselineAndSigmaSD(const std::vector<T>& signal, int startBin, int endBin,
                                               double& baseLine, double& baseLineSigma);

template <typename T>
std::vector<float> CalculateBaselineAndSigmaIQR(const std::vector<T>& signal, int startBin, int endBin,
                                                double& baseLine, double& baseLineSigma);

template <typename T>
std::vector<float> CalculateBaselineAndSigmaExcludeOutliers(const std::vector<T>& signal, int startBin,
                                                            int endBin, double& baseLine,
                                                            double& baseLineSigma);

template <typename T>
double GetAverage(const std::vector<T>& signal, int startBin, int endBin);

template <typename T>
std::vector<float> GetSignalSmoothed(const std::vector<T>& signal, int averagingPoints = 3);

template <typename T>
std::vector<float> GetSignalSmoothed_ExcludeOutliers(const std::vector<T>& signal, int averagingPoints,
                                                     double& baseLine, double& baseLineSigma);

template <typename T>
std::vector<float> GetDerivative(const std::vector<T>& signal);

template <typename T>
std::vector<std::pair<int, float>> GetPointsOverThreshold(const std::vector<T>& signal,
                                                          std::pair<int, int>& range,
                                                          const std::pair<double, double>& thrPar,
                                                          int nPointsOver, int nPointsFlat,
                                                          double baseLineSigma);

template <typename T>
int GetMaxBin(const std::vector<T>& signal, int startBin = 0, int endBin = 0) {
    if (endBin <= 0 || endBin > (int)signal.size()) endBin = signal.size();
    if (startBin < 0) startBin = 0;

    return std::distance(signal.begin(),
                         std::max_element(signal.begin() + startBin, signal.begin() + endBin));
}

template <typename T>
int GetMinBin(const std::vector<T>& signal, int startBin = 0, int endBin = 0) {
    if (endBin <= 0 || endBin > (int)signal.size()) endBin = signal.size();
    if (startBin < 0) startBin = 0;

    return std::distance(signal.begin(),
                         std::min_element(signal.begin() + startBin, signal.begin() + endBin));
}

template <typename T>
double GetIntegral(const std::vector<T>& signal, int startBin, int endBin);

template <typename T>
double GetMaxPeakWidth(const std::vector<T>& signal);

template <typename T>
double GetSlopeIntegral(const std::vector<std::pair<T, float>>& signal);
template <typename T>
double GetRiseSlope(const std::vector<std::pair<T, float>>& signal);
template <typename T>
double GetRiseTime(const std::vector<std::pair<T, float>>& signal);

template <typename T>
double GetRiseTime(const std::vector<std::pair<T, float>>& signal);

template <typename T, typename U>
TGraph GetGraphPair(const std::vector<std::pair<T, U>>& points,const std::string& title="");

template <typename T>
TGraph GetGraph(const std::vector<T>& signal, const std::string& title = "");

template <typename T>
void GetWhiteNoise(std::vector<T>& noise, double amplitude);

template <typename T>
void GetSinNoise(std::vector<T>& noise, double maxAmplitude, int period, double phase = 0);

std::vector<std::pair<double, double>> GetIntWindow(TGraph& signal, double intWindow);
std::array<std::pair<double, double>, 3> GetTripleMax(TGraph& signal);
std::pair<double, double> GetMin(TGraph& signal);
std::pair<double, double> GetMax(TGraph& signal, std::string option);
std::pair<double, double> GetMaxAmplitude(TGraph& signal);
std::pair<double, double> GetTripleMaxAverage(TGraph& signal);
std::pair<double, double> GetTripleMaxIntegral(TGraph& signal);
std::pair<double, double> GetMaxGauss(TGraph& signal);
std::pair<double, double> GetMaxLandau(TGraph& signal);
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
