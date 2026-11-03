#include "TRestDataSetGainMap.h"

#include <TCanvas.h>
#include <TDirectory.h>
#include <TF1.h>
#include <TH2D.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <tuple>

#include "TRestDataSet.h"
#include "TRestDetectorReadout.h"
#include "TRestDetectorReadoutManager.h"
#include "TRestPeakFit.h"
#include "TRestTools.h"

/// \class TRestDataSetGainMap
///
/// Gain map of a readout, a process of TRestDataSetProcessManager: corrects the gain inhomogeneities of
/// big readouts (e.g. TREX-DM) by calibrating each segment. The extent of the readout in X and Y (the
/// bounding box of its channels, TRestDetectorReadout::GetChannelsBoundingBox) is divided in `segments`,
/// and the events of a segment are those with the spatial observables inside it. The readout is the one
/// of the reference dataset (its TRestDetectorReadoutManager, TRestDataSet::GetMetadataOfType). In the
/// reference (referenceFileName with the cuts, see TRestDataSetProcess), the peaks of known energy are
/// fitted in the spectrum of `observable` of each segment and of the whole readout, and the energy against
/// their position is fitted with a line. The columns added, for all the events, are:
/// * `<name>_<calibratedObservable>` (e.g. gainMap_Energy): calibrated with the segment of the event,
///   slope * observable + intercept (keV);
/// * `<name>_<calibratedObservable>NoSegmentation`: calibrated with the whole readout.
///
/// A detector with several readouts (e.g. the two sides of TREX-DM) has a gain map per readout, each one
/// on its own dataset (generated with the cuts of the events of that readout).
///
/// The fit window of each peak is its `range` if it is given; otherwise ±window (20%) of its position
/// estimated from the maximum of the spectrum, which should be the first peak (position = energy *
/// maximum / first energy). The fits are made with TRestPeakFit (RooFit): a gaussian (as legacy) or a
/// Crystal Ball, optionally with a constant or linear background under the peak.
///
/// ### Parameters
/// * observable: the observable calibrated.
/// * calibratedObservable: name of the calibrated observable in the columns ("Energy", as
///   TRestDataSetCalibration).
/// * spatialObservableX, spatialObservableY: observables of the position of the segmentation, in the frame
///   of the readout.
/// * spatialObservableXSecondary, spatialObservableYSecondary (optional): an event of a segment of the
///   fit must also have them inside the segment.
/// * readout: the readout (instance in the TRestDetectorReadoutManager of the reference); empty: the data
///   readout of the manager (its `readout`, or the only one).
/// * peaks: `energy` (keV by default) and optionally `range` of each peak; the first one should be the
///   maximum of the spectrum.
/// * range: range of the observable of the spectra; automatic if not given (up to where the last 1% of the
///   spectrum starts, as legacy).
/// * nBins: number of bins of the spectra (100).
/// * segments: number of segments in X and Y ([1, 1]), of equal size in the extent of the readout.
/// * splitX, splitY: edges of the segments (instead of segments and the readout), e.g. [0, 50, 246].
/// * zeroPoint: add the point (0, 0) to the linear fits (false; added anyway if there is only one peak).
/// * peakModel: "gaussian" (default, as legacy) or "crystalBall" (low energy tail).
/// * background: "none" (default, as legacy), "constant" or "linear" under each peak.
/// * window: half width of the fit window of the peaks without range, relative to their estimated
///   position (0.2).
/// * peakSearch: search each peak in its range (or its estimated window) and fit it in ±window around the
///   peak found, inside the range (false); useful when the gain of the segments differs. The search
///   (TRestPeakFit::FindPeak) only accepts significant peaks (prominence over 3 times the statistical
///   fluctuation, smoothed spectrum): if there is none, the peak is fitted in the whole range.
/// * referenceFileName, cuts: see TRestDataSetProcess.
///
/// ### Results
/// splitX, splitY (edges of the segments), slope and intercept of each segment ([x][y]), fullSlope and
/// fullIntercept of the whole readout; referenceUsed and parametersFileName (see TRestDataSetProcess).
///
/// Objects (folder RESTDataSetStore/TRestDataSetGainMap/<name> of the output dataset): `spectrum` and
/// `spectrum_<x>_<y>` (with the peak fits g0, g1..., drawn with them), `linearFit` and `linearFit_<x>_<y>`.
///
/// ### Review and refit (restRoot)
/// The gain map of a dataset processed by it is loaded with ReadFromFile; after a refit, UpdateFile saves
/// it in the dataset (its configuration and objects; the columns are not recomputed: run again the pipeline
/// with this dataset as referenceFileName, the gain map is read from it):
///
///    TRestDataSetGainMap gainMap;
///    gainMap.ReadFromFile("Dataset_Calibration_Calibrated.root", "gainMap");
///    gainMap.DrawSpectrum();
///    gainMap.DrawGainMap();
///    gainMap.Refit(0, 0, 0, {100000, 200000});  // segment [0][0], first peak, new range
///    gainMap.DrawSegmentSpectrum(0, 0);
///    gainMap.UpdateFile("Dataset_Calibration_Calibrated.root");
///
/// Example (in a TRestDataSetProcessManager):
///
///    gainMap:
///      class: "TRestDataSetGainMap"
///      referenceFileName: "Dataset_Calibration.root"
///      observable: "rawAnalysis_ThresholdIntegral"
///      spatialObservableX: "hitsAnalysis_MeanX"
///      spatialObservableY: "hitsAnalysis_MeanY"
///      segments: [5, 5]
///      peaks:
///        - {energy: "22.5 keV", range: [250000, 370000]}
///        - {energy: "8 keV"}
///
/// Differences with legacy (TRestDataSetGainMap): a process of a dataset pipeline (no GenerateGainMap,
/// CalibrateDataSet, Import/Export nor calibFileName/outputFileName: referenceFileName, ReadFromFile and
/// UpdateFile); the input is a dataset (not run files); a single readout, without modules (no planeId,
/// moduleId nor definitionCut: a gain map per readout, each one on its dataset), whose extent comes from
/// the readout of the dataset (legacy: readoutRange, default [-1, 246.24]); the columns are
/// `<name>_<calibratedObservable>` and `<name>_<calibratedObservable>NoSegmentation` (legacy:
/// `<name>_<observable>`, `_NoSegmentation` and `<name>_pmID`);
/// the segment of the column is clamped silently to the first or last segment (legacy warned for every
/// event); the zero point is kept in the refits; the gain map is drawn with the edges of the segments; no
/// REST_RefitGainMap macro nor excludeColumns (all the columns are written); no peak search with TSpectrum
/// (a legacy ROOT interface) nor autoRangePeaks: windows from the maximum of the spectrum (the first peak
/// instead of the highest energy) or the ranges, and the peaks fitted with RooFit (TRestPeakFit), optionally
/// with a Crystal Ball and a background.

