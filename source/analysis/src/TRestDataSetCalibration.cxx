#include "TRestDataSetCalibration.h"

#include <TDirectory.h>

#include <stdexcept>
#include <tuple>

#include "TRestDataSet.h"
#include "TRestPeakFit.h"
#include "TRestTools.h"

/// \class TRestDataSetCalibration
///
/// Energy calibration of a dataset, a process of TRestDataSetProcessManager. The spectrum of `observable`
/// in the reference (referenceFileName with the cuts, see TRestDataSetProcess) is built, the peaks of known
/// energy are fitted with gaussians and the energy against their position is fitted with a line; the
/// column `<name>_<calibratedObservable>` (e.g. calib_Energy) = slope * observable + intercept (keV) is
/// added to the dataset.
///
/// The position of the peaks is estimated from the maximum of the spectrum, which should be the first
/// peak: each peak is fitted in ±window (15%) of its estimated position, or in its `range` if it is given.
/// The fits are made with TRestPeakFit (RooFit): a gaussian (as legacy) or a Crystal Ball, optionally
/// with a constant or linear background under the peak.
///
/// ### Parameters
/// * observable: the observable calibrated.
/// * calibratedObservable: name of the calibrated observable, the column `<name>_<calibratedObservable>`
///   ("Energy": calib_Energy for a process named calib, as legacy).
/// * peaks: list of peaks, `energy` (keV by default, e.g. "5.9 keV") and optionally `range` (of the
///   observable where it is fitted, e.g. [9000, 13000]). The first one should be the maximum of the
///   spectrum.
/// * nBins: number of bins of the spectrum (1024).
/// * range: range of the observable of the spectrum, e.g. [0, 20000]; automatic if not given.
/// * zeroPoint: add the point (0, 0) to the linear fit (true, as legacy).
/// * peakModel: "gaussian" (default, as legacy) or "crystalBall" (low energy tail).
/// * background: "none" (default, as legacy), "constant" or "linear" under each peak.
/// * window: half width of the fit window of the peaks without range, relative to their estimated
///   position (0.15, as legacy).
/// * peakSearch: search each peak in its range (or its estimated window) and fit it in ±window around the
///   peak found, inside the range (false). The search (TRestPeakFit::FindPeak) only accepts significant
///   peaks (prominence over 3 times the statistical fluctuation, smoothed spectrum): if there is none, the
///   peak is fitted in the whole range.
/// * referenceFileName, cuts: see TRestDataSetProcess.
///
/// ### Results
/// * slope, intercept: of the calibration, energy = slope * observable + intercept.
/// * peaks: of each peak, `position` (mean of the fit), `fwhm` (of its gaussian core, relative to the mean,
///   %), `fitRange` (where it was fitted), `fitStatus` (0: converged) and `fitParameters`, the free
///   parameters of the fit as name: [value, error], which depend on peakModel and background:
///   - mean, sigma: of the gaussian (core), in units of the observable;
///   - alpha, n (crystalBall): the tail starts at alpha sigmas below the mean and falls with power n;
///   - signalCounts: counts of the peak (in the fit range);
///   - backgroundCounts (constant or linear background): counts of the background in the fit range;
///   - backgroundSlope (linear background): relative slope, from -1 to 1 over the fit range.
///
///   E.g. `{energy: 5.9, position: 5021, fwhm: 18.2, fitRange: [4300, 5800], fitStatus: 0,
///   fitParameters: {mean: [5021, 1.3], sigma: [389, 1.1], signalCounts: [102000, 330]}}`.
/// * referenceUsed, parametersFileName: see TRestDataSetProcess.
///
/// Objects (folder RESTDataSetStore/TRestDataSetCalibration/<name> of the output dataset): `spectrum` (with
/// the peak fits g0, g1..., drawn with it) and `linearFit` (energy against position, with the linear fit).
///
/// Example (in a TRestDataSetProcessManager):
///
///    calib:
///      class: "TRestDataSetCalibration"
///      referenceFileName: "Dataset_Calibration.root"
///      observable: "trackAnalysis_MaxTrackEnergy"
///      nBins: 1024
///      range: [0, 20000]
///      peaks:
///        - {energy: "5.9 keV"}
///
/// Differences with legacy (TRestDataSetCalibration): a process of a dataset pipeline (no Calibrate nor
/// dataSetName/outputFileName); calibFile replaced by referenceFileName (a dataset processed by this
/// calibration reuses its parameters, otherwise it is fitted); the column is `<name>_<calibratedObservable>`
/// (legacy calib_Energy, fixed); `peaks` with an optional fit range and the fit results; zeroPoint can be
/// disabled; the peaks are fitted with RooFit (TRestPeakFit, binned likelihood) instead of TH1::Fit,
/// optionally with a Crystal Ball and a background, in a configurable window; the FWHM factor is 2.355
/// (legacy 2.35).

