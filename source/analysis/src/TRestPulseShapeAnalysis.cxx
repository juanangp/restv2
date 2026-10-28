#include <TF1.h>
#include <TFitResult.h>
#include <TRandom.h>

#include "TRestPulseShapeAnalysis.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <stdexcept>

namespace {

constexpr double kAgetPeakU = 1.1664;         // u where u^3 e^{-3u} sin(u) is maximum
constexpr double kAgetPeakValue = 0.0440895;  // value of the shape at that point

using TF1Maker = std::function<std::unique_ptr<TF1>(double lower, double upper)>;

/// Fits the function (parameter 0 = amplitude, parameter 1 = position) in a window around the maximum
/// of the graph. Returns {position, amplitude}, or {-1, -1} if the fit fails.
std::pair<double, double> FitAroundMaximum(TGraph& signal, const char* label, double lowFrac,
                                           double highFrac, const TF1Maker& make) {
    constexpr double kFailed = -1;
    if (signal.GetN() == 0) return {kFailed, kFailed};

    const int maxBin = TMath::LocMax(signal.GetN(), signal.GetY());
    const double maxTime = signal.GetPointX(maxBin);

    std::unique_ptr<TF1> f = make(maxTime - maxTime * lowFrac, maxTime + maxTime * highFrac);
    TFitResultPtr fitResult = signal.Fit(f.get(), "QNRS");

    if (!fitResult.Get() || !fitResult->IsValid()) {
        std::cout << "WARNING: bad " << label << " fit with maximum at time = " << maxTime
                  << ". Failed parameters:";
        for (int i = 0; i < f->GetNpar(); i++) std::cout << " " << f->GetParameter(i);
        std::cout << ". Assigned fit parameters: " << kFailed << std::endl;
        return {kFailed, kFailed};
    }
    return {f->GetParameter(1), f->GetParameter(0)};
}

template <typename T>
std::vector<float> SubtractBaseline(const std::vector<T>& signal, double baseLine) {
    std::vector<float> data(signal.size());
    for (size_t i = 0; i < signal.size(); i++) data[i] = signal[i] - baseLine;
    return data;
}

///////////////////////////////////////////////
/// \brief This method is used to determine the value
/// of the baseline as average (arithmetic mean) of the
/// data in the range defined between startBin and endBin.
/// The baseline sigma is determined as the standard deviation
/// of the baseline in range provided.
template <typename T>
bool BaselineSigmaSD(const std::vector<T>& signal, int startBin, int endBin, double& baseLine,
                     double& baseLineSigma) {
    baseLine = 0;
    baseLineSigma = 0;

    double sum = 0, sum2 = 0;
    int nPoints = 0;
    for (int i = startBin; i < endBin; i++) {
        if (i < 0 || i >= (int)signal.size()) continue;
        const double v = signal[i];
        sum += v;
        sum2 += v * v;
        nPoints++;
    }

    if (nPoints > 0) {
        baseLine = sum / nPoints;
        baseLineSigma = std::sqrt(std::max(0.0, sum2 / nPoints - baseLine * baseLine));
    }
    return true;
}

///////////////////////////////////////////////
/// \brief This method is used to determine the value
/// of the baseline as the median of the data in
/// the range defined between startBin and endBin.
/// The baseline sigma is determined as the interquartile
/// range (IQR) in the baseline range provided. The IQR
/// is more robust towards outliers than the standard deviation.
template <typename T>
bool BaselineSigmaIQR(const std::vector<T>& signal, int startBin, int endBin, double& baseLine,
                      double& baseLineSigma) {
    baseLine = 0;
    baseLineSigma = 0;

    if (signal.empty()) return false;
    if (startBin < 0) startBin = 0;
    if (startBin >= (int)signal.size()) return false;
    if (endBin >= (int)signal.size()) endBin = signal.size() - 1;
    if (endBin < startBin) return false;

    std::vector<T> v(signal.begin() + startBin, signal.begin() + endBin + 1);
    if (v.empty()) return false;

    baseLine = TMath::Median((int)v.size(), v.data());

    std::sort(v.begin(), v.end());
    const size_t q1 = static_cast<size_t>(0.25 * (v.size() - 1));
    const size_t q3 = static_cast<size_t>(0.75 * (v.size() - 1));
    baseLineSigma = (v[q3] - v[q1]) / 1.349;  // IQR/1.349 = sigma for gaussian data
    return true;
}


template <typename T>
bool BaselineSigmaExcludeOutliers(const std::vector<T>& signal, int startBin, int endBin, double& baseLine,
                                  double& baseLineSigma) {
    baseLine = 0;
    baseLineSigma = 0;

    if (signal.empty()) return false;
    if (startBin < 0) startBin = 0;
    if (startBin >= (int)signal.size()) return false;
    if (endBin >= (int)signal.size()) endBin = signal.size() - 1;
    if (endBin < startBin) return false;

    std::vector<double> v;
    for (int i = startBin; i <= endBin; ++i) v.emplace_back(signal[i]);

    if (v.empty()) return false;

    std::sort(v.begin(), v.end());
    const size_t q1 = static_cast<size_t>(0.25 * (v.size() - 1));
    const size_t q3 = static_cast<size_t>(0.75 * (v.size() - 1));

    std::vector<double> filteredData;
    for (const auto& value : v) {
        if (value >= v[q1] && value <= v[q3]) filteredData.emplace_back(value);
    }

    if (filteredData.empty()) return false;

    baseLine = TMath::Median(filteredData.size(), filteredData.data());

    double mean = std::accumulate(filteredData.begin(), filteredData.end(), 0.0) / filteredData.size();
    double variance = 0.0;
    for (const auto& value : filteredData) variance += std::pow(value - mean, 2);
    baseLineSigma = std::sqrt(variance / filteredData.size());

    return true;
}

}  // namespace