static const bool TRestDataSetGainMap_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using G = TRestDataSetGainMap;
    reg.RegisterField<G>("observable", &G::fObservable);
    reg.RegisterField<G>("calibratedObservable", &G::fCalibratedObservable);
    reg.RegisterField<G>("spatialObservableX", &G::fSpatialObservableX);
    reg.RegisterField<G>("spatialObservableY", &G::fSpatialObservableY);
    reg.RegisterField<G>("spatialObservableXSecondary", &G::fSpatialObservableXSecondary);
    reg.RegisterField<G>("spatialObservableYSecondary", &G::fSpatialObservableYSecondary);
    reg.RegisterField<G>("readout", &G::fReadout);
    reg.RegisterField<G>("peaks", &G::fPeaks);
    reg.RegisterField<G>("range", &G::fRange);
    reg.RegisterField<G>("nBins", &G::fNBins);
    reg.RegisterField<G>("segments", &G::fSegments);
    reg.RegisterField<G>("splitX", &G::fSplitX);
    reg.RegisterField<G>("splitY", &G::fSplitY);
    reg.RegisterField<G>("zeroPoint", &G::fZeroPoint);
    reg.RegisterField<G>("peakModel", &G::fPeakModel);
    reg.RegisterField<G>("background", &G::fBackground);
    reg.RegisterField<G>("window", &G::fWindow);
    reg.RegisterField<G>("peakSearch", &G::fPeakSearch);
    reg.RegisterField<G>("slope", &G::fSlope);
    reg.RegisterField<G>("intercept", &G::fIntercept);
    reg.RegisterField<G>("fullSlope", &G::fFullSlope);
    reg.RegisterField<G>("fullIntercept", &G::fFullIntercept);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDataSetGainMap", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDataSetGainMap>(instanceName, params);
        });
    return true;
}();

/// Index of the segment of a value in the edges `split`: the first or last segment outside them
int SegmentIndex(const std::vector<double>& split, double value) {
    const int last = static_cast<int>(split.size()) - 2;
    if (last < 0) return 0;
    const auto upper = std::upper_bound(split.begin(), split.end(), value);
    if (upper == split.end()) return last;
    return std::max(static_cast<int>(std::distance(split.begin(), upper)) - 1, 0);
}

