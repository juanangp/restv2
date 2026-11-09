/// Unit tests of TRestPulseShapeAnalysis (GoogleTest, BUILD_TESTING): baseline and sigma, integrals and
/// averages, smoothing, points over threshold and rise parameters, peaks, maxima of graphs (also with
/// fits), integration windows, shaping responses and noise, on small synthetic signals.

#include <TF1.h>
#include <TGraph.h>
#include <TRandom.h>
#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "TRestPulseShapeAnalysis.h"

namespace PSA = TRestPulseShapeAnalysis;

namespace {

/// Graph of (time, value) points
TGraph MakeGraph(const std::vector<std::pair<double, double>>& points) {
    TGraph graph;
    for (const auto& [x, y] : points) graph.AddPoint(x, y);
    return graph;
}

/// Baseline subtracted signal of 50 bins with a pulse {5, 10, 20, 10, 5} starting at bin 10
std::vector<float> PulseSignal() {
    std::vector<float> signal(50, 0.f);
    const std::vector<float> pulse = {5, 10, 20, 10, 5};
    for (size_t i = 0; i < pulse.size(); i++) signal[10 + i] = pulse[i];
    return signal;
}

}  // namespace

TEST(PulseShapeAnalysis, BaselineAndSigma) {
    const std::vector<float> signal = {1, 2, 3, 4, 5, 100};
    double baseline = 0, sigma = 0;

    // Mean and standard deviation of [0, 5)
    EXPECT_TRUE(PSA::GetBaselineSigma(signal, 0, 5, baseline, sigma));
    EXPECT_DOUBLE_EQ(baseline, 3);
    EXPECT_NEAR(sigma, std::sqrt(2.), 1e-12);

    // Median and IQR / 1.349: quartiles 2 and 4
    EXPECT_TRUE(PSA::GetBaselineSigma(signal, 0, 5, baseline, sigma, "robust"));
    EXPECT_DOUBLE_EQ(baseline, 3);
    EXPECT_NEAR(sigma, 2 / 1.349, 1e-12);

    // Values between the quartiles (2, 3, 4): median 3, standard deviation sqrt(2/3)
    EXPECT_TRUE(PSA::GetBaselineSigma(signal, 0, 5, baseline, sigma, "outliers"));
    EXPECT_DOUBLE_EQ(baseline, 3);
    EXPECT_NEAR(sigma, std::sqrt(2. / 3), 1e-12);

    // Empty range: 0 and 0 with all the options
    for (const std::string option : {"", "robust", "outliers"}) {
        baseline = sigma = 1;
        EXPECT_TRUE(PSA::GetBaselineSigma(signal, 4, 4, baseline, sigma, option));
        EXPECT_EQ(baseline, 0) << option;
        EXPECT_EQ(sigma, 0) << option;
    }
}

TEST(PulseShapeAnalysis, CalculateBaselineAndSigmaSubtractsTheBaseline) {
    const std::vector<short> signal = {10, 12, 8, 10, 50, 10};
    double baseline = 0, sigma = 0;
    const auto subtracted = PSA::CalculateBaselineAndSigmaSD(signal, 0, 4, baseline, sigma);
    EXPECT_DOUBLE_EQ(baseline, 10);
    EXPECT_NEAR(sigma, std::sqrt(2.), 1e-12);
    ASSERT_EQ(subtracted.size(), signal.size());
    for (size_t i = 0; i < signal.size(); i++) EXPECT_FLOAT_EQ(subtracted[i], signal[i] - 10.f);

    // The robust baseline ignores the outlier
    const auto robust = PSA::CalculateBaselineAndSigmaIQR(signal, 0, 6, baseline, sigma);
    EXPECT_DOUBLE_EQ(baseline, 10);
    EXPECT_EQ(robust.size(), signal.size());
}

