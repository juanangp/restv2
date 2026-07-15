#pragma once

#include <TGraph.h>
#include <TMath.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <numeric>
#include <vector>
#include <string>

/// This namespace define generic functions to calculate different signal parameters
namespace TRestPulseShapeAnalysis {

template <typename T>
std::vector<float> CalculateBaselineAndSigma(const std::vector<T>& signal, int startBin, int endBin,
                                 double& baseLine, double& baseLineSigma, std::string option="");

template <typename T>
std::vector<float> CalculateBaselineAndSigmaSD(const std::vector<T>& signal, int startBin, int endBin,
                                 double& baseLine, double& baseLineSigma);

template <typename T>
std::vector<float> CalculateBaselineAndSigmaIQR(const std::vector<T>& signal, int startBin, int endBin,
                                  double& baseLine, double& baseLineSigma);

template <typename T>
std::vector<float> CalculateBaselineAndSigmaExcludeOutliers(const std::vector<T>& signal, int startBin, int endBin,
                                  double& baseLine, double& baseLineSigma);

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
std::vector<std::pair<int, float> > GetPointsOverThreshold(const std::vector<T>& signal, std::pair<int,int>& range,
                                                               const std::pair<double,double>& thrPar, int nPointsOver,
                                                               int nPointsFlat, double baseLineSigma);

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
double GetSlopeIntegral(const std::vector<std::pair<T, float> >& signal);
template <typename T>
double GetRiseSlope(const std::vector<std::pair<T, float> >& signal);
template <typename T>
double GetRiseTime(const std::vector<std::pair<T, float> >& signal);

template <typename T>
double GetRiseTime(const std::vector<std::pair<T, float> >& signal);

template <typename T>
TGraph GetGraphPair(const std::vector<std::pair<T, float>> &points, const std::string& title="");

template <typename T>
TGraph GetGraph(const std::vector<T>& signal, const std::string& title="");

std::vector<std::pair<double, double> > GetIntWindow(TGraph& signal, double intWindow);
std::array<std::pair<double, double>, 3> GetTripleMax(TGraph& signal);
std::pair<double, double> GetMin(TGraph& signal);
std::pair<double, double> GetMax(TGraph& signal, std::string option);
std::pair<double, double> GetMaxAmplitude(TGraph& signal);
std::pair<double,double> GetTripleMaxAverage(TGraph& signal);
std::pair<double, double> GetTripleMaxIntegral(TGraph& signal);
std::pair<double,double> GetMaxGauss(TGraph& signal);
std::pair<double,double> GetMaxLandau(TGraph& signal);
std::pair<double,double> GetMaxAget(TGraph& signal);


}
