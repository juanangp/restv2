/// Unit tests of TRestPeakFit (GoogleTest, BUILD_TESTING): peak fits (RooFit) of synthetic gaussian peaks
/// in histograms filled with a fixed seed, the peak search (FindPeak, FindPeakWindow), the fit window
/// estimate and the linear fit of the calibration (FitLine).

#include <TGraph.h>
#include <TH1D.h>
#include <TRandom3.h>
#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <string>

#include "TRestPeakFit.h"

namespace {

/// Histogram of 100 bins in [0, 100] not attached to any directory
TH1D MakeHistogram(const std::string& name) {
    TH1D histogram(name.c_str(), "", 100, 0, 100);
    histogram.SetDirectory(nullptr);
    return histogram;
}

/// Histogram with `n` entries of a gaussian (mean, sigma), and `background` entries uniform in [0, 100]
TH1D GaussianSpectrum(const std::string& name, int n, double mean, double sigma, int background = 0,
                      unsigned seed = 1234) {
    TH1D histogram = MakeHistogram(name);
    TRandom3 random(seed);
    for (int i = 0; i < n; i++) histogram.Fill(random.Gaus(mean, sigma));
    for (int i = 0; i < background; i++) histogram.Fill(random.Uniform(0, 100));
    return histogram;
}

/// Histogram with two gaussian peaks (at 30 and 70, sigma 3 bins) over a flat level of 10, no fluctuations
TH1D TwoPeaks(const std::string& name) {
    TH1D histogram = MakeHistogram(name);
    for (int bin = 1; bin <= histogram.GetNbinsX(); bin++) {
        const double x = histogram.GetBinCenter(bin);
        histogram.SetBinContent(bin, 10 + 1000 * std::exp(-(x - 30) * (x - 30) / 18.) +
                                         400 * std::exp(-(x - 70) * (x - 70) / 18.));
    }
    return histogram;
}

}  // namespace

TEST(PeakFit, ModelAndBackgroundFromString) {
    EXPECT_EQ(TRestPeakFit::ModelFromString("gaussian"), TRestPeakFit::Model::Gaussian);
    EXPECT_EQ(TRestPeakFit::ModelFromString("crystalBall"), TRestPeakFit::Model::CrystalBall);
    EXPECT_THROW(TRestPeakFit::ModelFromString("lorentzian"), std::runtime_error);
    EXPECT_EQ(TRestPeakFit::BackgroundFromString("none"), TRestPeakFit::Background::None);
    EXPECT_EQ(TRestPeakFit::BackgroundFromString("constant"), TRestPeakFit::Background::Constant);
    EXPECT_EQ(TRestPeakFit::BackgroundFromString("linear"), TRestPeakFit::Background::Linear);
    EXPECT_THROW(TRestPeakFit::BackgroundFromString("quadratic"), std::runtime_error);
}

TEST(PeakFit, GaussianPeak) {
    TH1D histogram = GaussianSpectrum("gaussianPeak", 10000, 50, 5);
    const auto peak = TRestPeakFit::FitPeak(histogram, 30, 70, "g0");

    EXPECT_TRUE(peak.valid);
    EXPECT_EQ(peak.status, 0);
    EXPECT_NEAR(peak.mean, 50, 0.3);
    EXPECT_NEAR(peak.sigma, 5, 0.3);
    EXPECT_GT(peak.meanError, 0);
    EXPECT_LT(peak.meanError, 0.2);
    // Practically all the entries are within 4 sigma
    EXPECT_NEAR(peak.signal, 10000, 300);
    EXPECT_EQ(peak.background, 0);
    EXPECT_NEAR(peak.GetRelativeFWHM(), 2.35482 * 5 / 50 * 100, 1.5);
    // Free parameters of a gaussian without background
    EXPECT_EQ(peak.parameters.count("mean"), 1u);
    EXPECT_EQ(peak.parameters.count("sigma"), 1u);
    EXPECT_EQ(peak.parameters.count("signalCounts"), 1u);
    EXPECT_EQ(peak.parameters.count("backgroundCounts"), 0u);

    // The fit is kept in the histogram, with the same mean
    ASSERT_NE(histogram.GetFunction("g0"), nullptr);
    EXPECT_DOUBLE_EQ(TRestPeakFit::GetPeakMean(histogram, "g0"), peak.mean);
    EXPECT_TRUE(std::isnan(TRestPeakFit::GetPeakMean(histogram, "missing")));
}

