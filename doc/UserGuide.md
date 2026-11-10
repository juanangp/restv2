# REST user guide

This guide describes how to use restv2, the C++/ROOT framework of REST (Rare Event Searches Toolkit): how to
write the YAML configurations, what the events and the REST files contain, how the processes, cuts, plots and
datasets work, and where to find the details of each library and package. It is written for the users of REST;
the build and installation are described in the [README](../README.md).

## Contents

1. [Overview](#1-overview)
2. [Configuration files (YAML)](#2-configuration-files-yaml)
3. [Events and their format](#3-events-and-their-format)
4. [Processes and the processing chain](#4-processes-and-the-processing-chain)
5. [TRestRun and the REST file](#5-trestrun-and-the-rest-file)
6. [Cuts, selection and plots](#6-cuts-selection-and-plots)
7. [Datasets](#7-datasets)
8. [Detector, simulation and data taking](#8-detector-simulation-and-data-taking)
9. [Logging, restRoot and macros](#9-logging-restroot-and-macros)

## 1. Overview

### 1.1 Events, metadata and processes

REST is built on three kinds of objects (`source/core`), from which every library derives its own classes:

| Kind | Base class | What it is | Examples |
|---|---|---|---|
| Event | `TRestEvent` | The data of one event at one stage of the processing. Each event type is written in its own tree. | `TRestRawSignalEvent`, `TRestDetectorHitsEvent`, `TRestTrackEvent`, `TRestGeant4Event` |
| Metadata | `TRestMetadata` | Any object configured from a YAML section: a run, a readout, a medium, a cut, a simulation... It is stored in the output file. | `TRestRun`, `TRestDetectorReadoutManager`, `TRestDetectorMedium`, `TRestCut` |
| Event process | `TRestEventProcess` (a metadata) | Transforms an input event into an output event (possibly of another type) and computes **observables**, values per event written in the analysis tree. | `TRestRawSignalAnalysisProcess`, `TRestDetectorSignalToHitsProcess` |

The processes are chained in a `TRestEventProcessManager` (the *pipeline*). For the high-level analysis the
analysis trees of many runs are joined in a **dataset** (`TRestDataSet`), processed by **dataset processes**
(`TRestDataSetProcess`: calibration, gain map, log odds, TMVA, signal selection).

### 1.2 Executables

| Executable | Package | Use |
|---|---|---|
| `restManager` | core ([source/bin](../source/bin/README.md)) | Runs a YAML configuration with a `TRestManager` section: event processing, dataset generation and processing, plots. |
| `restRoot` | core ([source/bin](../source/bin/README.md)) | ROOT prompt with the REST libraries and macros loaded; opens a REST file given as argument. |
| `restG4` | [restG4](../packages/restG4/README.md) | Geant4 simulation, writes `TRestGeant4Event`. |
| `restCRY` | [restG4](../packages/restG4/README.md) | Cosmic-ray showers with CRY, for the `cry` generator of restG4. |
| `restGarfield` | [restGarfield](../packages/restGarfield/README.md) | Transport and gain tables of a detector medium with Garfield++. |
| `femclient` | [restDAQ](../packages/restDAQ/README.md) | Data taking with AFTER/AGET electronics, writes REST files. |

```
restManager --c config.yaml [--i input.root] [--o output.root] [--j 4] [--e 1000] [--set name=value]
restRoot [file.root]
```

All the `restManager` options (`--v`, `--r`, `--maxRepeated`...) are listed in
[source/bin/README.md](../source/bin/README.md).

### 1.3 A processing from end to end

1. `restManager` reads the YAML file (its `include` keys expanded, its `parameters` and references resolved,
   the `--set` values applied) and creates the `TRestManager` section: every map with a `class` inside it is
   created as a metadata (run, pipeline, cuts, readouts, media, plots...).
2. The `TRestRun` opens the input file (`--i`, `inputFileName`), completes the run parameters and opens the
   output file (`--o`, `outputFileName`).
3. The `TRestEventProcessManager` initializes the processes, resolves which event each process reads, and
   loops over the entries: each entry is read, passed through the chain and, if no process rejected it (and it
   passes the cuts), written to the output event trees and to the `AnalysisTree`.
4. The configurations actually used (all the metadata, with the values resolved) are written to the
   `RESTMetadataStore` of the output file, together with the metadata of the input file.
5. The `TRestAnalysisPlot` sections are drawn.

A minimal configuration (raw signal analysis with a cut and a plot; complete examples in
[libraries/raw/examples](../libraries/raw/examples) and the other `examples` directories):

```yaml
manager:
  class: "TRestManager"
  run:
    class: "TRestRun"
    inheritInputRun: true                        # run number, tag... from the input file
    outputFileName: "R[runNumber]_[runTag]_RAWANA.root"
  goodEvents:
    class: "TRestCut"
    conditions:
      signals: "rawAna_NGoodSignals > 0"
  pipeline:
    class: "TRestEventProcessManager"
    nThreads: 4
    rawAna:
      class: "TRestRawSignalAnalysisProcess"
      observables: ["all"]
      cuts: ["goodEvents"]
  plots:
    class: "TRestAnalysisPlot"
    save: "R{run.runNumber}_plots.pdf"
    plots:
      - name: "Integral"
        variables: [{name: "rawAna_ThresholdIntegralSum", nBins: 200}]
```

```
restManager --c rawAnalysis.yaml --i R01234_Calibration.root
```

## 2. Configuration files (YAML)

### 2.1 Sections

A configuration is a YAML file whose sections are metadata: maps with a `class` key. The name of a section
(its YAML key) is the name of the metadata instance, unless it has a `name` key. `restManager` runs the first
section of class `TRestManager` (its key is free); the other executables run their own manager class
(`TRestGeant4Manager` for restG4, `TRestDetectorTransportManager` for restGarfield...).

Every section accepts, besides the keys of its class:

| Key | Meaning |
|---|---|
| `class` | Class of the metadata (required). |
| `name` | Instance name (default: the key of the section). Used to find the metadata (e.g. the `medium` or `readout` of a process), as prefix of the observables of a process and as key in the `RESTMetadataStore`. |
| `verbose` | `silent`, `error`, `warning`, `info` or `debug` (see [section 9](#9-logging-restroot-and-macros)). |

**Only registered keys are accepted.** Each class reads a fixed list of keys (its registered fields, those of
its base classes included); an unknown key (e.g. a misspelling, or the legacy `verboseLevel`) or a value that
cannot be converted is an error whose message lists the valid keys. The keys of each class are documented in
its class documentation (`.cxx` file, also in the Doxygen pages) and in the READMEs of the libraries.

**Options are case insensitive.** A text field with a fixed set of values (e.g. `baselineOption`, `method`,
`fluctuations`, `peakModel`) accepts any case and is stored with the spelling of its list of options; an
unknown option is an error that lists the valid ones. Names (files, volumes, particles, observables) keep
their case.

### 2.2 Values with units

REST stores every quantity in its units: energy in keV, time in us, length in mm, mass in kg, voltage in V,
magnetic field in T, pressure in bar and angle in rad. A numerical field of physical meaning accepts a value
with units, converted to the REST units when it is read (and stored converted):

```yaml
energy: 5.9 keV          # 5.9
shapingTime: 0.2 us      # 0.2
driftField: 100 V/cm     # 10 (V/mm)
gasThickness: 9 cm       # 90
pressure: 1.4 bar
angle: 30 deg
```

Units of each quantity: `meV eV keV MeV GeV TeV J kJ`, `ns us ms s hr day yr`, `nm um mm cm m km`,
`mg g kg ton`, `mV V kV`, `mT T G`, `bar mbar atm Pa kPa MPa torr`, `rad deg degree arcmin arcsec`, and
expressions of them (`kV/cm`, `mm/us`). A number without units is taken in REST units. The list is in
[TRestSystemOfUnits.h](../source/tools/inc/TRestSystemOfUnits.h).

### 2.3 Parameters (`parameters`, `${PARAM:name}`, `--set`)

Any configuration (restManager, restG4, restCRY, restGarfield, femclient) can have a `parameters` section at
its root: name: value (a number, with units or not, or a text), used anywhere as `${PARAM:name}`:

```yaml
parameters:
  meshVoltage: 300 V
  tag: "Calibration"

manager:
  class: "TRestManager"
  run:
    class: "TRestRun"
    runTag: "${PARAM:tag}"
    runInfo:
      meshVoltage: "${PARAM:meshVoltage}"
```

- `--set name=value` (repeatable) replaces a value for one execution, without changing the file, e.g.
  `restManager --c config.yaml --i input.root --set meshVoltage=320V`. A name that is not in `parameters` is
  an error.
- A parameter that is not defined is an error, and so is a parameter that is not used (probably a typo; in
  restG4 the parameters can also be used by the GDML geometry, see
  [section 8](#8-detector-simulation-and-data-taking)).
- The `parameters` section is removed when the configuration is resolved: the configurations stored in the
  output files are the resolved ones.

### 2.4 References

| Reference | Replaced by |
|---|---|
| `${ENV:VAR}` | The environment variable `VAR` (empty if it is not defined), e.g. `runUser: "${ENV:USER}"`. Also valid as a parameter value. |
| `${PARAM:name}` | The parameter `name` (above). |
| `${a.b}` | The scalar at the path of keys `a.b` of the configuration. |

A reference that is not found or is not a scalar is an error.

### 2.5 Sections in other files (`include`)

Any map can take its content from another file with the key `include`; its own keys override the included
ones (maps are merged recursively, lists and scalars replaced):

```yaml
run:
  include: "common/run.yaml"          # the content of the section
  runTag: "calib"                     # overrides the one of common/run.yaml
pipeline:
  include: "analysis.yaml:pipeline"   # the section `pipeline` of a file with several
  rawAna:
    signalThreshold: 4                # changes only this key of the included process
```

The path is relative to the including file (absolute paths and `${ENV:VAR}` are accepted, `${PARAM:...}` is
not). Included files can include others; a cycle, a missing file or key is an error. The `parameters` are those
of the main file, resolved after the includes are expanded. Example:
[packages/restG4/examples/04.MuonScan](../packages/restG4/examples/04.MuonScan).

### 2.6 Conditional blocks (`select`)

A map with the keys `select` and `cases` is replaced by the case whose name is the value of `select`
(`default` if none matches; neither is an error), usually a parameter. The other keys of the map are common to
all the cases: the case is merged over them (maps merged recursively, the case wins). A case can also be a
scalar (then the block has no other keys):

```yaml
parameters:
  generator: "Gammas"
  energy: 5.5
...
generator:
  select: "${PARAM:generator}"
  source:                         # common to all the cases
    angular: {type: "flux", direction: [0, 0, 1]}
  cases:
    Gammas:
      type: "point"
      source:
        particle: "gamma"
        energy: {type: "mono", energy: "${PARAM:energy} MeV"}
    Muons:
      type: "cosmic"
      source:
        particle: "mu-"
runTag:
  select: "${PARAM:generator}"
  cases: {Gammas: "gammas_${PARAM:energy}MeV", default: "${PARAM:generator}"}
```

The blocks are resolved after the parameters (`restG4 sim.yaml --set generator=Muons` changes the case), so
the parameters of all the cases must be defined and count as used; only the case chosen remains in the
configuration stored in the output file.

There is no `globals` section: what was global in legacy REST is a parameter, a key of the run, or the
`verbose` of a container (inherited by its sections).

## 3. Events and their format

Each event type is written in its own tree of the REST file, **named as the event class** (e.g. the tree
`TRestDetectorHitsEvent`), one entry per event. The data are stored in flat branches of basic types and
`std::vector` (no objects), so they can be read with plain ROOT or RDataFrame without the REST libraries.
Variable-size collections (signals, hits, tracks) are stored **back-to-back** in vectors, with the IDs and
sizes or offsets needed to split them.

### 3.1 Common information (`TRestEvent`)

Every event tree, and the `AnalysisTree`, has these branches:

| Branch | Type | Content |
|---|---|---|
| `runOrigin` | `int` | Run number the event comes from. |
| `subRunOrigin` | `int` | Subrun number the event comes from. |
| `eventID` | `int` | Event identifier. |
| `subEventID` | `int` | Subevent identifier (several subevents can share an `eventID`, e.g. the delayed decays of a restG4 decay chain). |
| `timeSeconds` | `long long` | Whole seconds of the event timestamp (unix time). |
| `timeNanoSecs` | `int` | Nanoseconds of the timestamp, in [0, 1e9). The C++ member is `fInfo.timeNanoSeconds`. |
| `ok` | `bool` | Validity flag. |
| `subEventTag` | `std::string` | Free label of the subevent. |

In C++: `GetID()`, `GetSubID()`, `GetSubEventTag()`, `GetRunOrigin()`, `GetSubRunOrigin()`, `GetTime()`
(seconds, as a double), `isOk()`, `PrintEvent()` and `DrawEvent(option)`.

### 3.2 Hits and their type (`REST_HitType`)

The hits of `TRestDetectorHitsEvent`, `TRestTrackEvent` and `TRestGeant4Event` share the same storage
(`TRestHitsData`): parallel vectors of `float` x, y, z (mm), time and energy, and an `int` type. The type
tells which coordinates the hit defines; it is the product of the primes of its coordinates (X = 2, Y = 3,
Z = 5):

| Type | `unknown` | `X` | `Y` | `Z` | `XY` | `XZ` | `YZ` | `XYZ` |
|---|---|---|---|---|---|---|---|---|
| Value | -1 | 2 | 3 | 5 | 6 | 10 | 15 | 30 |

**The coordinates a hit does not define are stored as NaN** (e.g. y of an `XZ` hit of an X strip of a
Micromegas). Strip readouts give `XZ` and `YZ` hits, 3D readouts and simulations `XYZ` hits. A coordinate is
defined if the type is divisible by its prime (`TRestHitsData::IsDefined(type, axis)`).

### 3.3 `TRestRawSignalEvent`

The digitized signals of one trigger: each signal is a vector of 16-bit ADC samples identified by its signal
ID (the DAQ channel; the readout decodes it into a physical channel). Library:
[raw](../libraries/raw/README.md).

| Branch | Type | Content |
|---|---|---|
| `fSigSamples` | `vector<short>` | The samples of all the signals, one after the other. |
| `fSigIDs` | `vector<int>` | Signal ID of each signal (unique in an event). |
| `fSigOffsets` | `vector<int>` | Index in `fSigSamples` of the first sample of each signal. |

Signal `i` is `fSigSamples[fSigOffsets[i] ... fSigOffsets[i+1] - 1]` (up to the end of `fSigSamples` for the
last one). In C++: `GetNumberOfSignals()`, `GetSignal(i)` / `GetSignalByID(id)` (a view with
`GetSignalID()`, `GetNPoints()`, `GetPoint(bin)`, `GetData()`, `GetGraph()`).

**`TRestRawDAQSignalEvent`** (written by restDAQ) is a `TRestRawSignalEvent` written in the same tree
`TRestRawSignalEvent`, with four more branches, one entry per FEM (the signals of each FEM are consecutive, in
the order of these entries). Its files are read as a plain `TRestRawSignalEvent`, which ignores them.

| Branch | Type | Content |
|---|---|---|
| `fFEMIDs` | `vector<int>` | FEM id (`id` of the FEM in `TRestRawDAQMetadata`). |
| `fFEMEventCounts` | `vector<unsigned int>` | Event counter of the FEM (0xFFFFFFFF: event without header). |
| `fFEMTimestamps` | `vector<unsigned long long>` | Timestamp of the FEM, in ticks of 10 ns. |
| `fFEMNSignals` | `vector<int>` | Number of signals of the FEM. |

### 3.4 `TRestDetectorSignalEvent`

The signals of the readout channels as (time, amplitude) points in double precision. Produced from raw
signals by `TRestRawToDetectorSignalProcess` (time = `triggerStarts + sampling * bin`) or from hits by
`TRestDetectorHitsToSignalProcess` (time = hit time + drift time, in us). Library:
[detector](../libraries/detector/README.md).

| Branch | Type | Content |
|---|---|---|
| `fSigTime` | `vector<double>` | Time of the points of all the signals, back-to-back. |
| `fSigAmplitude` | `vector<double>` | Amplitude (charge) of each point. |
| `fSigIDs` | `vector<int>` | DAQ channel ID of each signal (one signal per ID). |
| `fSigOffsets` | `vector<int>` | Index of the first point of each signal in `fSigTime` and `fSigAmplitude`. |

In C++: `GetNumberOfSignals()`, `GetSignal(i)` / `GetSignalByID(id)` (a view with `GetSignalID()`,
`GetNPoints()`, `GetTime()`, `GetCharge()`, `GetPoint(i)`, `GetGraph()`).

### 3.5 `TRestDetectorHitsEvent`

A flat list of hits: energy deposits (simulation), drifted electrons, or positions reconstructed from the
signals. Library: [detector](../libraries/detector/README.md).

| Branch | Type | Content |
|---|---|---|
| `fHitX`, `fHitY`, `fHitZ` | `vector<float>` | Position (mm); NaN for the coordinates the hit does not define. |
| `fHitTime` | `vector<float>` | Time (us). |
| `fHitEnergy` | `vector<float>` | Energy (keV in simulations; the signal charge for hits reconstructed from data). |
| `fHitType` | `vector<int>` | `REST_HitType` of each hit. |

In C++: `GetNumberOfHits()`, `GetX(n)`, `GetY(n)`, `GetZ(n)`, `GetTime(n)`, `GetEnergy(n)`, `GetType(n)`, and
`GetHits()`, a `TRestHits` view with the analysis methods (mean position, sigmas, skewness...).

### 3.6 `TRestTrackEvent`

The tracks of one processing stage (each track process writes a new event with its tracks). A track is a group
of hits of one type, stored contiguously. Library: [track](../libraries/track/README.md).

| Branch | Type | Entries | Content |
|---|---|---|---|
| `fHitX`, `fHitY`, `fHitZ`, `fHitTime`, `fHitEnergy`, `fHitType` | `vector<float>`, type `vector<int>` | one per hit | The hits of all the tracks, back-to-back, as in 3.5. |
| `fSigmaX`, `fSigmaY`, `fSigmaZ` | `vector<float>` | one per hit | Spatial uncertainty of each hit (volume hits). |
| `fTrackID` | `vector<int>` | one per track | Track ID. |
| `fParentID` | `vector<int>` | one per track | ID of the track it comes from in the input event of the stage (0 if none). |
| `fTrackNHits` | `vector<int>` | one per track | Number of hits of each track. |

The hits of track `n` start at the sum of `fTrackNHits[0 ... n-1]`. In C++: `GetNumberOfTracks()`,
`GetTrack(n)` / `GetTrackById(id)` (a `TRestTrack` view), `GetNumberOfHits(n)`.

### 3.7 `TRestGeant4Event`

The event of restG4 (tree `TRestGeant4Event`): primaries, deposited energies and, with `storeTracks`, the
tracks and their hits. Particles are stored as PDG encodings, processes and volumes as IDs whose names are in
the `TRestGeant4Metadata` of the file (`TRestGeant4PhysicsInfo`, `TRestGeant4GeometryInfo`; process ID =
Geant4 process type * 1000 + subtype). Energies in keV, positions in mm, times in us. Library:
[geant4](../libraries/geant4/README.md).

| Group | Branches (type) | Entries |
|---|---|---|
| Event | `fTotalDepositedEnergy`, `fSensitiveVolumeEnergy` (double, keV); `fEventTimeWall`, `fEventTimeWallPrimaryGeneration` (double, wall time in s) | scalars |
| First primary vertex | `fPrimaryPositionX/Y/Z` (float) | scalars |
| Primaries | `fPrimaryParticleIDs` (int), `fPrimaryEnergies` (double), `fPrimaryDirectionX/Y/Z` (float) | one per primary |
| Subevent (SubID > 0) | `fSubEventParticleID` (int), `fSubEventEnergy` (double), `fSubEventPositionX/Y/Z`, `fSubEventDirectionX/Y/Z` (float) | scalars |
| Energy per volume | `fVolumeIDs` (int), `fVolumeDepositedEnergy` (double) | one per volume with energy |
| Energy per volume, particle and process | `fCrossVolumeIDs`, `fCrossParticleIDs`, `fCrossProcessIDs` (int), `fCrossDepositedEnergies` (double) | one per combination |
| Tracks | `fTrackIDs`, `fTrackParentIDs` (0: primary), `fTrackParticleIDs`, `fTrackCreatorProcessIDs` (int); `fTrackDepositedEnergy`, `fTrackInitialEnergies`, `fTrackGlobalTimestamp`, `fTrackTimeOffset`, `fTrackTimeLength`, `fTrackLength`, `fTrackWeight` (double); `fTrackInitialPositionX/Y/Z` (float) | one per track |
| Hits of the tracks | `fTrackStartIndices`, `fTrackNHits` (int) | one per track |
| Children of the tracks | `fTrackSecondariesIDs` (int, one per child); `fTrackSecondariesIndex` (index of the first child), `fTrackSecondariesOffsets` (number of children) (int, one per track) | |
| Hits | `fHitX/Y/Z`, `fHitTime`, `fHitEnergy` (float), `fHitType` (int); `fHitProcessID` (0: initial step), `fHitVolumeID` (int); `fHitKineticEnergy`, `fHitMomentumDirectionX/Y/Z` (float); `fHitHadronicTargetIsotopeA/Z` (int, 0 if none) | one per hit |

The hits of track `n` are `[fTrackStartIndices[n], fTrackStartIndices[n] + fTrackNHits[n])`; the children of
track `n` are `fTrackSecondariesIDs[fTrackSecondariesIndex[n] ... + fTrackSecondariesOffsets[n] - 1]`. In C++:
`GetSensitiveVolumeEnergy()`, `GetEnergyInVolume(name)`, `GetNumberOfPrimaries()`,
`GetPrimaryParticleName(n)`, `GetNumberOfTracks()`, `GetTrack(n)` / `GetTrackByID(id)` (a `TRestGeant4Track`
view), `GetParticleName(id)`, `GetProcessName(id)`, `GetVolumeName(id)`, `PrintG4Event()`.

### 3.8 Format version

The layout of the branches of each event class has a version, written in the UserInfo of its tree as a
`TParameter<int>` named `formatVersion` (it travels with the tree when it is copied or merged). All the event
classes are at version 1. When a file is opened, `TRestRun` checks that the version is one the class reads (an
error otherwise), and a missing branch is an error when the file is opened, not silently empty data. The trees
without version (written before 2026-11-02) are read, except `TRestGeant4Event`, whose older layout is not
supported.

```cpp
auto* tree = file.Get<TTree>("TRestDetectorHitsEvent");
auto* version = (TParameter<int>*)tree->GetUserInfo()->FindObject("formatVersion");
```

### 3.9 Reading events

With REST (`restRoot`, or a macro linked to the REST libraries):

```cpp
TRestRun run("R01234_HITS.root");
auto& hits = run.GetEvent<TRestDetectorHitsEvent>("TRestDetectorHitsEvent");
const double& energy = run.GetObservable<double>("hitsAna_Energy");   // type of the branch
for (long long entry = 0; entry < run.GetEntries(); entry++) {
    run.GetEntry(entry);              // reads every event tree and the AnalysisTree
    for (size_t n = 0; n < hits.GetNumberOfHits(); n++) { /* hits.GetX(n), hits.GetEnergy(n)... */ }
}
```

`run.HasEvent(name)` tells if the file has an event type, `run.GetEntryWithID(eventID)` loads an event by ID,
and `run.GetEvent()` is the last event type saved in the file (the one shown by the browser).

With plain ROOT or RDataFrame (no REST libraries needed); the event trees and the `AnalysisTree` have the same
entries, so they can be used as friends:

```cpp
ROOT::RDataFrame hits("TRestDetectorHitsEvent", "R01234_HITS.root");
auto h = hits.Define("energy", "Sum(fHitEnergy)").Histo1D({"e", "", 200, 0, 20}, "energy");

auto* tree = file.Get<TTree>("TRestRawSignalEvent");
tree->AddFriend("AnalysisTree");
tree->Draw("fSigSamples", "rawAna_NGoodSignals > 0 && eventID == 12");
```

## 4. Processes and the processing chain

### 4.1 Common keys of the processes and observables

| Key | Meaning |
|---|---|
| `observables` | Observables written to the analysis tree: names offered by the process, `["all"]` or `["none"]` (default: none). A name the process does not offer with its configuration is an error at initialization (the message lists the available ones). |
| `cuts` | Names of `TRestCut` sections of the manager: after the process, the event is rejected if it does not pass them ([section 6](#6-cuts-selection-and-plots)). |
| `verbose` | Verbose of the process (default: the one of the pipeline). |

The observables are written in the `AnalysisTree` as **`<processName>_<observable>`**, where `processName`
is the instance name of the process (e.g. `rawAna_NGoodSignals`, `trackLineAnalysis_OriginX`), while the
`observables` list uses the bare name (`NGoodSignals`). They can be of any type (`int`, `double`, `bool`,
`std::string`, `std::vector<double>`...). The observables offered by each process, and its other keys, are in
its class documentation and in the library READMEs.

A process whose input event is the same as its output (e.g. an analysis) copies the event and adds
observables; processes that change the event type (e.g. `TRestDetectorSignalToHitsProcess`) create a new
event of the output type.

**Random numbers.** The processes with randomness (`TRestDetectorHitsDriftProcess`,
`TRestDetectorAvalancheProcess`, `TRestDetectorHitsSmearingProcess`) have a `seed` key: 0 is a random seed;
otherwise the generator is re-seeded at each event from the seed and the event and subevent IDs, so the result
does not depend on the number of threads.

### 4.2 The pipeline (`TRestEventProcessManager`)

```yaml
pipeline:
  class: "TRestEventProcessManager"
  eventsToProcess: 0           # 0: all the entries (restManager --e)
  nThreads: 1                  # restManager --j
  inputAnalysisStorage: true   # copy the observables of the input file to the output AnalysisTree
  inputEventStorage: false     # also store the events of the input file
  outputEventStorage: true     # store every event type of the chain (false: only the last one)
  firstProcess:
    class: "..."
  secondProcess:
    class: "..."
```

| Key | Default | Meaning |
|---|---|---|
| `eventsToProcess` | 0 | Maximum number of entries processed, 0 for all. |
| `nThreads` | 1 | Number of threads (see 4.4). |
| `inputAnalysisStorage` | true | The output `AnalysisTree` copies the observables of the input file. With false it has only the event information and the observables of this pipeline. |
| `inputEventStorage` | false | The events of the input file are also stored as read, except those with the name of a stored process output. |
| `outputEventStorage` | true | Each event type produced by the chain is stored in its own tree (the output of the last process of that type). With false, only the output event of the last process (e.g. the raw signals of a simulation chain without its intermediate hits and signals). |

Every map with `class` inside the pipeline is created as a metadata: the processes form the chain **in their
order in the file**; other metadata (e.g. a readout) are kept and can be used by the processes.

### 4.3 Event routing

The route of the events is not configured: each process declares its input and output event types.

- The input of a process is the output of the **latest previous process** producing that event type; if no
  previous process produces it, the event is read from the input file. If neither exists, it is an error
  before the event loop.
- A process of any event type (e.g. `TRestSelectionProcess` without `event`) takes the output of the previous
  process; it cannot be the first one.
- A process that reads its events from an external file (DAQ readers such as
  `TRestRawMultiFEMINOSToSignalProcess` or `TRestRawFeminosRootToSignalProcess`) has no input event: the input
  file of the run is read by it, and the pipeline runs single-thread. With an unknown number of events the
  loop goes on until the end of that file (or `eventsToProcess`).
- An event is written only if every process returned it as accepted and it passed the cuts of every process.

Example with a type change:
[libraries/detector/examples/SignalToHits.yaml](../libraries/detector/examples/SignalToHits.yaml)
(`TRestDetectorSignalEvent` → `TRestDetectorHitsEvent`); a long chain:
[examples/FemDAQToTracks.yaml](../examples/FemDAQToTracks.yaml).

### 4.4 Multithreading

With `nThreads > 1` (or `restManager --j n`) the entries are split in `n` consecutive ranges, each processed
by a copy of the manager in its own thread and written to `<output>.thread<i>.root`; at the end these files
are merged in order into the output file and removed. The result has the same entries, in the same order, as
a single-thread processing (with fixed seeds, the same values). With a process that reads an external file
(no input event) the pipeline runs single-thread, with a warning.

SIGINT (CTRL-C), SIGTERM, SIGHUP and SIGQUIT stop the processing cleanly: the entries processed are written.

## 5. TRestRun and the REST file

### 5.1 Run parameters

The `TRestRun` section describes the run (the keys of `TRestRunInfo`) and its files:

```yaml
run:
  class: "TRestRun"
  runNumber: 1234             # "auto": a new number ($REST_HOME/.rest/runNumber, or $HOME/.rest)
  subRunNumber: 0
  runType: "calibration"
  runUser: "${ENV:USER}"
  runTag: "AlphaCAMM"
  runDescription: "Calibration with an Am-241 source"
  experimentName: "AlphaCAMM"
  runInfo:                    # free information of the run, stored as written
    meshVoltage: 300 V
    driftVoltage: 1000 V
  inputFileName: "Run01234_Calibration.root"           # restManager --i
  inputFormat: "Run[runNumber]_[runTag].root"          # run parameters taken from the input file name
  inheritInputRun: false      # true: the parameters not given are taken from the TRestRun of the input file
  mainDataPath: "/data/rest"  # prefix of a relative output file name
  outputFileName: "R[runNumber]_[runTag]_ANA.root"     # restManager --o
  outputBasketSize: 1048576   # bytes per basket of the output event trees, 0: ROOT default
```

| Key | Written by | Meaning |
|---|---|---|
| `runNumber`, `subRunNumber` | user | Run and subrun numbers (`runNumber` -1 if not set). |
| `runType`, `runUser`, `runTag`, `runDescription`, `experimentName` | user | Texts ("Null" if not set). |
| `runInfo` | user | Map key: value (e.g. voltages, gain), stored as written; read in REST units with `GetParameterValue("runInfo.meshVoltage")`. |
| `startTime`, `endTime` | REST | Start and end of the data taking, unix time in seconds. |
| `entriesSaved` | REST | Entries written in the file. |
| `mergedSubRuns` | REST | Subruns of a file merged with `REST_MergeRuns` (empty for a single subrun). |
| `outputEvent` | REST | Last event type saved, selected when the file is opened. |

How each parameter is obtained:

1. What is written in the configuration always wins.
2. A parameter not given is taken from the input file name if `inputFormat` has its tag (`[runNumber]`,
   `[runTag]`, `[runInfo.key]`...; each tag matches a part of the name without `_`, directories and extension
   are ignored). This is needed when the input is not a REST file (e.g. raw data).
3. Otherwise, with `inheritInputRun: true`, from the `TRestRun` of the input file.
4. `startTime`, `endTime`, `mergedSubRuns` and `runInfo` always come from the input run (the `runInfo` keys of
   the configuration are added to it or replace its values); a process reading raw data sets the times again.

### 5.2 File names

`outputFileName` and `inputFormat` are patterns with `[key]` placeholders: any key of the run as written in the
YAML (`[runNumber]`, `[subRunNumber]`, `[runTag]`, `[runType]`, `[runInfo.meshVoltage]`...) and `[version]` (the
REST version). `[runNumber]` is written with 5 digits and `[subRunNumber]` with 3 (zero padded). Any other
placeholder is an error (the legacy `[fRunTag]` names no longer exist).

- Without `outputFileName` (or `"Null"`), the output is `/dev/null`: the pipeline runs but nothing is written
  (plots of the pipeline output are then an error).
- An `outputFileName` that resolves to the input file is an error.
- An input that is not a REST run file (e.g. a DAQ file) is not opened by the run: it is read by the reader
  process of the pipeline. A pattern with `*` or `?` is not opened either (it is the input of plots).

### 5.3 Structure of a REST file

| Object | Type | Content |
|---|---|---|
| `<event class>` (e.g. `TRestRawSignalEvent`, `TRestDetectorHitsEvent`) | `TTree` | One tree per stored event type ([section 3](#3-events-and-their-format)), one entry per saved event, with `formatVersion` in its UserInfo. |
| `AnalysisTree` | `TTree` | One entry per saved event (the same entries as the event trees): the event information branches of 3.1 (`runOrigin`, `subRunOrigin`, `eventID`, `subEventID`, `timeSeconds`, `timeNanoSecs`, `ok`, `subEventTag`), the observables of the input file (unless `inputAnalysisStorage: false`) and the observables of the processes, `<processName>_<observable>`. |
| `RESTMetadataStore/` | directory | One `TObjString` per metadata instance, named as the instance, with its configuration as YAML (including `class`): the `TRestRun` (with its resolved parameters and file names), the pipeline with its processes, the cuts, readouts, media..., and the metadata of the input file (the history of the processing). |
| `Readouts/<instance>/` | directory | With a `TRestDetectorReadoutManager`: each readout used, its geometry (`Geometry`, `GeometryMD5`) and its decoding (`Decodings/`), so the file is self-contained. |
| `Geometry`, `GeometryMD5` | `TGeoManager`, `TObjString` | Files of restG4 and their analysis: the simulated geometry and its hash. |
| plot objects | `TCanvas`, histograms | Only if a `TRestAnalysisPlot` has this file as `outputFileName`. |

The configuration of a metadata can be read with plain ROOT:

```cpp
TFile file("R01234_ANA.root");
std::cout << file.Get<TObjString>("RESTMetadataStore/restRun")->GetString() << std::endl;
```

### 5.4 Opening a REST file

- `restRoot R01234_ANA.root` opens it with `REST_OpenInputFile`: the run is `_run` and its selected event
  `_ev` (a dataset is opened as `_dataSet`).
- `TRestRun run("R01234_ANA.root")` in a macro: `run.PrintMetadata()` (the run), `run.PrintAllMetadata()` (all
  the metadata of the file), `run.GetParameter("runInfo.meshVoltage")`, `run.GetMetadata(instance)` (the YAML
  node), `run.GetMetadataClass("TRestGeant4Metadata")` and `run.GetMetadataOfType<T>()` (the metadata as
  objects, with their readouts or geometry loaded).
- `REST_ViewEvents("R01234_ANA.root" [, "TRestDetectorHitsEvent"])` opens the event browser.

### 5.5 Merging subruns (`REST_MergeRuns`)

```
restRoot
root [0] REST_MergeRuns("R01234_*_ANA.root", "R01234_ANA.root")
root [1] REST_MergeRuns("Simulation_seed*.root", "Simulation.root")
```

- **Subruns of a run**: the files must have the same `runNumber`, consecutive `subRunNumber` (also of files
  already merged), the same trees and branches and the same processing (all the metadata but the `TRestRun`).
  The merged `TRestRun` has the first `subRunNumber`, `mergedSubRuns` with all of them, the earliest
  `startTime`, the latest `endTime` and the sum of `entriesSaved`.
- **Simulations** (files with a `TRestGeant4Metadata`) of the same configuration with different seeds (e.g.
  parallel `restG4 -s` jobs) are merged as one simulation: `nEvents` (and the CRY exposure) are added, a
  repeated seed is an error, and the `eventID` of each file is shifted by the events of the previous ones.

## 6. Cuts, selection and plots

### 6.1 `TRestCut`

A set of named conditions, C++ expressions of branches of the analysis tree, all of which must be fulfilled:

```yaml
fiducial:
  class: "TRestCut"
  conditions:
    originX: "trackLineAnalysis_OriginX > 20 && trackLineAnalysis_OriginX < 225"
    originY: "trackLineAnalysis_OriginY > 20 && trackLineAnalysis_OriginY < 225"
```

Cuts are sections of the manager (or of the dataset YAML) and are used by name:

- by any process, with `cuts: ["fiducial"]`: the event is rejected after the process if it does not pass them.
  They can use the observables of the process, of the previous ones and of the input file; in the pipeline only
  scalar observables (numbers or booleans).
- by `TRestSelectionProcess`, which only applies its cuts and copies the event; `event` is the event type
  (empty: the output of the previous process; required if it is the first process):

  ```yaml
  select:
    class: "TRestSelectionProcess"
    event: "TRestTrackEvent"
    cuts: ["fiducial"]
  ```

- by `TRestAnalysisPlot` (globally, per panel or per histogram), by `TRestDataSet` (on the entries) and by the
  dataset processes (on their reference).

### 6.2 `TRestAnalysisPlot`

A combined canvas of text panels and histograms of the observables of a run file, a dataset or a pattern of
them, filled in a single RDataFrame loop. Without `inputFileName`, it draws the output of the manager (pipeline,
generated or processed dataset); a manager with only cuts and plots draws the files given with `--i`:

```
restManager --c libraries/track/examples/TrackLinePlots.yaml --i "R*_LINEAR.root"
```

| Key | Meaning |
|---|---|
| `inputFileName` | Run file, dataset or pattern (empty: the output of the manager). |
| `cuts` | `TRestCut` names applied to all the data. |
| `outputFileName` | ROOT file of the canvas and histograms (`[key]` of the first run and `{expressions}`); a data file is updated keeping its data. Empty: not written. |
| `save` | Image of the canvas (pdf, png...), e.g. `"Plots_{run.runTag}.pdf"`. |
| `previewPlot` | Show the canvas; the files are saved when Enter is pressed. |
| `canvasSize`, `canvasDivisions`, `canvasDivisionMargins`, `legendPosition`, `paletteStyle` | Layout. |
| `panels` | Text panels: `labels` (`text`, `position`), `textSize`, `precision`, `cuts`. |
| `plots` | Plots: `name`, `title`, `xLabel`, `yLabel`, `logX/Y/Z`, `legend`, `timeDisplay`, `normalize`, `scale`, `save`... and `histos` (`name`, `variables`, `cuts`, `option`, colours and styles). |

Labels are texts with `{expression|format}`: `entries`, `runLength` (hours), `meanRate` (Hz), `startTime`,
`endTime`, `cuts`, `run.key` (e.g. `run.runTag`, `run.runInfo.meshVoltage`), `instance.key` (any metadata of
the first file), `mean/sigma/min/max(observable)` and arithmetic of them. Histogram variables are observables
or C++ expressions of them; vector observables fill one entry per element. Ranges accept units:

```yaml
plots:
  class: "TRestAnalysisPlot"
  canvasDivisions: [2, 1]
  save: "Plots_{run.runTag}.pdf"
  cuts: ["fiducial"]
  panels:
    - labels:
        - {text: "Run {run.runNumber}: {entries/runLength|%.1f} events per hour", position: [0.1, 0.8]}
  plots:
    - name: "Origin"
      option: "colz"
      variables:
        - {name: "trackLineAnalysis_OriginX", range: [0, 250 mm], nBins: 125}
        - {name: "trackLineAnalysis_OriginY", range: [0, 250 mm], nBins: 125}
```

All the keys are described in [TRestAnalysisPlot.cxx](../source/core/src/TRestAnalysisPlot.cxx); examples:
[TrackLinePlots.yaml](../libraries/track/examples/TrackLinePlots.yaml),
[TrackLineDataSetPlots.yaml](../libraries/track/examples/TrackLineDataSetPlots.yaml).

## 7. Datasets

### 7.1 `TRestDataSet`

A dataset joins the analysis trees of several runs (or datasets) in a single file and keeps the description of
every run (`TRestRunInfo`); each entry has the column `runIndex`, the position of its run, so the parameters of
the run of any entry are known and totals (e.g. the run time) are computed from the runs when needed.

```yaml
fiducial:
  class: "TRestCut"
  conditions:
    balance: "trackLineAnalysis_TrackBalance > 0.9"
dataSet:
  class: "TRestDataSet"
  filePattern: "R*_LINEAR.root"
  observables: ["trackLineAnalysis_*"]
  runConditions:
    runNumber: "runNumber > 370 && runNumber < 375"
    dates: 'startTime > date("2022/04/28") && endTime < date("2022/11/25 13:20")'
    mesh: "runInfo.meshVoltage > 300 V"
  cuts: ["fiducial"]
  columns:
    energyPerVolt: "trackLineAnalysis_Energy / runInfo.meshVoltage"
  outputFileName: "D_[runRange]_[runTag|Mixed].root"
```

| Key | Meaning |
|---|---|
| `filePattern` | Files the dataset is generated from (`*` and `?` in the file name): run files, or datasets to merge (not mixed). |
| `observables` | Patterns of the columns written (empty: all). `eventID`, `subEventID`, `timeSeconds` and `runIndex` are always written. |
| `runConditions` | name: expression on the parameters of each run, as written in its `TRestRunInfo` (`runNumber`, `subRunNumber`, `runTag`, `runType`, `runUser`, `experimentName`, `startTime`, `endTime`, `entriesSaved`, `runInfo.key`). Numbers (also with units) are compared in REST units, and **a literal with units such as `300 V` is converted to REST units**; texts as strings (`runTag == "Calibration"`); dates with `date("YYYY/MM/DD HH:MM")` (local time; also `YYYY-MM-DD`, with or without the time). A run is selected if it fulfills all of them. |
| `cuts` | `TRestCut` names (sections of the same YAML) applied to the entries. |
| `columns` | name: expression of new columns, in order, with observables and run parameters (the value of the run of each entry). |
| `outputFileName` | File written (see 7.3 for the placeholders). |
| `inputFileName` | Only to open a dataset already generated (as it is); not for the generation. |

A run present twice (same `runNumber` and `subRunNumber`, also inside the `mergedSubRuns` of a merged file) is
an error. The runs must have the same setup (processes, analysis tree branches, readouts and geometry); the
parameters of the processes may differ (each different processing is kept, with a warning listing the
differences).

### 7.2 Generating a dataset

With the macro (YAML with the `TRestDataSet` and its `TRestCut` as top-level sections, no manager):

```
restRoot
root [0] REST_GenerateDataSet("libraries/track/examples/TrackLineDataSet.yaml", "dataSet")
root [1] REST_GenerateDataSet("dataset.yaml", "dataSet", "D_Calibration.root", "R01234_*_ANA.root")
```

or with restManager, the `TRestDataSet` (and its cuts) inside a `TRestManager`, without `TRestRun`: `--i` is its
`filePattern` and `--o` its `outputFileName`:

```
restManager --c Generation.yaml --i "R*_ANA.root" --o "D_[runStart]_[runEnd]_[runTag|Mixed].root"
```

With a `TRestDataSetProcessManager` in the same manager, the generated dataset is processed in memory and only
the processed one is written (`--o` is then its output; the generated one is also written if the `TRestDataSet`
has an `outputFileName`).

### 7.3 Output names

`outputFileName` accepts `[key]` of the runs of the dataset:

| Placeholder | Value |
|---|---|
| `[runStart]`, `[runEnd]` | Lowest and highest `runNumber` (5 digits). |
| `[runRange]` | `00010_00012`, or `00010` with a single run number. |
| `[runTag]`, `[runType]`, `[runInfo.key]`... | A parameter of the runs, if it is the same in all of them (otherwise an error). |
| `[runTag\|Mixed]` | The same with a default, used if the runs do not share the value. |

E.g. `D_[runStart]_[runEnd]_[runTag|Mixed].root` → `D_00010_00012_Calibration.root`.

### 7.4 Structure of a dataset file

| Object | Type | Content |
|---|---|---|
| `AnalysisTree` | `TTree` | The selected entries and columns, with `runIndex` (`int`). The columns added by dataset processes are also here. |
| `RESTDataSetInfo` | `TObjString` | The runs, a YAML list in the order of `runIndex`; each element has `run` (its `TRestRunInfo`: numbers, tags, `runInfo`, times, `entriesSaved`), `fileName`, `entries` (of the run file), `selectedEntries` (in the dataset) and `processing` (index in `RESTDataSetProcessing`). |
| `RESTDataSetProcessing` | `TObjString` | The different processings of the runs (configurations of their `TRestEventProcessManager`), a YAML list. |
| `RESTMetadataStore/` | directory | The `TRestDataSet` (with `filePattern` and the file as `outputFileName`), the `TRestCut` used, the metadata of the first input file with runs selected (processing, readouts, `TRestGeant4Metadata`...; not its `TRestRun`) and, for a processed dataset, the dataset pipeline and each dataset process with its results. |
| `Readouts/`, `Geometry` | | The readouts and geometry of that input file; a `TRestDetectorReadoutManager` with `inputFileName: "Dataset.root"` loads a readout of the dataset. |
| `RESTDataSetStore/<class>/<name>/` | directory | Objects of each dataset process (spectra, fits, PDFs, ROC curves...). |

Merging datasets: a `filePattern` of datasets joins their runs (renumbering `runIndex`); the result is a dataset
like any other.

### 7.5 Using a dataset

```cpp
// restRoot Dataset.root opens it as _dataSet
TRestDataSet dataSet("Dataset_Calibration.root");
dataSet.PrintDataSetInfo();                      // runs, times, entries
auto frame = dataSet.GetDataFrame();             // ROOT::RDF::RNode with all the columns
const auto& runs = dataSet.GetRuns();            // runs[i].run is the TRestRunInfo of runIndex i
double seconds = dataSet.GetTotalTime();         // sum of endTime - startTime of the runs
auto* readouts = dataSet.GetMetadataOfType<TRestDetectorReadoutManager>().front();
```

`Define(name, expression)` adds a column and `Write(fileName)` writes the dataset. A dataset file is also the
`inputFileName` of a `TRestAnalysisPlot`, as a run file. With plain ROOT,
`ROOT::RDataFrame("AnalysisTree", "Dataset.root")`.

### 7.6 Dataset processes (`TRestDataSetProcessManager`)

A `TRestDataSetProcessManager` section of the `TRestManager` (instead of `TRestRun` and the event pipeline)
processes a dataset: each dataset process, in order, obtains its parameters on a **reference** dataset and adds
columns to the processed dataset, which is written once.

```yaml
manager:
  class: "TRestManager"
  fiducial:
    class: "TRestCut"
    conditions:
      inside: "hitsAnalysis_MeanX * hitsAnalysis_MeanX + hitsAnalysis_MeanY * hitsAnalysis_MeanY < 2500"
  dataSetProcessing:
    class: "TRestDataSetProcessManager"
    inputFileName: "Dataset_Calibration.root"               # --i
    inputFormat: "Dataset_[sample].root"
    outputFileName: "Dataset_[sample]_Calibrated.root"      # --o
    calib:
      class: "TRestDataSetCalibration"
      referenceFileName: "Dataset_Calibration.root"
      cuts: ["fiducial"]
      observable: "hitsAnalysis_Energy"
      range: [0, 20000]
      peaks:
        - {energy: "5.9 keV"}
        - {energy: "2.9 keV"}
```

Keys of the manager: `inputFileName` (`--i`), `outputFileName` (`--o`, with the `[key]` of 7.3 or the tags of
`inputFormat`) and `inputFormat` (tags read from the input name, so the same YAML processes several datasets
with only `--i`).

Keys common to every dataset process:

| Key | Meaning |
|---|---|
| `referenceFileName` | Dataset where the parameters are obtained, used as it is. Empty: the processed dataset, with the columns of the previous processes. A pattern must match a single dataset unless `referenceSelection` is given. `restManager --r <process>=<file>` sets it. |
| `referenceSelection` | `closest`: among the matches, the dataset closest in time to the processed one (e.g. the calibration nearest to a background). |
| `cuts` | `TRestCut` names of the manager applied to the reference. |
| `referenceUsed`, `parametersFileName` | Results written by REST: the reference used, and the dataset where the parameters were computed. |

If the reference was already processed by a process with the same name and class, its stored parameters are
used and nothing is computed (e.g. a background calibrated with the parameters of a calibrated calibration).
The parameters (results) are stored in the configuration of the process in `RESTMetadataStore`, and its
objects in `RESTDataSetStore/<class>/<name>`.

| Process | Library | Columns added | Parameters obtained |
|---|---|---|---|
| `TRestDataSetCalibration` | analysis | `<name>_<calibratedObservable>` (default `calib_Energy`) = slope · observable + intercept (keV) | `slope`, `intercept`, fit of each peak (`peaks`, RooFit, gaussian or Crystal Ball) |
| `TRestDataSetGainMap` | detector | `<name>_<calibratedObservable>` per segment of the readout and `..NoSegmentation` | Calibration of each segment (`segments`, `splitX/Y`) and of the whole readout |
| `TRestDataSetLogOdds` | analysis | `<name>_<observable>` = log(1 − p) − log(p) for each observable, and `<name>_total` | PDF of each observable in the reference |
| `TRestDataSetTMVA` | analysis | `<name>_score` | A trained TMVA method (BDT by default), signal = reference, background = `backgroundFileName`; ROC integral, separation, KS tests |
| `TRestDataSetSignalSelection` | analysis | (filters the entries) | `range` of `observable` between the `quantiles` of the reference, and its `efficiency` |
| `TRestDataSetCombine` | analysis | The columns of another dataset of the same events (`combineFileName`, `observables`, `prefix`), matched by run, `eventID` and `subEventID`, and `<name>_matched` (0 where the event is not there: default values, or filtered without `keepUnmatched`) | None (e.g. the vetoes and the Micromegas analysed apart, combined) |

Examples (each file documents how to run it):
[DataSetCalibration.yaml](../libraries/detector/examples/DataSetCalibration.yaml),
[DataSetLogOdds.yaml](../examples/DataSetLogOdds.yaml), [DataSetTMVA.yaml](../examples/DataSetTMVA.yaml),
[DataSetSignalSelection.yaml](../examples/DataSetSignalSelection.yaml) (see
[examples/README.md](../examples/README.md)).
The macro `REST_ViewTMVA(dataset, name[, pdf])` draws the results of a `TRestDataSetTMVA`, and
`TRestDataSetGainMap::ReadFromFile`, `DrawSpectrum`, `DrawGainMap` and `Refit` inspect a gain map in restRoot.

## 8. Detector, simulation and data taking

### 8.1 Detector description

The [detector library](../libraries/detector/README.md) describes the detector:

- **Readouts** map positions to channels and back: `TRestMicromegasReadout` (strip planes),
  `TRestGDMLReadout` (volumes of a GDML geometry, e.g. vetoes, with a readout volume for the light
  attenuation) and `TRestFibreReadout` (a scintillator crossed by fibre lattices, e.g. LiquidO: the closest
  fibre, the fibres within a distance, and the fibre placements for the GDML of the simulation). A readout is built once from its YAML and a decoding file (`daqChannel physicalID` rows)
  and exported to a ROOT file with the macros `REST_Detector_GenerateMicromegasReadout`,
  `REST_Detector_GenerateGDMLReadout` and `REST_Detector_GenerateFibreReadout`; the pipelines load it with a `TRestDetectorReadoutManager`
  (`readouts`: `instanceName`, `decodingName`, `inputFileName`; `readout`: the readout of the data), which also
  stores it in the output file.
- **Media** (`TRestDetectorMedium`): a material at its `pressure` and `temperature`, with a `charge` response
  (`wValue`, `fanoFactor`, `driftField`, fixed transport parameters or the transport and gain tables of its
  `mediumFile`, computed by restGarfield) and a `light` response (`lightYield`, `attenuationLength`,
  `effectiveLightSpeed`). The light is simulated in three processes: `TRestDetectorScintillationProcess`
  (photons produced) and `TRestDetectorLightCollectionProcess` (photons reaching each channel and their
  photoelectrons with the quantum efficiency: `localCapture` for opaque scintillators such as LiquidO,
  `attenuation` for transparent ones read at an end such as the vetoes).
  The processes find the medium and the readout by name (`medium`, `readout`; optional if there is only one).

```yaml
readoutManager:
  class: "TRestDetectorReadoutManager"
  readouts:
    - inputFileName: "AlphaCAMM_readout.root"
      instanceName: "AlphaCAMM_Readout"
      decodingName: "nominal_decoding"
medium:
  class: "TRestDetectorMedium"
  name: "ArIso"
  pressure: 1 bar
  charge:
    wValue: 26.2 eV
    fanoFactor: 0.2
    driftField: 100 V/cm
    driftVelocity: 11.5 mm/us
```

The conversions between libraries (Geant4 → hits, hits → tracks, raw ↔ detector signals) are in the
[connectors library](../libraries/connectors/README.md); the full simulation chain to raw signals is
[Geant4ToRaw.yaml](../libraries/connectors/examples/Geant4ToRaw.yaml).

### 8.2 Simulation

- **restG4** ([README](../packages/restG4/README.md), [examples](../packages/restG4/examples/README.md)):
  `restG4 simulation.yaml [-t threads] [-n events] [-s seed] [-o output.root] [--set name=value]`. The
  configuration has a `TRestGeant4Manager` with a `TRestRun`, a `TRestGeant4Metadata` (geometry, `nEvents`,
  `seed`, `generator`, `activeVolumes`, `storeTracks`, biasing...) and a `TRestGeant4PhysicsLists`. The output
  is a REST file with the tree `TRestGeant4Event` (3.7), the metadata and the `Geometry`. The `parameters` also
  replace the GDML `<constant>`s and `<!ENTITY>`s of the same name; `--update-gdml` writes them into the GDML
  files and exits. Normalize with `TRestGeant4Metadata::GetEquivalentNumberOfEvents()`.
- **restCRY**: `restCRY CRYShowers.yaml` writes cosmic-ray showers (a `TRestGeant4CRYShowers` section),
  simulated by the restG4 generator `type: "cry"`, which stores the exposure time for rates
  ([12.Generators](../packages/restG4/examples/12.Generators)).
- **restGarfield** ([README](../packages/restGarfield/README.md)): `restGarfield ArIso.yaml` computes the
  transport and gain tables of a medium with Garfield++ and stores them in its medium file, read by the detector
  library without Garfield++.
- The simulated events are analysed with the [geant4 library](../libraries/geant4/README.md)
  (`TRestGeant4AnalysisProcess`, `TRestGeant4QuenchingProcess`) and converted to detector hits by
  `TRestGeant4ToDetectorHitsProcess`.

### 8.3 Data taking

[restDAQ](../packages/restDAQ/README.md) (CMake option `REST_DAQ`) builds `femclient`, the data taking of
AFTER/AGET electronics (ARC, FEMINOS, DCC, TCM): `femclient restDAQ.yaml`, with a `TRestRawDAQMetadata`
(electronics, FEMs, run limits) and a `TRestRun` (`runNumber: auto`). It writes REST files with the tree
`TRestRawSignalEvent` (3.3), processed directly with restManager. Files of other DAQs are read by the reader
processes of the [raw library](../libraries/raw/README.md) (`TRestRawMultiFEMINOSToSignalProcess`,
`TRestRawFeminosRootToSignalProcess`, `TRestRawFemDAQToSignalProcess`).

## 9. Logging, restRoot and macros

### 9.1 Verbose levels

| Level | Messages written |
|---|---|
| `silent` / `error` | Errors only (no message has the level silent, so both give the same output). |
| `warning` | Also warnings and essential information. |
| `info` | Also the information of each step (default global level). |
| `debug` | Also debug messages. |

- `verbose` can be given to any section; without it, a section inside a container (the processes of a pipeline,
  the sections of the manager) takes the one of its container, and the rest the global level
  (`restManager --v <level>`). The `verbose` of a process also applies to the messages of the events and
  utilities it uses.
- Similar warning and info messages written many times from the same line of code are suppressed after 10 in
  a window of 10 s (the number suppressed is printed at the end); `restManager --maxRepeated <n>` (0: no
  suppression) and `--repeatedInterval <s>` change it. Nothing is suppressed with verbose `debug`.

### 9.2 restRoot and macros

`restRoot` is ROOT with the REST libraries, their headers and the compiled macros loaded: the `REST_*` macros
are called directly (do not `.L` or `.x` them). A REST file given as argument is opened (`_run`, `_ev` or
`_dataSet`).

| Macro | Use |
|---|---|
| `REST_OpenInputFile(file)` | Opens a run file (`_run`, `_ev`) or a dataset (`_dataSet`). |
| `REST_ViewEvents(file [, eventType])` | Event browser. |
| `REST_GenerateDataSet(yaml, name [, output, input])` | Generates a dataset ([7.2](#72-generating-a-dataset)). |
| `REST_MergeRuns(pattern, output)` | Merges subruns or simulations ([5.5](#55-merging-subruns-rest_mergeruns)). |
| `REST_ViewTMVA(dataset, name [, pdf])` | Results of a `TRestDataSetTMVA`. |

The list is in [source/macros/README.md](../source/macros/README.md). The libraries and packages add their own
macros (e.g. `REST_Raw_PulsesViewer`, `REST_DAQViewer`, `REST_Detector_ViewMicromegasReadout`,
`REST_Geant4_ReadEvents`, `REST_Geant4_ViewGeometry`, `REST_Track_ViewTrackLine`, `REST_Garfield_DrawMedium`),
described in their READMEs.
