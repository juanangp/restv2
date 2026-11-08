# REST (restv2)

REST (Rare Event Searches Toolkit) is a C++/ROOT framework to simulate, process and analyse the data of rare event
experiments. The events of a run are processed by chains of processes configured in YAML files (`restManager`), and
the results are stored in REST files (ROOT files with the events, the analysis tree and the metadata).

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
| [doc](doc) | Doxygen configuration (target `doc`). |

## Requirements

- CMake ≥ 3.18 and a C++17 compiler (GCC ≥ 8 or Clang).
- ROOT ≥ 6.26 with Eve/OpenGL and RooFit.
- yaml-cpp ≥ 0.6.
- Optional: Geant4 (`restG4`), Garfield++ (`restGarfield`), CRY (`restCRY`), GoogleTest (tests), Doxygen
  (documentation).

## Build and install

```bash
mkdir -p build && cd build
cmake ..
make -j8 install
source ../install/thisREST.sh
```

The installation goes to `install/` in the repository (or `$REST_PATH` if defined), never to `/usr/local`.

The libraries and packages are enabled by default and disabled when their dependencies are missing; the CMake
options are listed in [SETUP_AND_RUN.md](SETUP_AND_RUN.md).

## Usage

```bash
restManager --c config.yaml --i input.root --o output.root   # processing chain of a YAML configuration
restG4 simulation.yaml                                        # Geant4 simulation
restRoot                                                      # ROOT with the REST libraries and macros
```

The executables and their options are described in [source/bin](source/bin/README.md).

## Tests and documentation

```bash
cmake .. -DBUILD_TESTING=ON && make -j8
ctest -L restG4 --output-on-failure       # restG4 examples
cmake --build . --target doc              # Doxygen HTML in build/doc/html
```

## Migration from REST v2.x

- [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md): changes for users of the legacy framework.
- [LEGACY_INVENTORY.md](LEGACY_INVENTORY.md) and [LEGACY_COMPARISON.md](LEGACY_COMPARISON.md): legacy classes and
  their equivalents.
- [MIGRATION_PENDING.md](MIGRATION_PENDING.md): what is still to be migrated.

## License

See [LICENSE](LICENSE).