TEST(PeakFit, RefitReplacesFunction) {
    TH1D histogram = GaussianSpectrum("refit", 5000, 50, 5);
    TRestPeakFit::FitPeak(histogram, 30, 70, "fit");
    TRestPeakFit::FitPeak(histogram, 35, 65, "fit");
    EXPECT_EQ(histogram.GetListOfFunctions()->GetSize(), 1);
}

TEST(PeakFit, GaussianPeakWithConstantBackground) {
    // 5000 uniform entries in [0, 100]: 3000 in the fit range [20, 80]
    TH1D histogram = GaussianSpectrum("constantBackground", 10000, 50, 5, 5000);
    const auto peak = TRestPeakFit::FitPeak(histogram, 20, 80, "g0", TRestPeakFit::Model::Gaussian,
                                            TRestPeakFit::Background::Constant);

    EXPECT_TRUE(peak.valid);
    EXPECT_NEAR(peak.mean, 50, 0.4);
    EXPECT_NEAR(peak.sigma, 5, 0.4);
    EXPECT_NEAR(peak.signal, 10000, 500);
    EXPECT_NEAR(peak.background, 3000, 400);
    EXPECT_EQ(peak.parameters.count("backgroundCounts"), 1u);
}

TEST(PeakFit, CrystalBallModel) {
    TH1D histogram = GaussianSpectrum("crystalBall", 10000, 50, 5);
    const auto peak = TRestPeakFit::FitPeak(histogram, 30, 70, "cb", TRestPeakFit::Model::CrystalBall);
    // A gaussian peak fitted with a Crystal Ball: the same core (the tail parameters may be at their limits)
    EXPECT_NEAR(peak.mean, 50, 0.5);
    EXPECT_NEAR(peak.sigma, 5, 0.5);
    EXPECT_EQ(peak.parameters.count("alpha"), 1u);
    EXPECT_EQ(peak.parameters.count("n"), 1u);
}

TEST(PeakFit, NotFittedWithoutBinsOrCounts) {
    TH1D histogram = GaussianSpectrum("fewBins", 1000, 50, 5);
    // A single bin (center 50.5) in [50, 51]
    const auto narrow = TRestPeakFit::FitPeak(histogram, 50, 51, "g0");
    EXPECT_FALSE(narrow.valid);
    EXPECT_EQ(narrow.status, -1);
    EXPECT_TRUE(std::isnan(narrow.mean));

    TH1D empty = MakeHistogram("empty");
    const auto noCounts = TRestPeakFit::FitPeak(empty, 20, 80, "g0");
    EXPECT_FALSE(noCounts.valid);
    EXPECT_TRUE(std::isnan(noCounts.mean));
}

TEST(PeakFit, EstimateWindow) {
    TH1D histogram = MakeHistogram("window");
    histogram.SetBinContent(51, 100);  // maximum at 50.5
    // The peak of 5 keV when the maximum is the peak of 10 keV: at 25.25, +- 10%
    const auto [start, end] = TRestPeakFit::EstimateWindow(histogram, 10, 5, 0.1);
    EXPECT_DOUBLE_EQ(start, 25.25 * 0.9);
    EXPECT_DOUBLE_EQ(end, 25.25 * 1.1);
}

