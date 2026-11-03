#ifndef TRESTDATASETCALIBRATION_H
#define TRESTDATASETCALIBRATION_H

#include <TGraph.h>
#include <TH1D.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "TRestDataSetProcess.h"
#include "TRestSystemOfUnits.h"

/// \class TRestDataSetCalibrationPeak
/// \brief A peak of a calibration (TRestDataSetCalibration, TRestDataSetGainMap): its energy and
/// the range of the observable where it is fitted.
class TRestDataSetCalibrationPeak : public TRestMetadata {
   public:
    /// Energy of the peak (keV by default, e.g. "5.9 keV")
    TRestWithUnits fEnergy = 0;
    /// Range of the observable where the peak is fitted; automatic if the first value is not lower than the
    /// second (default)
    std::pair<double, double> fRange = {0, 0};
    /// Result (TRestDataSetCalibration): mean of the gaussian fit, in units of the observable
    double fPosition = 0;
    /// Result (TRestDataSetCalibration): FWHM of the gaussian fit relative to its mean, in %
    double fFWHM = 0;
    /// Result (TRestDataSetCalibration): range of the observable where the peak was fitted
    std::pair<double, double> fFitRange = {0, 0};
    /// Result (TRestDataSetCalibration): status of the fit (0: converged)
    int fFitStatus = -1;
    /// Result (TRestDataSetCalibration): free parameters of the fit, name: [value, error], depending on
    /// peakModel and background (TRestPeakFit::Peak::parameters). A YAML map: a std::map member here
    /// crashes rootcling (ROOT 6.36) when the detector dictionary is generated
    YAML::Node fFitParameters;

    TRestDataSetCalibrationPeak();
    TRestDataSetCalibrationPeak(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestDataSetCalibrationPeak"; }

    bool HasRange() const { return fRange.first < fRange.second; }
};

/// \class TRestDataSetCalibration
/// \brief Energy calibration of a dataset: fits the peaks of known energy in the spectrum of an observable
/// of the reference and adds the calibrated energy, a linear function of the observable. See the class
/// documentation in TRestDataSetCalibration.cxx.
class TRestDataSetCalibration : public TRestDataSetProcess {
   private:
    /// Spectrum of the observable in the reference, with the peak fits
    std::unique_ptr<TH1D> fSpectrum;  //!
    /// Energy against the position of the peaks, with the linear fit
    std::unique_ptr<TGraph> fLinearFit;  //!

   protected:
    /// \brief Fits the peaks of the spectrum of the observable in the reference and the calibration line:
    /// the results peaks (position, fwhm, fit...), slope and intercept; the spectrum and linearFit kept
    void ComputeParameters(const TRestDataSet& reference, ROOT::RDF::RNode frame) override;
    /// \brief Reads the spectrum and linearFit of a stored calibration
    void ReadObjects(TDirectory* directory) override;

   public:
    /// Observable calibrated
    std::string fObservable = "";
    /// Name of the calibrated observable: the column <name>_<calibratedObservable> (calib_Energy as legacy)
    std::string fCalibratedObservable = "Energy";
    /// Peaks of known energy; the first one should be the maximum of the spectrum
    std::vector<TRestDataSetCalibrationPeak> fPeaks;
    /// Number of bins of the spectrum
    int fNBins = 1024;
    /// Range of the observable of the spectrum
    std::pair<double, double> fRange = {0, 0};
    /// Add the point (0, 0) to the linear fit (as legacy)
    bool fZeroPoint = true;
    /// Shape of the peaks: "gaussian" (as legacy) or "crystalBall" (TRestPeakFit)
    std::string fPeakModel = "gaussian";
    /// Background under the peaks: "none" (as legacy), "constant" or "linear" (TRestPeakFit)
    std::string fBackground = "none";
    /// Half width of the fit window of a peak without range, relative to its estimated position (as legacy)
    double fWindow = 0.15;
    /// Search each peak in its range (or estimated window) and fit it around the peak found
    /// (TRestPeakFit::FindPeak)
    bool fPeakSearch = false;
    /// Results: energy = slope * observable + intercept (keV)
    double fSlope = 0;
    double fIntercept = 0;

    TRestDataSetCalibration();
    TRestDataSetCalibration(const std::string& instanceName, const YAML::Node& node);
    ~TRestDataSetCalibration() override;

    std::string GetClassName() const override { return "TRestDataSetCalibration"; }
    void LoadConfig() override;

    /// \brief Adds the column GetEnergyColumn() = slope * observable + intercept (keV)
    void Apply(TRestDataSet& dataSet) const override;
    /// \brief Writes the spectrum (with the peak fits) and linearFit
    void WriteObjects(TDirectory* directory) const override;

    /// \brief Name of the calibrated energy column, <name>_<calibratedObservable>
    std::string GetEnergyColumn() const { return fName + "_" + fCalibratedObservable; }
    /// \brief Spectrum of the observable in the reference, with the peak fits (nullptr before the
    /// parameters are obtained)
    TH1D* GetSpectrum() const { return fSpectrum.get(); }
    /// \brief Energy against the position of the peaks, with the linear fit (nullptr before the parameters
    /// are obtained)
    TGraph* GetLinearFit() const { return fLinearFit.get(); }
};

#endif
