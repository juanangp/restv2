# REST core library (source/core)

`RestCore` is the shared library with the framework of REST: the base classes of events, metadata and
processes, the run (input/output ROOT files), the managers that read a YAML configuration and run it, the
cuts, the datasets and their processes, the analysis plots and the event browser. It depends only on
`RestTools` (source/tools: logging, units, YAML and file utilities) and ROOT; the physics libraries
(`libraries/raw`, `detector`, `track`, `geant4`, `connectors`) derive their events, processes and
metadata from it.

## Main classes

### Events

| Class | Role |
|---|---|
| `TRestEvent` | Abstract base of all events: identity and timestamp (`TRestEventInfo`), branches of its event tree, format version, `DrawEvent`. |
| `EventRegistry` | Factory of event classes by name, used to create the events of a file or of a process chain. |
| `TRestHits`, `TRestHitsData` | View over shared hit arrays (position, time, energy, type) with geometric and statistical methods and drawing helpers, used by the hits and track events. Hits are only added or removed on the last view of the storage (`std::logic_error` otherwise); `Sort()` orders by descending energy by default. |

### Metadata and configuration

| Class | Role |
|---|---|
| `TRestMetadata` | Abstract base of every object configured from a YAML section (`class`, `name`, `verbose` and its registered keys); stored in the `RESTMetadataStore` directory of the output files. |
| `MetadataClassRegistry` | Factory of metadata classes by the `class` key of a section. |
| `TRestMetadataFieldRegistry` | Registered fields (YAML key ↔ member) of each class; unknown keys in a configuration throw. |
| `TRestMetadataStore` | The metadata of a REST file recreated as objects (e.g. readouts, geometry). |
| `TRestRunInfo` | Description of a run: numbers, tags, `runInfo`, times, entries. |
| `TRestRun` | A `TRestRunInfo` plus the input/output ROOT files, event trees, `AnalysisTree` and observables. |

### Processing

| Class | Role |
|---|---|
| `TRestEventProcess` | Abstract base of the event processes: input/output event types, `InitProcess`, `ProcessEvent`, observables and `cuts`. |
| `TRestEventProcessManager` | The pipeline: chains the processes, routes the events, stores the outputs and runs the event loop. |
| `TRestSelectionProcess` | Keeps the events passing its `TRestCut`, for any event type. |
| `TRestCut` | Named conditions (C++ expressions of observables) used by processes, datasets and plots. |
| `TRestManager` | Top section of a configuration: creates its metadata and runs the event pipeline (also multithreaded), the dataset generation/processing and the plots. |

### Datasets and analysis

| Class | Role |
|---|---|
| `TRestDataSet` | Selected analysis trees of several runs (or merged datasets) in one tree, with the `TRestRunInfo` of each run (`runIndex`), built with RDataFrame. |
| `TRestDataSetProcess` | Abstract base of the dataset processes: parameters computed on a reference dataset, columns added to the processed one. |
| `TRestDataSetProcessManager` | Pipeline of dataset processes, writing the processed dataset once. |
| `TRestAnalysisPlot` | Combined canvas of text panels and histograms of the observables of run files or datasets. |

### Visualization

| Class | Role |
|---|---|
| `TRestBrowser` | GUI event browser of a REST file (entry/ID navigation, event type, plot option, selection). |
| `TRestEventViewer` | Default viewer: draws the event with its `DrawEvent` option in a canvas. |
| `TRestEveEventViewer` | Base of the 3D viewers built on ROOT Eve. |

## Typical flow

A YAML file has a `TRestManager` section with the metadata it needs, and is run with `restManager`
(source/bin):

```yaml
manager:
  class: TRestManager
  run:
    class: TRestRun
    runNumber: 1
    runTag: Test
    inputFileName: input.root
    outputFileName: "R[runNumber]_[runTag].root"
  pipeline:
    class: TRestEventProcessManager
    eventsToProcess: 0
    nThreads: 1
    myProcess:
      class: TRestSomeProcess   # a process of a REST library
      observables: ["all"]
```

```
restManager --c config.yaml [--i input.root] [--o output.root] [--j 4]
```

`restManager` builds the `TRestManager`, which creates every section with `class` and gives them the
`TRestRun`. `TRestManager::Run` runs the `TRestEventProcessManager` on the run: each entry is read, passed
through the process chain and, if accepted (by the processes and their cuts), written to the output event
trees and the `AnalysisTree`. `SaveMetadata` stores the configurations in the output file and `RunPlots`
draws the `TRestAnalysisPlot` sections. Instead of a run and a pipeline, a manager can contain a
`TRestDataSet` (generation of a dataset from run files) and/or a `TRestDataSetProcessManager` (processing of
a dataset), or only plots.

The output files are read back with `TRestRun` (events, `GetObservable`, metadata) or `TRestDataSet`, in
`restRoot` or in macros.

## Examples and macros

- `source/macros`: `REST_OpenInputFile.C` (used by `restRoot file.root`), `REST_ViewEvents.C` (event
  browser), `REST_GenerateDataSet.C` (dataset from a YAML section) and `REST_MergeRuns.C`.
- `examples/` and `libraries/*/examples/`: YAML configurations of complete chains, datasets
  (e.g. `libraries/detector/examples/DataSetCalibration.yaml`) and plots.
- The class documentation of `TRestDataSet`, `TRestDataSetProcessManager` and `TRestAnalysisPlot`
  (in their `.cxx` files) describes all their keys in detail.
