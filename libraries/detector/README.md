# The detector library

This library is used to define the detector: the readout topology (Micromegas planes or GDML volumes) that
maps positions to channels, and the medium with its charge and light response. It also implements the
processes of event reconstruction from real detector data (signal shaping, recovery, peak finding, signals
to hits) and of emulation of the physical response of the detector (electron drift and diffusion,
avalanche, smearing, light attenuation, spatial transformations, hits to signals).

The detector library (`RestDetector`) describes the detector of rest-for-physics-toolkit and the processing of
its events in physical units: the hits (energy deposits, drifted electrons or reconstructed positions) and the
signals of the readout channels, the readouts that map positions to channels, the medium with its charge and
light responses and the transport and gain tables computed by restGarfield, the processes that go from hits to
signals (simulation) and from signals to hits (reconstruction), and the gain map of datasets. The conversions
from and to other libraries (Geant4 events, raw signals, tracks) are in the [connectors
library](../connectors/README.md).

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
| `TRestFibreReadout` | Scintillator crossed by families of parallel fibres (e.g. LiquidO): box (`size`, `position`) and, per family, axis, pitch, offset and GDML volume; the closest fibre to a position, the fibres within a distance, and the placements of the fibres for the GDML of the simulation. |
| `TRestFibreReadoutParameters`, `TRestFibreFamily` | The `readoutParameters` of a `TRestFibreReadout` and each family of fibres (`fibres`). |
| `TRestDetectorReadoutManager` | Imports the readouts of a run or dataset from their ROOT files and selects the readout of the data (`readout`); used by the processes and the gain map. |
| `TRestReadoutInfo` | One readout to import: `instanceName`, `decodingName`, `inputFileName`. |

## Medium, responses and tables

| Class | Description |
| --- | --- |
| `TRestDetectorMedium` | A material (gas, liquid or solid) at its pressure and temperature, with its medium file (tables of restGarfield) and its responses (`charge`, `light`). |
| `TRestDetectorChargeResponse` | Ionisation (W-value, Fano factor), electron transport at the drift field (from the tables or fixed parameters) and avalanche gain tables of a medium. |
| `TRestDetectorLightResponse` | Scintillation light of a medium: light yield (photons per keV) and propagation to the readout in a transparent scintillator (attenuation length and effective light speed). |
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
| `TRestDetectorScintillationProcess` | Primary scintillation: the energy of each hit becomes scintillation photons, Poisson(E · lightYield of the medium). Observable `NPhotons`. |
| `TRestDetectorLightCollectionProcess` | Collection of the photons by the readout channels and their photoelectrons (`quantumEfficiency` of the photosensors, binomial), with a model per scintillator: `localCapture` (opaque, e.g. LiquidO: the channels within `maxDistance` capture them with `captureProbability(r)`, a formula or a graph of an optical simulation, multinomial) or `attenuation` (transparent, e.g. vetoes: the photons reach the readout of their channel attenuated and delayed with the distance, light response of the medium). Observables `NPhotoelectrons`, `NHitsWithoutChannel`. Replaces legacy `TRestDetectorLightAttenuationProcess`. |
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
| `TRestDataSetTardis` | Dataset process (`TRestDataSetProcessManager`): adds `<name>_timeDelay`, the time since the previous event in chronological order across the runs, or with `channels` since the previous event in the same channels of the readout (neighbours within `channelTolerance`, each group of channels: X and Y strips, fibre families); its `cuts` filter the dataset. |
| `TRestDataSetGainMap` | Dataset process (`TRestDataSetProcessManager`): divides the extent of the readout in segments, fits the calibration peaks of each segment and adds the columns of the energy calibrated per segment and with the whole readout; refits and drawings in restRoot. |

## Macros

The macros are compiled and available in `restRoot` (called directly, without `.L`).

