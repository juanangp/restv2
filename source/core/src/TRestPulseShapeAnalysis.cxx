#include <TF1.h>
#include <TFitResult.h>
#include <TRestPulseShapeAnalysis.h>

template <typename T>
std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigma(const std::vector<T>& signal, int startBin, int endBin,
                                 double& baseLine, double& baseLineSigma, std::string option){

    if (option == "robust") {
        return TRestPulseShapeAnalysis::CalculateBaselineAndSigmaIQR(signal, startBin, endBin, baseLine, baseLineSigma);
    } else if (option == "outliers") {
        return TRestPulseShapeAnalysis::CalculateBaselineAndSigmaExcludeOutliers(signal, startBin, endBin, baseLine, baseLineSigma);
    } else {
        return TRestPulseShapeAnalysis::CalculateBaselineAndSigmaSD(signal, startBin, endBin, baseLine, baseLineSigma);
    }

}

template std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigma<short>(
    const std::vector<short>& signal, int startBin, int endBin,
                                 double& baseLine, double& baseLineSigma, std::string option);
template std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigma<float>(
    const std::vector<float>& signal, int startBin, int endBin,
                                 double& baseLine, double& baseLineSigma, std::string option);

///////////////////////////////////////////////
/// \brief This method is used to determine the value
/// of the baseline as average (arithmetic mean) of the
/// data in the range defined between startBin and endBin.
/// The baseline sigma is determined as the standard deviation
/// of the baseline in range provided.
template <typename T>
std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigmaSD(const std::vector<T>& signal, int startBin,
                                                          int endBin, double& baseLine,
                                                          double& baseLineSigma) {
    baseLine = 0;
    baseLineSigma = 0;

    int nPoints = 0;

    for (int i = startBin; i < endBin; i++) {
        if (i < 0 || i >= (int)signal.size()) continue;
        baseLine += signal[i];
        baseLineSigma += signal[i] * signal[i];
        nPoints++;
    }

    if (nPoints > 0) {
        baseLine /= (double)nPoints;
        baseLineSigma = TMath::Sqrt(baseLineSigma / (double)nPoints - baseLine * baseLine);
    }

    std::vector<float> data (signal.size());
    for (size_t i=0; i<signal.size();i++)data[i] = signal[i] - baseLine;

    return data;

}
template std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigmaSD<short>(
    const std::vector<short>& signal, int startBin, int endBin, double& baseLine,
    double& baseLineSigma);
template std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigmaSD<float>(
    const std::vector<float>& signal, int startBin, int endBin, double& baseLine,
    double& baseLineSigma);
///////////////////////////////////////////////
/// \brief This method is used to determine the value
/// of the baseline as the median of the data in
/// the range defined between startBin and endBin.
/// The baseline sigma is determined as the interquartile
/// range (IQR) in the baseline range provided. The IQR
/// is more robust towards outliers than the standard deviation.
template <typename T>
std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigmaIQR(const std::vector<T>& signal, int startBin,
                                                           int endBin, double& baseLine,
                                                           double& baseLineSigma) {
    baseLine = 0;
    baseLineSigma = 0;

    if (signal.empty()) return {};
    if (startBin < 0) startBin = 0;
    if (startBin >= (int)signal.size()) return {};
    if (endBin >= (int)signal.size()) endBin = signal.size() - 1;

    if (endBin < startBin) return{};

    auto first = signal.begin() + startBin;
    auto last = signal.begin() + endBin + 1;
    std::vector<T> v(first, last);
    if (v.empty()) return {};

    baseLine = TMath::Median((int)v.size(), v.data());

    std::sort(v.begin(), v.end());
    const size_t q1 = static_cast<size_t>(0.25 * (v.size() - 1));
    const size_t q3 = static_cast<size_t>(0.75 * (v.size() - 1));
    baseLineSigma =
        (v[q3] - v[q1]) /
        1.349;  // IQR/1.349 equals the standard deviation in case of normally distributed data

    std::vector<float> data (signal.size());
    for (size_t i=0; i<signal.size();i++)data[i] = signal[i] - baseLine;

    return data;
}

template std::vector<float>TRestPulseShapeAnalysis::CalculateBaselineAndSigmaIQR<short>(
    const std::vector<short>& signal, int startBin, int endBin, double& baseLine,
    double& baseLineSigma);
template std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigmaIQR<float>(
    const std::vector<float>& signal, int startBin, int endBin, double& baseLine,
    double& baseLineSigma);