template <typename T>
bool TRestPulseShapeAnalysis::GetBaselineSigma(const std::vector<T>& signal, int startBin, int endBin,
                                               double& baseLine, double& baseLineSigma,
                                               std::string option) {
    if (option == "robust") return BaselineSigmaIQR(signal, startBin, endBin, baseLine, baseLineSigma);
    if (option == "outliers")
        return BaselineSigmaExcludeOutliers(signal, startBin, endBin, baseLine, baseLineSigma);
    return BaselineSigmaSD(signal, startBin, endBin, baseLine, baseLineSigma);
}

template bool TRestPulseShapeAnalysis::GetBaselineSigma<short>(const std::vector<short>& signal,
                                                               int startBin, int endBin, double& baseLine,
                                                               double& baseLineSigma, std::string option);
template bool TRestPulseShapeAnalysis::GetBaselineSigma<float>(const std::vector<float>& signal,
                                                               int startBin, int endBin, double& baseLine,
                                                               double& baseLineSigma, std::string option);

template <typename T>
std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigma(const std::vector<T>& signal,
                                                                      int startBin, int endBin,
                                                                      double& baseLine, double& baseLineSigma,
                                                                      std::string option) {
    if (!GetBaselineSigma(signal, startBin, endBin, baseLine, baseLineSigma, option)) return {};
    return SubtractBaseline(signal, baseLine);
}

template std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigma<short>(
    const std::vector<short>& signal, int startBin, int endBin, double& baseLine, double& baseLineSigma,
    std::string option);
template std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigma<float>(
    const std::vector<float>& signal, int startBin, int endBin, double& baseLine, double& baseLineSigma,
    std::string option);


template <typename T>
std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigmaSD(const std::vector<T>& signal,
                                                                        int startBin, int endBin,
                                                                        double& baseLine,
                                                                        double& baseLineSigma) {
    BaselineSigmaSD(signal, startBin, endBin, baseLine, baseLineSigma);
    return SubtractBaseline(signal, baseLine);
}
template std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigmaSD<short>(
    const std::vector<short>& signal, int startBin, int endBin, double& baseLine, double& baseLineSigma);
template std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigmaSD<float>(
    const std::vector<float>& signal, int startBin, int endBin, double& baseLine, double& baseLineSigma);

template <typename T>
std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigmaIQR(const std::vector<T>& signal,
                                                                         int startBin, int endBin,
                                                                         double& baseLine,
                                                                         double& baseLineSigma) {
    if (!BaselineSigmaIQR(signal, startBin, endBin, baseLine, baseLineSigma)) return {};
    return SubtractBaseline(signal, baseLine);
}
template std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigmaIQR<short>(
    const std::vector<short>& signal, int startBin, int endBin, double& baseLine, double& baseLineSigma);
template std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigmaIQR<float>(
    const std::vector<float>& signal, int startBin, int endBin, double& baseLine, double& baseLineSigma);

template <typename T>
std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigmaExcludeOutliers(
    const std::vector<T>& signal, int startBin, int endBin, double& baseLine, double& baseLineSigma) {
    if (!BaselineSigmaExcludeOutliers(signal, startBin, endBin, baseLine, baseLineSigma)) return {};
    return SubtractBaseline(signal, baseLine);
}
template std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigmaExcludeOutliers<short>(
    const std::vector<short>& signal, int startBin, int endBin, double& baseLine, double& baseLineSigma);
template std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigmaExcludeOutliers<float>(
    const std::vector<float>& signal, int startBin, int endBin, double& baseLine, double& baseLineSigma);

///////////////////////////////////////////////
/// \brief This method performs the average of
/// the data points in a given range defined
/// between startBin and endBin
template <typename T>
double TRestPulseShapeAnalysis::GetAverage(const std::vector<T>& signal, int startBin, int endBin) {
    if (signal.empty()) return 0;

    int nPoints = 0;
    double avg = 0;

    for (int i = startBin; i < endBin; i++) {
        if (i < 0 || i >= (int)signal.size()) continue;
        avg += signal[i];
        nPoints++;
    }

    if (nPoints > 0) avg /= (double)nPoints;

    return avg;
}

template double TRestPulseShapeAnalysis::GetAverage<short>(const std::vector<short>& signal, int startBin,
                                                           int endBin);
template double TRestPulseShapeAnalysis::GetAverage<float>(const std::vector<float>& signal, int startBin,
                                                           int endBin);

