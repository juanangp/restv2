# Migration guide: legacy REST (v2.x) to restv2

This guide is for two audiences:

- **users** of the legacy REST framework (`rest-for-physics/framework`, RML/XML configurations) moving their
  configurations and analyses to restv2;
- **developers** porting a legacy class (event, metadata, process) to restv2.

It only describes what changes with respect to legacy. The general usage of restv2 (installation, formats
of events and files, YAML configuration, datasets) is in the [user guide](doc/UserGuide.md); each component
documents its classes in its README (`source/*/README.md`, `libraries/*/README.md`, `packages/*/README.md`)
and each class in the Doxygen comment of its `.cxx`. The migration status of every legacy class, with the
decisions taken, is tracked in [MIGRATION_STATUS.md](MIGRATION_STATUS.md) (in Spanish).

## Contents

1. [What changed](#1-what-changed)
2. [Migrating configurations and analyses (users)](#2-migrating-configurations-and-analyses-users)
   - [2.1 RML to YAML](#21-rml-to-yaml)
   - [2.2 Variables, parameters and includes](#22-variables-parameters-and-includes)
   - [2.3 Values, units, options and verbose](#23-values-units-options-and-verbose)
   - [2.4 Running](#24-running)
   - [2.5 Output files](#25-output-files)
   - [2.6 Pipelines and observables](#26-pipelines-and-observables)
   - [2.7 Cuts, selection and plots](#27-cuts-selection-and-plots)
   - [2.8 Run parameters, file names and merging](#28-run-parameters-file-names-and-merging)
   - [2.9 Datasets](#29-datasets)
   - [2.10 Dataset processes](#210-dataset-processes)
   - [2.11 Readouts, medium and detector processes](#211-readouts-medium-and-detector-processes)
   - [2.12 restG4](#212-restg4)
   - [2.13 restGarfield and restDAQ](#213-restgarfield-and-restdaq)
3. [Class equivalences](#3-class-equivalences)
4. [Developer guide: porting a legacy class](#4-developer-guide-porting-a-legacy-class)
   - [4.1 Where the files go](#41-where-the-files-go)
   - [4.2 Events](#42-events)
   - [4.3 Metadata](#43-metadata)
   - [4.4 Event processes](#44-event-processes)
   - [4.5 Dataset processes](#45-dataset-processes)
   - [4.6 Documentation, tests, format and dictionary](#46-documentation-tests-format-and-dictionary)
   - [4.7 Checklist](#47-checklist)
5. [Common issues](#5-common-issues)

## 1. What changed

| Aspect | Legacy | restv2 |
|--------|--------|--------|
| Repository | `framework` with the libraries and packages as git submodules | One repository: `source/` (tools, core, analysis, bin, macros), `libraries/` (raw, detector, track, geant4, connectors), `packages/` (restG4, restGarfield, restDAQ) |
| Configuration | RML (XML): sections, `<parameter>`, attributes, `<globals>`, `<variable>`, `<for>`, `<if>`, `<include>` | YAML: one map per metadata with its `class`; `parameters`, `${PARAM:x}`, `${ENV:X}`, `include`, `select` |
| Reading the configuration | Reflection: any `<parameter name="par">` sets member `fPar`; unknown keys ignored | Only registered fields (`TRestMetadataFieldRegistry`); an unknown key or a value not convertible is an error |
| Classes | `TObject`, `ClassDef`/`ClassImp`, created by name through ROOT (`TClass`) | Plain C++ classes registered in `EventRegistry` / `MetadataClassRegistry`; ROOT dictionaries still generated (LinkDef from the headers) for restRoot and macros |
| Event storage | `EventTree`, one object branch per event (ROOT streamers, schema evolution) | One tree per event class (named as the class), flat branches of numbers and vectors, format version in the tree UserInfo |
| Metadata storage | Metadata objects streamed in the file | YAML (resolved configuration and results) in `RESTMetadataStore`; ROOT objects (geometry, readouts, histograms) written apart |
| Analysis tree | `TRestAnalysisTree` | Plain `TTree` `AnalysisTree` (read with RDataFrame), same `processName_observable` branch names |
| Pipeline | `TRestProcessRunner` + `TRestThread`, `<addTask>` | `TRestEventProcessManager` (event routing from the input/output event of each process); threads are independent managers over entry ranges, outputs merged; no `TRestTask` |
| Managers | `TRestManager` | `TRestManager` (restManager), `TRestGeant4Manager` (restG4), `TRestDetectorTransportManager` (restGarfield), `TRestEventProcessManager`, `TRestDataSetProcessManager`, `TRestDetectorReadoutManager` |
| Datasets | `TRestDataSet`, `TRestDataSetPlot`, `TRestDataSetCalibration/GainMap/Odds` as standalone metadata | `TRestDataSet` on RDataFrame storing every run (`TRestRunInfo`); calibration, gain map, log odds, TMVA and selection as dataset processes of `TRestDataSetProcessManager` |
| Logging | `TRestStringOutput`, verbose per class: silent, essential, info, debug, extreme | `TRestLogManager`, `verbose` per instance: silent, error, warning, info, debug; thread safe |
| Types | `Double_t`, `Int_t`, `TString`, `TVector2/3` | `double`, `int`, `std::string`, `std::pair`/`std::vector`, `ROOT::Math::XYZVector` |
| Helpers | `TRestStringHelper`, `TRestTools`, `TRestPhysics`, `TRestReflector` | `TRestTools`, `TRestYAMLUtils`, `TRestConstants`, `REST_Units` (`TRestSystemOfUnits.h`); analysis algorithms in `TRestPulseShapeAnalysis` and `TRestHitsAnalysis` |

**Legacy files are not read.** A restv2 run file needs `AnalysisTree` and `RESTMetadataStore`
(`TRestTools::isValidTRestRun`); legacy ROOT outputs must be produced again from the raw data (the DAQ
formats are read by processes, see [section 3](#raw)) or simulated again. There are no conversions between
event format versions either.

## 2. Migrating configurations and analyses (users)

### 2.1 RML to YAML

Each legacy section becomes a YAML map whose key is the instance name and whose `class` is the class;
`<parameter name="x" value="v"/>` and XML attributes become keys. The same pipeline in both formats:

```xml
<TRestManager>
  <TRestRun file="run.xml"/>
  <TRestProcessRunner name="Signals" verboseLevel="silent">
    <parameter name="eventsToProcess" value="0"/>
    <parameter name="threadNumber" value="2"/>
    <addProcess type="TRestRawToDetectorSignalProcess" name="detSignal" value="ON"
                baseLineRange="(${BL_MIN},${BL_MAX})" pointThreshold="3" observable="all"/>
  </TRestProcessRunner>
  <addTask type="processEvents" value="ON"/>
</TRestManager>
```

```yaml
parameters:
  blMin: 20
  blMax: 140

manager:
  class: "TRestManager"
  run:
    include: "run.yaml"
  pipeline:
    class: "TRestEventProcessManager"
    verbose: "silent"
    eventsToProcess: 0
    nThreads: 2
    detSignal:
      class: "TRestRawToDetectorSignalProcess"
      baselineRange: ["${PARAM:blMin}", "${PARAM:blMax}"]  # quoted: { } in a flow list
      pointsThreshold: 3
      observables: ["all"]
```

| Legacy | restv2 |
|--------|--------|
| `<TRestProcessRunner>` | section of class `TRestEventProcessManager` |
| `<addProcess type="X" name="n">` | `n: {class: "X", ...}`, in order inside the pipeline |
| `value="ON"/"OFF"` of a process | no such key: remove or comment out the process |
| `<addTask type="processEvents">` | nothing: TRestManager runs its pipeline, then its plots |
| `threadNumber` | `nThreads` |
| `verboseLevel` | `verbose` (see [2.3](#23-values-units-options-and-verbose)) |
| `title`, `store`, `type` (of a section) | not keys (an error); `name` and `verbose` are accepted by every section |
| `observable="all"` or `<observable name="x" value="ON"/>` | `observables: ["all"]` or a list of names; default `none` |
| `<cut name="obs" value="(a,b)"/>` in a process | `cuts: ["name"]`, names of `TRestCut` sections ([2.7](#27-cuts-selection-and-plots)) |
| `"(a,b)"`, `"(x,y,z)"` | `[a, b]`, `[x, y, z]` |
| `<if condition="${X}==a">` | a `select` block on a parameter: `select: "${PARAM:x}"` and `cases` (only the case chosen is stored) |
| `<for>`, math expressions (`"72*4*4"`, `2*${E}`) | not supported: write the values; use `parameters` and `--set` for variations |

Parameter names of several classes were renamed or removed; the class Doxygen (`.cxx`) lists the keys
and the differences with legacy. An unknown key is reported with the list of the valid ones
([section 5](#5-common-issues)).

### 2.2 Variables, parameters and includes

| Legacy | restv2 |
|--------|--------|
| `<globals><variable name="X" value="v" overwrite="false"/></globals>`, used as `${X}` | `parameters:` at the root of the file (`x: v`, a number with or without units, or a text), used as `${PARAM:x}`; an undefined one is an error |
| Variable overridden by an environment variable | `--set x=value` (repeatable, all executables); a name not in `parameters` and a parameter used nowhere are errors |
| Environment variables `${HOME}` | `${ENV:HOME}` (empty if undefined) |
| Reference to other values | `${a.b}`: the scalar at path `a.b` of the same file |
| `<TRestRun file="run.xml"/>`, `<include file="..."/>` | `include: "file.yaml"` or `include: "file.yaml:key"` inside a map; local keys override (deep merge of maps), paths relative to the including file |
| `[TRestRun::fRunTag]`, `[fRunNumber]` in names | `[runTag]`, `[runNumber]`, `[runInfo.key]`: keys of the run as written in the YAML ([2.8](#28-run-parameters-file-names-and-merging)) |
| `${GDML_GAS}`... in GDML files | GDML `<constant>` (numbers) and internal entities (`<!ENTITY gas "XenonNeon">`, `&gas;`, texts) with defaults; a parameter with the same name replaces them ([2.12](#212-restg4)) |

The `parameters` section is removed when the configuration is resolved: the files store the resolved
configuration (and geometry), not the parameters.

### 2.3 Values, units, options and verbose

- **Units**: the REST units are those of legacy (keV, us, mm, V, T, bar, rad...), but the unit is written in
  the value (`subEventTimeDelay: 100 us`, `driftField: 1 kV/cm`) instead of a `units="us"` attribute. In a
  list each element has its own unit (`energyRangeStored: [0, 20 MeV]`); a number without unit is in REST
  units. An unknown unit or a non-number in a value with units is an error (legacy gave 0 or factor 1).
- **Booleans** are YAML booleans (`true`/`false`).
- **Text options** with a fixed set of values (`baselineOption`, `method`, `triggerMode`, `state`,
  `projections`, `peakModel`...) are case insensitive and normalised to their documented spelling, also in
  the stored YAML; an unknown value is an error listing the options (legacy compared exactly, some
  processes lowered the case). Names (files, volumes, particles, observables) keep their case.
- **Verbose**: `verbose` (silent, error, warning, info, debug; legacy `essential` is `warning`, `extreme`
  is `debug`) is read by every metadata. Without it, the metadata of a container (manager, pipeline) take
  the one of the container, the others the global one (`restManager --v`).

### 2.4 Running

`restManager --c config.yaml` as legacy, with these differences (full list in
[source/bin/README.md](source/bin/README.md)):

| Legacy | restv2 |
|--------|--------|
| `--i`/`--f` input: a file, a pattern or a `.list` (several files chained) | `--i` one file per pipeline (a pattern only for plots and datasets); merge files with `REST_MergeRuns` or run each one |
| `--o`, `--e`, `--j`, `--v` | same (`--o`, `--e`, `--j` = `nThreads`, `--v`) |
| `--d RUNID` | `runNumber` in the YAML (`auto`: a new number from `$REST_HOME/.rest/runNumber`) or from the input file name ([2.8](#28-run-parameters-file-names-and-merging)) |
| `--p plots.pdf` | `save` of a `TRestAnalysisPlot` |
| `--batch`, `--fork`, `restManager TASK_NAME ARGS` | removed (no TRestTask; macros in restRoot) |
| | new: `--set name=value`, `--r process=dataset` (reference of a dataset process), `--maxRepeated`, `--repeatedInterval` |

`restRoot` opens a REST file given as argument (`_run` and `_ev`, or `_dataSet` for a dataset) and loads
the compiled macros. Renamed or replaced legacy macros:

| Legacy macro | restv2 |
|--------------|--------|
| `REST_MergeFiles`, `REST_Geant4_MergeRestG4Files` | `REST_MergeRuns(filePattern, outputFile)` |
| `REST_GenerateDataSets` | `REST_GenerateDataSet(yamlFile, dataSetName, outputFile, inputFiles)` or restManager |
| `REST_RefitGainMap` | `TRestDataSetGainMap::ReadFromFile`, `Refit`, `UpdateFile` in restRoot |
| `REST_ViewEvents`, `REST_OpenInputFile` | same names |
| `REST_Detector_GenerateGasFile` | restGarfield ([2.13](#213-restgarfield-and-restdaq)) |
| `REST_Detector_ViewReadout` | `REST_Detector_ViewMicromegasReadout`, `REST_Detector_ViewGDMLReadout` |
| `REST_Geant4_FindIsotopes`, `REST_Geant4_FindGammasEmitted` | `REST_Geant4_ListIsotopes` |
| `REST_Geant4_ReadNEvents(_inROI)`, `REST_Geant4_GetROIEvents(_Fiducial)` | `REST_Geant4_ReadEvents` |
| `REST_Geant4_ViewEvent`, `REST_Geant4_ViewGeometry`, `REST_Geant4_GenerateResponseMatrix` | same names |

The other legacy macros are not migrated.

### 2.5 Output files

The structure of a run file and of a dataset file is described in the [user guide](doc/UserGuide.md). The
differences with legacy:

- one tree per stored event type, named as its class (`TRestRawSignalEvent`, `TRestDetectorHitsEvent`...)
  with flat branches, instead of one `EventTree` with streamed objects;
- `AnalysisTree` is a plain TTree; its observables keep the legacy names `processName_observable`;
- the metadata are YAML strings in `RESTMetadataStore` (the resolved configuration, with the values used
  and the results), not streamed objects; the geometry (`Geometry`) and the readouts (`Readouts`) are
  ROOT objects next to them;
- the output event of the last process is recorded in the TRestRun (`outputEvent`), so the browser, the
  macros and `TRestRun::GetEvent` open the file with it;
- without `outputFileName` (or with `"Null"`) the output is `/dev/null`: the pipeline runs, nothing is
  written. An `outputFileName` that resolves to the input file is an error.

### 2.6 Pipelines and observables

- The event route is not configured: each process declares its input and output event classes. The input
  of a process is the output of the latest previous process producing that event, otherwise the event of
  the input file (an error before the loop if neither exists). Each process has its own output event.
- Storage: `outputEventStorage: true` stores one tree per output event type (the last process of each
  type), `false` only the final event; `inputEventStorage: true` also stores the events of the input file;
  `inputAnalysisStorage: false` drops the observables of the input file from the output analysis tree.
- Threads: with `nThreads > 1` the entries are split among independent managers, each writing its own
  file, merged at the end. A pipeline starting with a generator or with a process reading an external file
  of unknown length (e.g. `.aqs`) runs single-thread.
- Observables: the `observables` list uses the bare names (`OriginX`); the branches are
  `processName_observable` (`trackLineAnalysis_OriginX`) as legacy. A name that the process does not give
  with its configuration (misspelled, or optional and not enabled) is an error listing the available ones
  (legacy ignored it). The default is `none`.
- A process reads an observable of the input file or of a previous process with
  `TRestRun::GetObservable<T>(fullName)` (legacy `TRestAnalysisTree::GetObservableValue<T>`); T must be the
  branch type.
- **Channel selection** is only done by `TRestRawSignalChannelSelectionProcess` (legacy
  `TRestRawSignalRemoveChannelsProcess`: `removeChannelIds`/`removeChannelRange`, and
  `keepChannelIds`/`keepChannelRange`). The processes have no `signalsRange` nor `channelType`: each
  detector (e.g. Micromegas and vetoes) is analysed in its own run, and the analysis trees are combined by
  event ID. The veto analysis (legacy `TRestRawVetoAnalysisProcess`) is
  `libraries/raw/examples/VetoAnalysis.yaml`; its flags and groups are RDataFrame expressions
  (`Any(vetoAna_AmpSgnMaxMethod > 500)`).

### 2.7 Cuts, selection and plots

- **`TRestCut`**: `conditions`, name → C++ expression of analysis tree branches, all fulfilled (legacy
  `<cut variable=... condition=...>` elements and AND/OR):

  ```yaml
  fiducial:
    class: "TRestCut"
    conditions:
      originX: "trackLineAnalysis_OriginX > 20 && trackLineAnalysis_OriginX < 225"
  ```

  Any process accepts `cuts: ["fiducial"]`: after the process the event is rejected if it does not pass
  them (scalar observables of the process, of the previous ones and of the input file).
- **`TRestSelectionProcess`** (legacy `TRestEventSelectionProcess`): only applies its `cuts`, for any event
  type (`event`); no list of event IDs.
- **`TRestAnalysisPlot`** (legacy `TRestDataSetPlot` and `TRestAnalysisPlot`): canvas of panels and
  histograms of the analysis trees of a file, a pattern of files or a dataset, filled with one RDataFrame
  loop. Its input is its `inputFileName` (or `--i` in a manager without TRestRun); without it, TRestManager
  draws the output of its pipeline after running it. Labels are texts with `{expression|format}`
  (variables, metadata `run.key`, `mean/sigma/min/max(observable)`), replacing the legacy keys variable,
  metadata, observable and expression. Cuts are TRestCut names at global, panel and histogram level. It
  writes in its own `outputFileName` (not the run one); `save` writes an image as legacy `canvas save`.
  Example `libraries/track/examples/TrackLinePlots.yaml`; keys in `TRestAnalysisPlot.cxx`.

### 2.8 Run parameters, file names and merging

- What is written in the TRestRun section always wins. Each of runNumber, subRunNumber, runType, runUser
  (legacy `user`), runTag, runDescription and experimentName not given is taken from the input file name
  if `inputFormat` has its tag (e.g. `Run[runNumber]_[runTag].root`), otherwise, with
  `inheritInputRun: true`, from the TRestRun of the input file. Legacy `"preserve"` is an error.
  `overwrite` and `readOnly` do not exist.
- `runInfo`: free information of the run (e.g. `meshVoltage: 300 V`), copied from the input file and
  usable as `[runInfo.key]`, in labels and in dataset conditions.
- File patterns: `[key]` is a key of the run as written in the YAML (`[runNumber]`, `[runTag]`,
  `[runInfo.key]`); legacy member names (`[fRunTag]`) and unknown keys are errors.
- `REST_MergeRuns(pattern, output)` (legacy `REST_MergeFiles`) only merges consecutive subruns of one
  run with the same trees and processing (the TRestRun gets `mergedSubRuns`), and simulations of the same
  configuration with different seeds (legacy `REST_Geant4_MergeRestG4Files`: counts added, event IDs
  shifted).

### 2.9 Datasets

The usage of `TRestDataSet` is described in the [user guide](doc/UserGuide.md). Differences with legacy:

| Legacy | restv2 |
|--------|--------|
| File names and relevant quantities (`quantity` with accumulate/unique/max...) | The description of every run (`TRestRunInfo`) and a `runIndex` column per entry; totals (`GetTotalTime`...) computed from the runs |
| `filter` (contains/greaterThan/lowerThan/equalsTo), `startTime`/`endTime` | `runConditions`: expressions on the run parameters (`runInfo.meshVoltage > 300 V`, `startTime > date("2022/04/28")`) |
| `cut` elements | `cuts`: TRestCut names |
| `addColumn` | `columns`: name → expression, may use run parameters |
| `processObservables: final` | `observables: ["final_*"]` (patterns) |
| Merging keeps the metadata of the first dataset | Datasets merged with their runs; a repeated run is an error |
| Metadata of the first file, not checked | Processing, readouts and geometry of the first run; Generate throws if the runs differ in processes, analysis tree branches, readouts or geometry (process parameters may differ: `RESTDataSetProcessing`) |
| Temporary files; txt/csv export | One Snapshot; ROOT output only |
| `REST_GenerateDataSets` | `REST_GenerateDataSet` or a TRestDataSet section in restManager (`--i` = `filePattern`, `--o` = `outputFileName`) |
| `TRestDataSetPlot` | `TRestAnalysisPlot` with the dataset as `inputFileName` |

`filePattern` are the files a dataset is generated from; `inputFileName` is a dataset already generated,
opened as it is. Requires ROOT 6.26 or later. The sensitivity classes are not migrated.

### 2.10 Dataset processes

Legacy `TRestDataSetCalibration`, `TRestDataSetGainMap` and `TRestDataSetOdds` were standalone metadata
called from macros (`calibFile`, `oddsFile`). In restv2 they are dataset processes run by restManager in a
`TRestDataSetProcessManager` section (`inputFileName`/`--i` a dataset, processes in order,
`outputFileName`/`--o`):

- each process computes its parameters on `referenceFileName` (a dataset; empty: the dataset processed;
  a pattern with `referenceSelection: closest`; `--r process=dataset`) with the `cuts` of the manager, and
  adds columns `<name>_<observable>` (`calib_Energy` as legacy). If the reference was processed by a
  process with the same name and class, its stored results are reused;
- results are stored in the YAML of each process and objects (spectra, fits, PDFs) in
  `RESTDataSetStore/<class>/<name>`;
- `TRestDataSetGainMap` covers a single readout (one gain map per readout), without modules (no
  `planeId`, `moduleId`, `definitionCut`); its extent is the bounding box of the channels of the readout;
- `TRestDataSetLogOdds` (legacy `TRestDataSetOdds`) adds `<name>_<observable>` and `<name>_total`;
- new: `TRestDataSetTMVA` (from the unmerged framework pull request #448) and
  `TRestDataSetSignalSelection`;
- peak fits use RooFit (`TRestPeakFit`, gaussian or Crystal Ball, optional background) instead of
  TSpectrum; `autoRangePeaks` is replaced by the optional `peakSearch`.

Examples: `libraries/detector/examples/DataSetCalibration.yaml`, `examples/DataSetLogOdds.yaml`,
`examples/DataSetTMVA.yaml`, `examples/DataSetSignalSelection.yaml`.

### 2.11 Readouts, medium and detector processes

- **Readouts**: legacy `TRestDetectorReadout` (planes, modules, channels, pixels defined in RML) is
  replaced by `TRestMicromegasReadout` (strips from `readoutParameters` and a decoding file) and
  `TRestGDMLReadout` (one channel per GDML volume matching `volume`, optional `readoutVolume` where the
  signal is collected; legacy `channelPrefix`). A readout is built once with
  `REST_Detector_GenerateMicromegasReadout` / `REST_Detector_GenerateGDMLReadout` into a ROOT file, and
  loaded by a `TRestDetectorReadoutManager` section (`readouts`, and `readout`: the readout of the data of
  the file). Examples in `libraries/detector/examples`.
- **One detector per file**: legacy had one readout with several planes/modules selected by the
  processes. In restv2 the manager loads all the readouts (for viewing), but each file holds the data of
  one: split simulations with the `volumes` of `TRestGeant4ToDetectorHitsProcess` and acquired data with
  `TRestRawSignalChannelSelectionProcess`.
- **Medium**: legacy `TRestDetectorGas` (`.gas` files generated or downloaded inside the pipeline) and
  `TRestDetectorDriftVolume` are replaced by `TRestDetectorMedium` (`state`, `components`, `pressure`,
  `temperature`, `mediumFile`) with its `charge` response (`wValue`, `fanoFactor`, `driftField`, fixed
  `driftVelocity`/`longDiff`/`transDiff`/`attachment`, or transport and gain tables of a medium file
  generated by restGarfield) and `light` response (`lightYield`, `attenuationLength`, `effectiveLightSpeed`). It is a
  metadata of the manager, found by the processes by name (`medium`). Legacy `.gas` files are not read.
- **Process changes** (details in the Doxygen of each class and in [section 3](#3-class-equivalences)):
  - `TRestDetectorHitsDriftProcess` replaces `TRestDetectorElectronDiffusionProcess` and
    `TRestDetectorGarfieldDriftProcess` (gaussian diffusion from the medium; attachment exp(−η·d), legacy
    (1 − η)^(d/cm)); no microscopic drift (restGarfield);
  - `TRestDetectorAvalancheProcess` has a new meaning: gain and Polya θ from a gain table of the medium;
    legacy `detectorGain` with a gaussian resolution is `TRestDetectorHitsSmearingProcess`;
  - `TRestDetectorHitsToSignalProcess` rejects hits outside the drift volume or without channel (legacy
    `TRestDetectorFiducializationProcess`); `TRestDetectorSignalShapingProcess` (new) shapes before the
    digitization, which `TRestDetectorSignalToRawSignalProcess` no longer does (nor noise: use
    `TRestRawSignalAddNoiseProcess` after);
  - `TRestDetectorHitsTransformationProcess` replaces the normalization, translation, rotation and
    specular processes;
  - `TRestDetectorSignalToHitsProcess` takes the drift plane and sense from the readout, the drift
    velocity from the medium when `driftVelocity` is not given, and smooths the pulse before
    onlyMax/tripleMax/tripleMaxAverage (`smoothingPoints`, 5 by default; legacy did not smooth);
  - `TRestDetectorSignalRecoveryProcess` takes the neighbours from the readout
    (`GetAdjacentChannels`), recovers any number of consecutive dead channels and preserves the energy;
  - seeded processes (`seed`) re-seed at each event from (seed, eventID, subEventID): the result does not
    depend on the number of threads.
- Event viewer: the geometry is in mm (`geomScale` 1 by default, legacy 0.1).

### 2.12 restG4

`restG4 config.yaml` reads a section of class `TRestGeant4Manager` with a `TRestRun`, a
`TRestGeant4Metadata` and a `TRestGeant4PhysicsLists` (legacy `<restG4>` RML). All the examples of
`packages/restG4/examples` are migrated (the `.C` validations are GoogleTest tests in
`packages/restG4/test`). Main changes, from `08.Alphas`:

| Legacy RML | restv2 YAML |
|------------|-------------|
| `<parameter name="gdmlFile" .../>` | `gdmlFilename` |
| `<detector><parameter name="energyRange" value="(0,2*${E})" units="MeV"/>` | `energyRangeStored: [0, 10 MeV]` |
| `<detector><volume name="gas" sensitive="true" maxStepSize="0.5mm"/>` | `activeVolumes: [{name: "gas", sensitive: true, maxStep: 0.5 mm}]` (no `sensitiveVolumes` list; only active volumes can be sensitive) |
| `<generator type="volume" from="source"><source particle="alpha"><energy type="mono" .../>` | `generator: {type: "volume", from: "source", source: {particle: "alpha", energy: {type: "mono", energy: 5 MeV}, ...}}` |
| `<physicsList name="X"><option name="ICM" value="true"/>` | `physicsList: [{name: "X", option: [{name: "ICM", value: "true"}]}]` |
| `${REST_ENERGY}` | `parameters` + `${PARAM:energy}`, `--set energy=1` |

- **Generator**: the keys are the legacy ones, but each section is read by the class of its `type`
  (spatial: point, volume, surface, cosmic, cry; energy and angular: mono, flat, log, formula, TH1D,
  formula2, TH2D, cosmic). Removed: back to back, the `custom` and `source` types; a Decay0 source is
  `use: "file.dat"`. Options and differences in `libraries/geant4/src/TRestGeant4PrimaryGenerator.cxx`.
- **CRY**: legacy `TRestGeant4ParticleSourceCry` called CRY during the simulation. Now `restCRY` (optional,
  built with CRY) writes the showers and their exposure, and the generator `type: "cry"` simulates each
  shower once, storing `showersUsed` and `exposureTime` (examples in `packages/restG4/examples/12.Generators`).
- **GDML**: no environment variables; `<constant>` and internal entities with defaults, replaced by the
  parameters with the same name. `restG4 config.yaml --update-gdml` writes the parameters into the GDML
  files so that viewers show the geometry simulated. The examples share
  `packages/restG4/examples/materials.xml` (fixed mixtures named by fractions and pressure, e.g.
  `Argon1%Isobutane1.4bar`) instead of parametrised materials (`gasPressure`, `targetGasDensity`).
- **Biasing**: `biasingVolumes` (chained stages in one execution, outer to inner; example
  `09.Pb210_Shield/Biasing.yaml`).
- **`TRestGeant4Event` branches** are flat numbers: positions and directions as X, Y, Z floats, particle,
  process and volume names as IDs of the tables of the metadata (getters return the names).
- **Analysis**: `TRestGeant4AnalysisProcess` defines its observables by `selections` of volumes
  (regular expressions searched in the names: `"gas"` matches any name containing it, `"^gas$"` only that
  one), particles and processes, one value per event (`<name>_Energy`, `_MeanPosX`...; `entering`,
  `perVolume`, `interactions`), instead of observables parsed from their names (`<volume>VolumeEDep`...).
  It replaces `TRestGeant4VetoAnalysisProcess` and `TRestGeant4NeutronTaggingProcess`. Global and primary
  observables keep the legacy names. Example `libraries/geant4/examples/Geant4Analysis.yaml`.
- The geometry is stored in every run derived from a simulation (legacy: only in the restG4 file).

### 2.13 restGarfield and restDAQ

- **restGarfield** (`packages/restGarfield`, optional, the only part linked to Garfield++) replaces the
  legacy gas file generation (`TRestDetectorGas`, `REST_Detector_GenerateGasFile`): `restGarfield
  config.yaml` reads a `TRestDetectorTransportManager` section (the `medium` and the `transport` and `gain`
  tables to compute) and writes a medium file in REST format, read by `TRestDetectorMedium` without
  Garfield++. Example `packages/restGarfield/examples/ArIso.yaml`; `REST_Garfield_DrawMedium` draws it.
- **restDAQ** (`packages/restDAQ`, option `REST_DAQ`) is the FEMDAQ program (`femclient`, not part of
  legacy REST) writing REST files directly:

| FEMDAQ | restDAQ |
|--------|---------|
| `femclient -c config.yml` (CLI11) | `femclient config.yaml` (also `-c`; `-e`, `--read-only`, `--tcm`, `--set`) |
| `run:` rawDataPath, experiment, tag, type, Info | TRestRun: mainDataPath, experimentName, runTag, runType, runInfo, outputFileName |
| `run:` electronics, FEM, TCM, nEvents, time, maxFileSize, updateRate | TRestRawDAQMetadata (a new class: legacy `TRestRawDAQMetadata` is not migrated) |
| Run number: largest RunNNNNN + 1 | `runNumber: auto`, one per `fopen` |
| Tree `SignalEvent` and TObjString configuration | REST run file: tree `TRestRawSignalEvent` written as `TRestRawDAQSignalEvent` (FEM IDs, counters, timestamps) |
| `EventDisplay.C`, `DrawPulses.C`, `AnalyzePulses.C` | `TRestDAQViewer` (`REST_DAQViewer`), `TRestRawPulsesViewer` (`REST_Raw_PulsesViewer`), `TRestRawSignalAnalysisProcess` |

Old FEMDAQ files are still converted by `TRestRawFemDAQToSignalProcess`.

## 3. Class equivalences

Status from [MIGRATION_STATUS.md](MIGRATION_STATUS.md). "Same name" means ported to the restv2 API,
usually with changes documented in its `.cxx`; "not migrated" means a decision not to migrate (unless
noted). The `axion`, `wimp` and `model` libraries, `masks` and the legacy packages restSQL and restP3DB
are not migrated.

### Core and tools (`source/core`, `source/tools`)

| Legacy | restv2 |
|--------|--------|
| TRestEvent, TRestMetadata, TRestEventProcess, TRestRun, TRestManager | Same names, new API ([section 4](#4-developer-guide-porting-a-legacy-class)); TRestRun derives from the new TRestRunInfo |
| TRestProcessRunner, TRestThread | TRestEventProcessManager (TRestManager runs the threads) |
| TRestAnalysisTree | Plain TTree `AnalysisTree` |
| TRestTask | Not migrated (macros and TRestAnalysisPlot) |
| TRestCut | Same name, redesigned (`conditions`) |
| TRestDataSet | Same name, redesigned |
| TRestAnalysisPlot, TRestDataSetPlot | TRestAnalysisPlot (redesigned, RDataFrame) |
| TRestHits, TRestVolumeHits | TRestHits (a view over TRestHitsData, with the volume hits) |
| TRestBrowser, TRestEventViewer, TRestEveEventViewer | Same names |
| TRestGDMLParser | Namespace TRestGDMLParser (tools) |
| TRestSystemOfUnits | Same header, namespace REST_Units, TRestWithUnits for configuration fields |
| TRestTools | Namespace TRestTools |
| TRestStringHelper, TRestStringOutput, TRestReflector, TRestVersion | TRestTools, TRestYAMLUtils, TRestLogManager, TRestMetadataFieldRegistry |
| TRestPhysics | TRestConstants (and TRestTools) |
| TRestDataBase | To be migrated, postponed until a real database exists (`runNumber: auto` covers the run number) |
| TRestMesh, TRestMetadataPlot, TRestMessenger, TRestComplex | Not migrated |
| — | New: TRestRunInfo, TRestMetadataStore, TRestSelectionProcess, TRestDataSetProcess, TRestDataSetProcessManager, TRestYAMLUtils |

### Analysis (`framework/analysis` → `source/analysis`, `source/core`)

| Legacy | restv2 |
|--------|--------|
| TRestEventSelectionProcess | TRestSelectionProcess (core) |
| TRestDataSetCalibration | TRestDataSetCalibration (dataset process) |
| TRestDataSetGainMap | TRestDataSetGainMap (dataset process, detector library) |
| TRestDataSetOdds | TRestDataSetLogOdds (dataset process) |
| TRestDataSetTMVA, TRestDataSetTMVAClassification (framework PR #448) | TRestDataSetTMVA |
| TRestEventRateAnalysisProcess, TRestEventTimeSelectionProcess, TRestSummaryProcess, TRestDataQualityProcess, TRestDataQualityRules, TRestBenchMarkProcess, TRestRealTimeAddInputFileProcess, TRestRealTimeDrawingProcess, TRestMySQLToAnalysisProcess | Not migrated |
| Sensitivity: TRestComponent(DataSet/Formula), TRestResponse, TRestExperiment(List), TRestSensitivity | Not migrated |
| — | New: TRestDataSetSignalSelection; helpers TRestPulseShapeAnalysis, TRestHitsAnalysis, TRestPeakFit, TRestFFTAnalysis |

<a id="raw"></a>

### Raw (`libraries/raw`)

| Legacy | restv2 |
|--------|--------|
| TRestRawSignalEvent | Same name (storage TRestRawSignalData + TRestRawSignal views) |
| TRestRawSignal | View over the event storage; its analysis methods are in TRestPulseShapeAnalysis |
| TRestRawSignalAnalysisProcess, TRestRawSignalAddNoiseProcess, TRestRawCommonNoiseReductionProcess, TRestRawSignalShapingProcess, TRestRawSignalRecoverSaturationProcess, TRestRawZeroSupressionToRawProcess, TRestRawBaseLineCorrectionProcess | Same names (no `signalsRange` nor `channelType`) |
| TRestRawSignalRemoveChannelsProcess | TRestRawSignalChannelSelectionProcess (keep/remove ids and ranges) |
| TRestRawFemDAQToSignalProcess, TRestRawMultiFEMINOSToSignalProcess, TRestRawFeminosRootToSignalProcess | Same names |
| TRestRawToSignalProcess (base of decoders) | Not migrated (decoders derive from TRestEventProcess) |
| TRestRawAFTERToSignalProcess, TRestRawMultiCoBoAsAdToSignalProcess, TRestRawFEUDreamToSignalProcess, TRestRawTDSToSignalProcess, TRestRawUSTCToSignalProcess, TRestRawBiPoToSignalProcess, TRestRawMemoryBufferToSignalProcess | Not migrated |
| TRestRawPeaksFinderProcess | TRestDetectorSignalPeaksFinderProcess (detector) |
| TRestRawVetoAnalysisProcess | Not migrated: TRestRawSignalChannelSelectionProcess + TRestRawSignalAnalysisProcess (`examples/VetoAnalysis.yaml`) |
| TRestRawSignalFittingProcess, TRestRawSignalConvolutionFittingProcess, TRestRawSignalGeneralFitProcess, TRestRawFindResponseSignalProcess, TRestRawSignalChannelActivityProcess, TRestRawSignalIdTaggingProcess, TRestRawSignalRangeReductionProcess, TRestRawBiPoAnalysisProcess, TRestRawReadoutMetadata, TRestRawDAQMetadata, TRestRawFFT | Not migrated (TRestRawDAQMetadata of restv2 is a new class of restDAQ; FFT in TRestFFTAnalysis) |
| TRestRawSignalViewerProcess | Not migrated (DrawEvent, TRestBrowser) |
| — | New: TRestRawDAQSignalEvent, TRestRawDAQMetadata, TRestRawPulsesViewer |

### Detector (`libraries/detector`)

| Legacy | restv2 |
|--------|--------|
| TRestDetectorHitsEvent, TRestDetectorSignalEvent | Same names (storage + views) |
| TRestDetectorSignal | View over TRestDetectorSignalData |
| TRestDetectorReadout, ReadoutPlane, ReadoutModule, ReadoutChannel, ReadoutPixel, ReadoutMapping, TRestDetectorGeometry, TRestDetectorSetup | TRestDetectorReadout (base), TRestMicromegasReadout, TRestGDMLReadout, TRestFibreReadout (new: fibre lattices), TRestDetectorReadoutManager |
| TRestDetectorGas, TRestDetectorDriftVolume | TRestDetectorMedium (with TRestDetectorChargeResponse, TRestDetectorLightResponse; tables TRestDetectorTransportTable, TRestDetectorGainTable, TRestDetectorTransportManager) |
| TRestDetectorHitsAnalysisProcess, TRestDetectorHitsSmearingProcess, TRestDetectorHitsToSignalProcess, TRestDetectorSignalToHitsProcess, TRestDetectorSignalRecoveryProcess, TRestDetectorLightAttenuationProcess | Same names |
| TRestDetectorAvalancheProcess | Same name, new meaning (gain tables) |
| TRestDetectorElectronDiffusionProcess, TRestDetectorGarfieldDriftProcess | TRestDetectorHitsDriftProcess |
| TRestDetectorFiducializationProcess | TRestDetectorHitsToSignalProcess |
| TRestDetectorHitsNormalizationProcess, TranslationProcess, RotationProcess, SpecularProcess | TRestDetectorHitsTransformationProcess |
| TRestDetectorHitsGaussAnalysisProcess, TRestDetectorHitsReadoutAnalysisProcess | Not migrated: TRestDetectorHitsAnalysisProcess (`gaussFit`, FiducialEnergy) |
| TRestDetectorHitmapAnalysisProcess | Not migrated: TRestDetectorHitsTransformationProcess + MeanX/Y/Z |
| TRestDetectorHitsReductionProcess | Not migrated: TRestTrackReductionProcess |
| TRestDetectorSingleChannelAnalysisProcess, TRestDetectorGainMap, TRestDetectorPositionMappingProcess | Not migrated: TRestDataSetGainMap |
| TRestDetectorSignalChannelActivityProcess | Not migrated: TRestAnalysisPlot of SignalsID/SignalsPhysicalID (`connectors/examples/ChannelActivity.yaml`) |
| TRestDetectorHitsShuffleProcess, TRestDetectorHits3DReconstructionProcess, TRestDetectorTriggerAnalysisProcess, TRestDetectorDaqChannelSwitchingProcess, TRestDetector | Not migrated |
| TRestDetectorHitsEventViewer, TRestDetectorReadoutEventViewer, TRestDetectorSignalViewerProcess | Not migrated (DrawEvent, TRestBrowser, TRestEventViewer) |
| — | New: TRestDetectorHitsDriftProcess, TRestDetectorSignalShapingProcess, TRestDetectorSignalPeaksFinderProcess, TRestDataSetGainMap |

### Track (`libraries/track`)

| Legacy | restv2 |
|--------|--------|
| TRestTrackEvent, TRestTrack | Same names; no track levels: an event holds the tracks of one stage (`parentID` = track of the input event) |
| TRestTrackAnalysisProcess, TRestTrackLineAnalysisProcess | Same names, readout independent (`projections`), renamed observables (see their `.cxx`) |
| TRestTrackReductionProcess, TRestTrackLinearizationProcess, TRestTrackBlobAnalysisProcess | Same names |
| TRestTrackPathMinimizationProcess | Same name, own shortest path solver (no Concorde) |
| TRestTrackReconnectionProcess | Not migrated: Branches observable of TRestTrackPathMinimizationProcess |
| TRestTrackPointLikeAnalysisProcess | Not migrated: Size and MaxNodeSize of TRestTrackAnalysisProcess |
| TRestTrack2DAnalysisProcess, TRestTrack3DAnalysisProcess, TRestTrackDetachIsolatedNodesProcess | Not migrated |
| TRestTrackViewerProcess | Not migrated (DrawEvent, TRestBrowser) |

### Geant4 (`libraries/geant4`)

| Legacy | restv2 |
|--------|--------|
| TRestGeant4Event, TRestGeant4Track, TRestGeant4Metadata, TRestGeant4PhysicsLists, TRestGeant4PhysicsInfo, TRestGeant4GeometryInfo, TRestGeant4PrimaryGeneratorInfo, TRestGeant4BiasingVolume, TRestGeant4EventViewer | Same names |
| TRestGeant4Hits, TRestGeant4Particle | Integrated in TRestGeant4Event / TRestGeant4Track (TRestGeant4ParticleState) |
| TRestGeant4ParticleSource | Same name; energy and angle are TRestGeant4EnergyDistribution / TRestGeant4AngularDistribution (one class per `type`), position TRestGeant4SpatialGenerator; sampled by TRestGeant4PrimaryGenerator |
| TRestGeant4ParticleSourceCry | restCRY + TRestGeant4CRYShowers + generator `cry` (TRestGeant4SpatialGeneratorCRY) |
| TRestGeant4ParticleSourceCosmics, TRestGeant4ParticleSourceDecay0 | Not migrated as classes: `cosmic`/TH2D distributions; Decay0 files with `use` |
| TRestGeant4AnalysisProcess, TRestGeant4QuenchingProcess | Same names (analysis redesigned with `selections`) |
| TRestGeant4VetoAnalysisProcess, TRestGeant4NeutronTaggingProcess | Not migrated: TRestGeant4AnalysisProcess (`perVolume`, `interactions`) + TRestGeant4QuenchingProcess |
| TRestGeant4BlobAnalysisProcess | Not migrated |
| — | New: TRestGeant4Manager, TRestGeant4CRYShowers |

### Connectors (`libraries/connectors`)

| Legacy | restv2 |
|--------|--------|
| TRestRawToDetectorSignalProcess | Same name (`baselineRange`, `pointsThreshold`, `pointsOverThreshold`; new SignalsID, SignalsPhysicalID observables) |
| TRestDetectorSignalToRawSignalProcess | Same name (no shaping nor noise inside; `mode`, trigger `observable`) |
| TRestGeant4ToDetectorHitsProcess, TRestDetectorHitsToTrackProcess | Same names |
| TRestDetectorHitsToTrackFastProcess | Not migrated (TRestDetectorHitsToTrackProcess is as fast) |
| TRestTrackToDetectorHitsProcess, TRestRawReadoutAnalysisProcess, TRestRawReadoutMetadataProcess | Not migrated |
| — | New: TRestDAQViewer |

### Packages

| Legacy | restv2 |
|--------|--------|
| restG4 | `packages/restG4` (restG4, and restCRY) |
| Gas generation in TRestDetectorGas | `packages/restGarfield` |
| FEMDAQ (separate program) | `packages/restDAQ` |
| restSQL, restP3DB | Not migrated |
| `libraries/legacy` (TRestLegacyProcess, TRestRawSignalRecoverChannelsProcess, TRestRawZeroSuppresionProcess) | Not migrated (already obsolete) |

## 4. Developer guide: porting a legacy class

The examples below are condensed from real classes: `TRestDetectorHitsEvent`, `TRestCut`,
`TRestDetectorHitsSmearingProcess` and `TRestDataSetLogOdds`, with their legacy counterparts. Read the
restv2 class closest to yours before starting.

### 4.1 Where the files go

- A class goes to the component of the classes it needs: `source/core` (base framework),
  `source/analysis` (high-level analysis and algorithms; above core), a library in `libraries/`
  (`inc/X.h`, `src/X.cxx`), `libraries/connectors` (processes between two libraries, built only if both
  are), or a package (executables needing external software: Geant4, Garfield++, CRY).
- Generic algorithms go to namespaces working on plain data (`TRestPulseShapeAnalysis` for pulses,
  `TRestHitsAnalysis` for points and energies), not into processes, so any class can use them.
- Each component has `README.md` (list the class there), `examples/` (a YAML using it), `macros/`
  (compiled and available in restRoot) and `test/`.
- A new library also needs its `add_subdirectory`, its entry in `REST_LIBRARIES` and in the test loop of the
  top `CMakeLists.txt`.

### 4.2 Events

Legacy events were TObjects streamed as one object per event, with ROOT containers inside:

```cpp
// Legacy
class TRestDetectorHitsEvent : public TRestEvent {   // TRestEvent : public TObject
    TRestHits* fHits;                                 //-> streamed object
   public:
    virtual void Initialize();
    TPad* DrawEvent(const TString& option = "");
    ClassDef(TRestDetectorHitsEvent, 1);
};
```

In restv2 an event derives from `TRestEvent` (not a TObject), keeps its data in a **storage** of flat
vectors written as branches, and offers **views** over it for the analysis methods:

```cpp
// restv2
class TRestDetectorHitsEvent : public TRestEvent {
    TRestHitsData fHitsData;                         // storage: x, y, z, time, energy, type vectors
    mutable TRestHits fHits{&fHitsData, 0, 0};       // view over the storage
    std::vector<float>* fPtrX = nullptr;             //! branch addresses for reading
    ...
   public:
    std::string GetClassName() const override { return "TRestDetectorHitsEvent"; }
    void Initialize() override {
        TRestEvent::Initialize();                    // resets the event info
        fHitsData.clear();
        RebuildHitsView();
    }
    void CreateBranches(TTree* tree) override {
        TRestEvent::CreateBranches(tree);
        tree->Branch("fHitX", &fHitsData.x);
        ...
    }
    void SetBranchAddresses(TTree* tree) override {
        TRestEvent::SetBranchAddresses(tree);
        fPtrX = &fHitsData.x;
        BindBranch(tree, "fHitX", &fPtrX);           // throws if the branch is missing
        ...
    }
    void RefreshViews() const override { ... }       // called by TRestRun after reading an entry
    void CopyFrom(const TRestEvent* other) override {
        TRestEvent::CopyFrom(other);
        if (auto* source = dynamic_cast<const TRestDetectorHitsEvent*>(other)) {
            fHitsData = source->fHitsData;
            RebuildHitsView();
        }
    }
    TPad* DrawEvent(const std::string& option = "") const override;

    TRestDetectorHitsEvent() = default;
    TRestDetectorHitsEvent(const TRestDetectorHitsEvent& other)   // views rebuilt over the copy
        : TRestEvent(other), fHitsData(other.fHitsData) { RebuildHitsView(); }
    TRestDetectorHitsEvent& operator=(const TRestDetectorHitsEvent& other);  // same rule
};
```

```cpp
// .cxx: registration by name (used by TRestRun and the process managers)
namespace {
const bool kRegistered = []() {
    EventRegistry::Instance().Register("TRestDetectorHitsEvent",
                                       []() { return std::make_unique<TRestDetectorHitsEvent>(); });
    return true;
}();
}  // namespace
```

Rules:

- **Branches**: only numbers, strings and vectors of them (flat). Objects of the legacy event (TVector3,
  TRestGeant4Hits, names) become components and IDs (e.g. `TRestGeant4Event` stores positions as X, Y, Z
  floats and particle names as IDs of the metadata tables). Call the base `CreateBranches` /
  `SetBranchAddresses` first and bind with `BindBranch` (ROOT's `SetBranchAddress` only prints an error).
- **Storage and views**: the analysis methods work on views (`TRestHits`, `TRestRawSignal`,
  `TRestDetectorSignal`, `TRestTrack`) pointing into the storage. Rebuild them after any change of the
  storage, in `RefreshViews` (after reading) and in `CopyFrom`. The **copy constructor and assignment must
  be written** and rebuild the views: the implicit ones would leave the views of the copy on the storage
  of the original. `TRestEvent`'s own copy does not copy the drawn pad nor the branch pointer.
- **`Initialize`** is pure virtual but defined: call `TRestEvent::Initialize()`, then clear the storage.
- **Format version**: `GetFormatVersion()` (1 by default) is written in the tree UserInfo. Any change of
  the branches (added, removed, renamed or retyped) increases it; then either bind the older branches only
  for older versions (`GetInputFormatVersion()`) or raise `GetMinimumFormatVersion()`. A branch name is
  never reused with another type.
- **`DrawEvent(const std::string&) const`** returns a pad owned by the event (`fPad`, deleted by the
  next call and by the destructor). Options are `name(rootOption)[value]` separated by `:`
  (`ParseDrawOption`); draw ROOT objects with `DrawOwned`; `print` calls `PrintEvent`.
- Events have no verbose of their own: their messages use the context of the process using them.
- `MoveFrom` can be overridden to move the storage instead of copying it.

### 4.3 Metadata

Legacy metadata read every `<parameter>` into the member of the same name by reflection, through
`InitFromConfigFile`, and ignored unknown ones:

```cpp
// Legacy
class TRestCut : public TRestMetadata {
    std::vector<std::string> fCutStrings;
    ...
    void InitFromConfigFile() override;     // parses <cut> elements, ReadAllParameters()
    ClassDefOverride(TRestCut, 2);
};
```

In restv2 every key is a **registered field**, read and written back explicitly:

```cpp
// restv2: TRestCut.h
class TRestCut : public TRestMetadata {
   public:
    std::map<std::string, std::string> fConditions;     // YAML key "conditions"

    TRestCut();                                                                 // no configuration
    TRestCut(const std::string& instanceName, const YAML::Node& node);          // from a YAML node
    TRestCut(const std::string& fileName, const std::string& sectionName);      // from a file section
    std::string GetClassName() const override { return "TRestCut"; }
    void LoadConfig() override;
    void Initialize() override {}
};
```

```cpp
// restv2: TRestCut.cxx
static const bool TRestCut_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestCut>("conditions", &TRestCut::fConditions);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestCut", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestCut>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestCut::TRestCut() : TRestMetadata() { fName = "TRestCut"; }
TRestCut::TRestCut(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) { LoadConfig(); }
TRestCut::TRestCut(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) { LoadConfig(); }

void TRestCut::LoadConfig() {
    UpdateParamsFromYAML<TRestCut>(fNode);     // node -> members (registered fields only)
    for (const auto& [name, condition] : fConditions)
        if (condition.empty()) throw std::runtime_error("TRestCut '" + fName + "': empty condition " + name);
    UpdateYAMLFromParams<TRestCut>(fNode);     // members -> node: the values used are stored
}
```

Rules:

- **Fields**: `RegisterField<Class>("key", &Class::fMember)` for any type convertible from YAML (numbers,
  strings, bools, `std::pair`, `std::vector`, `std::map`, `YAML::Node` for structures the class reads
  itself and checks with `TRestYAMLUtils::CheckKeys`). Other forms: a `std::vector` of metadata (one per
  map of the list), `RegisterNestedField` (a metadata member configured from its subsection),
  `RegisterFactoryField` (a polymorphic metadata created by a factory from its `type`, held in a
  `std::unique_ptr`), `RegisterChildMetadata` (a container creating the metadata with `class` it holds).
  Do not read `fNode[...]` by hand.
- **Unknown keys throw** (`MetadataClassRegistry::Create`, `CheckConfig`), listing the valid ones; only
  `class`, `name` and `verbose` are accepted besides the registered fields of the class and its bases. A
  class reading a section shared with another object declares the other keys in `GetSharedNodeKeys`.
  Metadata read from a file written by REST are not checked.
- **Units**: a member with units is a `TRestWithUnits` (a double in REST units, read from `"5.9 keV"`);
  inside pairs and vectors too (`std::pair<TRestWithUnits, TRestWithUnits>`).
- **Options**: normalise text options between `UpdateParamsFromYAML` and `UpdateYAMLFromParams` with
  `fOption = TRestTools::NormalizeOption("key", fOption, kOptions)` and compare with the canonical
  spelling.
- **Validation** goes to `LoadConfig` (or `InitProcess` for processes) and throws `std::runtime_error`
  with the class and instance name.
- **Results** (fit parameters, tables loaded, the medium used...) are registered fields too: set them and
  call `UpdateYAMLFromParams` again; they are written with the configuration in `RESTMetadataStore`
  (`WriteMetadata`). ROOT objects (histograms, geometry) are written by overriding `WriteMetadata` and read
  back in `LoadFromFile`.
- **Other metadata**: a process finds a sibling metadata of the manager by name with
  `fManager->FindMetadata<TRestDetectorMedium>(fMediumName, user, "medium")` (optional name if there is
  only one), and the run parameters with `fRunInfo->GetParameter("runInfo.key")` /
  `GetParameterValue(...)`.
- **Logging**: `RESTInfo << ... << RESTendl;` (also `RESTError`, `RESTWarning`, `RESTLog`, `RESTMetadata`,
  `RESTDebug`) uses the verbose and name of the metadata whose method writes it; no per-class macro is
  needed (legacy `DECLARE_LOG_CLASS` style was removed). Each statement is one message, thread safe.
- Do not include `TRestTools.h` or `TRestYAMLUtils.h` in headers: include them in the `.cxx`.

### 4.4 Event processes

```cpp
// Legacy
class TRestDetectorHitsSmearingProcess : public TRestEventProcess {
    TRestDetectorHitsEvent* fInputEvent = nullptr;    //!
    TRestDetectorHitsEvent* fOutputEvent = nullptr;   //! owned by the process
    Double_t fEnergyRef = 5.9;                        // <parameter name="energyRef" ...> by reflection
    ...
    RESTValue GetInputEvent() const override { return fInputEvent; }
    RESTValue GetOutputEvent() const override { return fOutputEvent; }
    TRestEvent* ProcessEvent(TRestEvent* inputEvent) override;   // nullptr rejects the event
    const char* GetProcessName() const override { return "smearingProcess"; }
    ClassDefOverride(TRestDetectorHitsSmearingProcess, 4);
};
// in ProcessEvent: SetObservableValue("energy", value); ... return fOutputEvent;
```

```cpp
// restv2
class TRestDetectorHitsSmearingProcess : public TRestEventProcess {
    TRandom3 fRandom;                                  //!
   public:
    TRestWithUnits fEnergyReference = 5.9;             // registered as "energyReference"
    double fResolutionReference = 15;
    unsigned long fSeed = 0;

    TRestDetectorHitsSmearingProcess();
    TRestDetectorHitsSmearingProcess(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorHitsSmearingProcess(const std::string& fileName, const std::string& sectionName);
    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override;
    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }
    std::string GetClassName() const override { return "TRestDetectorHitsSmearingProcess"; }
};
```

```cpp
// restv2 .cxx (fields and class registered as in 4.3)
void TRestDetectorHitsSmearingProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();                   // common fields: observables, cuts
    UpdateParamsFromYAML<TRestDetectorHitsSmearingProcess>(fNode);
    UpdateYAMLFromParams<TRestDetectorHitsSmearingProcess>(fNode);
}

void TRestDetectorHitsSmearingProcess::InitProcess() {
    if (fEnergyReference <= 0) throw std::runtime_error("...");
    fRandom.SetSeed(fSeed);
    // RegisterObservable("Gain", fGain);   // bound once; the branch is <processName>_Gain
}

bool TRestDetectorHitsSmearingProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto& in = dynamic_cast<const TRestDetectorHitsEvent&>(input);
    auto& out = dynamic_cast<TRestDetectorHitsEvent&>(output);
    out.Initialize();
    out.SetEventInfo(&in);
    if (fSeed != 0) fRandom.SetSeed(TRestDetectorHitsEvent::EventSeed(fSeed, in));
    ...
    return out.GetNumberOfHits() > 0;                  // false rejects the event
}
```

Rules:

- **Event types** are class names (`GetInputEvent`/`GetOutputEvent`), also the tree names. `"None"` as
  input is a generator (gets the output event as input); an empty input takes the event of the previous
  process (processes of any type, e.g. `TRestSelectionProcess`).
- **Output event**: owned by the manager, distinct from the input even for the same type. Clear it
  (`Initialize`), copy the event info (`SetEventInfo`) and fill it; a process that keeps the event copies
  it with `output.CopyFrom(&input)`. Return `false` to reject the event.
- **Observables**: `RegisterObservable("Name", fMember)` in `InitProcess` binds a member (any type:
  numbers, bools, strings, vectors) to the branch `processName_Name`; set the member in each
  `ProcessEvent` (reset it for every event). Register only the observables that the configuration enables
  (an observable requested but not offered is an error, and offering a name twice throws). Use bare names
  (`Energy`, not `energy_total`); renamed observables are listed in the class Doxygen.
- **Cuts** are not the process's concern: the base applies the `cuts` of the configuration after
  `ProcessEvent`. Read observables of other processes with `fRunInfo->GetObservable<T>(name)` in
  `InitProcess`.
- **Sibling metadata** (readouts, medium) are found through `fManager` in `InitProcess`; store the name of
  the one used as a result (`UpdateYAMLFromParams`).
- **Random numbers**: a `seed` field (0: random), the generator re-seeded at each event from (seed,
  eventID, subEventID) (`TRestDetectorHitsEvent::EventSeed`), so results do not depend on threads.
- **Threads**: each thread builds its own manager and processes from the YAML, so members need no locks;
  shared static state does. Read geometries only with `TRestTools::ReadGeometry` (shared, read-only) or
  `TRestTools::ReadGeometryCopy` (owned copy, for navigation), never `TGeoManager::Import`/`TFile::Get`.
- **External input** (DAQ files): return `-1` from `GetInputEventCount`, `true` from `InputFinished()` at
  the end of the file, and the fraction read from `GetInputFraction()`.
- `EndProcess` reports summaries (e.g. events skipped). No `GetProcessName`, `PrintMetadata` boilerplate
  nor `ClassDef`.

### 4.5 Dataset processes

Legacy dataset tools (`TRestDataSetOdds::ComputeLogOdds`, `TRestDataSetCalibration::Calibrate`) opened
the dataset, computed and wrote a new file themselves. In restv2 a `TRestDataSetProcess` only provides the
two steps; the manager handles the files, the reference and the storage:

```cpp
class TRestDataSetLogOdds : public TRestDataSetProcess {
   protected:
    // Parameters (results) from the reference with the cuts applied; called only if not stored in it
    void ComputeParameters(const TRestDataSet& reference, ROOT::RDF::RNode frame) override;
   public:
    std::vector<TRestDataSetLogOddsObservable> fObservables;   // registered "observables" (PDFs inside)
    TRestDataSetLogOdds(const std::string& instanceName, const YAML::Node& node);
    std::string GetClassName() const override { return "TRestDataSetLogOdds"; }
    void LoadConfig() override;
    void Apply(TRestDataSet& dataSet) const override;           // adds the columns
    void WriteObjects(TDirectory* directory) const override;    // PDFs as histograms
};

void TRestDataSetLogOdds::Apply(TRestDataSet& dataSet) const {
    ROOT::RDF::RNode frame = dataSet.GetDataFrame();
    std::vector<std::string> columns;
    for (const auto& observable : fObservables) {
        auto logOdds = [observable](double v) { return LogOdds(observable.GetProbability(v)); };
        frame = DefineColumn(frame, GetColumn(observable.fObservable), logOdds,
                             {AsDouble(frame, observable.fObservable)});
        columns.push_back(GetColumn(observable.fObservable));
    }
    dataSet.SetDataFrame(frame, columns);
}
```

- `ComputeParameters` fills the results (registered fields) and calls `UpdateYAMLFromParams`; book all the
  RDataFrame results before reading any (one loop).
- `Apply` defines columns `<name>_<observable>` with `DefineColumn` (redefines if the dataset is processed
  again) and `AsDouble` (input columns of any type as double); capture values, not `this`.
- `WriteObjects`/`ReadObjects` store and restore the objects in `RESTDataSetStore/<class>/<name>`.
- Extra cuts (e.g. a background) by TRestCut name with `GetCutExpression`.
- Base classes are in core; a dataset process goes to `source/analysis` or to the library whose classes it
  needs (`TRestDataSetGainMap` in detector).

### 4.6 Documentation, tests, format and dictionary

- **Doxygen** in the `.cxx` of each class: what it does, the YAML keys with defaults, observables or
  results, an example and the "Differences with legacy"; the header documents the public API. Update the
  README of the component and, for a legacy class, MIGRATION_STATUS.md.
- **Unit tests** in the `test/src/*.cxx` of the component (GoogleTest, `rest_add_tests` in
  `cmake/RestTests.cmake`, label = component): build the inputs in memory (e.g. a process from
  `YAML::Load("{key: value}")` and events filled by hand), no external data. Build with
  `-DBUILD_TESTING=ON`, run `ctest -L <component>`. See [CONTRIBUTING.md](CONTRIBUTING.md).
- **Format**: clang-format with a 110 column limit (`pre-commit run --all-files`).
- **ROOT dictionary**: generated for each library by `rest_attach_dictionary` (top `CMakeLists.txt`) from
  a LinkDef written from the headers (`cmake/scripts/RestGenerateLinkDef.cmake`: the class or namespace
  named as each header file). Name the header as its main class. The headers are collected with a file
  glob: **re-run cmake after adding a header**. No `ClassDef` is needed (only GUI classes with signals and
  slots use it).
- Use C++ types (`double`, `std::string`, `ROOT::Math::XYZVector`); `TString` only where a ROOT call
  needs it. Generic string and file helpers are in `TRestTools`.

### 4.7 Checklist

1. Decide the component and check MIGRATION_STATUS.md (is it migrated, replaced, or covered elsewhere?).
2. Events: storage + views, branches, `BindBranch`, copy constructor/assignment, `RefreshViews`,
   `CopyFrom`, `Initialize`, `DrawEvent`, `EventRegistry` registration, format version.
3. Metadata/processes: the three constructors, registered fields, `LoadConfig` (read, validate,
   normalise, write back), `MetadataClassRegistry` registration, input/output events, observables in
   `InitProcess`, per-event seeds.
4. Example YAML in `examples/`, README entry, Doxygen with the differences with legacy.
5. Unit tests, clang-format, re-run cmake.

## 5. Common issues

**`EventRegistry: unknown type 'X'` or `MetadataClassRegistry: unknown type 'X'`.** The class is not
registered: its `.cxx` lacks the registration lambda, or its library is not loaded. Executables and tests
link the REST libraries with `rest_link_registered_libraries` (`--no-as-needed`); macros run in restRoot,
which loads all the libraries. Legacy class names that no longer exist give the same error
([section 3](#3-class-equivalences)).

**`TRestEvent: the tree ... has no branch ...: not compatible with this REST`.** The file was written with
another branch layout of the event class (or a new branch was added without increasing
`GetFormatVersion`). Legacy files are not readable at all.

**`unknown parameter(s) 'xxx'. Valid parameters: ...`.** A key that is not a field of the class: a typo, or
a legacy key (`verboseLevel` instead of `verbose`, `title`, `threadNumber`, `type` instead of `state` in
`TRestDetectorMedium`, renamed parameters). Use one of the listed keys; a new parameter needs a registered
field.

**`Invalid xxx "value", options: ...`.** A text option (`baselineOption`, `method`, `triggerMode`...) with
a value that is not one of its options, whatever the case. Use a listed option.

**`${PARAM:x}: parameter 'x' ...` or "parameters not used".** A parameter used but not defined in
`parameters`, a `--set` of a name that is not there, or a parameter used neither in the YAML nor in the
GDML (probably a typo).

**An observable requested is not available.** The name in `observables` is misspelled or belongs to an
option that is not enabled (e.g. `GaussSigmaX` without `gaussFit`); the message lists the available ones.

**"Deleting previous geometry" and a crash with several threads.** Reading a TGeoManager from a file
deletes `gGeoManager`; with `nThreads > 1` each thread opens its own files. Read geometries only with
`TRestTools::ReadGeometry` / `ReadGeometryCopy`, which are serialised and keep `gGeoManager`.

**`'TRestTools' / 'TRestYAMLUtils' has not been declared`.** The common headers (`TRestMetadata.h`,
`TRestSystemOfUnits.h`, `TRestRun.h`) do not include them, to keep rebuilds small: include them in the
`.cxx` that uses them and keep them out of headers.

**A new class is not available in restRoot.** Re-run cmake so that the header is collected for the
dictionary, and check that the header file is named as the class.

**ROOT version.** ROOT 6.26 or later is required (RDataFrame `DefinePerSample`), with the components
ROOTDataFrame, RooFit and TMVA (top `CMakeLists.txt`).