/// Edges of `segments` segments of equal size in [first, last]
std::vector<double> EqualSplit(int segments, double first, double last) {
    std::vector<double> split;
    for (int i = 0; i <= segments; i++) split.push_back(first + (last - first) / segments * i);
    return split;
}

std::string FitName(size_t peak) { return "g" + std::to_string(peak); }
std::string SegmentName(const std::string& prefix, int x, int y) {
    return prefix + "_" + std::to_string(x) + "_" + std::to_string(y);
}

/// Canvas for the segments: the one given (divided as the readout, or not divided) or a new one
TCanvas* SegmentsCanvas(TCanvas* canvas, const std::string& name, int nX, int nY) {
    if (!canvas) canvas = new TCanvas(name.c_str(), name.c_str());
    int pads = 0;
    for (const auto* object : *canvas->GetListOfPrimitives())
        if (object->InheritsFrom(TVirtualPad::Class())) ++pads;
    if (pads == 0) {
        canvas->Divide(nX, nY);
    } else if (pads != nX * nY) {
        throw std::runtime_error("TRestDataSetGainMap: the canvas " + std::string(canvas->GetName()) +
                                 " has " + std::to_string(pads) + " pads, " + std::to_string(nX * nY) +
                                 " needed");
    }
    return canvas;
}

/// Pad of the segment [x][y]: bottom left is the lowest x and y
int SegmentPad(int x, int y, int nX, int nY) { return nX * (nY - 1 - y) + x + 1; }

/// The peak fits are drawn with the spectrum (its functions): shown or hidden, in basic colors 2, 3...
/// skipping the one of the spectrum
void SetFitsDrawn(const TH1D& spectrum, size_t nPeaks, bool drawn) {
    const int spectrumColor = spectrum.GetLineColor();
    for (size_t i = 0; i < nPeaks; i++) {
        auto* fit = spectrum.GetFunction(FitName(i).c_str());
        if (!fit) continue;
        int color = static_cast<int>(i) + 2;
        if (color >= spectrumColor) color++;
        fit->SetLineColor(color);
        fit->SetBit(TF1::kNotDraw, !drawn);
    }
}
}  // namespace

TRestDataSetGainMap::TRestDataSetGainMap() : TRestDataSetProcess() { fName = "TRestDataSetGainMap"; }

TRestDataSetGainMap::TRestDataSetGainMap(const std::string& instanceName, const YAML::Node& node)
    : TRestDataSetProcess(instanceName, node) {
    LoadConfig();
}

void TRestDataSetGainMap::LoadConfig() {
    UpdateParamsFromYAML<TRestDataSetGainMap>(fNode);
    const std::string context = "TRestDataSetGainMap '" + fName + "'";
    if (fObservable.empty()) throw std::runtime_error(context + ": no observable");
    if (fCalibratedObservable.empty()) throw std::runtime_error(context + ": no calibratedObservable");
    if (fSpatialObservableX.empty() || fSpatialObservableY.empty())
        throw std::runtime_error(context + ": spatialObservableX and spatialObservableY are needed");
    if (fPeaks.empty()) throw std::runtime_error(context + ": no peaks");
    if (fNBins <= 0) throw std::runtime_error(context + ": nBins must be positive");
    if (fWindow <= 0 || fWindow >= 1) throw std::runtime_error(context + ": window must be in (0, 1)");
    // Checked here (throw for an unknown name)
    TRestPeakFit::ModelFromString(fPeakModel);
    TRestPeakFit::BackgroundFromString(fBackground);
    for (const auto* split : {&fSplitX, &fSplitY}) {
        if (split->empty()) continue;
        if (split->size() < 2 || !std::is_sorted(split->begin(), split->end()) ||
            std::adjacent_find(split->begin(), split->end()) != split->end()) {
            throw std::runtime_error(context + ": splitX and splitY need at least 2 increasing edges");
        }
    }
    // Given edges set the number of segments
    if (!fSplitX.empty()) fSegments.first = static_cast<int>(fSplitX.size()) - 1;
    if (!fSplitY.empty()) fSegments.second = static_cast<int>(fSplitY.size()) - 1;
    if (fSegments.first < 1 || fSegments.second < 1)
        throw std::runtime_error(context + ": segments must be at least [1, 1]");
    UpdateYAMLFromParams<TRestDataSetGainMap>(fNode);
}

