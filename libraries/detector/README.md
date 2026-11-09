# The detector library

This library is used to define the detector: the readout topology (Micromegas planes or GDML volumes) that
maps positions to channels, and the medium with its charge and light response. It also implements the
processes of event reconstruction from real detector data (signal shaping, recovery, peak finding, signals
to hits) and of emulation of the physical response of the detector (electron drift and diffusion,
avalanche, smearing, light attenuation, spatial transformations, hits to signals).

The detector library (`RestDetector`) describes the detector of restv2 and the processing of its events in
physical units: the hits (energy deposits, drifted electrons or reconstructed positions) and the signals
of the readout channels, the readouts that map positions to channels, the medium with its charge and
light responses and the transport and gain tables computed by restGarfield, the processes that go from
hits to signals (simulation) and from signals to hits (reconstruction), and the gain map of datasets. The
conversions from and to other libraries (Geant4 events, raw signals, tracks) are in the
[connectors library](../connectors/README.md).

## Events

| Class | Description |
| --- | --- |
| `TRestDetectorHitsEvent` | Event with a flat list of hits: position, energy, time and type (the coordinates each hit defines: XYZ, XZ, YZ...), with the hit analysis of `TRestHits` (branches `fHitX`, `fHitY`, `fHitZ`, `fHitTime`, `fHitEnergy`, `fHitType`). |
| `TRestDetectorSignalEvent` | Event with the signals of the readout channels: (time, amplitude) points in double precision, identified by the DAQ channel (branches `fSigTime`, `fSigAmplitude`, `fSigIDs`, `fSigOffsets`). |
| `TRestDetectorSignal` | Lightweight view of one signal of a `TRestDetectorSignalEvent` (points, graph, sort, addition, scaling). |
| `TRestDetectorSignalData` | Contiguous storage of the points, IDs and offsets shared by the signal views. |

## Readouts

A readout gives the channel of a position and the position of a channel, the distance to the readout
(drift) and the adjacent channels. It is built once from its YAML (geometry and a decoding file of
`daqChannel physicalID` rows) and exported to a ROOT file with a macro; the pipelines import it with a
`TRestDetectorReadoutManager`, which also stores it in the output files.

| Class | Description |
| --- | --- |
| `TRestDetectorReadout` | Abstract base: ROOT `TGeoManager` geometry, decoding physical ID ↔ DAQ ID, position lookup, distance to the readout, export/import to ROOT files, 3D views. |
| `TRestMicromegasReadout` | Micromegas strip plane (X and Y strips of interleaved pixels) with `nChannels`, `pitch`, position, normal along an axis (X, Y or Z; the strips measure the two other axes), rotation about the normal and the drift volume (`gasThickness`, the cathode). |
| `TRestMicromegasReadoutParameters` | The `readoutParameters` of a `TRestMicromegasReadout`. |
| `TRestGDMLReadout` | Readout taken from the GDML geometry of the simulation (e.g. vetoes): channels selected by regular expressions on the volume paths, with an optional readout volume (light guide) for the light attenuation. |
| `TRestGDMLReadoutParameters`, `TRestGDMLReadoutInfo` | The `readoutParameters` of a `TRestGDMLReadout` and each group of channels (`volume`, `readoutVolume`, `offset`, `multiplier`). |
| `TRestFibreReadout` | Scintillator crossed by families of parallel fibres (e.g. LiquidO): box (`size`, `position`), quantum efficiency of the photosensors and, per family, axis, pitch, offset, capture probability versus the distance to the fibre (formula or graph of an optical simulation) and GDML volume; the closest fibre to a position, the light collected by each fibre, and the placements of the fibres for the GDML of the simulation. |
| `TRestFibreReadoutParameters`, `TRestFibreFamily` | The `readoutParameters` of a `TRestFibreReadout` and each family of fibres (`fibres`). |
| `TRestDetectorReadoutManager` | Imports the readouts of a run or dataset from their ROOT files and selects the readout of the data (`readout`); used by the processes and the gain map. |
| `TRestReadoutInfo` | One readout to import: `instanceName`, `decodingName`, `inputFileName`. |

## Medium, responses and tables