///////////////////////////////////////////////
/// \brief Return smoothing of signal as the
/// moving average of the data
///
/// \param averagingPoints defines the number of
/// neighbouring points used to average the signal
///
template <typename T>
std::vector<float> TRestPulseShapeAnalysis::GetSignalSmoothed(const std::vector<T>& signal,
                                                              int averagingPoints) {
    const int pulseDepth = signal.size();
    std::vector<float> smoothed(pulseDepth, 0);

    if (pulseDepth == 0) return smoothed;

    averagingPoints = std::max(1, averagingPoints);
    if (averagingPoints > pulseDepth) averagingPoints = pulseDepth;
    averagingPoints = (averagingPoints / 2) * 2 + 1;  // make it odd >= averagingPoints
    if (averagingPoints > pulseDepth) averagingPoints -= 1;
    if (averagingPoints <= 0) averagingPoints = 1;

    const int halfWindow = averagingPoints / 2;
    float sumAvg = TRestPulseShapeAnalysis::GetAverage(signal, 0, averagingPoints);

    // Points at the beginning, where we can calculate a moving average
    for (int i = 0; i <= halfWindow && i < pulseDepth; i++) smoothed[i] = sumAvg;

    for (int i = halfWindow + 1; i < pulseDepth - halfWindow; i++) {
        int removeIndex = i - halfWindow - 1;
        sumAvg -= signal[removeIndex] / averagingPoints;

        int addIndex = i + halfWindow;
        sumAvg += signal[addIndex] / averagingPoints;
        smoothed[i] = sumAvg;
    }

    // Points at the end, where we can calculate a moving average
    for (int i = std::max(0, pulseDepth - halfWindow); i < pulseDepth; i++) smoothed[i] = sumAvg;

    return smoothed;
}
template std::vector<float> TRestPulseShapeAnalysis::GetSignalSmoothed<short>(
    const std::vector<short>& signal, int averagingPoints);
template std::vector<float> TRestPulseShapeAnalysis::GetSignalSmoothed<float>(
    const std::vector<float>& signal, int averagingPoints);

///////////////////////////////////////////////
/// \brief Return smoothing of signal, the points
/// that are too far away from the median baseline
/// will be ignored to improve the smoothing result.
///
/// \param averagingPoints defines the number of
/// neighbouring points used to average the signal
///
template <typename T>
std::vector<float> TRestPulseShapeAnalysis::GetSignalSmoothed_ExcludeOutliers(const std::vector<T>& signal,
                                                                              int averagingPoints,
                                                                              double& baseLine,
                                                                              double& baseLineSigma) {
    const int pulseDepth = signal.size();
    std::vector<float> smoothed(pulseDepth, 0);

    if (pulseDepth == 0) return smoothed;

    CalculateBaselineAndSigmaIQR(signal, 5, int(pulseDepth - 5), baseLine, baseLineSigma);

    averagingPoints = std::max(1, averagingPoints);
    if (averagingPoints > pulseDepth) averagingPoints = pulseDepth;
    averagingPoints = (averagingPoints / 2) * 2 + 1;  // make it odd >= averagingPoints
    if (averagingPoints > pulseDepth) averagingPoints -= 1;
    if (averagingPoints <= 0) averagingPoints = 1;

    const int halfWindow = averagingPoints / 2;
    float sumAvg = TRestPulseShapeAnalysis::GetAverage(signal, 0, averagingPoints);

    // Points at the beginning, where we can calculate a moving average
    for (int i = 0; i <= halfWindow && i < pulseDepth; i++) smoothed[i] = sumAvg;

    // Points in the middle
    float amplitude;
    for (int i = halfWindow + 1; i < pulseDepth - halfWindow; i++) {
        int removeIndex = i - halfWindow - 1;
        amplitude = signal[removeIndex];
        sumAvg -= (std::abs(amplitude - baseLine) > 3 * baseLineSigma) ? baseLine / averagingPoints
                                                                       : amplitude / averagingPoints;

        int addIndex = i + halfWindow;
        amplitude = signal[addIndex];
        sumAvg += (std::abs(amplitude - baseLine) > 3 * baseLineSigma) ? baseLine / averagingPoints
                                                                       : amplitude / averagingPoints;
        smoothed[i] = sumAvg;
    }

    // Points at the end, where we can calculate a moving average
    for (int i = std::max(0, pulseDepth - halfWindow); i < pulseDepth; i++) smoothed[i] = sumAvg;

    return smoothed;
}

template std::vector<float> TRestPulseShapeAnalysis::GetSignalSmoothed_ExcludeOutliers<Short_t>(
    const std::vector<Short_t>& signal, int averagingPoints, double& baseLine, double& baseLineSigma);
template std::vector<float> TRestPulseShapeAnalysis::GetSignalSmoothed_ExcludeOutliers<float>(
    const std::vector<float>& signal, int averagingPoints, double& baseLine, double& baseLineSigma);

///////////////////////////////////////////////
/// \brief Return derivative of a vector of data
/// points
///
template <typename T>
std::vector<float> TRestPulseShapeAnalysis::GetDerivative(const std::vector<T>& signal) {
    std::vector<float> derivative(signal.size() > 1 ? signal.size() - 1 : 0, 0);
    if (signal.size() < 2) return derivative;

    for (size_t i = 0; i + 1 < signal.size(); i++) {
        derivative[i] = signal[i + 1] - signal[i];
    }

    return derivative;
}

template std::vector<float> TRestPulseShapeAnalysis::GetDerivative(const std::vector<float>& signal);
template std::vector<float> TRestPulseShapeAnalysis::GetDerivative(const std::vector<Short_t>& signal);