void TRestDataSetGainMap::PrintMetadata() const {
    TRestMetadata::PrintMetadata();
    RESTMetadata << "Slope [x][y] (bottom row y = 0):" << RESTendl;
    const int nX = static_cast<int>(fSlope.size());
    const int nY = nX > 0 ? static_cast<int>(fSlope[0].size()) : 0;
    for (int j = nY - 1; j >= 0; j--) {
        std::string row;
        for (int i = 0; i < nX; i++) row += TRestTools::Format("%11.3e", fSlope[i][j]);
        RESTMetadata << row << RESTendl;
    }
    RESTMetadata << "Full slope " << fFullSlope << ", full intercept " << fFullIntercept << RESTendl;
}

const TRestDetectorReadout& TRestDataSetGainMap::FindReadout(const TRestDataSet& dataSet) const {
    const auto managers = dataSet.GetMetadataOfType<TRestDetectorReadoutManager>();
    const std::string user = "TRestDataSetGainMap '" + fName + "'";
    if (managers.size() != 1) {
        throw std::runtime_error(user + ": the dataset " + dataSet.fInputFileName + " has " +
                                 std::to_string(managers.size()) +
                                 " TRestDetectorReadoutManager (one expected); give splitX and splitY");
    }
    std::string name = fReadout;
    return *managers.front()->SelectReadout(name, user);
}

std::pair<int, int> TRestDataSetGainMap::GetSegment(double x, double y) const {
    return {SegmentIndex(fSplitX, x), SegmentIndex(fSplitY, y)};
}

double TRestDataSetGainMap::GetSlope(double x, double y) const {
    const auto [i, j] = GetSegment(x, y);
    if (i >= static_cast<int>(fSlope.size()) || j >= static_cast<int>(fSlope[i].size())) return 0;
    return fSlope[i][j];
}

double TRestDataSetGainMap::GetIntercept(double x, double y) const {
    const auto [i, j] = GetSegment(x, y);
    if (i >= static_cast<int>(fIntercept.size()) || j >= static_cast<int>(fIntercept[i].size())) return 0;
    return fIntercept[i][j];
}

std::pair<double, double> TRestDataSetGainMap::FitPeaks(TH1D& spectrum, TGraph& graph) const {
    graph.Set(0);
    if (spectrum.Integral() <= 0) {
        RESTError << "TRestDataSetGainMap '" << fName << "': empty spectrum " << spectrum.GetName()
                  << RESTendl;
        return {0, 0};
    }

    // The position of the peaks from the maximum of the spectrum, the first peak
    const double maximumEnergy = fPeaks.front().fEnergy;
    for (size_t c = 0; c < fPeaks.size(); c++) {
        const double energy = fPeaks[c].fEnergy;
        const auto zone = fPeaks[c].HasRange()
                              ? fPeaks[c].fRange
                              : TRestPeakFit::EstimateWindow(spectrum, maximumEnergy, energy, fWindow);
        auto window = zone;
        if (fPeakSearch) {
            if (const auto found = TRestPeakFit::FindPeakWindow(spectrum, zone, fWindow)) {
                window = *found;
            } else {
                RESTWarning << "TRestDataSetGainMap '" << fName << "': no peak found for " << energy << " keV"
                            << " of " << spectrum.GetName() << " in (" << zone.first << ", " << zone.second
                            << "), fitted in it" << RESTendl;
            }
        }
        const auto [start, end] = window;
        const auto fit =
            TRestPeakFit::FitPeak(spectrum, start, end, FitName(c), TRestPeakFit::ModelFromString(fPeakModel),
                                  TRestPeakFit::BackgroundFromString(fBackground));
        if (!fit.valid) {
            RESTWarning << "TRestDataSetGainMap '" << fName << "': the fit of the peak " << energy
                        << " keV of " << spectrum.GetName() << " in (" << start << ", " << end
                        << ") did not converge" << RESTendl;
        }
        RESTDebug << "  " << spectrum.GetName() << ": peak " << energy << " keV fitted in (" << start << ", "
                  << end << "): " << fit.mean << RESTendl;
        graph.SetPoint(graph.GetN(), fit.mean, energy);
    }
    if (fZeroPoint) graph.SetPoint(graph.GetN(), 0, 0);
    return TRestPeakFit::FitLine(graph);
}

