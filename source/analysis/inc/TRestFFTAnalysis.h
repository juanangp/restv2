#ifndef TRESTFFTANALYSIS_H
#define TRESTFFTANALYSIS_H

#include <complex>
#include <string>
#include <vector>

/// \brief Stateless FFT helpers working on std::vector, in the style of TRestPulseShapeAnalysis.
///
/// A spectrum is the full complex spectrum (N bins) of a real signal, so it
/// keeps Hermitian symmetry: X[N-k] = conj(X[k]).
namespace TRestFFTAnalysis {

using Spectrum = std::vector<std::complex<double>>;

// --- Transforms ---

/// \brief Forward FFT of signal[nStart, size - nEnd). Returns {} on invalid ranges.
template <typename T>
Spectrum ForwardFFT(const std::vector<T>& signal, int nStart = 0, int nEnd = 0);

/// \brief Inverse FFT (normalized by 1/N). Returns the real part.
std::vector<double> BackwardFFT(const Spectrum& spectrum);

// --- Spectrum generators (return the spectrum; call BackwardFFT for the time response) ---

Spectrum ProduceDelta(int t0, int nfft);
Spectrum GaussianSecondOrderResponse(int nfft, double f1, double f2, double Ao, double sigma);
Spectrum SecondOrderAnalyticalResponse(int nfft, double f1, double f2, double to);

// --- In-place filters (they keep Hermitian symmetry) ---

void ButterWorthFilter(Spectrum& spectrum, int cutOff, int order);
void KillFrequencies(Spectrum& spectrum, int cutOff);
void ApplyLowPassFilter(Spectrum& spectrum, int cutFrequency);
void RemoveBaseline(Spectrum& spectrum);
void RenormalizeNode(Spectrum& spectrum, int n, double factor);

// --- Spectrum arithmetic over bins [from, to); to <= 0 means N/2 ---

bool MultiplyBy(Spectrum& spectrum, const Spectrum& other, int from = 0, int to = 0);
bool DivideBy(Spectrum& spectrum, const Spectrum& other, int from = 0, int to = 0);
bool ApplyResponse(Spectrum& spectrum, const Spectrum& response, int cutOff);

// --- Debug output ---

bool WriteFrequencyToTextFile(const Spectrum& spectrum, const std::string& filename);
template <typename T>
bool WriteSignalToTextFile(const std::vector<T>& signal, const std::string& filename);

}  // namespace TRestFFTAnalysis

#endif
