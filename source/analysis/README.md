# REST analysis (source/analysis)

`libRestAnalysis` holds the generic analysis algorithms of REST, written as free functions on standard
containers (`std::vector`, `ROOT::Math::XYZVector`, `TGraph`, `TH1`) so that any event type or library can
use them, and the dataset processes built on them. It depends on `RestCore` and `RestTools`.

## Contents

### Algorithms (namespaces)

- `TRestPulseShapeAnalysis` (TRestPulseShapeAnalysis.h): signal parameters of pulses: baseline and its
  sigma (mean/SD, median/IQR or without outliers), maxima (point, triple max, gaussian, Landau or AGET fits),
  integrals, points over threshold, rise time and slope, smoothing, peak finding (GetPeaks), shaping
  responses and convolution, and generated noise.
- `TRestHitsAnalysis` (TRestHitsAnalysis.h): sets of hits or track nodes: clustering (FindClusters), shortest
  path through the points (GetShortestPath), reduction to nodes (KMeansClustering, MergePoints), line fit and
  residual (GetLine, GetLineNodes, GetLineResidual), track ends (GetTrackEnds), gaussian sigma and twist.
- `TRestPeakFit` (TRestPeakFit.h): RooFit fits of the peaks of a spectrum (gaussian or Crystal Ball, with an
  optional constant or linear background), peak search and fit windows, and the linear calibration fit.
- `TRestFFTAnalysis` (TRestFFTAnalysis.h): FFT helpers on `std::vector` (forward/backward transforms through
  ROOT/FFTW), response spectra, low-pass filters and deconvolution. The in-place filters keep the
  Hermitian symmetry of the spectrum (`ApplyLowPassFilter` and `KillFrequencies` zero the same bins and
  their mirrors), `RemoveBaseline` sets the DC bin to 0, `RenormalizeNode` multiplies a bin by the factor
  and `ButterWorthFilter` throws `std::invalid_argument` for a cutOff of 0.

### Dataset processes (TRestDataSetProcess, run by TRestDataSetProcessManager)

- `TRestDataSetCalibration`: energy calibration of a dataset: fits the peaks of known energy of an observable
  in a reference dataset and adds the column `<name>_<calibratedObservable>` (linear in the observable).
  Its peaks are `TRestDataSetCalibrationPeak`.
- `TRestDataSetLogOdds`: builds the PDF of observables in a reference dataset and adds their log odds
  `<name>_<observable>` and their sum `<name>_total`. Its observables are `TRestDataSetLogOddsObservable`.
- `TRestDataSetTMVA`: trains a classifier of ROOT TMVA (a BDT by default) with the reference as signal and a
  background dataset, and adds its score `<name>_score`; the trained weights, the ROC integral, the
  separations and the overtraining tests are its results, the ROC curve and the score distributions its
  objects, drawn by `DrawResults` (macro `REST_ViewTMVA` of restRoot).
- `TRestDataSetSignalSelection`: keeps the entries with an observable (e.g. `logOdds_total`, `bdt_score`)
  in the range holding a quantile interval of the reference (`quantiles`, e.g. [0, 0.8] or [0.2, 1]):
  fixes a signal efficiency on a calibration and applies it to a background (filter).
- `TRestDataSetCombine`: adds the columns of another dataset of the same events (`combineFileName`, e.g.
  the vetoes analysed apart, to the dataset of the Micromegas), matched by run, eventID and subEventID; the
  entries without match are kept with default values and `<name>_matched` 0 (`keepUnmatched`, true by
  default) or filtered out.

All are configured in YAML with their registered fields (see the class documentation) and store their
results (fit parameters, PDFs) in their configuration, so a dataset processed with them can be used as the
reference of later runs.

## Use in the framework

- `TRestPulseShapeAnalysis`: the raw and detector signal processes and events (`libraries/raw`,
  `libraries/detector`), e.g. TRestRawSignalAnalysisProcess, TRestRawSignalShapingProcess,
  TRestRawBaseLineCorrectionProcess, TRestDetectorSignalToHitsProcess.
- `TRestHitsAnalysis`: the hits and track processes (`libraries/detector`, `libraries/track`), e.g.
  TRestDetectorHitsAnalysisProcess, TRestTrackReductionProcess, TRestTrackLineAnalysisProcess.
- `TRestPeakFit`: TRestDataSetCalibration and TRestDataSetGainMap (`libraries/detector`).
- `TRestFFTAnalysis`: available for signal processing; not used by the processes yet.

## Examples

- `libraries/detector/examples/DataSetCalibration.yaml`: calibration of datasets with TRestDataSetCalibration.
- `examples/DataSetLogOdds.yaml`: log odds of calibrated datasets with TRestDataSetLogOdds.
- `examples/DataSetTMVA.yaml`: BDT of calibrated datasets with TRestDataSetTMVA.
- `examples/DataSetSignalSelection.yaml`: selection of the entries with the BDT score of the 80% of the
  calibration with TRestDataSetSignalSelection.
- `examples/DataSetTardis.yaml`: time since the previous event with TRestDataSetTardis (libraries/detector).