std::pair<double, double> TRestDataSetGainMap::UpdateLinearFit(TH1D& spectrum, TGraph& graph) const {
    graph.Set(0);
    for (size_t i = 0; i < fPeaks.size(); i++) {
        const double mean = TRestPeakFit::GetPeakMean(spectrum, FitName(i));
        if (std::isnan(mean)) {
            RESTWarning << "TRestDataSetGainMap '" << fName << "': no fit " << FitName(i) << " of the peak "
                        << static_cast<double>(fPeaks[i].fEnergy) << " keV in " << spectrum.GetName()
                        << RESTendl;
            continue;
        }
        graph.SetPoint(graph.GetN(), mean, static_cast<double>(fPeaks[i].fEnergy));
    }
    if (fZeroPoint) graph.SetPoint(graph.GetN(), 0, 0);
    return TRestPeakFit::FitLine(graph);
}

void TRestDataSetGainMap::ComputeParameters(const TRestDataSet& reference, ROOT::RDF::RNode frame) {
    // The extent of the readout, divided in segments
    if (fSplitX.empty() || fSplitY.empty()) {
        const auto [minimum, maximum] = FindReadout(reference).GetChannelsBoundingBox();
        if (!std::isfinite(minimum.X()) || !std::isfinite(maximum.X())) {
            throw std::runtime_error("TRestDataSetGainMap '" + fName +
                                     "': the readout has no channels (give splitX and splitY)");
        }
        if (fSplitX.empty()) fSplitX = EqualSplit(fSegments.first, minimum.X(), maximum.X());
        if (fSplitY.empty()) fSplitY = EqualSplit(fSegments.second, minimum.Y(), maximum.Y());
        RESTInfo << "TRestDataSetGainMap '" << fName << "': readout extent x [" << minimum.X() << ", "
                 << maximum.X() << "], y [" << minimum.Y() << ", " << maximum.Y() << "]" << RESTendl;
    }

    // Automatic range: up to where the last 1% of the spectrum starts (as legacy)
    if (!(fRange.first < fRange.second)) {
        auto automatic = frame.Histo1D({"automatic", "", fNBins, 0, 0}, fObservable);
        const TH1D& histogram = *automatic;
        const double integral = histogram.Integral();
        const double xMax = histogram.GetXaxis()->GetXmax();
        double fraction = 1, atEnd = 0;
        const double step = 0.66;
        while (integral > 0 && atEnd / integral < 0.01 && fraction > 0.001) {
            fraction *= step;
            atEnd = histogram.Integral(histogram.FindFixBin(xMax * fraction), histogram.FindFixBin(xMax));
        }
        fRange.second = xMax * fraction / step;
        RESTDebug << "TRestDataSetGainMap '" << fName << "': range (" << fRange.first << ", " << fRange.second
                  << ")" << RESTendl;
    }

    // All the spectra booked before the event loop
    auto full = frame.Histo1D({"spectrum", "", fNBins, fRange.first, fRange.second}, fObservable);
    const int nX = GetNumberOfSegmentsX(), nY = GetNumberOfSegmentsY();
    std::vector<std::vector<ROOT::RDF::RResultPtr<TH1D>>> segments(nX);
    auto inside = [](const std::string& column, double lower, double upper) {
        return TRestTools::Format("%s >= %.17g && %s < %.17g", column.c_str(), lower, column.c_str(), upper);
    };
    for (int i = 0; i < nX; i++) {
        for (int j = 0; j < nY; j++) {
            std::string cut = inside(fSpatialObservableX, fSplitX[i], fSplitX[i + 1]) + " && " +
                              inside(fSpatialObservableY, fSplitY[j], fSplitY[j + 1]);
            if (!fSpatialObservableXSecondary.empty())
                cut += " && " + inside(fSpatialObservableXSecondary, fSplitX[i], fSplitX[i + 1]);
            if (!fSpatialObservableYSecondary.empty())
                cut += " && " + inside(fSpatialObservableYSecondary, fSplitY[j], fSplitY[j + 1]);
            const std::string name = SegmentName("spectrum", i, j);
            segments[i].push_back(frame.Filter(cut).Histo1D(
                {name.c_str(), "", fNBins, fRange.first, fRange.second}, fObservable));
        }
    }

    auto keep = [this](const TH1D& histogram) {
        std::unique_ptr<TH1D> copy(static_cast<TH1D*>(histogram.Clone()));
        copy->SetDirectory(nullptr);
        copy->SetTitle((";" + fObservable + ";Counts").c_str());
        return copy;
    };
    auto newGraph = [this](const std::string& name) {
        auto graph = std::make_unique<TGraph>();
        graph->SetName(name.c_str());
        graph->SetTitle((";" + fObservable + ";Energy (keV)").c_str());
        return graph;
    };

    fFullSpectrum = keep(*full);
    fFullLinearFit = newGraph("linearFit");
    std::tie(fFullIntercept, fFullSlope) = FitPeaks(*fFullSpectrum, *fFullLinearFit);

    fSegmentSpectra.clear();
    fSegmentSpectra.resize(nX);
    fSegmentLinearFits.clear();
    fSegmentLinearFits.resize(nX);
    fSlope.assign(nX, std::vector<double>(nY, 0));
    fIntercept.assign(nX, std::vector<double>(nY, 0));
    for (int i = 0; i < nX; i++) {
        for (int j = 0; j < nY; j++) {
            fSegmentSpectra[i].push_back(keep(*segments[i][j]));
            fSegmentLinearFits[i].push_back(newGraph(SegmentName("linearFit", i, j)));
            std::tie(fIntercept[i][j], fSlope[i][j]) =
                FitPeaks(*fSegmentSpectra[i][j], *fSegmentLinearFits[i][j]);
        }
    }
    UpdateYAMLFromParams<TRestDataSetGainMap>(fNode);
}