| Macro | Description |
| --- | --- |
| `REST_Detector_GenerateMicromegasReadout(yamlFile, outputFile, decodingName, checkOverlaps)` | Builds a `TRestMicromegasReadout` from the `readout` section of a YAML file and exports it (decoding `decodingName`, `nominal_decoding` by default) to a ROOT file. With `checkOverlaps`, nothing is written if the geometry has overlaps. Returns 0, or 1 on any error. |
| `REST_Detector_GenerateFibreReadout(yamlFile, outputFile, gdmlFile, decodingName, checkOverlaps)` | Same for a `TRestFibreReadout` (identity decoding without `decodingFile`); with `gdmlFile`, also writes the placements of the fibres to include in the GDML of the simulation as an external entity. |
| `REST_Detector_GenerateGDMLReadout(yamlFile, outputFile, decodingName, checkOverlaps)` | Same for a `TRestGDMLReadout` (decoding `veto_decoding` by default; default files those of `libraries/detector/examples`, from the repository root). |
| `REST_Detector_GenerateReadouts(yamlFile, outputFile, decodingName, checkOverlaps)` | Builds every readout section of a YAML file (`TRestMicromegasReadout`, `TRestGDMLReadout`, `TRestFibreReadout`, e.g. the planes of a detector) and exports them all to the same ROOT file (a single readout file per project; the manager has one entry per readout with the same `inputFileName`). The fibre placements go to `<readout name>_fibres.xml` next to the output. The `decodings` of a readout (name and decoding file) are stored besides the one of `decodingFile`. Returns 0, or 1 on any error (the output file is then removed). |
| `REST_Detector_AddDecoding(readoutFile, readoutName, decodingFile, decodingName)` | Adds (or replaces) a decoding of a readout already in a readout file without rewriting its geometry, e.g. to update the decoding of a detector (its physical IDs must be among those of the decodings already stored). The readout manager chooses it with `decodingName`. Returns 0, or 1 on any error. |
| `REST_Detector_ViewMicromegasReadout(instanceName, decodingName, inputFile)` | Imports a readout from its ROOT file (closed after the import), tests the position lookup and draws it with some channels highlighted. |
| `REST_Detector_ViewGDMLReadout(instanceName, decodingName, inputFile)` | Same for the veto readout. |
| `REST_Detector_TimeWalk(fileName, method, selection, nEntries, refFraction, smoothingPoints, outputFileName)` | Time walk diagnostic of the pulses of a `TRestDetectorSignalEvent` file: delay against the relative amplitude, the amplitude and the strip distance (strips of the data readout of the file). |

## Examples

The files of `examples/` are run from that directory, except `GDMLReadout.yaml`, whose decoding and GDML paths
are relative to the repository root. The pipelines read the readouts of
[rest-common-readouts](https://github.com/rest-for-physics-toolkit/rest-common-readouts) by URL (`IAXO.root`
with `IAXO_D1_Readout`, `AlphaCAMM.root` with `AlphaCAMM_Readout`; downloaded once and cached); the veto one
is generated here.

| File | Contents |
| --- | --- |
| `MicromegasReadout.yaml`, `iaxo_d1_iaxolab.dec` | Micromegas readout of IAXO-D1 (120 strips per axis, rotated 45 degrees) and its decoding: an example of the generation of a readout (the one used by the pipelines is `IAXO.root` of rest-common-readouts). |
| `GDMLReadout.yaml`, `veto.dec` | Veto readout from the GDML of the IAXO restG4 example, with the light guides as readout volumes. |
| `ReadoutManager.yaml` | `TRestDetectorReadoutManager` section loading both readouts (IAXO-D1 of rest-common-readouts and the veto), the Micromegas one as data readout. |
| `iaxo_readout_output.root`, `veto_readout_output.root` | Readout files generated by the macros. |
| `SignalToHits.yaml` | Pipeline `TRestDetectorSignalEvent` → `TRestDetectorHitsEvent` with the IAXO-D1 readout. |
| `SignalRecovery.yaml` | Recovery of dead channels of the IAXO-D1 readout. |
| `SignalPeaks.yaml` | Peaks and coincidence windows of the signals. |
| `DataSetCalibration.yaml` | Energy calibration of datasets with `TRestDataSetCalibration` and a `TRestDataSetGainMap`, with plots; its outputs are the inputs of `examples/DataSetLogOdds.yaml` and `examples/DataSetTMVA.yaml`. |

Generate and view the readouts (in restRoot):

```
REST_Detector_GenerateMicromegasReadout("MicromegasReadout.yaml", "iaxo_readout_output.root")
REST_Detector_ViewMicromegasReadout("IAXO_D1_Readout", "nominal_decoding", "iaxo_readout_output.root")
REST_Detector_ViewGDMLReadout("Veto_Readout", "veto_decoding", "veto_readout_output.root")
```

In a CI, with the overlaps checked and the value of the macro as exit code (`-e` and `gSystem->Exit`:
`restRoot` does not run a bare function call given as argument):

```
restRoot -b -q -e 'gSystem->Exit(REST_Detector_GenerateReadouts("TREXDM.yaml", "TREXDM.root", "nominal_decoding", true));'
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