| Class | Description |
| --- | --- |
| `TRestDetectorMedium` | A material (gas, liquid or solid) at its pressure and temperature, with its medium file (tables of restGarfield) and its responses (`charge`, `light`). |
| `TRestDetectorChargeResponse` | Ionisation (W-value, Fano factor), electron transport at the drift field (from the tables or fixed parameters) and avalanche gain tables of a medium. |
| `TRestDetectorLightResponse` | Scintillation light of a medium: light yield (photons per keV) and propagation to the readout (attenuation length and effective light speed). |
| `TRestDetectorTransportTable` | Drift velocity, diffusion, Townsend and attachment coefficients versus the field (configuration of restGarfield and one table of the medium file), scaled with E/p for gases. |
| `TRestDetectorGainTable` | Mean avalanche gain and Polya parameter versus the amplification field for a gap (configuration of restGarfield and one table of the medium file). |
| `TRestDetectorTransportManager` | Top section of a restGarfield configuration: a medium and the transport and gain tables to compute for it. |

## Processes

Hits processes (`TRestDetectorHitsEvent` to `TRestDetectorHitsEvent`):

| Class | Description |
| --- | --- |
| `TRestDetectorHitsDriftProcess` | Converts the energy of the hits into ionisation electrons (Fano or Poisson fluctuations) and drifts them to the readout with the diffusion and attachment of the medium. |
| `TRestDetectorAvalancheProcess` | Multiplies the drifted electrons by the avalanche gain of the amplification gap, with Polya fluctuations, from a gain table of the medium. |
| `TRestDetectorHitsSmearingProcess` | Applies the energy resolution of the detector (one gaussian gain per event), optionally without the Fano part. |
| `TRestDetectorHitsTransformationProcess` | Scales the energy and mirrors, rotates and translates the hit positions. |
| `TRestDetectorScintillationProcess` | Converts the energy of the hits into the photoelectrons of the channels collecting their light (light yield of the medium, collection probability of the readout, e.g. the fibres of a `TRestFibreReadout`), Poisson per channel. Observables `NPhotoelectrons`, `NHitsWithoutLight`. |
| `TRestDetectorLightAttenuationProcess` | Attenuates and delays the energy deposits of a scintillator with the distance to the readout of their channel (light response of the medium). Observable `NHitsWithoutChannel`. |
| `TRestDetectorHitsAnalysisProcess` | Observables of the hits: number, energy, mean position, sigma, skewness, gaussian sigma, fiducial energy, and per projection with balances. |

Signal processes:

| Class | Input → output | Description |
| --- | --- | --- |
| `TRestDetectorHitsToSignalProcess` | hits → signals | Collects the hits in the readout channels at their arrival time (drift velocity of the medium); observable `NHitsRejected`. |
| `TRestDetectorSignalShapingProcess` | signals → signals | Convolves the signals with the response of the electronics in physical time units (gaus, exponential, shaper, shaperSin). |
| `TRestDetectorSignalRecoveryProcess` | signals → signals | Recovers the signals of dead channels from their adjacent channels, preserving the charge. |
| `TRestDetectorSignalPeaksFinderProcess` | signals → signals | Finds the peaks of all the signals and groups them in time coincidence windows (observables of the peaks and windows). |
| `TRestDetectorSignalToHitsProcess` | signals → hits | Reconstructs hits from the signals with the channel positions and the drift velocity (methods `tripleMaxAverage`, `gaussFit`, `integral`, `intWindow`...). |

The configuration keys, the observables and a YAML example of each process are given in its class
documentation.

## Datasets

| Class | Description |
| --- | --- |
| `TRestDataSetGainMap` | Dataset process (`TRestDataSetProcessManager`): divides the extent of the readout in segments, fits the calibration peaks of each segment and adds the columns of the energy calibrated per segment and with the whole readout; refits and drawings in restRoot. |

## Macros

The macros are compiled and available in `restRoot` (called directly, without `.L`).