static const bool TRestDataSetCalibrationPeak_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestDataSetCalibrationPeak>("energy", &TRestDataSetCalibrationPeak::fEnergy);
    reg.RegisterField<TRestDataSetCalibrationPeak>("range", &TRestDataSetCalibrationPeak::fRange);
    reg.RegisterField<TRestDataSetCalibrationPeak>("position", &TRestDataSetCalibrationPeak::fPosition);
    reg.RegisterField<TRestDataSetCalibrationPeak>("fwhm", &TRestDataSetCalibrationPeak::fFWHM);
    reg.RegisterField<TRestDataSetCalibrationPeak>("fitRange", &TRestDataSetCalibrationPeak::fFitRange);
    reg.RegisterField<TRestDataSetCalibrationPeak>("fitStatus", &TRestDataSetCalibrationPeak::fFitStatus);
    reg.RegisterField<TRestDataSetCalibrationPeak>("fitParameters",
                                                   &TRestDataSetCalibrationPeak::fFitParameters);
    return true;
}();

static const bool TRestDataSetCalibration_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestDataSetCalibration>("observable", &TRestDataSetCalibration::fObservable);
    reg.RegisterField<TRestDataSetCalibration>("calibratedObservable",
                                               &TRestDataSetCalibration::fCalibratedObservable);
    reg.RegisterField<TRestDataSetCalibration>("peaks", &TRestDataSetCalibration::fPeaks);
    reg.RegisterField<TRestDataSetCalibration>("nBins", &TRestDataSetCalibration::fNBins);
    reg.RegisterField<TRestDataSetCalibration>("range", &TRestDataSetCalibration::fRange);
    reg.RegisterField<TRestDataSetCalibration>("zeroPoint", &TRestDataSetCalibration::fZeroPoint);
    reg.RegisterField<TRestDataSetCalibration>("peakModel", &TRestDataSetCalibration::fPeakModel);
    reg.RegisterField<TRestDataSetCalibration>("background", &TRestDataSetCalibration::fBackground);
    reg.RegisterField<TRestDataSetCalibration>("window", &TRestDataSetCalibration::fWindow);
    reg.RegisterField<TRestDataSetCalibration>("peakSearch", &TRestDataSetCalibration::fPeakSearch);
    reg.RegisterField<TRestDataSetCalibration>("slope", &TRestDataSetCalibration::fSlope);
    reg.RegisterField<TRestDataSetCalibration>("intercept", &TRestDataSetCalibration::fIntercept);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDataSetCalibration", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDataSetCalibration>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestDataSetCalibrationPeak::TRestDataSetCalibrationPeak() : TRestMetadata() {
    fName = "TRestDataSetCalibrationPeak";
}

TRestDataSetCalibrationPeak::TRestDataSetCalibrationPeak(const std::string& name, const YAML::Node& node)
    : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestDataSetCalibrationPeak::LoadConfig() {
    UpdateParamsFromYAML<TRestDataSetCalibrationPeak>(fNode);
    if (static_cast<double>(fEnergy) <= 0)
        throw std::runtime_error("TRestDataSetCalibrationPeak: the energy must be positive");
    UpdateYAMLFromParams<TRestDataSetCalibrationPeak>(fNode);
}

TRestDataSetCalibration::TRestDataSetCalibration() : TRestDataSetProcess() {
    fName = "TRestDataSetCalibration";
}

TRestDataSetCalibration::TRestDataSetCalibration(const std::string& instanceName, const YAML::Node& node)
    : TRestDataSetProcess(instanceName, node) {
    LoadConfig();
}

TRestDataSetCalibration::~TRestDataSetCalibration() = default;

void TRestDataSetCalibration::LoadConfig() {
    UpdateParamsFromYAML<TRestDataSetCalibration>(fNode);
    if (fObservable.empty())
        throw std::runtime_error("TRestDataSetCalibration '" + fName + "': no observable");
    if (fCalibratedObservable.empty())
        throw std::runtime_error("TRestDataSetCalibration '" + fName + "': no calibratedObservable");
    if (fPeaks.empty()) throw std::runtime_error("TRestDataSetCalibration '" + fName + "': no peaks");
    if (fNBins <= 0)
        throw std::runtime_error("TRestDataSetCalibration '" + fName + "': nBins must be positive");
    if (fWindow <= 0 || fWindow >= 1)
        throw std::runtime_error("TRestDataSetCalibration '" + fName + "': window must be in (0, 1)");
    fPeakModel = TRestTools::NormalizeOption("peakModel", fPeakModel, TRestPeakFit::kModels);
    fBackground = TRestTools::NormalizeOption("background", fBackground, TRestPeakFit::kBackgrounds);
    UpdateYAMLFromParams<TRestDataSetCalibration>(fNode);
}

