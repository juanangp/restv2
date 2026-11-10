/// Unit tests of TRestFFTAnalysis (GoogleTest, BUILD_TESTING): transforms (round trip, delta, constant),
/// filters, spectrum arithmetic (a product with the spectrum of a delta is a circular shift) and the text
/// output, on synthetic signals. The tests that need the FFT are skipped if ROOT has no FFTW (empty
/// spectrum).

#include <TMath.h>
#include <TRandom3.h>
#include <gtest/gtest.h>

#include <cmath>
#include <complex>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "TRestFFTAnalysis.h"

namespace fs = std::filesystem;
using TRestFFTAnalysis::Spectrum;

namespace {

/// Random signal of n bins (fixed seed)
std::vector<double> RandomSignal(int n, unsigned seed = 7) {
    TRandom3 random(seed);
    std::vector<double> signal(n);
    for (auto& value : signal) value = random.Gaus(0, 10);
    return signal;
}

/// Forward FFT; the test is skipped if it cannot be computed (ROOT without FFTW)
#define FORWARD_OR_SKIP(spectrum, signal)                           \
    const Spectrum spectrum = TRestFFTAnalysis::ForwardFFT(signal); \
    if (spectrum.empty()) GTEST_SKIP() << "FFT not available (ROOT without FFTW)"

/// Whether the spectrum is that of a real signal: X[N - k] = conj(X[k]), real DC
void ExpectHermitian(const Spectrum& spectrum) {
    const int n = spectrum.size();
    EXPECT_NEAR(spectrum[0].imag(), 0, 1e-9);
    for (int k = 1; k < n; k++) {
        EXPECT_NEAR(spectrum[k].real(), spectrum[n - k].real(), 1e-9) << "bin " << k;
        EXPECT_NEAR(spectrum[k].imag(), -spectrum[n - k].imag(), 1e-9) << "bin " << k;
    }
}

/// Directory of the current test in the work directory, created empty
fs::path TestDirectory() {
    const auto* test = ::testing::UnitTest::GetInstance()->current_test_info();
    const fs::path directory =
        fs::path(TESTS_WORK_DIR) / (std::string(test->test_suite_name()) + "_" + test->name());
    fs::remove_all(directory);
    fs::create_directories(directory);
    return directory;
}

}  // namespace

TEST(FFTAnalysis, RoundTrip) {
    const auto signal = RandomSignal(64);
    FORWARD_OR_SKIP(spectrum, signal);
    ASSERT_EQ(spectrum.size(), signal.size());
    ExpectHermitian(spectrum);

    const auto back = TRestFFTAnalysis::BackwardFFT(spectrum);
    ASSERT_EQ(back.size(), signal.size());
    for (size_t i = 0; i < signal.size(); i++) EXPECT_NEAR(back[i], signal[i], 1e-9) << "bin " << i;
}

TEST(FFTAnalysis, RangeOfTheSignal) {
    // nStart and nEnd bins excluded
    const std::vector<float> signal(40, 2.f);
    const auto spectrum = TRestFFTAnalysis::ForwardFFT(signal, 4, 4);
    if (spectrum.empty()) GTEST_SKIP() << "FFT not available (ROOT without FFTW)";
    ASSERT_EQ(spectrum.size(), 32u);
    // A constant: only the DC bin, N * value
    EXPECT_NEAR(spectrum[0].real(), 64, 1e-9);
    for (size_t k = 1; k < spectrum.size(); k++) EXPECT_NEAR(std::abs(spectrum[k]), 0, 1e-9);

    // Invalid ranges
    EXPECT_TRUE(TRestFFTAnalysis::ForwardFFT(signal, 20, 20).empty());
    EXPECT_TRUE(TRestFFTAnalysis::ForwardFFT(signal, -1, 0).empty());
}

TEST(FFTAnalysis, ProduceDelta) {
    const auto delta = TRestFFTAnalysis::ProduceDelta(5, 32);
    if (delta.empty()) GTEST_SKIP() << "FFT not available (ROOT without FFTW)";
    ASSERT_EQ(delta.size(), 32u);
    for (const auto& value : delta) EXPECT_NEAR(std::abs(value), 1, 1e-12);
    const auto signal = TRestFFTAnalysis::BackwardFFT(delta);
    for (int i = 0; i < 32; i++) EXPECT_NEAR(signal[i], i == 5 ? 1 : 0, 1e-12) << "bin " << i;

    // Outside the signal: all zeros
    const auto outside = TRestFFTAnalysis::ProduceDelta(40, 32);
    ASSERT_EQ(outside.size(), 32u);
    for (const auto& value : outside) EXPECT_EQ(std::abs(value), 0);
}

