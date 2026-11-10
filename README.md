# REST (restv2)

REST (Rare Event Searches Toolkit) is a C++/ROOT framework to simulate, process and analyse the data of rare event
experiments. The events of a run are processed by chains of processes configured in YAML files (`restManager`), and
the results are stored in REST files (ROOT files with the events, the analysis tree and the metadata).

## Overview

REST was born as a collaborative software effort to provide common tools for the acquisition, simulation and
data analysis of gaseous Time Projection Chambers (TPCs), and its use is not exclusive of detector data
analysis. restv2 is a new version of the framework: the same concepts, with YAML configurations, a single
repository for the framework, the libraries and the packages, and ROOT RDataFrame for the datasets.

The framework (`source/core`) provides three interfaces, the abstract classes `TRestEvent`, `TRestMetadata`
and `TRestEventProcess`, from which every REST library derives its **specific objects**:

- **Events** (`TRestEvent`): the data of one event at a stage of the processing, e.g. raw signals of the
  electronics, simulated Geant4 hits, detector hits or tracks. Each event type is written in its own tree of
  the REST file.
- **Metadata** (`TRestMetadata`): any object configured from a YAML section (its `class`, `name` and
  registered keys, unknown keys are an error), e.g. a readout, a detector medium or the conditions of a
  simulation. The metadata used in a processing is stored in the output file, so every result keeps its full
  configuration.
- **Event processes** (`TRestEventProcess`): transform an input event into an output event (possibly of
  another type) and compute **observables**, the values of each event stored in the analysis tree.

Processes are combined into **processing chains** (`TRestEventProcessManager`) with full traceability. The
`TRestManager` section of a YAML file encapsulates everything needed to launch a processing: the run
(`TRestRun`, the input and output files), the process chain, the metadata of the processes, the cuts
(`TRestCut`) and the plots (`TRestAnalysisPlot`); `restManager` reads it and runs it. Any REST file contains
the event trees, the `AnalysisTree` with the observables and the metadata store, and is read back with
`TRestRun` in `restRoot` or in macros.

For the high-level analysis, the analysis trees of many runs are merged into a **dataset** (`TRestDataSet`,
with the cuts and the information of each run), and **dataset processes** (`TRestDataSetProcess`) compute
parameters on a reference dataset and apply them to others: energy calibration and gain maps, log odds,
multivariate classifiers of TMVA and signal selections at a fixed efficiency.

The framework also provides an event browser and viewers (`TRestBrowser`, `TRestEventViewer`), units in any
YAML value (`REST_Units`, e.g. `5.3 MeV`), logging with verbose levels per object, and `restRoot`, ROOT with
the REST libraries and macros loaded.

## Contents

| Directory | Contents |
|---|---|
| [source/tools](source/tools/README.md) | Generic functions: strings, files, YAML configuration, units, logging. |
| [source/core](source/core/README.md) | Events, metadata, runs, processes and their managers, analysis tree, datasets, browser. |
| [source/analysis](source/analysis/README.md) | Pulse shape analysis and other generic analysis functions. |
| [source/macros](source/macros/README.md) | Macros of `restRoot`. |
| [source/bin](source/bin/README.md) | Executables: `restManager`, `restRoot`... |
| [libraries/raw](libraries/raw/README.md) | Raw signal events (DAQ data), their processes and viewers. |
| [libraries/detector](libraries/detector/README.md) | Hits and detector signals, readouts, medium, response and their processes. |
| [libraries/geant4](libraries/geant4/README.md) | Geant4 simulation events and metadata, and their processes. |
| [libraries/track](libraries/track/README.md) | Track events and their processes. |
| [libraries/connectors](libraries/connectors/README.md) | Processes converting events between libraries, the DAQ viewer. |
| [packages/restG4](packages/restG4/README.md) | Geant4 simulation (`restG4`, `restCRY`) and its [examples](packages/restG4/examples/README.md). |
| [packages/restGarfield](packages/restGarfield/README.md) | Garfield++ transport and gain tables of the detector media. |
| [packages/restDAQ](packages/restDAQ/README.md) | Data taking of AFTER/AGET electronics (`femclient`). |
| [examples](examples/README.md) | High-level analysis of datasets (log odds, TMVA, signal selection). |
| [doc](doc) | Doxygen configuration (target `doc`). |

## Requirements

Required:

