# The connectors library

This library contains the processes that inter-connect the fundamental REST libraries, transforming an
event type into another one, e.g. raw signals into detector signals or detector hits into a track event,
and other classes that need two libraries at once, such as the DAQ viewer.

The connectors library (`RestConnectors`) holds what needs several libraries at once:

- the processes that convert the events of one library into the events of another one (e.g. raw signals
  into detector signals), the links of the pipelines that go through several libraries;
- the DAQ viewer, an event display of raw signal files with the readouts of the detector library.

Each class is built only if all the libraries it connects are built (otherwise CMake reports it as
disabled); the library is not built if no class is enabled.

## Processes

| Class | Conversion | Requires | Description |
| --- | --- | --- | --- |
| `TRestGeant4ToDetectorHitsProcess` | `TRestGeant4Event` to `TRestDetectorHitsEvent` | geant4, detector | Transfers the Geant4 hits of the selected volumes (the sensitive ones by default) as XYZ hits. |
| `TRestRawToDetectorSignalProcess` | `TRestRawSignalEvent` to `TRestDetectorSignalEvent` | raw, detector | Converts ADC samples into time and amplitude points, optionally with zero suppression; channel activity observables. |
| `TRestDetectorSignalToRawSignalProcess` | `TRestDetectorSignalEvent` to `TRestRawSignalEvent` | detector, raw | Digitizes the signals: samples in an acquisition window from a trigger time, in ADC units with saturation (simulation to raw data). |
| `TRestDetectorHitsToTrackProcess` | `TRestDetectorHitsEvent` to `TRestTrackEvent` | detector, track | Groups in tracks the hits closer than a distance, optionally attaching the isolated low energy hits (time walk). |

The configuration keys, the observables and a YAML example of each process are given in its class
documentation.

## DAQ viewer

| Class | Requires | Description |
| --- | --- | --- |
| `TRestDAQViewer` | raw, detector | GUI event display of the files with a `TRestRawSignalEvent` tree (e.g. written by femclient, package restDAQ): for each readout, the amplitude spectrum, the hit map and the signals of the event, plus the rate. In the live mode it follows the file while it is written and opens the next subrun or run; Reload Run reloads the current run from its first subrun (000), histograms reset. |

It is configured with a restv2 YAML with a `TRestDetectorReadoutManager` (the readouts), a
`TRestRawSignalAnalysisProcess` (the pulse parameters) and the `parameters` of the spectrum binning; the
YAML of an analysis can be used as it is.

## Macros

| Macro | Description |
| --- | --- |
| `REST_DAQViewer(configFile = "", dataFile = "")` | Opens the `TRestDAQViewer` window with the given configuration and data file (empty: those of the last session). |

```
restRoot
[0] REST_DAQViewer("libraries/connectors/examples/DAQViewer.yaml", "Run00012_..._000.root")
```

## Examples

The examples are in `examples/`; the paths are relative to the repository root. Some of them need readout
files to be generated first (see the comments at the top of each file).

| File | Description |
| --- | --- |
| `Geant4ToHits.yaml` | `TRestGeant4Event` (restG4 output) to `TRestDetectorHitsEvent`. |
| `Geant4ToDriftHits.yaml` | Geant4 hits drifted and diffused to the AlphaCAMM readout. |
| `Geant4ToRaw.yaml` | Full simulation chain: Geant4 hits, drift, smearing, readout signals, shaping, digitization and noise, to raw signals analysed as the data. |
| `Geant4ToRawVeto.yaml` | Raw signals of the vetoes with the acquisition window of the Micromegas (trigger time from an observable). |
| `RawToHits.yaml` | `TRestRawSignalEvent` to `TRestDetectorSignalEvent` to `TRestDetectorHitsEvent`. |
| `HitsToTrack.yaml` | `TRestDetectorHitsEvent` to `TRestTrackEvent`, with hits and track observables, reduction, path minimization and blob analysis. |
| `DAQViewer.yaml` | Configuration of the DAQ viewer (readouts, pulse parameters and spectrum binning). |
| `ChannelActivity.yaml` | Plots of the activity per DAQ and readout channel and of the maximum amplitude of the signals (observables of `TRestRawToDetectorSignalProcess`; legacy channel activity process). |

```
restManager --c libraries/connectors/examples/Geant4ToHits.yaml --i <restG4 output file>
restManager --c libraries/connectors/examples/Geant4ToDriftHits.yaml --i <restG4 output file>
restManager --c libraries/connectors/examples/Geant4ToRaw.yaml --i <restG4 output file>
restManager --c libraries/connectors/examples/Geant4ToRawVeto.yaml --i <restG4 output file>
restManager --c libraries/connectors/examples/RawToHits.yaml --i <file with TRestRawSignalEvent>
restManager --c libraries/connectors/examples/HitsToTrack.yaml --i <file with TRestDetectorHitsEvent>
restManager --c libraries/connectors/examples/ChannelActivity.yaml --i <file with the observables of rawToSignal>

restRoot
[0] REST_DAQViewer("libraries/connectors/examples/DAQViewer.yaml", "<file with TRestRawSignalEvent>")
```

The output of `HitsToTrack.yaml` can be analysed further with the examples of the
[track library](../track/README.md).