TEST(PeakFit, FindPeak) {
    const TH1D histogram = TwoPeaks("twoPeaks");
    // The most prominent one in the whole range, the other one in its half
    const auto highest = TRestPeakFit::FindPeak(histogram, 0, 100);
    ASSERT_TRUE(highest.has_value());
    EXPECT_NEAR(*highest, 30, 1);
    const auto second = TRestPeakFit::FindPeak(histogram, 50, 100);
    ASSERT_TRUE(second.has_value());
    EXPECT_NEAR(*second, 70, 1);
    // Less than 3 bins
    EXPECT_FALSE(TRestPeakFit::FindPeak(histogram, 50, 51).has_value());

    // A flat spectrum has no peaks
    TH1D flat = MakeHistogram("flat");
    for (int bin = 1; bin <= flat.GetNbinsX(); bin++) flat.SetBinContent(bin, 100);
    EXPECT_FALSE(TRestPeakFit::FindPeak(flat, 0, 100).has_value());
}

TEST(PeakFit, FindPeakWindow) {
    const TH1D histogram = TwoPeaks("peakWindow");
    const auto window = TRestPeakFit::FindPeakWindow(histogram, {50, 100}, 0.1);
    ASSERT_TRUE(window.has_value());
    EXPECT_NEAR(window->first, 70 * 0.9, 1);
    EXPECT_NEAR(window->second, 70 * 1.1, 1.2);
    // Limited to the zone
    const auto limited = TRestPeakFit::FindPeakWindow(histogram, {60, 80}, 0.5);
    ASSERT_TRUE(limited.has_value());
    EXPECT_DOUBLE_EQ(limited->first, 60);
    EXPECT_DOUBLE_EQ(limited->second, 80);

    TH1D flat = MakeHistogram("flatWindow");
    for (int bin = 1; bin <= flat.GetNbinsX(); bin++) flat.SetBinContent(bin, 100);
    EXPECT_FALSE(TRestPeakFit::FindPeakWindow(flat, {0, 100}, 0.1).has_value());
}

TEST(PeakFit, FitLine) {
    TGraph graph;
    graph.SetName("line");
    for (double x : {1., 2., 3., 4.}) graph.AddPoint(x, 2 * x + 1);
    const auto [intercept, slope] = TRestPeakFit::FitLine(graph);
    EXPECT_NEAR(intercept, 1, 1e-6);
    EXPECT_NEAR(slope, 2, 1e-6);
    EXPECT_NE(graph.GetFunction("linearFit"), nullptr);
}

TEST(PeakFit, FitLineRemovesNotFinitePoints) {
    TGraph graph;
    graph.SetName("withNaN");
    graph.AddPoint(1, 3);
    graph.AddPoint(std::nan(""), 4);
    graph.AddPoint(3, 7);
    const auto [intercept, slope] = TRestPeakFit::FitLine(graph);
    EXPECT_EQ(graph.GetN(), 2);
    EXPECT_NEAR(intercept, 1, 1e-6);
    EXPECT_NEAR(slope, 2, 1e-6);
}

TEST(PeakFit, FitLineSinglePointThroughOrigin) {
    // A single point: (0, 0) is added
    TGraph graph;
    graph.SetName("single");
    graph.AddPoint(2, 4);
    const auto [intercept, slope] = TRestPeakFit::FitLine(graph);
    EXPECT_EQ(graph.GetN(), 2);
    EXPECT_NEAR(intercept, 0, 1e-6);
    EXPECT_NEAR(slope, 2, 1e-6);
}

TEST(PeakFit, FitLineErrors) {
    TGraph empty;
    empty.SetName("empty");
    EXPECT_THROW(TRestPeakFit::FitLine(empty), std::runtime_error);

    TGraph onlyNaN;
    onlyNaN.SetName("onlyNaN");
    onlyNaN.AddPoint(std::nan(""), 1);
    EXPECT_THROW(TRestPeakFit::FitLine(onlyNaN), std::runtime_error);

    TGraph sameX;
    sameX.SetName("sameX");
    sameX.AddPoint(1, 1);
    sameX.AddPoint(1, 2);
    EXPECT_THROW(TRestPeakFit::FitLine(sameX), std::runtime_error);
}