///////////////////////////////////////////////
/// \brief It returns a vector of the data points
/// that are found over threshold.
/// The parameters provided to this method are
/// used to identify those points.
///
/// \param thrPar A std::pair defining two parameters: *pointThreshold* and
/// *signalThreshold*. Both numbers
/// define the number of sigmas over the baseline fluctuation, stored in
/// baseLineSigma. The first parameter,
/// *pointThreshold*, serves to identify if a single point is over threshold by
/// satisfying the condition that
/// is above the baseline by the number of sigmas given in *pointThreshold*.
/// Once a certain number of
/// consecutive points have been identified, the parameter *signalThreshold*
/// will serve to reject the signals
/// (consecutive points over threshold) that their standard deviation is lower
/// that *signalThreshold* times
/// the baseline fluctuation.
///
/// \param nPointsOver Only data points with at least *nPointsOver* consecutive
/// points will be considered.
///
/// \param nPointsFlat It will serve to terminate the points over threshold
/// identification in signals where
/// we find an overshoot, being the baseline not returning to zero (or its
/// original value) at the signal tail.
///
/// \param baseLine value of the signal baseline calculated beforehand
///
/// \param baseLineSigma value of the baseline fluctuation calculated beforehand
///

template <typename T>
std::vector<std::pair<int, float>> TRestPulseShapeAnalysis::GetPointsOverThreshold(
    const std::vector<T>& signal, std::pair<int, int>& range, const std::pair<double, double>& thrPar,
    int nPointsOver, int nPointsFlat, double baseLineSigma) {
    if (signal.empty()) return {};

    if (range.first < 0) range.first = 0;
    if (range.second <= 0 || range.second > (int)signal.size()) range.second = signal.size();

    const int start = std::max(0, (int)range.first);
    const int stop = std::min((int)signal.size(), (int)range.second);
    if (start >= stop) return {};

    std::vector<std::pair<int, float>> pointsOverThreshold;

    double pointTh = thrPar.first;
    double signalTh = thrPar.second;

    double threshold = pointTh * baseLineSigma;

    for (int i = start; i < stop; i++) {
        // Filling a pulse with consecutive points that are over threshold
        if (signal[i] > threshold) {
            int pos = i;
            std::vector<double> pulse;
            pulse.push_back(signal[i]);
            i++;

            // If the pulse ends in a flat end above the threshold, the parameter
            // nPointsFlat will serve to artificially end the pulse.
            // If nPointsFlat is big enough, this parameter will not affect the
            // decision to cut this anomalous behaviour. And all points over threshold
            // will be added to the pulse vector.
            int flatN = 0;
            while (i < stop && signal[i] > threshold) {
                if (TMath::Abs(signal[i] - signal[i - 1]) > threshold) {
                    flatN = 0;
                } else {
                    flatN++;
                }

                if (flatN < nPointsFlat) {
                    pulse.push_back(signal[i]);
                    i++;
                } else {
                    break;
                }
            }

            if (pulse.size() >= (unsigned int)nPointsOver) {
                double mean = std::accumulate(pulse.begin(), pulse.end(), 0.0) / pulse.size();
                double sq_sum = std::inner_product(pulse.begin(), pulse.end(), pulse.begin(), 0.0);
                double stdev = std::sqrt(sq_sum / pulse.size() - mean * mean);

                if (stdev > signalTh * baseLineSigma)
                    for (unsigned int j = 0; j < pulse.size(); j++)
                        pointsOverThreshold.push_back(std::make_pair(pos + j, pulse[j]));
            }
        }
    }

    return pointsOverThreshold;
}
template std::vector<std::pair<int, float>> TRestPulseShapeAnalysis::GetPointsOverThreshold<Short_t>(
    const std::vector<short>& signal, std::pair<int, int>& range, const std::pair<double, double>& thrPar,
    int nPointsOver, int nPointsFlat, double baseLineSigma);
template std::vector<std::pair<int, float>> TRestPulseShapeAnalysis::GetPointsOverThreshold<float>(
    const std::vector<float>& signal, std::pair<int, int>& range, const std::pair<double, double>& thrPar,
    int nPointsOver, int nPointsFlat, double baseLineSigma);

///////////////////////////////////////////////
/// \brief It returns the integral of the signal in the
/// range passed as argument
///
template <typename T>
double TRestPulseShapeAnalysis::GetIntegral(const std::vector<T>& signal, int startBin, int endBin) {
    double sum = 0;
    for (int i = startBin; i < endBin; i++)
        if (i >= 0 && i < (int)signal.size()) sum += signal[i];

    return sum;
}
template double TRestPulseShapeAnalysis::GetIntegral(const std::vector<short>& signal, int startBin,
                                                     int endBin);
template double TRestPulseShapeAnalysis::GetIntegral(const std::vector<float>& signal, int startBin,
                                                     int endBin);

template <typename T>
TGraph TRestPulseShapeAnalysis::GetGraph(const std::vector<T>& signal, const std::string& title) {
    size_t nPoints = signal.size();
    if (nPoints <= 0) return TGraph();  // return empty TGraph

    TGraph graph(nPoints);

    for (size_t i = 0; i < nPoints; ++i) {
        graph.SetPoint(i, i, signal[i]);
    }
    graph.SetTitle(title.c_str());
    return graph;
}