template <typename T>
std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigmaExcludeOutliers(const std::vector<T>& signal, int startBin, int endBin,
                                  double& baseLine, double& baseLineSigma){

    baseLine = 0;
    baseLineSigma = 0;

    if (signal.empty()) return {};
    if (startBin < 0) startBin = 0;
    if (startBin >= (int)signal.size()) return {};
    if (endBin >= (int)signal.size()) endBin = signal.size() - 1;

    if (endBin < startBin) return{};

    auto first = signal.begin() + startBin;
    auto last = signal.begin() + endBin + 1;
    std::vector<T> v(first, last);
    if (v.empty()) return {};

    std::sort(v.begin(), v.end());
    const size_t q1 = static_cast<size_t>(0.25 * (v.size() - 1));
    const size_t q3 = static_cast<size_t>(0.75 * (v.size() - 1));

    std::vector<Short_t> filteredData;
      for (const auto& value : signal) {
        if (value >= v[q1] && value <= v[q3]) {
          filteredData.emplace_back(value);
        }
      }

     if (filteredData.empty()) {
       baseLine = TMath::Median(signal.size(), &signal[0]);
       baseLineSigma = 0;
     } else {
        baseLine = TMath::Median(filteredData.size(), &filteredData[0]);
       double mean = std::accumulate(filteredData.begin(), filteredData.end(), 0.0) / filteredData.size();
       double variance = 0.0;
         for (const auto& value : filteredData) {
           variance += std::pow(value - mean, 2);
         }
       baseLineSigma = std::sqrt(variance / filteredData.size());
     }

    std::vector<float> data (signal.size());
    for (size_t i=0; i<signal.size();i++)data[i] = signal[i] - baseLine;

    return data;

}

template std::vector<float>TRestPulseShapeAnalysis::CalculateBaselineAndSigmaExcludeOutliers<short>(
    const std::vector<short>& signal, int startBin, int endBin, double& baseLine,
    double& baseLineSigma);
template std::vector<float> TRestPulseShapeAnalysis::CalculateBaselineAndSigmaExcludeOutliers<float>(
    const std::vector<float>& signal, int startBin, int endBin, double& baseLine,
    double& baseLineSigma);

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

template double TRestPulseShapeAnalysis::GetAverage<short>(const std::vector<short>& signal,
                                                               int startBin, int endBin);
