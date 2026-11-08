#ifndef TRESTDATASETGAINMAP_H
#define TRESTDATASETGAINMAP_H

#include <TGraph.h>
#include <TH1D.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "TRestDataSetCalibration.h"
#include "TRestDataSetProcess.h"

class TCanvas;
class TRestDetectorReadout;

/// \class TRestDataSetGainMap
/// \brief Gain map of a readout: calibrates each segment of the readout to correct its gain
/// inhomogeneities. See the class documentation in TRestDataSetGainMap.cxx.
class TRestDataSetGainMap : public TRestDataSetProcess {
   private:
    /// Spectrum of each segment [x][y] and of the whole readout, with the peak fits g0, g1...
    std::vector<std::vector<std::unique_ptr<TH1D>>> fSegmentSpectra;  //!
    std::unique_ptr<TH1D> fFullSpectrum;                              //!
    /// Energy against the position of the peaks of each segment [x][y] and of the whole readout, with the
    /// linear fit
    std::vector<std::vector<std::unique_ptr<TGraph>>> fSegmentLinearFits;  //!
    std::unique_ptr<TGraph> fFullLinearFit;                                //!

    /// \brief The readout `readout` of the TRestDetectorReadoutManager of a dataset (its data readout if
    /// it is empty, TRestDetectorReadoutManager::SelectReadout); throws if it is not found
    const TRestDetectorReadout& FindReadout(const TRestDataSet& dataSet) const;
    /// \brief Fits the peaks of a spectrum (see TRestDataSetGainMap.cxx) and the line of the calibration:
    /// (intercept, slope). The graph gets the points and the linear fit.
    std::pair<double, double> FitPeaks(TH1D& spectrum, TGraph& graph) const;
    /// \brief The calibration line from the peak fits g0, g1... kept in the spectrum (after a refit):
    /// (intercept, slope)
    std::pair<double, double> UpdateLinearFit(TH1D& spectrum, TGraph& graph) const;

   protected:
    /// \brief Divides the extent of the readout of the reference in segments (splitX, splitY, unless
    /// given) and fits the peaks and the calibration line of the spectrum of each segment and of the whole
    /// readout: the results slope and intercept [x][y], fullSlope and fullIntercept; the spectra and linear
    /// fits kept
    void ComputeParameters(const TRestDataSet& reference, ROOT::RDF::RNode frame) override;
    /// \brief Reads the spectra and linear fits of a stored gain map
    void ReadObjects(TDirectory* directory) override;

   public:
    /// Observable calibrated
    std::string fObservable = "";
    /// Name of the calibrated observable: the columns <name>_<calibratedObservable> and
    /// <name>_<calibratedObservable>NoSegmentation (as TRestDataSetCalibration)
    std::string fCalibratedObservable = "Energy";
    /// Observables of the position, in X and Y, of the segmentation (in the frame of the readout)
    std::string fSpatialObservableX = "";
    /// Observable of the position in Y of the segmentation
    std::string fSpatialObservableY = "";
    /// Secondary observables of the position (optional): an event of a segment of the fit must also have
    /// them in the segment
    std::string fSpatialObservableXSecondary = "";
    /// Secondary observable of the position in Y (optional)
    std::string fSpatialObservableYSecondary = "";
    /// Readout (instance of the TRestDetectorReadoutManager of the reference dataset) whose extent is
    /// segmented; empty: the data readout of the manager
    std::string fReadout = "";
    /// Peaks of known energy, in decreasing energy
    std::vector<TRestDataSetCalibrationPeak> fPeaks;
    /// Range of the observable of the spectra; automatic if the first value is not lower than the second
    std::pair<double, double> fRange = {0, 0};
    /// Number of bins of the spectra
    int fNBins = 100;
    /// Number of segments in X and Y, of equal size in the extent of the readout
    std::pair<int, int> fSegments = {1, 1};
    /// Edges of the segments in X and Y: given, or from segments and the extent of the readout (results)
    std::vector<double> fSplitX = {};
    /// Edges of the segments in Y
    std::vector<double> fSplitY = {};
    /// Add the point (0, 0) to the linear fits
    bool fZeroPoint = false;
    /// Shape of the peaks: "gaussian" (as legacy) or "crystalBall" (TRestPeakFit)
    std::string fPeakModel = "gaussian";
    /// Background under the peaks: "none" (as legacy), "constant" or "linear" (TRestPeakFit)
    std::string fBackground = "none";
    /// Half width of the fit window of a peak without range, relative to its estimated position
    double fWindow = 0.2;
    /// Search each peak in its range (or estimated window) and fit it around the peak found
    /// (TRestPeakFit::FindPeak)
    bool fPeakSearch = false;
    /// Results: calibration of each segment [x][y], energy = slope * observable + intercept
    std::vector<std::vector<double>> fSlope = {};
    /// Results: intercept of the calibration of each segment [x][y]
    std::vector<std::vector<double>> fIntercept = {};
    /// Results: calibration of the whole readout
    double fFullSlope = 0;
    /// Results: intercept of the calibration of the whole readout
    double fFullIntercept = 0;