template TGraph TRestPulseShapeAnalysis::GetGraph<short>(const std::vector<short>& signal,
                                                         const std::string& title);
template TGraph TRestPulseShapeAnalysis::GetGraph<float>(const std::vector<float>& signal,
                                                         const std::string& title);

template <typename T, typename U>
TGraph TRestPulseShapeAnalysis::GetGraphPair(const std::vector<std::pair<T, U>>& points,
                                             const std::string& title) {
    size_t nPoints = points.size();
    if (nPoints <= 0) return TGraph();  // return empty TGraph

    TGraph graph(nPoints);

    for (size_t i = 0; i < nPoints; ++i) {
        graph.SetPoint(i, points[i].first, points[i].second);
    }
    graph.SetTitle(title.c_str());
    return graph;
}

template TGraph TRestPulseShapeAnalysis::GetGraphPair<int, float>(
    const std::vector<std::pair<int, float>>& points, const std::string& title);

template TGraph TRestPulseShapeAnalysis::GetGraphPair<float, float>(
    const std::vector<std::pair<float, float>>& points, const std::string& title);

template TGraph TRestPulseShapeAnalysis::GetGraphPair<double, double>(
    const std::vector<std::pair<double, double>>& points, const std::string& title);

///////////////////////////////////////////////
/// \brief It returns the width of the pulses
/// as the number of bins between the half of the
/// maximum
///
template <typename T>
double TRestPulseShapeAnalysis::GetMaxPeakWidth(const std::vector<T>& signal) {
    if (signal.empty()) return 0;

    int maxIndex = GetMaxBin(signal);
    double maxValue = signal[maxIndex];

    const int signalSize = signal.size();

    int rightIndex = maxIndex;
    while (rightIndex + 1 < signalSize && signal[rightIndex] > maxValue / 2.) {
        rightIndex++;
    }

    int leftIndex = maxIndex;
    while (leftIndex - 1 >= 0 && signal[leftIndex] > maxValue / 2.) {
        leftIndex--;
    }

    return rightIndex - leftIndex;
}
template double TRestPulseShapeAnalysis::GetMaxPeakWidth(const std::vector<short>& signal);
template double TRestPulseShapeAnalysis::GetMaxPeakWidth(const std::vector<float>& signal);

///////////////////////////////////////////////
/// \brief It returns the (time, amplitude) of all
/// the peaks found in the signal, see the header
/// for the definition of peak
///
std::vector<std::pair<double, double>> TRestPulseShapeAnalysis::GetPeaks(
    const std::vector<std::pair<double, double>>& points, double threshold, double distance, int halfWindow,
    int maxGreaterEqual) {
    std::vector<std::pair<double, double>> peaks;
    const int nPoints = points.size();
    if (nPoints == 0) return peaks;

    halfWindow = std::max(1, halfWindow);

    // Moving average of 2 * halfWindow + 1 points, shrunk at the edges
    std::vector<double> smoothed(nPoints, 0);
    for (int i = 0; i < nPoints; i++) {
        const int startBin = std::max(0, i - halfWindow);
        const int endBin = std::min(nPoints - 1, i + halfWindow);
        for (int j = startBin; j <= endBin; j++) smoothed[i] += points[j].second;
        smoothed[i] /= endBin - startBin + 1;
    }

    int lastPeak = -1;
    for (int i = halfWindow; i < nPoints - halfWindow; i++) {
        if (smoothed[i] <= threshold) continue;
        if (lastPeak >= 0 && points[i].first - points[lastPeak].first < distance) continue;

        int nGreaterEqual = 0;
        for (int j = i - halfWindow; j <= i + halfWindow && nGreaterEqual <= maxGreaterEqual; j++) {
            if (j != i && smoothed[i] <= smoothed[j]) nGreaterEqual++;
        }
        if (nGreaterEqual > maxGreaterEqual) continue;

        // On flat tops the candidate is at the left of the actual maximum
        int maxBin = i;
        for (int j = i + 1; j < nPoints && points[j].first - points[i].first <= distance; j++) {
            if (smoothed[j] > smoothed[maxBin]) maxBin = j;
        }

        const int startBin = std::max(0, maxBin - 1);
        const int endBin = std::min(nPoints - 1, maxBin + 1);
        double amplitude = 0;
        for (int j = startBin; j <= endBin; j++) amplitude += points[j].second;
        amplitude /= endBin - startBin + 1;

        peaks.emplace_back(points[maxBin].first, amplitude);
        lastPeak = maxBin;
    }

    return peaks;
}

///////////////////////////////////////////////
/// \brief It performs a gaussian fit to the signal
/// which is stored inside a TGraph. It returns
/// a TVector 2 with the maximum and the mean of the
/// gaussian fit
///
std::pair<double, double> TRestPulseShapeAnalysis::GetMaxGauss(TGraph& signal) {
    return FitAroundMaximum(signal, "gaussian", 0.1, 0.2, [](double lo, double hi) {
        return std::make_unique<TF1>("gaus", "gaus", lo, hi);
    });
}

///////////////////////////////////////////////
/// \brief It performs a landau fit to the signal
/// which is stored inside a TGraph. It returns
/// a TVector 2 with the maximum and the mean of the
/// landau fit
///
std::pair<double, double> TRestPulseShapeAnalysis::GetMaxLandau(TGraph& signal) {
    return FitAroundMaximum(signal, "landau", 0.1, 0.2, [](double lo, double hi) {
        return std::make_unique<TF1>("landau", "landau", lo, hi);
    });
}