void TRestDataSetGainMap::Apply(TRestDataSet& dataSet) const {
    ROOT::RDF::RNode frame = dataSet.GetDataFrame();

    // The calibration copied: the data frame can be used after this process
    const std::vector<double> splitX = fSplitX, splitY = fSplitY;
    const std::vector<std::vector<double>> slope = fSlope, intercept = fIntercept;
    const std::string observable = AsDouble(frame, fObservable);
    const std::string x = AsDouble(frame, fSpatialObservableX);
    const std::string y = AsDouble(frame, fSpatialObservableY);
    auto calibrate = [splitX, splitY, slope, intercept](double value, double posX, double posY) {
        const size_t i = SegmentIndex(splitX, posX), j = SegmentIndex(splitY, posY);
        if (i >= slope.size() || j >= slope[i].size()) return std::numeric_limits<double>::quiet_NaN();
        return slope[i][j] * value + intercept[i][j];
    };
    frame = DefineColumn(frame, GetEnergyColumn(), calibrate, {observable, x, y});
    frame = DefineColumn(
        frame, GetEnergyNoSegmentationColumn(),
        TRestTools::Format("%.17g * (%s) + %.17g", fFullSlope, fObservable.c_str(), fFullIntercept));

    dataSet.SetDataFrame(frame, {GetEnergyColumn(), GetEnergyNoSegmentationColumn()});
}

TH1D* TRestDataSetGainMap::GetSegmentSpectrum(int x, int y) const {
    if (x < 0 || y < 0 || x >= static_cast<int>(fSegmentSpectra.size()) ||
        y >= static_cast<int>(fSegmentSpectra[x].size()))
        return nullptr;
    return fSegmentSpectra[x][y].get();
}

TGraph* TRestDataSetGainMap::GetSegmentLinearFit(int x, int y) const {
    if (x < 0 || y < 0 || x >= static_cast<int>(fSegmentLinearFits.size()) ||
        y >= static_cast<int>(fSegmentLinearFits[x].size()))
        return nullptr;
    return fSegmentLinearFits[x][y].get();
}

void TRestDataSetGainMap::Refit(int x, int y, size_t peak, const std::pair<double, double>& range) {
    TH1D* spectrum = GetSegmentSpectrum(x, y);
    TGraph* graph = GetSegmentLinearFit(x, y);
    if (!spectrum || !graph) {
        RESTError << "TRestDataSetGainMap '" << fName << "': no segment [" << x << "][" << y
                  << "] (fitted or read?)" << RESTendl;
        return;
    }
    if (peak >= fPeaks.size()) {
        RESTError << "TRestDataSetGainMap '" << fName << "': no peak " << peak << RESTendl;
        return;
    }
    TRestPeakFit::FitPeak(*spectrum, range.first, range.second, FitName(peak),
                          TRestPeakFit::ModelFromString(fPeakModel),
                          TRestPeakFit::BackgroundFromString(fBackground));
    std::tie(fIntercept[x][y], fSlope[x][y]) = UpdateLinearFit(*spectrum, *graph);
    UpdateYAMLFromParams<TRestDataSetGainMap>(fNode);
}