template double TRestPulseShapeAnalysis::GetAverage<float>(const std::vector<float>& signal,
                                                               int startBin, int endBin);

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

    if (baseLine == 0) CalculateBaselineAndSigmaIQR(signal, 5, int(pulseDepth - 5), baseLine, baseLineSigma);

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
std::vector<std::pair<int, float> > TRestPulseShapeAnalysis::GetPointsOverThreshold(
    const std::vector<T>& signal, std::pair<int,int>& range, const std::pair<double,double>& thrPar, int nPointsOver,
    int nPointsFlat, double baseLineSigma) {
    if (signal.empty()) return {};

    if (range.first < 0) range.first=0;
    if (range.second <= 0 || range.second > (int)signal.size()) range.second = signal.size();

    const int start = std::max(0, (int)range.first);
    const int stop = std::min((int)signal.size(), (int)range.second);
    if (start >= stop) return {};

    std::vector<std::pair<int, float> > pointsOverThreshold;

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
template std::vector<std::pair<int, float> > TRestPulseShapeAnalysis::GetPointsOverThreshold<Short_t>(
    const std::vector<short>& signal, std::pair<int,int>& range, const std::pair<double,double>& thrPar, int nPointsOver,
    int nPointsFlat, double baseLineSigma);
template std::vector<std::pair<int, float> > TRestPulseShapeAnalysis::GetPointsOverThreshold<float>(
    const std::vector<float>& signal, std::pair<int,int>& range, const std::pair<double,double>& thrPar, int nPointsOver,
    int nPointsFlat, double baseLineSigma);

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
TGraph TRestPulseShapeAnalysis::GetGraph(const std::vector<T>& signal, const std::string& title){
    size_t nPoints = signal.size();
    if (nPoints <= 0) return TGraph(); //return empty TGraph

    TGraph graph(nPoints);

    for (size_t i = 0; i < nPoints; ++i) {
      graph.SetPoint(i, i, signal[i]);
    }
    graph.SetTitle(title.c_str());
    return graph;
}

template TGraph TRestPulseShapeAnalysis::GetGraph<short>(const std::vector<short>& signal, const std::string& title);
template TGraph TRestPulseShapeAnalysis::GetGraph<float>(const std::vector<float>& signal, const std::string& title);

template <typename T>
TGraph TRestPulseShapeAnalysis::GetGraphPair(const std::vector<std::pair<T, float>> &points, const std::string& title) {
      size_t nPoints = points.size();
      if (nPoints <= 0) return TGraph(); //return empty TGraph

      TGraph graph(nPoints);

      for (size_t i = 0; i < nPoints; ++i) {
        graph.SetPoint(i, points[i].first, points[i].second);
      }
      graph.SetTitle(title.c_str());
      return graph;
}

template TGraph TRestPulseShapeAnalysis::GetGraphPair<int>(const std::vector<std::pair<int, float>> &points, const std::string& title);
template TGraph TRestPulseShapeAnalysis::GetGraphPair<float>(const std::vector<std::pair<float, float>> &points, const std::string& title);

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
/// \brief It performs a gaussian fit to the signal
/// which is stored inside a TGraph. It returns
/// a TVector 2 with the maximum and the mean of the
/// gaussian fit
///
std::pair<double,double> TRestPulseShapeAnalysis::GetMaxGauss(TGraph& signal) {
    int maxBin = TMath::LocMax(signal.GetN(), signal.GetY());
    double maxTime = signal.GetPointX(maxBin);
    double gaussMax = -1, gaussMean = -1;
    double lowerLimit = maxTime - maxTime * 0.1;  // us
    double upperLimit = maxTime + maxTime * 0.2;  // us

    TF1* gaus = new TF1("gaus", "gaus", lowerLimit, upperLimit);
    TFitResultPtr fitResult = signal.Fit(gaus, "QNRS");

    if (fitResult->IsValid()) {
        gaussMax = gaus->GetParameter(0);
        gaussMean = gaus->GetParameter(1);
    } else {
        // the fit failed, return -1 to indicate failure
        std::cout << std::endl
                  << "WARNING: bad fit with maximum at time = " << maxTime
                  << "Failed fit parameters = " << gaus->GetParameter(0) << " || " << gaus->GetParameter(1)
                  << " || " << gaus->GetParameter(2) << "\n"
                  << "Assigned fit parameters : energy = " << gaussMax << ", time = " << gaussMean
                  << std::endl;
        /*
        TCanvas* c2 = new TCanvas("c2", "Signal fit", 200, 10, 1280, 720);
        signal.Draw();
        c2->Update();
        getchar();
        delete c2;
        */
    }

    delete gaus;

    return std::make_pair(gaussMean, gaussMax);
}

///////////////////////////////////////////////
/// \brief It performs a landau fit to the signal
/// which is stored inside a TGraph. It returns
/// a TVector 2 with the maximum and the mean of the
/// landau fit
///
std::pair<double,double> TRestPulseShapeAnalysis::GetMaxLandau(TGraph& signal) {
    int maxBin = TMath::LocMax(signal.GetN(), signal.GetY());
    double maxTime = signal.GetPointX(maxBin);
    double landauMax = -1, landauMean = -1;
    double lowerLimit = maxTime - maxTime * 0.1;  // us
    double upperLimit = maxTime + maxTime * 0.2;  // us

    TF1* landau = new TF1("landau", "landau", lowerLimit, upperLimit);
    TFitResultPtr fitResult = signal.Fit(landau, "QNRS");

    if (fitResult->IsValid()) {
        landauMax = landau->GetParameter(0);
        landauMean = landau->GetParameter(1);
    } else {
        // the fit failed, return -1 to indicate failure
        std::cout << std::endl
                  << "WARNING: bad fit with maximum at time = " << maxTime
                  << "Failed fit parameters = " << landau->GetParameter(0) << " || "
                  << landau->GetParameter(1) << " || " << landau->GetParameter(2) << "\n"
                  << "Assigned fit parameters : energy = " << landauMax << ", time = " << landauMean
                  << std::endl;
        /*
        TCanvas* c2 = new TCanvas("c2", "Signal fit", 200, 10, 1280, 720);
        signal.Draw();
        c2->Update();
        getchar();
        delete c2;
        */
    }

    delete landau;

    return std::make_pair(landauMean, landauMax);
}

///////////////////////////////////////////////
/// \brief It performs a fit of the signal to a
/// the aget response function. It returns a
/// TVector 2 with the maximum and the mean of the
/// gaussian fit
///
std::pair<double,double> TRestPulseShapeAnalysis::GetMaxAget(TGraph& signal) {
    int maxBin = TMath::LocMax(signal.GetN(), signal.GetY());
    double maxTime = signal.GetPointX(maxBin);
    double agetMax = -1, agetMean = -1;
    double lowerLimit = maxTime - maxTime * 0.25;  // us
    double upperLimit = maxTime + maxTime * 0.35;  // us

    // 1.1664 is the x value where the maximum of the base function (i.e. without parameters)
    TF1* aget =
        new TF1("aget",
                "[&](double *x, double *p){ double arg = (x[0] - par[1] + 1.1664) / par[2]; return par[0] / "
                "0.0440895 * exp(-3 * (arg)) * (arg) * (arg) *               (arg)*sin(arg);}",
                lowerLimit, upperLimit, 3);
    TFitResultPtr fitResult = signal.Fit(aget, "QNRS");

    if (fitResult->IsValid()) {
        agetMax = aget->GetParameter(0);
        agetMean = aget->GetParameter(1);
    } else {
        // the fit failed, return -1 to indicate failure
        std::cout << std::endl
                  << "WARNING: bad fit with maximum at time = " << maxTime
                  << "Failed fit parameters = " << aget->GetParameter(0) << " || " << aget->GetParameter(1)
                  << " || " << aget->GetParameter(2) << "\n"
                  << "Assigned fit parameters : energy = " << agetMax << ", time = " << agetMean << std::endl;
        /*
        TCanvas* c2 = new TCanvas("c2", "Signal fit", 200, 10, 1280, 720);
        signal.Draw();
        c2->Update();
        getchar();
        delete c2;
        */
    }

    delete aget;

    return std::make_pair(agetMean, agetMax);
}

///////////////////////////////////////////////
/// \brief It averages the pulse inside the time window
/// passed as argument. It returns a vector of pairs
/// with the integrated window time and energy (charge)
///
std::vector<std::pair<double, double> > TRestPulseShapeAnalysis::GetIntWindow(TGraph& signal,
                                                                              double intWindow) {
    const int nPoints = signal.GetN();

    std::map<int, std::pair<int, double> > windowMap;
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

    std::vector<std::pair<double, double> > result;

    for (const auto& [index, pair] : windowMap) {
        double hitTime = index * intWindow + intWindow / 2.;
        double energy = pair.second / pair.first;
        result.push_back(std::make_pair(hitTime, energy));
    }

    return result;
}

std::pair<double, double> TRestPulseShapeAnalysis::GetMin(TGraph& signal){

   int minBin = TMath::LocMin(signal.GetN(), signal.GetY());
   double minX = signal.GetX()[minBin];
   double minY = signal.GetY()[minBin];

   return std::pair<double, double>(minX, minY);
}

std::pair<double, double> TRestPulseShapeAnalysis::GetMax(TGraph& signal, std::string option){

  if (option == "onlyMax")          return GetMaxAmplitude(signal);
  if (option == "tripleMax")        return GetTripleMaxIntegral(signal);
  if (option == "tripleMaxAverage") return GetTripleMaxAverage(signal);
  if (option == "gaussFit")         return GetMaxGauss(signal);
  if (option == "landauFit")        return GetMaxLandau(signal);
  if (option == "agetFit")          return GetMaxAget(signal);

  //Default
  return GetMaxAmplitude(signal);

}

std::pair<double, double> TRestPulseShapeAnalysis::GetMaxAmplitude(TGraph& signal){

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

    if (eAvg == 0) return std::pair<double, double>(0,0);

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
double TRestPulseShapeAnalysis::GetSlopeIntegral(const std::vector<std::pair<T, float> >& signal) {
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
template double TRestPulseShapeAnalysis::GetSlopeIntegral(
    const std::vector<std::pair<int, float> >& signal);
template double TRestPulseShapeAnalysis::GetSlopeIntegral(
    const std::vector<std::pair<float, float> >& signal);

///////////////////////////////////////////////
/// \brief It returns the slope of the first positive
/// rise (risetime) over a vector of pairs, that should
/// correspond to the points over threshold for a given signal.
///
template <typename T>
double TRestPulseShapeAnalysis::GetRiseSlope(const std::vector<std::pair<T, float> >& signal) {
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
template double TRestPulseShapeAnalysis::GetRiseSlope(
    const std::vector<std::pair<int, float> >& signal);
template double TRestPulseShapeAnalysis::GetRiseSlope(
    const std::vector<std::pair<float, float> >& signal);

///////////////////////////////////////////////
/// \brief It returns the time of the first positive
/// rise or risetime over a vector of pairs, that should
/// correspond to the points over threshold for a given signal.
///
template <typename T>
double TRestPulseShapeAnalysis::GetRiseTime(const std::vector<std::pair<T, float> >& signal) {
    if (signal.size() < 2) {
        return 0;
    }

    auto max = std::max_element(std::begin(signal), std::end(signal),
                                [](const auto& p1, const auto& p2) { return p1.second < p2.second; });

    auto maxBin = max->first;
    auto startBin = signal.front().first;
    return maxBin - startBin;
}
template double TRestPulseShapeAnalysis::GetRiseTime(const std::vector<std::pair<int, float> >& signal);
template double TRestPulseShapeAnalysis::GetRiseTime(
    const std::vector<std::pair<float, float> >& signal);