| Software | Version | Used for |
|---|---|---|
| CMake | ≥ 3.18 | Build. |
| C++ compiler | C++17 (GCC ≥ 8 or Clang) | Build. |
| [ROOT](https://root.cern) | ≥ 6.26 (tested with 6.36), with the components Eve/OpenGL (RGL), Geom/GDML, RooFit and TMVA | Everything: files, trees, RDataFrame, viewers, peak fits (RooFit), TRestDataSetTMVA (TMVA). |
| [yaml-cpp](https://github.com/jbeder/yaml-cpp) | ≥ 0.6 | The YAML configurations. |

Optional (each one enables a part of restv2; without it that part is not built and CMake says so):

| Software | Version | Enables | CMake option |
|---|---|---|---|
| [Geant4](https://geant4.web.cern.ch) | ≥ 11.0 (tested with 11.0.3), with GDML (Xerces-C) and its data | `restG4` (packages/restG4) | `REST_G4` (ON if found, `Geant4_DIR`) |
| [CRY](https://nuclear.llnl.gov/simulation/main.html) | 1.7 | `restCRY`, cosmic-ray showers for restG4 | `REST_CRY` (ON if found, `CRY_PATH` or `$CRYHOME`) |
| [Garfield++](https://garfieldpp.web.cern.ch) | ≥ 4.0 | `restGarfield`, transport and gain tables of the media | `REST_GARFIELD` (ON if found, `$GARFIELD_INSTALL`) |
| readline and [prometheus-cpp](https://github.com/jupp0r/prometheus-cpp) | prometheus-cpp 1.2 (downloaded if not found) | `femclient` (packages/restDAQ) | `REST_DAQ` (OFF by default) |
| [GoogleTest](https://github.com/google/googletest) | ≥ 1.10 | The tests (`ctest`) | `BUILD_TESTING` (OFF by default) |
| [Doxygen](https://www.doxygen.nl) and Graphviz | | The HTML documentation (target `doc`) | Found automatically |
| ccache | | Faster rebuilds | `REST_USE_CCACHE` (ON if found) |

On Ubuntu 24.04 the system packages are (ROOT, Geant4, Garfield++ and CRY are installed from their own
distributions, see their web pages):

```bash
sudo apt install build-essential cmake git libyaml-cpp-dev \
    libgtest-dev libreadline-dev doxygen graphviz ccache      # optional
```

The Docker image of the continuous integration (repository `restv2-docker`) has all of them installed, and
can be used to build restv2 in a clean environment.

## Build and install

```bash
mkdir -p build && cd build
cmake ..
make -j8 install
source ../install/thisREST.sh
```

The installation goes to `install/` in the repository (or `$REST_PATH` if defined), never to `/usr/local`.

`thisREST.sh` sets `REST_PATH`, `PATH`, `LD_LIBRARY_PATH` and `ROOT_INCLUDE_PATH`, removing the entries of a
REST loaded before (e.g. a legacy installation sourced in `~/.bashrc`, whose headers or libraries would
otherwise be found first).

### CMake options

| Option | Default | Description |
|---|---|---|
| `CMAKE_CXX_STANDARD` | 17 | C++ standard (raised automatically if ROOT uses a newer one). |
| `RestRaw`, `RestDetector`, `RestTrack`, `RestGeant4`, `RestConnectors` | ON | The libraries; the connectors are built only with the libraries they connect. |
| `REST_G4` | ON | `restG4`; disabled if Geant4 is not found (`Geant4_DIR`). |
| `REST_CRY` | ON | `restCRY` (packages/restG4, CRY showers for the `cry` generator of restG4); disabled with a warning if CRY is not found (`CRY_PATH` or `$CRYHOME`, set by the CRY setup script). |
| `REST_GARFIELD` | ON | `restGarfield` (transport and gain tables of the media); disabled if Garfield++ is not found (`$GARFIELD_INSTALL` or `Garfield_DIR`). |
| `REST_DAQ` | OFF | `femclient` (packages/restDAQ); needs readline and prometheus-cpp (found with `find_package`, e.g. `-DCMAKE_PREFIX_PATH=<prometheus-cpp install>`, otherwise downloaded). |
| `BUILD_TESTING` | OFF | The tests (GoogleTest). |
| `REST_WERROR` | OFF | Compiler warnings as errors (recommended for development). |
| `REST_USE_CCACHE` | ON | `ccache` as compiler launcher if installed (fast rebuilds after reconfiguring or switching branches). |

The packages are part of the repository (not submodules). To skip them even if their dependencies are
installed, e.g. `cmake .. -DREST_G4=OFF -DREST_GARFIELD=OFF` (`REST_CRY` depends on `REST_G4`).

## Usage

```bash
restManager --c config.yaml --i input.root --o output.root   # processing chain of a YAML configuration
restG4 simulation.yaml                                        # Geant4 simulation
restRoot                                                      # ROOT with the REST libraries and macros
```

The executables and their options are described in [source/bin](source/bin/README.md). The
[user guide](doc/UserGuide.md) explains the configuration, the format of the events and of the REST files,
the processes, the datasets and the analysis.

## Tests and documentation

```bash
cmake .. -DBUILD_TESTING=ON && make -j8
ctest --output-on-failure                 # all the tests (make check: build and run them)
ctest -L detector --output-on-failure     # the unit tests of a component (tools, core, analysis, raw...)
ctest -L restG4 --output-on-failure       # restG4 examples
cmake --build . --target doc              # Doxygen HTML in build/doc/html
```

The validation of the pull requests (GitHub Actions) builds everything and runs the tests in the Docker image
of the `restv2-docker` repository; see [CONTRIBUTING.md](CONTRIBUTING.md) for the workflow (master only
changes through pull requests) and the continuous integration.

## Documentation

- [doc/UserGuide.md](doc/UserGuide.md): how to use restv2 (configuration, events and file formats, processes,
  runs, cuts and plots, datasets).
- The README of each component (table above) and the class documentation (Doxygen, target `doc`).
- [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md): moving from REST v2.x (legacy): configurations, equivalent classes,
  and how to port a legacy class to restv2.
- [MIGRATION_STATUS.md](MIGRATION_STATUS.md): status of the migration of the legacy classes, pending work and
  decision log.
- [CONTRIBUTING.md](CONTRIBUTING.md): workflow, tests and continuous integration.

## License

See [LICENSE](LICENSE).