void TRestDataSetGainMap::RefitAt(double x, double y, double energy, const std::pair<double, double>& range) {
    for (size_t i = 0; i < fPeaks.size(); i++) {
        if (std::abs(static_cast<double>(fPeaks[i].fEnergy) - energy) <= 1e-9 * std::abs(energy)) {
            const auto [indexX, indexY] = GetSegment(x, y);
            Refit(indexX, indexY, i, range);
            return;
        }
    }
    RESTError << "TRestDataSetGainMap '" << fName << "': no peak of " << energy << " keV" << RESTendl;
}

void TRestDataSetGainMap::RefitFull(size_t peak, const std::pair<double, double>& range) {
    if (!fFullSpectrum || !fFullLinearFit) {
        RESTError << "TRestDataSetGainMap '" << fName << "': no spectrum (fitted or read?)" << RESTendl;
        return;
    }
    if (peak >= fPeaks.size()) {
        RESTError << "TRestDataSetGainMap '" << fName << "': no peak " << peak << RESTendl;
        return;
    }
    TRestPeakFit::FitPeak(*fFullSpectrum, range.first, range.second, FitName(peak),
                          TRestPeakFit::ModelFromString(fPeakModel),
                          TRestPeakFit::BackgroundFromString(fBackground));
    std::tie(fFullIntercept, fFullSlope) = UpdateLinearFit(*fFullSpectrum, *fFullLinearFit);
    UpdateYAMLFromParams<TRestDataSetGainMap>(fNode);
}

void TRestDataSetGainMap::DrawSegmentSpectrum(int x, int y, bool drawFits, int color) const {
    TH1D* spectrum = GetSegmentSpectrum(x, y);
    if (!spectrum) {
        RESTError << "TRestDataSetGainMap '" << fName << "': no segment [" << x << "][" << y << "]"
                  << RESTendl;
        return;
    }
    spectrum->SetTitle(TRestTools::Format("x=[%g, %g) y=[%g, %g);%s;Counts", fSplitX[x], fSplitX[x + 1],
                                          fSplitY[y], fSplitY[y + 1], fObservable.c_str())
                           .c_str());
    if (color >= 0) spectrum->SetLineColor(color);
    SetFitsDrawn(*spectrum, fPeaks.size(), drawFits);
    spectrum->Draw("same");
}

void TRestDataSetGainMap::DrawSpectrum(bool drawFits, int color, TCanvas* canvas) const {
    const int nX = static_cast<int>(fSegmentSpectra.size());
    if (nX == 0) {
        RESTError << "TRestDataSetGainMap '" << fName << "': no spectra (fitted or read?)" << RESTendl;
        return;
    }
    const int nY = static_cast<int>(fSegmentSpectra[0].size());
    canvas = SegmentsCanvas(canvas, fName + "_spectra", nX, nY);
    for (int i = 0; i < nX; i++) {
        for (int j = 0; j < nY; j++) {
            canvas->cd(SegmentPad(i, j, nX, nY));
            DrawSegmentSpectrum(i, j, drawFits, color);
        }
    }
}

void TRestDataSetGainMap::DrawFullSpectrum(bool drawFits, int color, TCanvas* canvas) const {
    if (!fFullSpectrum) {
        RESTError << "TRestDataSetGainMap '" << fName << "': no spectrum (fitted or read?)" << RESTendl;
        return;
    }
    if (!canvas) canvas = new TCanvas((fName + "_fullSpectrum").c_str(), fName.c_str());
    canvas->cd();
    fFullSpectrum->SetTitle(("Full spectrum;" + fObservable + ";Counts").c_str());
    if (color >= 0) fFullSpectrum->SetLineColor(color);
    SetFitsDrawn(*fFullSpectrum, fPeaks.size(), drawFits);
    fFullSpectrum->Draw("same");
}

void TRestDataSetGainMap::DrawSegmentLinearFit(int x, int y) const {
    TGraph* graph = GetSegmentLinearFit(x, y);
    if (!graph) {
        RESTError << "TRestDataSetGainMap '" << fName << "': no segment [" << x << "][" << y << "]"
                  << RESTendl;
        return;
    }
    graph->SetTitle(TRestTools::Format("Linear fit x=[%g, %g) y=[%g, %g);%s;Energy (keV)", fSplitX[x],
                                       fSplitX[x + 1], fSplitY[y], fSplitY[y + 1], fObservable.c_str())
                        .c_str());
    graph->Draw("AL*");
}

