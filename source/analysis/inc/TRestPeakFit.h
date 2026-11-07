#ifndef TRESTPEAKFIT_H
#define TRESTPEAKFIT_H

#include <limits>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class TGraph;
class TH1;

/// This namespace defines generic functions to fit peaks of spectra (histograms) and the linear calibration
/// of their positions, e.g. for the energy calibrations (TRestDataSetCalibration, TRestDataSetGainMap).
///
/// The peaks are fitted with RooFit (binned maximum likelihood in the fit range, the bins inside it): a
/// gaussian or a Crystal Ball (gaussian with a power-law tail on the low side, e.g. incomplete charge
/// collection), plus optionally a constant or linear background under the peak. The fit is kept in the
/// histogram (its list of functions, so it is written, read and drawn with it) as a TF1 `name` with the
/// parameters Constant, Mean and Sigma (Alpha and N for the Crystal Ball, Background and Slope for the
/// background), in the histogram scale. The RooFitResult is not kept: written to a file it adds a
/// TProcessID (ProcessID0, for the references of the RooFit variables); its free parameters are in
/// Peak::parameters.
namespace TRestPeakFit {

/// Shape of a peak
enum class Model { Gaussian, CrystalBall };
/// Background under a peak
enum class Background { None, Constant, Linear };

/// Names of the models and of the backgrounds, for TRestTools::NormalizeOption
inline const std::vector<std::string> kModels = {"gaussian", "crystalBall"};
inline const std::vector<std::string> kBackgrounds = {"none", "constant", "linear"};

/// \brief Model of a name: "gaussian" or "crystalBall"; throws for any other
Model ModelFromString(const std::string& name);
/// \brief Background of a name: "none", "constant" or "linear"; throws for any other
Background BackgroundFromString(const std::string& name);

/// Result of a peak fit
struct Peak {
    double mean = std::numeric_limits<double>::quiet_NaN();
    double sigma = std::numeric_limits<double>::quiet_NaN();
    double meanError = std::numeric_limits<double>::quiet_NaN();
    double sigmaError = std::numeric_limits<double>::quiet_NaN();
    /// Counts of the peak and of the background in the fit range
    double signal = 0;
    double background = 0;
    /// Whether the fit converged
    bool valid = false;
    /// Status of the fit (RooFitResult::status, 0: converged), -1 if it was not done
    int status = -1;
    /// Free parameters of the fit, name: (value, error); they depend on the model: mean and sigma (units of
    /// the histogram axis), alpha and n (Crystal Ball tail: start in sigmas and power), signalCounts,
    /// backgroundCounts and backgroundSlope (linear background, -1 to 1 over the fit range)
    std::map<std::string, std::pair<double, double>> parameters;
    /// \brief FWHM of the gaussian core relative to the mean, in %
    double GetRelativeFWHM() const { return 2.35482 * sigma / mean * 100; }
};

/// \brief Fits a peak of a histogram in [start, end] (the bins inside it). The fit is kept in the
/// histogram as the TF1 `name` (a previous one is replaced).
Peak FitPeak(TH1& histogram, double start, double end, const std::string& name, Model model = Model::Gaussian,
             Background background = Background::None);

/// \brief Mean of the peak fit `name` kept in a histogram (FitPeak), NaN if there is none
double GetPeakMean(const TH1& histogram, const std::string& name);

/// \brief Fit window of a peak of known energy: its position estimated from the maximum of the spectrum,
/// which is the peak of `maximumEnergy` (position = energy * maximum position / maximumEnergy), ± a
/// fraction of it
std::pair<double, double> EstimateWindow(const TH1& histogram, double maximumEnergy, double energy,
                                         double fraction);

/// \brief Position of the most prominent significant peak of a histogram in [start, end] (the bins with
/// their center inside), robust to noisy spectra: the histogram is smoothed with a gaussian of `smoothing`
/// bins, and a local maximum is a peak if its prominence (height over the higher of the two minima that
/// separate it from a higher maximum or the edges) is at least `significance` times its expected
/// fluctuation (square root of its counts). Maxima at the edges of the range are not peaks.
/// \return nullopt if there is no significant peak
std::optional<double> FindPeak(const TH1& histogram, double start, double end, double smoothing = 3,
                               double significance = 3);

/// \brief Fit window around the peak found in a search zone (FindPeak): ± a fraction of its position, inside
/// the zone; nullopt if there is no significant peak
std::optional<std::pair<double, double>> FindPeakWindow(const TH1& histogram,
                                                        const std::pair<double, double>& zone,
                                                        double fraction);

/// \brief Linear fit (pol1, kept in the graph as "linearFit") of y against x, e.g. the energy against the
/// position of the peaks: (intercept, slope). The points not finite (e.g. the mean of a failed peak fit) are
/// removed with a warning, then points (0, 0) are added if there are less than 2 points. Throws
/// std::runtime_error if no point is left, if all the points have the same x or if the fit fails.
std::pair<double, double> FitLine(TGraph& graph);

}  // namespace TRestPeakFit

#endif
