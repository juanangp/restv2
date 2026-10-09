#include "TRestFFTAnalysis.h"

#include <TVirtualFFT.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <mutex>

#include "TRestLogManager.h"

namespace {

using TRestFFTAnalysis::Spectrum;

// FFTW only guarantees thread safety when executing a plan, not when creating
// or destroying it. Plan handling is serialized; execution runs unlocked.
std::mutex gPlanMutex;

class FFTPlan {
   public:
    FFTPlan(int n, const char* option) {
        std::lock_guard<std::mutex> lock(gPlanMutex);
        fFFT = TVirtualFFT::FFT(1, &n, option);
    }
    ~FFTPlan() {
        std::lock_guard<std::mutex> lock(gPlanMutex);
        delete fFFT;
    }
    FFTPlan(const FFTPlan&) = delete;
    FFTPlan& operator=(const FFTPlan&) = delete;

    TVirtualFFT* Get() const { return fFFT; }

   private:
    TVirtualFFT* fFFT = nullptr;
};

/// Complex-to-complex transform. Backward is NOT normalized.
Spectrum RunC2C(const std::vector<double>& re, const std::vector<double>& im, bool forward) {
    const int n = (int)re.size();
    if (n <= 0) return {};

    FFTPlan plan(n, forward ? "C2CFORWARD ES" : "C2CBACKWARD ES");
    if (!plan.Get()) {
        RESTError << "TRestFFTAnalysis: could not create the FFT plan (is FFTW available in this ROOT?)"
                  << RESTendl;
        return {};
    }

    plan.Get()->SetPointsComplex(re.data(), im.data());
    plan.Get()->Transform();

    std::vector<double> outRe(n), outIm(n);
    plan.Get()->GetPointsComplex(outRe.data(), outIm.data());

    Spectrum out(n);
    for (int i = 0; i < n; i++) out[i] = {outRe[i], outIm[i]};
    return out;
}

/// Sets bin k and its Hermitian mirror N-k. DC and Nyquist bins are real for a real signal.
void SetHermitian(Spectrum& s, int k, std::complex<double> value) {
    const int n = (int)s.size();
    if (k == 0 || 2 * k == n) {
        s[k] = value.real();
        return;
    }
    s[k] = value;
    s[n - k] = std::conj(value);
}

template <typename Op>
bool CombineSpectra(Spectrum& s, const Spectrum& other, int from, int to, const char* caller, Op op) {
    if (s.size() != other.size()) {
        RESTError << "TRestFFTAnalysis::" << caller << ": spectra have different sizes (" << s.size()
                  << " vs " << other.size() << ")" << RESTendl;
        return false;
    }
    const int n = (int)s.size();
    if (to <= 0 || to > n / 2) to = n / 2;
    for (int i = std::max(0, from); i < to; i++) SetHermitian(s, i, op(s[i], other[i]));
    return true;
}

}  // namespace