    /// \brief Builds an empty gain map.
    TRestDataSetGainMap();
    /// \brief Builds the gain map from its YAML section (LoadConfig).
    TRestDataSetGainMap(const std::string& instanceName, const YAML::Node& node);

    /// \brief Returns the class name, "TRestDataSetGainMap".
    std::string GetClassName() const override { return "TRestDataSetGainMap"; }
    /// \brief Reads and checks the parameters: throws without observable, spatial observables or peaks, or
    /// with invalid nBins, window, splitX, splitY or segments (given edges set the number of segments).
    void LoadConfig() override;
    /// \brief Prints the parameters, the slope of each segment and the calibration of the whole readout.
    void PrintMetadata() const override;

    /// \brief Adds the columns GetEnergyColumn() (calibrated with the segment of the event) and
    /// GetEnergyNoSegmentationColumn() (with the whole readout)
    void Apply(TRestDataSet& dataSet) const override;
    /// \brief Writes the spectra (with the peak fits) and linear fits of the segments and the whole readout
    void WriteObjects(TDirectory* directory) const override;

    /// \brief Names of the columns: <name>_<calibratedObservable> (calibration of the segment) and
    /// <name>_<calibratedObservable>NoSegmentation (of the whole readout)
    std::string GetEnergyColumn() const { return fName + "_" + fCalibratedObservable; }
    /// \brief Name of the column calibrated with the whole readout.
    std::string GetEnergyNoSegmentationColumn() const { return GetEnergyColumn() + "NoSegmentation"; }

    /// \brief Number of segments in X (from the edges splitX).
    int GetNumberOfSegmentsX() const { return static_cast<int>(fSplitX.size()) - 1; }
    /// \brief Number of segments in Y (from the edges splitY).
    int GetNumberOfSegmentsY() const { return static_cast<int>(fSplitY.size()) - 1; }
    /// \brief Indices [x][y] of the segment of a position (the first or last segment outside the edges)
    std::pair<int, int> GetSegment(double x, double y) const;
    /// \brief Calibration of the segment of a position: energy = slope * observable + intercept
    double GetSlope(double x, double y) const;
    /// \brief Intercept of the calibration of the segment of a position.
    double GetIntercept(double x, double y) const;

    /// \brief Fits again a peak (index in peaks) of a segment in a range of the observable and updates its
    /// calibration
    void Refit(int x, int y, size_t peak, const std::pair<double, double>& range);
    /// \brief Refit of the segment of a position and of the peak of an energy (keV)
    void RefitAt(double x, double y, double energy, const std::pair<double, double>& range);
    /// \brief Fits again a peak of the whole readout and updates its calibration
    void RefitFull(size_t peak, const std::pair<double, double>& range);

    /// \brief Spectra of the segments in a canvas divided as the readout (bottom left: lowest x and y); a
    /// canvas already divided can be given to draw several gain maps. Color < 0: not changed.
    void DrawSpectrum(bool drawFits = true, int color = -1, TCanvas* canvas = nullptr) const;
    /// \brief Spectrum of a segment in the current pad
    void DrawSegmentSpectrum(int x, int y, bool drawFits = true, int color = -1) const;
    /// \brief Spectrum of the whole readout
    void DrawFullSpectrum(bool drawFits = true, int color = -1, TCanvas* canvas = nullptr) const;
    /// \brief Linear fits of the segments in a canvas divided as the readout
    void DrawLinearFit(TCanvas* canvas = nullptr) const;
    /// \brief Linear fit of a segment in the current pad
    void DrawSegmentLinearFit(int x, int y) const;
    /// \brief Relative gain of each segment for a peak (index in peaks): its position over the one of the
    /// whole readout, or of the central segment
    void DrawGainMap(size_t peak = 0, bool fullReadoutAsReference = true, bool showText = true) const;

    /// \brief Spectra and linear fits of a segment [x][y] and of the whole readout (nullptr before the
    /// parameters are obtained)
    TH1D* GetSegmentSpectrum(int x, int y) const;
    /// \brief Spectrum of the whole readout (nullptr before the parameters are obtained).
    TH1D* GetFullSpectrum() const { return fFullSpectrum.get(); }
    /// \brief Linear fit of a segment [x][y] (nullptr before the parameters are obtained).
    TGraph* GetSegmentLinearFit(int x, int y) const;
    /// \brief Linear fit of the whole readout (nullptr before the parameters are obtained).
    TGraph* GetFullLinearFit() const { return fFullLinearFit.get(); }
};

#endif