TEST(PulseShapeAnalysis, IntegralAverageAndExtremes) {
    const std::vector<float> signal = {1, 5, 3, -2, 4};
    EXPECT_DOUBLE_EQ(PSA::GetIntegral(signal, 0, 5), 11);
    EXPECT_DOUBLE_EQ(PSA::GetIntegral(signal, 1, 3), 8);
    // Bins outside the signal ignored
    EXPECT_DOUBLE_EQ(PSA::GetIntegral(signal, -3, 10), 11);
    EXPECT_DOUBLE_EQ(PSA::GetAverage(signal, 3, 10), 1);
    EXPECT_DOUBLE_EQ(PSA::GetAverage(std::vector<float>{}, 0, 5), 0);

    EXPECT_EQ(PSA::GetMaxBin(signal), 1);
    EXPECT_EQ(PSA::GetMaxBin(signal, 2), 4);
    EXPECT_EQ(PSA::GetMinBin(signal), 3);
    EXPECT_EQ(PSA::GetMinBin(signal, 0, 3), 0);

    const auto derivative = PSA::GetDerivative(signal);
    EXPECT_EQ(derivative, (std::vector<float>{4, -2, -5, 6}));
    EXPECT_TRUE(PSA::GetDerivative(std::vector<float>{1}).empty());
}

TEST(PulseShapeAnalysis, MovingAverageAndSmoothing) {
    const std::vector<double> values = {1, 2, 3, 4, 5};
    // Shrunk at the edges
    EXPECT_EQ(PSA::GetMovingAverage(values, 1), (std::vector<double>{1.5, 2, 3, 4, 4.5}));
    EXPECT_EQ(PSA::GetMovingAverage(values, 0), values);

    // The edges take the closest full window
    EXPECT_EQ(PSA::GetSignalSmoothed(values, 3), (std::vector<float>{2, 2, 3, 4, 4}));
    // An even number of points is made odd (4 -> 5)
    EXPECT_EQ(PSA::GetSignalSmoothed(values, 4), (std::vector<float>{3, 3, 3, 3, 3}));
    EXPECT_TRUE(PSA::GetSignalSmoothed(std::vector<double>{}, 3).empty());
}

TEST(PulseShapeAnalysis, SmoothingExcludingOutliers) {
    // A flat baseline of 10 with a spike: the spike is replaced by the baseline before smoothing
    std::vector<float> signal(40, 10.f);
    for (size_t i = 0; i < signal.size(); i++) signal[i] += (i % 2 ? 0.5f : -0.5f);
    signal[20] = 1000;
    double baseline = 0, sigma = 0;
    const auto smoothed = PSA::GetSignalSmoothed_ExcludeOutliers(signal, 5, baseline, sigma);
    // Median of the bins [5, 35): 10.5
    EXPECT_NEAR(baseline, 10.5, 1e-6);
    ASSERT_EQ(smoothed.size(), signal.size());
    for (float value : smoothed) EXPECT_NEAR(value, 10, 0.5);
}

TEST(PulseShapeAnalysis, PointsOverThreshold) {
    const auto signal = PulseSignal();
    std::pair<int, int> range = {-1, 0};
    const auto points = PSA::GetPointsOverThreshold(signal, range, {2, 1}, 3, 10, 1);
    // The range is updated to the whole signal
    EXPECT_EQ(range, std::make_pair(0, 50));
    ASSERT_EQ(points.size(), 5u);
    for (size_t i = 0; i < points.size(); i++) {
        EXPECT_EQ(points[i].first, 10 + static_cast<int>(i));
        EXPECT_FLOAT_EQ(points[i].second, signal[10 + i]);
    }

    // Rise of the pulse: from bin 10 (5) to the maximum at bin 12 (20)
    EXPECT_DOUBLE_EQ(PSA::GetRiseTime(points), 2);
    EXPECT_DOUBLE_EQ(PSA::GetRiseSlope(points), 7.5);
    EXPECT_DOUBLE_EQ(PSA::GetSlopeIntegral(points), 35);

    // Not enough points over threshold, or a range without the pulse
    std::pair<int, int> all = {0, 0};
    EXPECT_TRUE(PSA::GetPointsOverThreshold(signal, all, {2, 1}, 6, 10, 1).empty());
    std::pair<int, int> before = {0, 10};
    EXPECT_TRUE(PSA::GetPointsOverThreshold(signal, before, {2, 1}, 3, 10, 1).empty());
    // Over threshold but flat (standard deviation below signalThreshold)
    std::vector<float> flat(30, 0.f);
    for (int i = 5; i < 15; i++) flat[i] = 10;
    std::pair<int, int> flatRange = {0, 0};
    EXPECT_TRUE(PSA::GetPointsOverThreshold(flat, flatRange, {2, 1}, 3, 100, 1).empty());
}