void TRestDataSetCalibration::ComputeParameters(const TRestDataSet&, ROOT::RDF::RNode frame) {
    auto histogram = frame.Histo1D({"spectrum", "", fNBins, fRange.first, fRange.second}, fObservable);
    fSpectrum.reset(static_cast<TH1D*>(histogram->Clone("spectrum")));
    fSpectrum->SetDirectory(nullptr);
    fSpectrum->SetTitle((";" + fObservable + ";Counts").c_str());
    if (fSpectrum->Integral() <= 0) {
        throw std::runtime_error("TRestDataSetCalibration '" + fName + "': the spectrum of " + fObservable +
                                 " is empty");
    }

    // The position of the peaks from the maximum, the first peak
    const double maximumEnergy = fPeaks.front().fEnergy;
    const auto model = TRestPeakFit::ModelFromString(fPeakModel);
    const auto background = TRestPeakFit::BackgroundFromString(fBackground);

    fLinearFit = std::make_unique<TGraph>();
    fLinearFit->SetName("linearFit");
    fLinearFit->SetTitle((";" + fObservable + ";Energy (keV)").c_str());
    for (size_t i = 0; i < fPeaks.size(); i++) {
        auto& peak = fPeaks[i];
        const double energy = peak.fEnergy;
        const auto zone = peak.HasRange()
                              ? peak.fRange
                              : TRestPeakFit::EstimateWindow(*fSpectrum, maximumEnergy, energy, fWindow);
        auto window = zone;
        if (fPeakSearch) {
            if (const auto found = TRestPeakFit::FindPeakWindow(*fSpectrum, zone, fWindow)) {
                window = *found;
            } else {
                RESTWarning << "TRestDataSetCalibration '" << fName << "': no peak found for " << energy
                            << " keV"
                            << " in (" << zone.first << ", " << zone.second << "), fitted in it" << RESTendl;
            }
        }
        const auto [start, end] = window;
        const auto fit =
            TRestPeakFit::FitPeak(*fSpectrum, start, end, "g" + std::to_string(i), model, background);
        if (!fit.valid) {
            RESTWarning << "TRestDataSetCalibration '" << fName << "': the fit of the peak " << energy
                        << " keV in (" << start << ", " << end << ") did not converge" << RESTendl;
        }
        peak.fPosition = fit.mean;
        peak.fFWHM = fit.GetRelativeFWHM();
        peak.fFitRange = window;
        peak.fFitStatus = fit.status;
        peak.fFitParameters = YAML::Node(YAML::NodeType::Map);
        for (const auto& [parameter, value] : fit.parameters)
            peak.fFitParameters[parameter] = std::vector<double>{value.first, value.second};
        fLinearFit->SetPoint(fLinearFit->GetN(), fit.mean, energy);
        RESTDebug << "  peak " << energy << " keV fitted in (" << start << ", " << end << "): " << fit.mean
                  << RESTendl;
    }
    if (fZeroPoint) fLinearFit->SetPoint(fLinearFit->GetN(), 0, 0);
    std::tie(fIntercept, fSlope) = TRestPeakFit::FitLine(*fLinearFit);
    UpdateYAMLFromParams<TRestDataSetCalibration>(fNode);

    RESTInfo << "TRestDataSetCalibration '" << fName << "': energy = " << fSlope << " * " << fObservable
             << " + " << fIntercept << " keV" << RESTendl;
}

void TRestDataSetCalibration::Apply(TRestDataSet& dataSet) const {
    const std::string expression =
        TRestTools::Format("%.17g * (%s) + %.17g", fSlope, fObservable.c_str(), fIntercept);
    dataSet.SetDataFrame(DefineColumn(dataSet.GetDataFrame(), GetEnergyColumn(), expression),
                         {GetEnergyColumn()});
}

void TRestDataSetCalibration::WriteObjects(TDirectory* directory) const {
    if (fSpectrum) directory->WriteObject(fSpectrum.get(), "spectrum");
    if (fLinearFit) directory->WriteObject(fLinearFit.get(), "linearFit");
}

void TRestDataSetCalibration::ReadObjects(TDirectory* directory) {
    if (!directory) return;
    if (auto* spectrum = directory->Get<TH1D>("spectrum")) {
        fSpectrum.reset(static_cast<TH1D*>(spectrum->Clone()));
        fSpectrum->SetDirectory(nullptr);
    }
    if (auto* graph = directory->Get<TGraph>("linearFit"))
        fLinearFit.reset(static_cast<TGraph*>(graph->Clone()));
}