TEST(FFTAnalysis, MultiplyByDeltaShiftsTheSignal) {
    const int n = 32, shift = 3;
    const auto signal = RandomSignal(n);
    FORWARD_OR_SKIP(original, signal);
    Spectrum spectrum = original;
    const auto delta = TRestFFTAnalysis::ProduceDelta(shift, n);
    ASSERT_TRUE(TRestFFTAnalysis::MultiplyBy(spectrum, delta));
    ExpectHermitian(spectrum);

    // Convolution with a delta at `shift`: circular shift
    const auto shifted = TRestFFTAnalysis::BackwardFFT(spectrum);
    for (int i = 0; i < n; i++) EXPECT_NEAR(shifted[i], signal[(i - shift + n) % n], 1e-9) << "bin " << i;

    // Deconvolution: back to the original
    ASSERT_TRUE(TRestFFTAnalysis::DivideBy(spectrum, delta));
    const auto back = TRestFFTAnalysis::BackwardFFT(spectrum);
    for (int i = 0; i < n; i++) EXPECT_NEAR(back[i], signal[i], 1e-9) << "bin " << i;

    // Different sizes
    Spectrum other(n / 2, 1.);
    EXPECT_FALSE(TRestFFTAnalysis::MultiplyBy(spectrum, other));
    EXPECT_FALSE(TRestFFTAnalysis::DivideBy(spectrum, other));
}

TEST(FFTAnalysis, KillFrequencies) {
    const int n = 128;
    std::vector<double> slow(n), signal(n);
    for (int i = 0; i < n; i++) {
        slow[i] = std::sin(2 * TMath::Pi() * 3 * i / n);
        signal[i] = slow[i] + 0.5 * std::sin(2 * TMath::Pi() * 20 * i / n);
    }
    FORWARD_OR_SKIP(original, signal);

    // Frequencies from 10 up removed: only the slow sine is left
    Spectrum spectrum = original;
    TRestFFTAnalysis::KillFrequencies(spectrum, 10);
    ExpectHermitian(spectrum);
    for (int k = 10; k <= n - 10; k++) EXPECT_EQ(std::abs(spectrum[k]), 0) << "bin " << k;
    const auto filtered = TRestFFTAnalysis::BackwardFFT(spectrum);
    for (int i = 0; i < n; i++) EXPECT_NEAR(filtered[i], slow[i], 1e-9) << "bin " << i;

    // ApplyLowPassFilter is the same
    Spectrum lowPass = original;
    TRestFFTAnalysis::ApplyLowPassFilter(lowPass, 10);
    for (int k = 0; k < n; k++) EXPECT_EQ(lowPass[k], spectrum[k]) << "bin " << k;
}

TEST(FFTAnalysis, RemoveBaseline) {
    auto signal = RandomSignal(64);
    for (auto& value : signal) value += 100;
    FORWARD_OR_SKIP(original, signal);
    Spectrum spectrum = original;
    TRestFFTAnalysis::RemoveBaseline(spectrum);
    EXPECT_EQ(std::abs(spectrum[0]), 0);
    const auto back = TRestFFTAnalysis::BackwardFFT(spectrum);
    double mean = 0;
    for (double value : back) mean += value / back.size();
    EXPECT_NEAR(mean, 0, 1e-9);

    Spectrum empty;
    TRestFFTAnalysis::RemoveBaseline(empty);  // nothing, no crash
    EXPECT_TRUE(empty.empty());
}

TEST(FFTAnalysis, ButterWorthFilter) {
    const int n = 64, cutOff = 8, order = 2;
    FORWARD_OR_SKIP(original, RandomSignal(n));
    Spectrum spectrum = original;
    TRestFFTAnalysis::ButterWorthFilter(spectrum, cutOff, order);
    ExpectHermitian(spectrum);
    // Up to the cut off unchanged, above it attenuated by 1 / sqrt(1 + (k / cutOff)^(2 order))
    for (int k = 0; k <= cutOff; k++) EXPECT_EQ(spectrum[k], original[k]) << "bin " << k;
    for (int k : {cutOff + 1, 2 * cutOff, n / 2}) {
        const double gain = 1 / std::sqrt(1 + std::pow(double(k) / cutOff, 2 * order));
        EXPECT_NEAR(std::abs(spectrum[k]), gain * std::abs(original[k]), 1e-9) << "bin " << k;
        EXPECT_NEAR(std::abs(spectrum[n - k]), gain * std::abs(original[n - k]), 1e-9) << "bin " << k;
    }

    EXPECT_THROW(TRestFFTAnalysis::ButterWorthFilter(spectrum, 0, order), std::invalid_argument);
}

