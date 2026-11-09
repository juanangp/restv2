# The raw library

This library is used to store and process the raw signals of the data acquisition: time pulses of ADC
samples with a fixed number of bins. It reads the binary and ROOT formats of the electronics into REST,
includes processes of signal conditioning (channel selection, baseline correction, common noise reduction,
saturation recovery, shaping, noise addition) and pulse-shape analysis, and defines the event and the
configuration of the data taking of restDAQ.

The raw library (`RestRaw`) holds the raw signal event of restv2, the event of the data acquisition
(restDAQ) and its configuration, and the processes that read the files of the DAQs, process the raw
signals and compute pulse-shape observables. A raw signal is a vector of 16-bit ADC samples identified by
its signal ID (the DAQ channel); the decoding into readout channels and the conversion into detector
signals are done by the detector and connectors libraries (e.g. `TRestRawToDetectorSignalProcess`, in the
[connectors library](../connectors/README.md)).

## Events and metadata

| Class | Description |
| --- | --- |
| `TRestRawSignalEvent` | Event with the signals of one trigger: samples of all the signals stored back-to-back with their IDs and offsets (branches `fSigSamples`, `fSigIDs`, `fSigOffsets`). |
| `TRestRawSignal` | Lightweight view of one signal of a `TRestRawSignalEvent` (ID, samples, graph, ADC saturation check). |
| `TRestRawSignalData` | Contiguous storage of the samples, IDs and offsets shared by the views. |
| `TRestRawDAQSignalEvent` | `TRestRawSignalEvent` written by restDAQ, with the id, event counter, timestamp and number of signals of each FEM. Written in the tree `TRestRawSignalEvent`, so its files are read as plain raw signal events. |
| `TRestRawDAQMetadata` | Configuration of the data taking of [restDAQ](../../packages/restDAQ/README.md) (electronics, FEMs, TCM, run limits, file size, decimals accepted as `1.5GB`, Prometheus), stored in each file it writes. |
| `TRestRawDAQFEM` | One FEM of `TRestRawDAQMetadata` (id, IP, FECs of a DCC). |

## Processes

Readers and converters (no input event, `TRestRawSignalEvent` output; the input file is the
`inputFileName` of the run):

| Class | Description |
| --- | --- |
| `TRestRawFeminosRootToSignalProcess` | ROOT files of feminos-daq (trees `events` and `run`); takes the run number, tag, description, start time and runInfo from the `run` tree. |
| `TRestRawMultiFEMINOSToSignalProcess` | Binary files (`.aqs`) of one or several Feminos cards (through a TCM); signal ID = channel + 72 (4 card + chip). |
| `TRestRawFemDAQToSignalProcess` | ROOT files of the old FEMDAQ (tree `SignalEvent`). The files of restDAQ are REST files and need no converter. |

Signal processing (`TRestRawSignalEvent` to `TRestRawSignalEvent`):

| Class | Description |
| --- | --- |
| `TRestRawSignalChannelSelectionProcess` | Keeps or removes signals by ID or ID range (e.g. separate the veto from the Micromegas, remove dead channels); drops events left without signals. |
| `TRestRawBaseLineCorrectionProcess` | Subtracts from each signal a moving-average baseline that excludes the pulse. |
| `TRestRawCommonNoiseReductionProcess` | Subtracts the common-mode noise, bin by bin, per block of channels sharing a noise source (e.g. one AGET chip). |
| `TRestRawZeroSupressionToRawProcess` | Removes the offset of zero-suppressed signals (first non-zero sample). |
| `TRestRawSignalRecoverSaturationProcess` | Recovers ADC-saturated pulses by fitting the AGET response to the non-saturated bins; observables of the recovered signals. |
| `TRestRawSignalShapingProcess` | Convolves the signals with a shaping kernel (gaus, exponential, shaper, shaperSin), to emulate the electronics in simulations; an event without signals passes empty. |
| `TRestRawSignalAddNoiseProcess` | Adds white and sinusoidal noise to the signals, for simulations. |

Analysis (observables in the analysis tree):

| Class | Description |
| --- | --- |
| `TRestRawSignalAnalysisProcess` | Baseline, amplitude, integral, peak time, rise time, width and saturation of each signal, and event-level sums, extrema and averages over the good signals. |

The configuration keys, the observables and a YAML example of each process are given in its class
documentation.

## Viewers and macros

| Class / macro | Description |
| --- | --- |
| `TRestRawPulsesViewer` | Event viewer of raw signal files (formerly DrawPulses of FEMDAQ): the signals of each event in one pad per ASIC, navigation by entry, save as PDF; follows a file being written by restDAQ. |
| `REST_Raw_PulsesViewer(fileName = "")` | Opens a `TRestRawPulsesViewer`, with the file `fileName` if given. |

```
restRoot
[0] REST_Raw_PulsesViewer("RawSignals.root")
```

A single event can also be drawn with `TRestRawSignalEvent::DrawEvent(option)`, where the option selects
the signals (an ID, or a range such as `"4320-4391"`).

## Examples

The examples are in `examples/`; the paths are relative to the repository root.

| File | Description |
| --- | --- |
| `MultiFEMINOSToRaw.yaml` | Decodes a `.aqs` file of the Feminos cards into `TRestRawSignalEvent` (`RawSignals.root`). |
| `FeminosRootToRaw.yaml` | Reads a ROOT file of feminos-daq and runs the chain to tracks: common noise reduction, raw to detector signals, hits and track analysis (processes of the detector, connectors and track libraries). It loads the readout `libraries/detector/examples/iaxo_readout_output.root` and the gas file `testdata/ArIso.root`, relative to the repository root (run it from there). |
| `VetoAnalysis.yaml` | Veto analysis (legacy `TRestRawVetoAnalysisProcess`) with the existing processes: keeps the veto channels, corrects their baseline and computes their amplitude and peak time; the comments give the legacy observables as expressions. |

```
restManager --c libraries/raw/examples/MultiFEMINOSToRaw.yaml --i R01234_..._-000.aqs
restManager --c libraries/raw/examples/FeminosRootToRaw.yaml --i R01234_....root
restManager --c libraries/raw/examples/VetoAnalysis.yaml --i <file with TRestRawSignalEvent>
```