///////////////////////////////////////////////
/// \brief Fits the AGET response to all the points of the graph. Initial values are taken from the graph
/// itself: the maximum point and the half-maximum point of the rise.
///
std::unique_ptr<TF1> TRestPulseShapeAnalysis::FitAget(TGraph& signal, bool withSine,
                                                      std::optional<double> fixedBaseline, bool verbose) {
    const int n = signal.GetN();
    if (n < 4) return nullptr;

    const double peakU = withSine ? kAgetPeakU : 1.0;
    const double baseline0 = fixedBaseline.value_or(0.0);
    const int maxBin = TMath::LocMax(n, signal.GetY());
    const double maxTime = signal.GetPointX(maxBin);
    const double maxAbove = std::max(1.0, signal.GetPointY(maxBin) - baseline0);
    const double xMin = signal.GetPointX(0), xMax = signal.GetPointX(n - 1);

    // The half maximum of the rise is ~0.4 widths before the maximum
    double width = (xMax - xMin) * 0.2;
    for (int i = 0; i < maxBin; i++) {
        if (signal.GetPointY(i) - baseline0 > maxAbove / 2) {
            if (maxTime > signal.GetPointX(i)) width = (maxTime - signal.GetPointX(i)) / 0.4;
            break;
        }
    }
    width = std::max(width, 1e-3);

    auto f = std::make_unique<TF1>(
        "agetFit",
        [withSine, peakU](const double* x, const double* p) {
            const double u = (x[0] - p[2]) / p[3] + peakU;
            if (u <= 0) return p[0];
            return p[0] + p[1] * AgetShape(u, withSine) / AgetShape(peakU, withSine);
        },
        xMin, xMax, 4);
    f->SetParNames("Baseline", "Amplitude", "Time", "Width");
    if (fixedBaseline) {
        f->FixParameter(0, *fixedBaseline);
    } else {
        f->SetParameter(0, 0);
         f->SetParLimits(0, -100, 350.0);
    }
    f->SetParameter(1, maxAbove);
    f->SetParLimits(1, 0, maxAbove * 100);
    f->SetParameter(2, maxTime);
    f->SetParLimits(2, xMin, xMax + (xMax - xMin));
    f->SetParameter(3, width);
    f->SetParLimits(3, width * 1e-3, width * 1e3);

    TFitResultPtr fitResult = signal.Fit(f.get(), verbose ? "RS" : "RNQS");
    if (!fitResult.Get() || !fitResult->IsValid()) return nullptr;
    return f;
}

///////////////////////////////////////////////
/// \brief Fits the AGET response (baseline 0) to the points in a window around the maximum. Returns
/// {time of the maximum, amplitude}, or {-1, -1} if the fit fails.
///
std::pair<double, double> TRestPulseShapeAnalysis::GetMaxAget(TGraph& signal) {
    if (signal.GetN() == 0) return {-1, -1};
    const double maxTime = signal.GetPointX(TMath::LocMax(signal.GetN(), signal.GetY()));

    TGraph window;
    for (int i = 0; i < signal.GetN(); i++) {
        const double x = signal.GetPointX(i);
        if (x >= maxTime * 0.75 && x <= maxTime * 1.35) window.AddPoint(x, signal.GetPointY(i));
    }

    const auto fit = FitAget(window, true, 0.0);
    if (!fit) {
        std::cout << "WARNING: bad AGET fit with maximum at time = " << maxTime << std::endl;
        return {-1, -1};
    }
    return {fit->GetParameter(2), fit->GetParameter(1)};
}

///////////////////////////////////////////////
/// \brief It averages the pulse inside the time window
/// passed as argument. It returns a vector of pairs
/// with the integrated window time and energy (charge)
///
std::vector<std::pair<double, double>> TRestPulseShapeAnalysis::GetIntWindow(TGraph& signal,
                                                                             double intWindow) {
    const int nPoints = signal.GetN();

    std::map<int, std::pair<int, double>> windowMap;
    for (int j = 0; j < nPoints; j++) {
        int index = signal.GetPointX(j) / intWindow;
        auto it = windowMap.find(index);
        if (it != windowMap.end()) {
            it->second.first++;
            it->second.second += signal.GetPointY(j);
        } else {
            windowMap[index] = std::make_pair(1, signal.GetPointY(j));
        }
    }

    std::vector<std::pair<double, double>> result;

    for (const auto& [index, pair] : windowMap) {
        double hitTime = index * intWindow + intWindow / 2.;
        double energy = pair.second / pair.first;
        result.push_back(std::make_pair(hitTime, energy));
    }

    return result;
}

std::pair<double, double> TRestPulseShapeAnalysis::GetMin(TGraph& signal) {
    int minBin = TMath::LocMin(signal.GetN(), signal.GetY());
    double minX = signal.GetX()[minBin];
    double minY = signal.GetY()[minBin];

    return std::pair<double, double>(minX, minY);
}