TEST(PulseShapeAnalysis, RiseParametersOfShortPulses) {
    const std::vector<std::pair<int, float>> single = {{3, 5.f}};
    EXPECT_DOUBLE_EQ(PSA::GetRiseTime(single), 0);
    EXPECT_DOUBLE_EQ(PSA::GetRiseSlope(single), 0);
    // The first point is the maximum
    const std::vector<std::pair<int, float>> falling = {{3, 5.f}, {4, 2.f}};
    EXPECT_DOUBLE_EQ(PSA::GetRiseSlope(falling), 0);
    EXPECT_DOUBLE_EQ(PSA::GetSlopeIntegral(falling), 5);
}

TEST(PulseShapeAnalysis, MaxPeakWidth) {
    const std::vector<float> signal = {0, 0, 5, 10, 20, 10, 5, 0, 0};
    // The first bins not above half of the maximum: 3 and 5
    EXPECT_DOUBLE_EQ(PSA::GetMaxPeakWidth(signal), 2);
    const std::vector<float> wide = {0, 15, 18, 20, 18, 15, 0};
    EXPECT_DOUBLE_EQ(PSA::GetMaxPeakWidth(wide), 6);
    EXPECT_DOUBLE_EQ(PSA::GetMaxPeakWidth(std::vector<float>{}), 0);
}

TEST(PulseShapeAnalysis, GetPeaks) {
    // Two gaussian pulses (amplitude 100, sigma 3) at t = 30 and t = 70
    std::vector<std::pair<double, double>> points;
    for (int t = 0; t < 100; t++) {
        const double value =
            100 * std::exp(-(t - 30) * (t - 30) / 18.) + 100 * std::exp(-(t - 70) * (t - 70) / 18.);
        points.emplace_back(t, value);
    }
    const auto peaks = PSA::GetPeaks(points, 5, 10);
    ASSERT_EQ(peaks.size(), 2u);
    EXPECT_NEAR(peaks[0].first, 30, 1);
    EXPECT_NEAR(peaks[1].first, 70, 1);
    // Amplitude: mean of the points within one bin of the peak
    const double amplitude = (100 + 2 * 100 * std::exp(-1. / 18)) / 3;
    EXPECT_NEAR(peaks[0].second, amplitude, 0.5);
    EXPECT_NEAR(peaks[1].second, amplitude, 0.5);

    // Over the threshold of none of them
    EXPECT_TRUE(PSA::GetPeaks(points, 200, 10).empty());
    EXPECT_TRUE(PSA::GetPeaks({}, 5, 10).empty());
}

