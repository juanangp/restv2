#ifndef TRESTFFTANALYSIS_H
#define TRESTFFTANALYSIS_H

#include <complex>
#include <string>
#include <vector>

/// \brief Stateless FFT helpers working on std::vector, in the style of TRestPulseShapeAnalysis.
///
/// A spectrum is the full complex spectrum (N bins) of a real signal, so it
/// keeps Hermitian symmetry: X[N-k] = conj(X[k]).
///
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
namespace TRestFFTAnalysis {

/// Full complex spectrum of N bins (bin k: frequency k / N in units of the inverse sampling time)
using Spectrum = std::vector<std::complex<double>>;

// --- Transforms ---

/// \brief Forward FFT of signal[nStart, size - nEnd) (ROOT TVirtualFFT, FFTW; thread safe). Returns {} on
/// invalid ranges or if the FFT plan cannot be created.
template <typename T>
Spectrum ForwardFFT(const std::vector<T>& signal, int nStart = 0, int nEnd = 0);

/// \brief Inverse FFT (normalized by 1/N). Returns the real part (empty if the FFT plan cannot be
/// created, e.g. ROOT without FFTW).
std::vector<double> BackwardFFT(const Spectrum& spectrum);

// --- Spectrum generators (return the spectrum; call BackwardFFT for the time response) ---

/// \brief Spectrum of a unit delta at bin t0 of a signal of nfft bins (all zeros if t0 is outside)
Spectrum ProduceDelta(int t0, int nfft);
/// \brief Spectrum of nfft bins of a second order response f1 / (f1 f2 - w^2 + i f1 w) times sqrt(Ao)
/// exp(-sigma w^2) (gaussian damping), with w = 2 k / 3 at bin k (scale factor of the legacy code)
Spectrum GaussianSecondOrderResponse(int nfft, double f1, double f2, double Ao, double sigma);
/// \brief Spectrum of nfft bins of a second order response f1 / (f1 w + i (f1 f2 - w^2)) delayed by `to`
/// (exp(-i w to)), with w = 2.59 k at bin k (scale factor of the legacy code)
Spectrum SecondOrderAnalyticalResponse(int nfft, double f1, double f2, double to);

// --- In-place filters (they keep Hermitian symmetry), up to the Nyquist bin N/2 included ---

/// \brief Butterworth low-pass filter: the bins k > cutOff are multiplied by 1 / sqrt(1 + (k / cutOff)^(2
/// order)); the bins up to cutOff are kept as they are. Throws std::invalid_argument if cutOff is 0.
void ButterWorthFilter(Spectrum& spectrum, int cutOff, int order);
/// \brief Sets to zero the bins from cutOff (included) to the Nyquist bin, and their mirrors
void KillFrequencies(Spectrum& spectrum, int cutOff);
/// \brief Sets to zero the frequencies from cutFrequency up: the bins from cutFrequency (included) to the
/// Nyquist bin and their mirrors, i.e. [cutFrequency, N - cutFrequency] (same as KillFrequencies)
void ApplyLowPassFilter(Spectrum& spectrum, int cutFrequency);
/// \brief Sets the DC bin (0) to zero, so the constant baseline is removed; nothing on an empty spectrum
void RemoveBaseline(Spectrum& spectrum);
/// \brief Multiplies bin n (and its mirror) by `factor`; nothing if n is outside the spectrum
void RenormalizeNode(Spectrum& spectrum, int n, double factor);

// --- Spectrum arithmetic over bins [from, to) (and their mirrors); to <= 0 means N/2 + 1 (Nyquist included)
// ---

/// \brief Multiplies the bins [from, to) by those of `other` (convolution in time); false (error message)
/// if the spectra have different sizes
bool MultiplyBy(Spectrum& spectrum, const Spectrum& other, int from = 0, int to = 0);
/// \brief Divides the bins [from, to) by those of `other` (deconvolution in time); false (error message) if
/// the spectra have different sizes
bool DivideBy(Spectrum& spectrum, const Spectrum& other, int from = 0, int to = 0);
/// \brief Deconvolves a response: the bins below cutOff are divided by `response`, those from cutOff to
/// the Nyquist bin are rescaled so their amplitude is continuous with bin cutOff - 1. False (error message)
/// if cutOff is not in (0, N/2) or the sizes differ.
bool ApplyResponse(Spectrum& spectrum, const Spectrum& response, int cutOff);

// --- Debug output ---

/// \brief Writes the spectrum to a text file, a line per bin: index, real part, imaginary part. False
/// (error message) if the file cannot be opened.
bool WriteFrequencyToTextFile(const Spectrum& spectrum, const std::string& filename);
/// \brief Writes the signal to a text file, a line per bin: index, value. False (error message) if the
/// file cannot be opened.
template <typename T>
bool WriteSignalToTextFile(const std::vector<T>& signal, const std::string& filename);

}  // namespace TRestFFTAnalysis

#endif