std::pair<double, double> TRestPulseShapeAnalysis::GetMax(TGraph& signal, std::string option) {
    if (option == "onlyMax") return GetMaxAmplitude(signal);
    if (option == "tripleMax") return GetTripleMaxIntegral(signal);
    if (option == "tripleMaxAverage") return GetTripleMaxAverage(signal);
    if (option == "gaussFit") return GetMaxGauss(signal);
    if (option == "landauFit") return GetMaxLandau(signal);
    if (option == "agetFit") return GetMaxAget(signal);

    // Default
    return GetMaxAmplitude(signal);
}

std::pair<double, double> TRestPulseShapeAnalysis::GetMaxAmplitude(TGraph& signal) {
    int maxBin = TMath::LocMax(signal.GetN(), signal.GetY());

    return std::pair<double, double>(signal.GetX()[maxBin], signal.GetY()[maxBin]);
}

///////////////////////////////////////////////
/// \brief It calculates the triple max over a TGraph
/// and returns an array of pairs with the time and
/// the energy of the maximum and the neighbouring
/// points(three points in total).
///
std::array<std::pair<double, double>, 3> TRestPulseShapeAnalysis::GetTripleMax(TGraph& signal) {
    int maxBin = TMath::LocMax(signal.GetN(), signal.GetY());

    std::array<std::pair<double, double>, 3> tripleMax;

    for (int i = 0; i < 3; i++) {
        int index = maxBin + i - 1;
        if (index >= 0 && index < signal.GetN()) {
            tripleMax[i] = std::make_pair(signal.GetPointX(index), signal.GetPointY(index));
        } else {
            tripleMax[i] = std::make_pair(signal.GetPointX(maxBin), signal.GetPointY(maxBin));
        }
    }

    return tripleMax;
}

///////////////////////////////////////////////
/// \brief It calculates the triple max average over
/// a TGraph and returns a std::pair with the average
/// time and energy (charge)
///
std::pair<double, double> TRestPulseShapeAnalysis::GetTripleMaxAverage(TGraph& signal) {
    auto tripleMax = TRestPulseShapeAnalysis::GetTripleMax(signal);
    double eAvg = 0;
    double hitTimeAvg = 0;
    for (const auto& [hitTime, energy] : tripleMax) {
        hitTimeAvg += hitTime * energy;
        eAvg += energy;
    }

    if (eAvg == 0) return std::pair<double, double>(0, 0);

    hitTimeAvg /= eAvg;
    eAvg /= 3.;

    return std::pair<double, double>(hitTimeAvg, eAvg);
}

///////////////////////////////////////////////
/// \brief It calculates the triple max integral over
/// a TGraph and returns the addition of the maximum plus
/// the neigbouring bins.
///
std::pair<double, double> TRestPulseShapeAnalysis::GetTripleMaxIntegral(TGraph& signal) {
    auto tripleMax = TRestPulseShapeAnalysis::GetTripleMax(signal);
    double totEn = 0;
    double hitTimeAvg = 0;
    for (const auto& [hitTime, energy] : tripleMax) {
        hitTimeAvg += hitTime * energy;
        totEn += energy;
    }

    if (totEn == 0) return std::pair<double, double>(0, 0);

    hitTimeAvg /= totEn;

    return std::pair<double, double>(hitTimeAvg, totEn);
}

///////////////////////////////////////////////
/// \brief It returns the integral of the first positive
/// rise (risetime) over a vector of pairs, that should
/// correspond to the points over threshold for a given signal.
///
template <typename T>
double TRestPulseShapeAnalysis::GetSlopeIntegral(const std::vector<std::pair<T, float>>& signal) {
    double sum = 0;
    double pVal = 0;
    for (const auto& [index, val] : signal) {
        if (val - pVal < 0) {
            break;
        }
        sum += val;
        pVal = val;
    }
    /*    auto max = std::max_element(std::begin(signal), std::end(signal),
                                    [](const auto& p1, const auto& p2) { return p1.second < p2.second; });

        for (auto it = signal.begin(); it != max; ++it) sum += it->second;
    */
    return sum;
}
template double TRestPulseShapeAnalysis::GetSlopeIntegral(const std::vector<std::pair<int, float>>& signal);
template double TRestPulseShapeAnalysis::GetSlopeIntegral(const std::vector<std::pair<float, float>>& signal);

///////////////////////////////////////////////
/// \brief It returns the slope of the first positive
/// rise (risetime) over a vector of pairs, that should
/// correspond to the points over threshold for a given signal.
///
template <typename T>
double TRestPulseShapeAnalysis::GetRiseSlope(const std::vector<std::pair<T, float>>& signal) {
    if (signal.size() < 2) return 0;

    auto max = std::max_element(std::begin(signal), std::end(signal),
                                [](const auto& p1, const auto& p2) { return p1.second < p2.second; });

    auto maxBin = max->first;
    auto startBin = signal.front().first;
    double hP = max->second;
    double lP = signal.front().second;

    if (maxBin == startBin) return 0;
    return (hP - lP) / (maxBin - startBin);
}
template double TRestPulseShapeAnalysis::GetRiseSlope(const std::vector<std::pair<int, float>>& signal);
template double TRestPulseShapeAnalysis::GetRiseSlope(const std::vector<std::pair<float, float>>& signal);