TEST(PulseShapeAnalysis, MaximaOfGraphs) {
    TGraph graph = MakeGraph({{0, 0}, {1, 1}, {2, 3}, {3, 1}, {4, -2}});

    EXPECT_EQ(PSA::GetMaxAmplitude(graph), std::make_pair(2., 3.));
    EXPECT_EQ(PSA::GetMin(graph), std::make_pair(4., -2.));
    const auto triple = PSA::GetTripleMax(graph);
    EXPECT_EQ(triple[0], std::make_pair(1., 1.));
    EXPECT_EQ(triple[1], std::make_pair(2., 3.));
    EXPECT_EQ(triple[2], std::make_pair(3., 1.));

    // Value weighted time (1 + 6 + 3) / 5 = 2, sum 5 and mean 5 / 3
    const auto integral = PSA::GetTripleMaxIntegral(graph);
    EXPECT_DOUBLE_EQ(integral.first, 2);
    EXPECT_DOUBLE_EQ(integral.second, 5);
    const auto average = PSA::GetTripleMaxAverage(graph);
    EXPECT_DOUBLE_EQ(average.first, 2);
    EXPECT_DOUBLE_EQ(average.second, 5. / 3);

    // GetMax dispatches by option (unknown: the maximum point)
    EXPECT_EQ(PSA::GetMax(graph, "onlyMax"), PSA::GetMaxAmplitude(graph));
    EXPECT_EQ(PSA::GetMax(graph, "tripleMax"), integral);
    EXPECT_EQ(PSA::GetMax(graph, "tripleMaxAverage"), average);
    EXPECT_EQ(PSA::GetMax(graph, "unknown"), PSA::GetMaxAmplitude(graph));

    // The maximum at the edge is repeated
    TGraph edge = MakeGraph({{0, 5}, {1, 2}});
    EXPECT_EQ(PSA::GetTripleMax(edge)[0], std::make_pair(0., 5.));

    // Empty graph
    TGraph empty;
    EXPECT_EQ(PSA::GetMaxAmplitude(empty), std::make_pair(-1., -1.));
    EXPECT_EQ(PSA::GetMin(empty), std::make_pair(-1., -1.));
    EXPECT_EQ(PSA::GetTripleMaxIntegral(empty), std::make_pair(-1., -1.));
    EXPECT_EQ(PSA::GetMax(empty, "gaussFit"), std::make_pair(-1., -1.));
}

TEST(PulseShapeAnalysis, GaussianFitOfTheMaximum) {
    // Gaussian pulse, amplitude 100 at t = 100, sigma 10
    std::vector<std::pair<double, double>> points;
    for (int t = 0; t < 200; t++) points.emplace_back(t, 100 * std::exp(-(t - 100.) * (t - 100.) / 200.));
    TGraph graph = MakeGraph(points);
    const auto [time, amplitude] = PSA::GetMaxGauss(graph);
    EXPECT_NEAR(time, 100, 0.1);
    EXPECT_NEAR(amplitude, 100, 0.5);
}

TEST(PulseShapeAnalysis, AgetFit) {
    // AGET response: baseline 0, amplitude 500, maximum at t = 100, width 20 (as the model of FitAget)
    constexpr double peakU = 1.1664;
    std::vector<std::pair<double, double>> points;
    for (int t = 0; t < 250; t++) {
        const double u = (t - 100.) / 20 + peakU;
        points.emplace_back(t, u <= 0 ? 0 : 500 * PSA::AgetShape(u) / PSA::AgetShape(peakU));
    }
    TGraph graph = MakeGraph(points);
    const auto fit = PSA::FitAget(graph, true, 0.0);
    ASSERT_TRUE(fit != nullptr);
    EXPECT_NEAR(fit->GetParameter(1), 500, 5);
    EXPECT_NEAR(fit->GetParameter(2), 100, 0.5);
    EXPECT_NEAR(fit->GetParameter(3), 20, 0.5);

    // Too few points
    TGraph few = MakeGraph({{0, 0}, {1, 1}, {2, 0}});
    EXPECT_TRUE(PSA::FitAget(few) == nullptr);
}

TEST(PulseShapeAnalysis, IntegrationWindows) {
    TGraph graph = MakeGraph({{0, 1}, {1, 3}, {2, 5}, {3, 7}, {-1, 4}});
    const auto windows = PSA::GetIntWindow(graph, 2);
    // Windows [-2, 0), [0, 2) and [2, 4): (center, mean value)
    ASSERT_EQ(windows.size(), 3u);
    EXPECT_EQ(windows[0], std::make_pair(-1., 4.));
    EXPECT_EQ(windows[1], std::make_pair(1., 2.));
    EXPECT_EQ(windows[2], std::make_pair(3., 6.));

    EXPECT_THROW(PSA::GetIntWindow(graph, 0), std::invalid_argument);
    EXPECT_THROW(PSA::GetIntWindow(graph, -1), std::invalid_argument);
}