void TRestDataSetGainMap::DrawLinearFit(TCanvas* canvas) const {
    const int nX = static_cast<int>(fSegmentLinearFits.size());
    if (nX == 0) {
        RESTError << "TRestDataSetGainMap '" << fName << "': no linear fits (fitted or read?)" << RESTendl;
        return;
    }
    const int nY = static_cast<int>(fSegmentLinearFits[0].size());
    canvas = SegmentsCanvas(canvas, fName + "_linearFits", nX, nY);
    for (int i = 0; i < nX; i++) {
        for (int j = 0; j < nY; j++) {
            canvas->cd(SegmentPad(i, j, nX, nY));
            DrawSegmentLinearFit(i, j);
        }
    }
}

void TRestDataSetGainMap::DrawGainMap(size_t peak, bool fullReadoutAsReference, bool showText) const {
    if (peak >= fPeaks.size()) {
        RESTError << "TRestDataSetGainMap '" << fName << "': no peak " << peak << RESTendl;
        return;
    }
    const int nX = static_cast<int>(fSegmentSpectra.size());
    if (nX == 0 || !fFullSpectrum) {
        RESTError << "TRestDataSetGainMap '" << fName << "': no spectra (fitted or read?)" << RESTendl;
        return;
    }
    const int nY = static_cast<int>(fSegmentSpectra[0].size());
    // Position of the peak: mean of its fit
    auto position = [&](const TH1D* spectrum) {
        return spectrum ? TRestPeakFit::GetPeakMean(*spectrum, FitName(peak))
                        : std::numeric_limits<double>::quiet_NaN();
    };
    const double reference = fullReadoutAsReference
                                 ? position(fFullSpectrum.get())
                                 : position(GetSegmentSpectrum((nX - 1) / 2, (nY - 1) / 2));

    const std::string name = fName + "_gainMap" + std::to_string(peak);
    auto* canvas = new TCanvas(name.c_str(), name.c_str());
    canvas->cd();
    const std::string title =
        TRestTools::Format("Gain map for energy %g keV;%s;%s", static_cast<double>(fPeaks[peak].fEnergy),
                           fSpatialObservableX.c_str(), fSpatialObservableY.c_str());
    auto* map = new TH2D(("h" + name).c_str(), title.c_str(), nX, fSplitX.data(), nY, fSplitY.data());
    for (int i = 0; i < nX; i++) {
        for (int j = 0; j < nY; j++) {
            map->SetBinContent(i + 1, j + 1, position(GetSegmentSpectrum(i, j)) / reference);
        }
    }
    map->SetStats(false);
    map->Draw("colz");
    map->SetBarOffset(0.2);
    if (showText) map->Draw("TEXT SAME");
}

void TRestDataSetGainMap::WriteObjects(TDirectory* directory) const {
    if (fFullSpectrum) directory->WriteObject(fFullSpectrum.get(), fFullSpectrum->GetName());
    if (fFullLinearFit) directory->WriteObject(fFullLinearFit.get(), fFullLinearFit->GetName());
    for (const auto& column : fSegmentSpectra)
        for (const auto& spectrum : column)
            if (spectrum) directory->WriteObject(spectrum.get(), spectrum->GetName());
    for (const auto& column : fSegmentLinearFits)
        for (const auto& graph : column)
            if (graph) directory->WriteObject(graph.get(), graph->GetName());
}

void TRestDataSetGainMap::ReadObjects(TDirectory* directory) {
    if (!directory) return;
    auto readHistogram = [directory](const std::string& key) -> std::unique_ptr<TH1D> {
        auto* object = directory->Get<TH1D>(key.c_str());
        if (!object) return nullptr;
        std::unique_ptr<TH1D> copy(static_cast<TH1D*>(object->Clone()));
        copy->SetDirectory(nullptr);
        return copy;
    };
    auto readGraph = [directory](const std::string& key) -> std::unique_ptr<TGraph> {
        auto* object = directory->Get<TGraph>(key.c_str());
        return object ? std::unique_ptr<TGraph>(static_cast<TGraph*>(object->Clone())) : nullptr;
    };
    fFullSpectrum = readHistogram("spectrum");
    fFullLinearFit = readGraph("linearFit");
    const int nX = GetNumberOfSegmentsX(), nY = GetNumberOfSegmentsY();
    fSegmentSpectra.clear();
    fSegmentSpectra.resize(std::max(nX, 0));
    fSegmentLinearFits.clear();
    fSegmentLinearFits.resize(std::max(nX, 0));
    for (int i = 0; i < nX; i++) {
        for (int j = 0; j < nY; j++) {
            fSegmentSpectra[i].push_back(readHistogram(SegmentName("spectrum", i, j)));
            fSegmentLinearFits[i].push_back(readGraph(SegmentName("linearFit", i, j)));
        }
    }
}