| Macro | Description |
| --- | --- |
| `REST_Detector_GenerateMicromegasReadout(yamlFile, outputFile, decodingName)` | Builds a `TRestMicromegasReadout` from the `readout` section of a YAML file and exports it (decoding `decodingName`, `nominal_decoding` by default) to a ROOT file. |
| `REST_Detector_GenerateFibreReadout(yamlFile, outputFile, gdmlFile, decodingName)` | Same for a `TRestFibreReadout` (identity decoding without `decodingFile`); with `gdmlFile`, also writes the placements of the fibres to include in the GDML of the simulation as an external entity. |
| `REST_Detector_GenerateGDMLReadout(yamlFile, outputFile, decodingName)` | Same for a `TRestGDMLReadout` (decoding `veto_decoding` by default; default files those of `libraries/detector/examples`, from the repository root). |
| `REST_Detector_ViewMicromegasReadout(instanceName, decodingName, inputFile)` | Imports a readout from its ROOT file (closed after the import), tests the position lookup and draws it with some channels highlighted. |
| `REST_Detector_ViewGDMLReadout(instanceName, decodingName, inputFile)` | Same for the veto readout. |
| `REST_Detector_TimeWalk(fileName, method, selection, nEntries, refFraction, smoothingPoints, outputFileName)` | Time walk diagnostic of the pulses of a `TRestDetectorSignalEvent` file: delay against the relative amplitude, the amplitude and the strip distance (strips of the data readout of the file). |

## Examples

The files of `examples/` are run from that directory (the readout files are found by relative paths),
except `GDMLReadout.yaml`, whose decoding and GDML paths are relative to the repository root.

| File | Contents |
| --- | --- |
| `MicromegasReadout.yaml`, `iaxo_d1_iaxolab.dec` | Micromegas readout of IAXO-D1 (120 strips per axis, rotated 45 degrees) and its decoding. |
| `AlphaCAMM.yaml`, `AlphaCAMM.dec` | Micromegas readout of AlphaCAMM (256 strips per axis, cathode at 90 mm), used by the simulation chains of the connectors library. |
| `GDMLReadout.yaml`, `veto.dec` | Veto readout from the GDML of the IAXO restG4 example, with the light guides as readout volumes. |
| `ReadoutManager.yaml` | `TRestDetectorReadoutManager` section loading both readouts, the Micromegas one as data readout. |
| `iaxo_readout_output.root`, `veto_readout_output.root`, `AlphaCAMM_readout.root` | Readout files generated by the macros. |
| `SignalToHits.yaml` | Pipeline `TRestDetectorSignalEvent` → `TRestDetectorHitsEvent` with the IAXO-D1 readout. |
| `SignalRecovery.yaml` | Recovery of dead channels of the IAXO-D1 readout. |
| `SignalPeaks.yaml` | Peaks and coincidence windows of the signals. |
| `DataSetCalibration.yaml` | Energy calibration of datasets with `TRestDataSetCalibration` and a `TRestDataSetGainMap`, with plots; its outputs are the inputs of `examples/DataSetLogOdds.yaml` and `examples/DataSetTMVA.yaml`. |

Generate and view the readouts (in restRoot):

```
REST_Detector_GenerateMicromegasReadout("MicromegasReadout.yaml", "iaxo_readout_output.root")
REST_Detector_GenerateMicromegasReadout("AlphaCAMM.yaml", "AlphaCAMM_readout.root")
REST_Detector_ViewMicromegasReadout("IAXO_D1_Readout", "nominal_decoding", "iaxo_readout_output.root")
REST_Detector_ViewGDMLReadout("Veto_Readout", "veto_decoding", "veto_readout_output.root")
```

The veto readout is generated from the repository root (`GDMLReadout.yaml` reads the GDML of
`packages/restG4/examples/13.IAXO`):

```
REST_Detector_GenerateGDMLReadout("libraries/detector/examples/GDMLReadout.yaml",
                                  "libraries/detector/examples/veto_readout_output.root")
```

Run the pipelines (the input file holds a `TRestDetectorSignalEvent` tree, or a dataset for the dataset
examples):

```
restManager --c SignalToHits.yaml --i <signals file>
restManager --c SignalRecovery.yaml --i <signals file>
restManager --c SignalPeaks.yaml --i <signals file>
restManager --c DataSetCalibration.yaml --i Dataset_Calibration.root
```