namespace TRestFFTAnalysis {

// ---------------------------------------------------------------------------
// Transforms
// ---------------------------------------------------------------------------

template <typename T>
Spectrum ForwardFFT(const std::vector<T>& signal, int nStart, int nEnd) {
    const int n = (int)signal.size() - nStart - nEnd;
    if (nStart < 0 || nEnd < 0 || n <= 0) {
        RESTError << "TRestFFTAnalysis::ForwardFFT: invalid range (size " << signal.size() << ", nStart "
                  << nStart << ", nEnd " << nEnd << ")" << RESTendl;
        return {};
    }

    std::vector<double> re(signal.begin() + nStart, signal.begin() + nStart + n);
    std::vector<double> im(n, 0.0);
    return RunC2C(re, im, true);
}

template Spectrum ForwardFFT<short>(const std::vector<short>&, int, int);
template Spectrum ForwardFFT<float>(const std::vector<float>&, int, int);
template Spectrum ForwardFFT<double>(const std::vector<double>&, int, int);

std::vector<double> BackwardFFT(const Spectrum& spectrum) {
    const int n = (int)spectrum.size();
    std::vector<double> re(n), im(n);
    for (int i = 0; i < n; i++) {
        re[i] = spectrum[i].real();
        im[i] = spectrum[i].imag();
    }

    const Spectrum out = RunC2C(re, im, false);

    std::vector<double> signal(out.size());
    for (size_t i = 0; i < out.size(); i++) signal[i] = out[i].real() / n;
    return signal;
}

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

Spectrum ProduceDelta(int t0, int nfft) {
    std::vector<double> delta(std::max(nfft, 0), 0.0);
    if (t0 >= 0 && t0 < nfft) delta[t0] = 1.0;
    return ForwardFFT(delta);
}

Spectrum GaussianSecondOrderResponse(int nfft, double f1, double f2, double Ao, double sigma) {
    Spectrum s(std::max(nfft, 0));
    const double a = std::sqrt(Ao);
    for (int i = 0; i <= nfft / 2; i++) {
        const double w = 2. * i / 3;  // factor del código original
        std::complex<double> value =
            std::complex<double>(f1, 0) / std::complex<double>(f1 * f2 - w * w, f1 * w);
        value *= a * std::exp(-sigma * w * w);
        SetHermitian(s, i, value);
    }
    return s;
}

Spectrum SecondOrderAnalyticalResponse(int nfft, double f1, double f2, double to) {
    Spectrum s(std::max(nfft, 0));
    for (int i = 0; i <= nfft / 2; i++) {
        const double w = 2.59 * i;  // factor del código original
        std::complex<double> value =
            std::complex<double>(f1, 0) / std::complex<double>(f1 * w, f1 * f2 - w * w);
        value *= std::exp(std::complex<double>(0, -w * to));
        SetHermitian(s, i, value);
    }
    return s;
}

// ---------------------------------------------------------------------------
// Filters
// ---------------------------------------------------------------------------

void ButterWorthFilter(Spectrum& s, int cutOff, int order) {
    const int n = (int)s.size();
    for (int i = std::max(0, cutOff + 1); i < n / 2; i++) {
        const double gain = 1.0 / std::sqrt(1.0 + std::pow((double)i / cutOff, 2.0 * order));
        SetHermitian(s, i, s[i] * gain);
    }
}

void KillFrequencies(Spectrum& s, int cutOff) {
    const int n = (int)s.size();
    for (int i = std::max(0, cutOff); i < n / 2; i++) SetHermitian(s, i, 0.0);
}

void ApplyLowPassFilter(Spectrum& s, int cutFrequency) {
    const int n = (int)s.size();
    const int lo = std::max(0, cutFrequency);
    const int hi = std::min(n, n - cutFrequency);
    for (int i = lo; i < hi; i++) s[i] = 0.0;
}

void RemoveBaseline(Spectrum& s) {
    if (s.size() < 2) return;
    s[0] = std::abs(s[1]);
}

void RenormalizeNode(Spectrum& s, int n, double factor) {
    if (n < 0 || n >= (int)s.size()) return;
    SetHermitian(s, n, s[n] / factor);
}

// ---------------------------------------------------------------------------
// Arithmetic
// ---------------------------------------------------------------------------

bool MultiplyBy(Spectrum& s, const Spectrum& other, int from, int to) {
    return CombineSpectra(s, other, from, to, "MultiplyBy",
                          [](const std::complex<double>& a, const std::complex<double>& b) { return a * b; });
}

bool DivideBy(Spectrum& s, const Spectrum& other, int from, int to) {
    return CombineSpectra(s, other, from, to, "DivideBy",
                          [](const std::complex<double>& a, const std::complex<double>& b) { return a / b; });
}

bool ApplyResponse(Spectrum& s, const Spectrum& response, int cutOff) {
    const int n = (int)s.size();
    if (cutOff <= 0 || cutOff >= n / 2) {
        RESTError << "TRestFFTAnalysis::ApplyResponse: cutOff must be in (0, " << n / 2 << "), got "
                  << cutOff << RESTendl;
        return false;
    }

    if (!DivideBy(s, response, 0, cutOff)) return false;

    // Above the cutOff the spectrum is not deconvolved: it is rescaled so its
    // amplitude is continuous with the last deconvolved bin.
    const double denom = std::norm(s[cutOff]);
    if (denom <= 0) return true;  // nothing above cutOff to rescale

    const double scale = std::sqrt(std::norm(s[cutOff - 1]) / denom);
    for (int i = cutOff; i < n / 2; i++) SetHermitian(s, i, s[i] * scale);
    return true;
}

// ---------------------------------------------------------------------------
// Debug output
// ---------------------------------------------------------------------------

bool WriteFrequencyToTextFile(const Spectrum& s, const std::string& filename) {
    std::ofstream out(filename);
    if (!out) {
        RESTError << "TRestFFTAnalysis: cannot open " << filename << RESTendl;
        return false;
    }
    out << std::scientific << std::setprecision(14);
    for (size_t i = 0; i < s.size(); i++) out << i << '\t' << s[i].real() << '\t' << s[i].imag() << '\n';
    return true;
}

template <typename T>
bool WriteSignalToTextFile(const std::vector<T>& signal, const std::string& filename) {
    std::ofstream out(filename);
    if (!out) {
        RESTError << "TRestFFTAnalysis: cannot open " << filename << RESTendl;
        return false;
    }
    out << std::scientific;
    for (size_t i = 0; i < signal.size(); i++) out << i << '\t' << signal[i] << '\n';
    return true;
}

template bool WriteSignalToTextFile<short>(const std::vector<short>&, const std::string&);
template bool WriteSignalToTextFile<float>(const std::vector<float>&, const std::string&);
template bool WriteSignalToTextFile<double>(const std::vector<double>&, const std::string&);

}  // namespace TRestFFTAnalysis