TEST(PulseShapeAnalysis, ShapingResponses) {
    EXPECT_DOUBLE_EQ(PSA::ShapingFunction("gaus", 0), 1);
    EXPECT_DOUBLE_EQ(PSA::ShapingFunction("exponential", 1), std::exp(-1.));
    // Causal responses are 0 before the signal
    EXPECT_EQ(PSA::ShapingFunction("exponential", -1), 0);
    EXPECT_EQ(PSA::ShapingFunction("shaper", -1), 0);
    EXPECT_THROW(PSA::ShapingFunction("square", 0), std::runtime_error);
    EXPECT_EQ(PSA::ShapingSupport("gaus"), std::make_pair(-3.5, 3.5));
    EXPECT_EQ(PSA::ShapingSupport("shaperSin"), std::make_pair(0., 5.));

    // Normalized to the gain; centered for gaus, causal (center 0) otherwise
    int center = -1;
    const auto gaus = PSA::BuildResponse("gaus", 2, 3, center);
    EXPECT_EQ(center, 7);
    EXPECT_EQ(gaus.size(), 14u);
    EXPECT_NEAR(std::accumulate(gaus.begin(), gaus.end(), 0.), 3, 1e-12);
    const auto exponential = PSA::BuildResponse("exponential", 2, 1, center);
    EXPECT_EQ(center, 0);
    EXPECT_EQ(exponential.size(), 10u);
    EXPECT_NEAR(std::accumulate(exponential.begin(), exponential.end(), 0.), 1, 1e-12);

    EXPECT_TRUE(PSA::BuildResponse("square", 2, 1, center).empty());
    EXPECT_TRUE(PSA::BuildResponse("gaus", 0, 1, center).empty());
}

TEST(PulseShapeAnalysis, Convolve) {
    int center = 0;
    const auto response = PSA::BuildResponse("exponential", 2, 1, center);
    std::vector<double> input(30, 0);
    input[10] = 2;
    const auto output = PSA::Convolve(input, response, center, false);
    ASSERT_EQ(output.size(), input.size());
    // The response (times 2) starting at bin 10
    for (int i = 0; i < 30; i++) {
        const double expected = i >= 10 && i < 10 + (int)response.size() ? 2 * response[i - 10] : 0;
        EXPECT_NEAR(output[i], expected, 1e-12) << "bin " << i;
    }
    // The negative samples are ignored on request
    input[10] = -2;
    for (double value : PSA::Convolve(input, response, center, true)) EXPECT_EQ(value, 0);
}

TEST(PulseShapeAnalysis, Noise) {
    gRandom->SetSeed(12345);
    std::vector<double> white(20000);
    PSA::GetWhiteNoise(white, 2);
    double mean = 0, mean2 = 0;
    for (double value : white) {
        mean += value / white.size();
        mean2 += value * value / white.size();
    }
    EXPECT_NEAR(mean, 0, 0.1);
    EXPECT_NEAR(std::sqrt(mean2 - mean * mean), 2, 0.1);

    std::vector<double> sine(500, 1);
    PSA::GetSinNoise(sine, 3, 20);
    for (double value : sine) EXPECT_LE(std::abs(value), 3);
    // No noise with a period or amplitude that is not positive
    PSA::GetSinNoise(sine, 3, 0);
    for (double value : sine) EXPECT_EQ(value, 0);
}

TEST(PulseShapeAnalysis, Graphs) {
    const std::vector<float> signal = {1, 2, 3};
    const TGraph graph = PSA::GetGraph(signal, "signal");
    ASSERT_EQ(graph.GetN(), 3);
    EXPECT_DOUBLE_EQ(graph.GetPointX(2), 2);
    EXPECT_DOUBLE_EQ(graph.GetPointY(2), 3);
    EXPECT_STREQ(graph.GetTitle(), "signal");
    EXPECT_EQ(PSA::GetGraph(std::vector<float>{}).GetN(), 0);

    const std::vector<std::pair<double, double>> points = {{0.5, 1}, {1.5, 4}};
    const TGraph pairs = PSA::GetGraphPair(points);
    ASSERT_EQ(pairs.GetN(), 2);
    EXPECT_DOUBLE_EQ(pairs.GetPointX(1), 1.5);
    EXPECT_DOUBLE_EQ(pairs.GetPointY(1), 4);
}