///////////////////////////////////////////////
/// \brief It returns the time of the first positive
/// rise or risetime over a vector of pairs, that should
/// correspond to the points over threshold for a given signal.
///
template <typename T>
double TRestPulseShapeAnalysis::GetRiseTime(const std::vector<std::pair<T, float>>& signal) {
    if (signal.size() < 2) {
        return 0;
    }

    auto max = std::max_element(std::begin(signal), std::end(signal),
                                [](const auto& p1, const auto& p2) { return p1.second < p2.second; });

    auto maxBin = max->first;
    auto startBin = signal.front().first;
    return maxBin - startBin;
}
template double TRestPulseShapeAnalysis::GetRiseTime(const std::vector<std::pair<int, float>>& signal);
template double TRestPulseShapeAnalysis::GetRiseTime(const std::vector<std::pair<float, float>>& signal);


template <typename T>
void TRestPulseShapeAnalysis::GetWhiteNoise(std::vector<T>& noise, double amplitude) {
    const size_t size = noise.size();
    for (size_t i = 0; i < size; ++i) {
        noise[i] = static_cast<T>(gRandom->Gaus(0.0, amplitude));
    }
}

template void TRestPulseShapeAnalysis::GetWhiteNoise(std::vector<short>& noise, double amplitude);
template void TRestPulseShapeAnalysis::GetWhiteNoise(std::vector<float>& noise, double amplitude);
template void TRestPulseShapeAnalysis::GetWhiteNoise(std::vector<double>& noise, double amplitude);

    template <typename T>
    void TRestPulseShapeAnalysis::GetSinNoise(std::vector<T>& noise, double maxAmplitude, int period, double phase) {
        const size_t size = noise.size();
        if (size == 0 || period <= 0 || maxAmplitude <= 0.0) {
            if (size > 0) std::fill(noise.begin(), noise.end(), static_cast<T>(0));
            return;
        }

        double uBase = gRandom->Uniform(0.0, 1.0);
        double sinBase = std::sin(uBase * TMath::Pi() / 2.0);
        double baseAmplitude = maxAmplitude * (sinBase * sinBase);

        double slowFreq = period * gRandom->Uniform(4.0, 8.0);
        double slowPhase = gRandom->Uniform(0.0, 2.0 * TMath::Pi());

        for (size_t i = 0; i < size; ++i) {
            double angle = (2.0 * TMath::Pi() * i / period) + phase;
            
            double slowAngle = (2.0 * TMath::Pi() * i / slowFreq) + slowPhase;
            double dynamicAmp = baseAmplitude * std::abs(std::sin(slowAngle));

            if (dynamicAmp > maxAmplitude) dynamicAmp = maxAmplitude;

            noise[i] = static_cast<T>(dynamicAmp * std::sin(angle));
        }
    }

template void TRestPulseShapeAnalysis::GetSinNoise(std::vector<short>& noise, double maxAmplitude, int period, double phase);
template void TRestPulseShapeAnalysis::GetSinNoise(std::vector<float>& noise, double maxAmplitude, int period, double phase);
template void TRestPulseShapeAnalysis::GetSinNoise(std::vector<double>& noise, double maxAmplitude, int period, double phase);

double TRestPulseShapeAnalysis::ShapingFunction(const std::string& type, double u) {
    if (type == "gaus") return std::exp(-0.5 * u * u);
    if (type != "exponential" && type != "shaper" && type != "shaperSin") {
        throw std::runtime_error("TRestPulseShapeAnalysis: unknown shaping type '" + type +
                                 "' (gaus, exponential, shaper or shaperSin)");
    }
    if (u < 0) return 0;
    if (type == "exponential") return std::exp(-u);
    return AgetShape(u, type == "shaperSin");
}

std::pair<double, double> TRestPulseShapeAnalysis::ShapingSupport(const std::string& type) {
    if (type == "gaus") return {-3.5, 3.5};
    return {0, 5};
}

std::vector<double> TRestPulseShapeAnalysis::BuildResponse(const std::string& type, double shapingTime,
                                                           double gain, int& center) {
    center = 0;
    if (!(shapingTime > 0)) return {};
    if (type != "gaus" && type != "exponential" && type != "shaper" && type != "shaperSin") return {};

    std::vector<double> r;
    if (type == "gaus") {
        const int cBin = (int)(shapingTime * 3.5);
        r.resize(2 * cBin);
        center = cBin;
        for (int i = 0; i < 2 * cBin; ++i) r[i] = ShapingFunction(type, (i - cBin) / shapingTime);
    } else {
        r.resize((int)(5 * shapingTime));
        for (size_t i = 0; i < r.size(); ++i) r[i] = ShapingFunction(type, (double)i / shapingTime);
    }

    const double sum = std::accumulate(r.begin(), r.end(), 0.0);
    if (!(sum > 0)) return {};
    for (double& v : r) v *= gain / sum;
    return r;
}

std::vector<double> TRestPulseShapeAnalysis::Convolve(const std::vector<double>& in,
                                                      const std::vector<double>& response, int center,
                                                      bool ignoreNegative) {
    const int nBins = (int)in.size();
    const int nr = (int)response.size();
    std::vector<double> out(nBins, 0.0);

    for (int m = 0; m < nBins; ++m) {
        const double v = in[m];
        if (v == 0 || (ignoreNegative && v < 0)) continue;

        const int kMin = std::max(0, center - m);
        const int kMax = std::min(nr, nBins - m + center);
        for (int k = kMin; k < kMax; ++k) out[m + k - center] += response[k] * v;
    }
    return out;
}