TEST(FFTAnalysis, RenormalizeNode) {
    const int n = 32;
    FORWARD_OR_SKIP(original, RandomSignal(n));
    Spectrum spectrum = original;
    TRestFFTAnalysis::RenormalizeNode(spectrum, 5, 2);
    EXPECT_NEAR(std::abs(spectrum[5] - 2. * original[5]), 0, 1e-9);
    EXPECT_NEAR(std::abs(spectrum[n - 5] - 2. * original[n - 5]), 0, 1e-9);
    ExpectHermitian(spectrum);

    // Outside the spectrum: nothing
    Spectrum unchanged = original;
    TRestFFTAnalysis::RenormalizeNode(unchanged, n, 2);
    TRestFFTAnalysis::RenormalizeNode(unchanged, -1, 2);
    for (int k = 0; k < n; k++) EXPECT_EQ(unchanged[k], original[k]);
}

TEST(FFTAnalysis, ApplyResponse) {
    const int n = 64, cutOff = 10;
    FORWARD_OR_SKIP(original, RandomSignal(n));
    const Spectrum response(n, 1.);

    // Invalid cut offs
    Spectrum spectrum = original;
    EXPECT_FALSE(TRestFFTAnalysis::ApplyResponse(spectrum, response, 0));
    EXPECT_FALSE(TRestFFTAnalysis::ApplyResponse(spectrum, response, n / 2));
    EXPECT_FALSE(TRestFFTAnalysis::ApplyResponse(spectrum, Spectrum(n / 2, 1.), cutOff));

    // A unit response: the bins below the cut off unchanged, those above rescaled so the amplitude is
    // continuous at the cut off
    ASSERT_TRUE(TRestFFTAnalysis::ApplyResponse(spectrum, response, cutOff));
    for (int k = 0; k < cutOff; k++) EXPECT_NEAR(std::abs(spectrum[k] - original[k]), 0, 1e-9);
    EXPECT_NEAR(std::abs(spectrum[cutOff]), std::abs(original[cutOff - 1]), 1e-9);
    const double scale = std::abs(original[cutOff - 1]) / std::abs(original[cutOff]);
    EXPECT_NEAR(std::abs(spectrum[cutOff + 5]), scale * std::abs(original[cutOff + 5]), 1e-9);
    ExpectHermitian(spectrum);
}

TEST(FFTAnalysis, ResponseGeneratorsAreHermitian) {
    const auto gaussian = TRestFFTAnalysis::GaussianSecondOrderResponse(64, 2, 3, 4, 0.01);
    ASSERT_EQ(gaussian.size(), 64u);
    ExpectHermitian(gaussian);
    // At bin 0 (w = 0): f1 / (f1 f2) * sqrt(Ao)
    EXPECT_NEAR(gaussian[0].real(), 1. / 3 * 2, 1e-12);

    const auto analytical = TRestFFTAnalysis::SecondOrderAnalyticalResponse(64, 2, 3, 0.5);
    ASSERT_EQ(analytical.size(), 64u);
    ExpectHermitian(analytical);
}

TEST(FFTAnalysis, WriteTextFiles) {
    const fs::path directory = TestDirectory();
    const std::vector<double> signal = {1, 2, 3, 4};
    const std::string signalFile = (directory / "signal.txt").string();
    ASSERT_TRUE(TRestFFTAnalysis::WriteSignalToTextFile(signal, signalFile));
    std::ifstream signalInput(signalFile);
    int index = 0, lines = 0;
    double value = 0;
    while (signalInput >> index >> value) {
        EXPECT_EQ(index, lines);
        EXPECT_DOUBLE_EQ(value, signal[lines]);
        lines++;
    }
    EXPECT_EQ(lines, 4);

    const Spectrum spectrum = {{1, 0}, {0.5, -0.25}, {2, 0}, {0.5, 0.25}};
    const std::string spectrumFile = (directory / "spectrum.txt").string();
    ASSERT_TRUE(TRestFFTAnalysis::WriteFrequencyToTextFile(spectrum, spectrumFile));
    std::ifstream spectrumInput(spectrumFile);
    double real = 0, imaginary = 0;
    lines = 0;
    while (spectrumInput >> index >> real >> imaginary) {
        EXPECT_DOUBLE_EQ(real, spectrum[lines].real());
        EXPECT_DOUBLE_EQ(imaginary, spectrum[lines].imag());
        lines++;
    }
    EXPECT_EQ(lines, 4);

    // A file that cannot be opened
    const std::string missing = (directory / "missing" / "signal.txt").string();
    EXPECT_FALSE(TRestFFTAnalysis::WriteSignalToTextFile(signal, missing));
    EXPECT_FALSE(TRestFFTAnalysis::WriteFrequencyToTextFile(spectrum, missing));
}
