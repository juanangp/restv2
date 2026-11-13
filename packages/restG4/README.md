# The restG4 package

This package is used to launch Geant4 simulations configured with REST YAML files and to write the
simulated events as REST run files, analysed with the geant4 library. It also generates cosmic-ray showers
with CRY.

`restG4` reads a YAML configuration file and writes a REST run file
with an event (`TRestGeant4Event`) per simulated event (or sub-event) that is stored. `restCRY` generates
cosmic-ray showers with CRY for the `cry` generator of restG4.

## Table of Contents

- Build
- Configuration
- Usage
- Volume biasing
- restCRY
- Examples and tests
- Structure of the output file

## Build

Optional package, built by default when Geant4 is found (`cmake -DREST_G4=ON|OFF`, `-DGeant4_DIR=...`). It needs
the geant4 library of REST. `restCRY` is built only if CRY is found (`-DCRY_PATH=<cry>` or the CRY setup script,
`$CRYHOME`); `-DREST_CRY=OFF` skips it.

## Configuration

The configuration file is a YAML file with a section of class `TRestGeant4Manager` that contains (the names of
the sections are free, their `class` identifies them):

- a `TRestRun`: run number, tags, `outputFileName` (e.g. `"data/Run_[runTag].root"`), `mainDataPath`...
- a `TRestGeant4Metadata`: the geometry (`gdmlFilename`, `geometryPath`), `nEvents`, `seed`, the `generator`
  (sources, energy and angular distributions, `cry`...), the `activeVolumes` (`sensitive`, `maxStep`, `chance`:
  probability to store the hits of the volume in an event), `activateAllVolumes` (all the volumes active),
  `killVolumes`, `energyRangeStored`, `saveAllEvents`, `storeTracks`, `registerEmptyTracks` (false: the tracks
  without hits in the stored volumes are not stored), `subEventTimeDelay`, `fullChain`, `biasingVolumes` (see
  Volume biasing)...
- a `TRestGeant4PhysicsLists`: the Geant4 physics lists (`physicsList`, only the ones supported by restG4, an
  unknown name is an error) and the production cuts.

```yaml
parameters:
  energy: 5

g4Manager:
  class: "TRestGeant4Manager"
  run:
    class: "TRestRun"
    runTag: "${PARAM:energy}MeV"
    outputFileName: "data/Run_[runTag].root"
  geant4Metadata:
    class: "TRestGeant4Metadata"
    gdmlFilename: "geometry/setup.gdml"
    nEvents: 10000
    activeVolumes:
      - name: "gas"
        sensitive: true
    generator:
      type: "volume"
      from: "source"
      source:
        particle: "alpha"
        energy:
          type: "mono"
          energy: ${PARAM:energy} MeV
  physicsG4:
    class: "TRestGeant4PhysicsLists"
    physicsList:
      - name: "G4EmStandardPhysics_option4"
```

The values can use the `parameters` of the file (`${PARAM:name}`, changed for one execution with `--set`) and
environment variables (`${ENV:USER}`). The GDML file is resolved relative to the directory of the
configuration file (then in `geometryPath`), as the files of the generator (histograms, Decay0, cry showers);
the output file relative to the launch directory. The entities of the GDML and the parameters used in its
constants are resolved before Geant4 reads it. A parameter that is used neither by the configuration nor by
the geometry is an error (a typo). The physical and logical volume names of the geometry must be unique.

The old `.rml` (XML) configuration files of REST are not supported. The full examples are in `examples/` (see
below), and the options of each section are documented in the classes of the geant4 library
(`TRestGeant4Metadata`, `TRestGeant4PrimaryGeneratorInfo`, `TRestGeant4PhysicsLists`).

## Usage

Typing `restG4 -h` will show all available options.

### Basic Usage

`restG4 simulation.yaml` will launch a simulation using `simulation.yaml` as the configuration file (also
`restG4 -c simulation.yaml`). The output file will be saved to the path specified in the configuration
(`outputFileName` of the `TRestRun`); without it nothing is saved.

### Multithreading

`restG4` makes use of the multithreading capabilities of Geant4 and can be used in multithreading mode to
significantly increase simulation speed.

- `restG4 simulation.yaml -t 8` (or `-j 8`, `--threads 8`) will launch a simulation using 8 worker threads.

By default `restG4` is executed in serial mode (one single thread) and is equivalent to the `-t 0` option.

Simulation results are determined by the random seed and the number of threads, so using different number of
threads with the same seed will produce different results. Using the same seed and same number of threads
produces the same results.

The multithreading implementation of `restG4` is new and although it looks to be working properly it may have
some bugs. If you find one of such bugs, please post an issue [here](https://github.com/rest-for-physics/restG4/issues)
with the `multithreading` label.

We encourage our users to use the multithreading feature especially when performing exploratory simulations.

### Overriding values from the configuration

The CLI interface allows to set a few different parameters without having to modify the configuration file.

- `restG4 simulation.yaml -o output.root` to specify the output file.
- `restG4 simulation.yaml -n 1000` to set the number of processed events (`n` in Geant4's `/run/beamOn n` or
  `nEvents` in the configuration). The scientific notation is accepted (`-n 1E6`).
- `restG4 simulation.yaml -g geometry.gdml` to specify the geometry file.
- `restG4 simulation.yaml -s 12345` to set the random seed (a positive number).
- `restG4 simulation.yaml --runNumber 10` to set the run number.
- `restG4 simulation.yaml --set energy=2 --set foil=1` to change parameters of the `parameters` section for this
  execution (the file is not changed).
- `restG4 simulation.yaml --set foil=1 --update-gdml` writes the parameters in the constants and entities of the
  GDML files (so any viewer shows the geometry that is simulated) and exits without simulating.

The interactive mode (`-i`) is not implemented.

### Ending the simulation early

The simulation run will naturally end when the selected number of events have been processed. They can either
be specified in the configuration file or via the `-n` flag on the CLI. However, the user can also specify
additional conditions to end the simulation early.

- `restG4 simulation.yaml -e 1000` will signal the simulation to stop once 1000 events have been marked as valid
  (saved on the output file). The final simulation will probably have a slightly higher number of entries than
  the value specified in the case of multithreading as the worker threads may still be working on valid events,
  or they may not have been saved yet. The number of processed events (the equivalent of `nEvents`) will be
  adjusted to the real number of processed events, overriding the selected value of the user, so the resulting
  simulation would be equivalent to a plain simulation using this value as `nEvents`. This holds more accurately
  the larger the `-e` parameter.
- `restG4 simulation.yaml --time 1h20m30s` will signal the simulation to stop after 1 hour 20 minutes and 30
  seconds have elapsed since the start. You can specify any value using this format such as `1h15m`, `30s`, etc.
- Sending the interrupt signal (typically via `CTRL+C`) will signal the simulation to stop, and it will attempt to
  save the results into disk before closing the process.

The same limits can be given in the configuration (`nRequestedEntries`, `simulationMaxTimeSeconds` of the
`TRestGeant4Metadata`). With the `cry` generator the simulation also ends when all the showers of the file have
been simulated.

## Volume biasing

To simulate a rare flux through a shielding (e.g. external gammas reaching a detector inside a lead castle),
`biasingVolumes` of the `TRestGeant4Metadata` defines virtual volumes (`virtualBox`, axis-aligned of side `size`,
or `virtualSphere` of radius `size`, centered at `position`) that are not part of the Geant4 geometry: they are
geometric tests applied to the steps. They must be nested, listed from the outermost to the innermost, each one
strictly inside the previous one (checked at start), and the primaries of the generator should start outside the
outermost one (a warning counts the ones inside, which are never stopped). `nEvents` is required.

With N volumes, one restG4 execution runs N + 1 stages (a Geant4 run each, in memory):

- Stage 0 simulates the `nEvents` events of the `generator`.
- In stage k (k < N), a particle entering volume k (the chord of a step, from its pre-step point outside the
  volume, crosses its surface: also a step that goes through the volume) is stopped. If its kinetic energy is
  inside the `energyRange` of the volume, its state is stored: particle (and ion charge), crossing point, the
  kinetic energy, direction and polarization of the pre-step point and the time interpolated at the crossing
  point (exact for neutral particles; for charged ones the energy loss in the part of the step before the
  surface is neglected). A particle entering with an energy outside `energyRange` is stopped and not stored: its
  contribution is lost. The secondaries created in that step after the crossing point are killed (they are
  part of the replayed history); a step in a kill volume is not biased.
- Stage k + 1 simulates each particle stored in stage k `biasingFactor` (of volume k, an integer >= 1) times,
  each one a new Geant4 event (with different random numbers) with the stored state as its primary. The particles
  of all the threads are collected, and sorted by event ID, before the next stage, so the result is
  reproducible with the same seed and threads.
- The last stage (N) is a normal simulation without stopping. Only its events are written to the output, with
  the usual conditions (`energyRangeStored`, `saveAllEvents`, `nRequestedEntries`...). Their primaries are the
  replayed particles (on the surface of the innermost volume), and their event IDs are the Geant4 event IDs of
  the last stage: event i replays the particle i / `biasingFactor` (of the innermost volume) stored by the
  previous stage.

All the output events have the same weight, 1 / `biasingTotalFactor`, the product of the factors of the volumes:
the output represents `biasingTotalFactor` times the statistics of the primaries simulated in stage 0. The
`TRestGeant4Metadata` of the output stores:

- `nEvents` (`GetNumberOfEvents`): the events of the generator simulated in stage 0.
- `biasingTotalFactor` (`GetBiasingTotalFactor`, `GetBiasingEventWeight` = 1 / factor).
- `biasingStageEvents`: the events simulated in each stage (`GetBiasingStageEvents(k)`, N + 1 values).
- `biasingStageParticles`: the particles stored in stage k entering volume k (`GetBiasingStageParticles(k)`).
- `GetEquivalentNumberOfEvents()` = `nEvents` x `biasingTotalFactor` x `GetBiasingCompletedFraction()`: the
  primaries the output represents, to normalize (e.g. a rate = counts / equivalent events x primaries per second
  of the source). The completed fraction is 1 when all the stages were completed, the fraction of the last stage
  simulated when it was stopped early (`-e`, `--time`), and 0 if the simulation was stopped before the last stage.
  With the `cry` generator, `GetEquivalentCRYExposureTime()` is the exposure the output represents.

Without `biasingVolumes` restG4 runs as before (a single stage), the equivalent number of events is `nEvents` and
the weight 1. The time limit (`--time`) is for the whole simulation; if it is reached (or CTRL-C) before the last
stage, or a stage stores no particle, the simulation ends with a message and the output has no events.

```yaml
  geant4Metadata:
    class: "TRestGeant4Metadata"
    nEvents: 100000
    biasingVolumes:                # from the outermost to the innermost
      - type: "virtualSphere"
        position: [0, 0, 0]
        size: 25 cm                # radius
        biasingFactor: 10
        energyRange: [1 keV, 2 MeV]
      - type: "virtualBox"
        position: [0, 0, 0]
        size: 12 cm                # side
        biasingFactor: 10
        energyRange: [1 keV, 2 MeV]
```

The full example is `examples/09.Pb210_Shield/Biasing.yaml` (gammas from a wall towards the lead box, two
stages of factor 10: weight 1 / 100).

## restCRY

`restCRY config.yaml [--set name=value ...]` (the options and the file in any order) generates cosmic-ray
showers with CRY from the section of class `TRestGeant4CRYShowers` of the configuration (altitude, latitude,
`subBoxLength`, `nShowers` and/or `exposureTime`) and writes them to its `outputFileName`, relative to the
configuration as the `filename` of the `cry` generator (both configurations side by side, launched from any
directory). The `cry` generator of restG4 simulates the showers of that file, one per event, and stores the
exposure time simulated in the `TRestGeant4Metadata`, to normalize to a rate. See
`examples/12.Generators/CRYShowers.yaml` and `CRYSimulation.yaml`.

## Examples and tests

`examples/` has a directory per example (`01.NLDBD` to `14.DetectorResponse`) with the configuration files and
the geometry; run each one from its directory (e.g. `cd 08.Alphas; restG4 alphas.yaml`), see
`examples/README.md`. They are installed in `examples/restG4`. With `BUILD_TESTING` the examples are tests
(`test/`, ctest label `restG4`): each test runs restG4 (and restManager) on a copy of `examples/` with few events
and checks the output.

## Structure of the output file

The output file is a REST run file (read with `TRestRun`, restManager or the REST browser):

- `TRestGeant4Event`: the tree of the events. Each entry is an event (or a sub-event of a radioactive decay chain,
  see `subEventTimeDelay`) with the primaries, the energy deposited in the sensitive volumes and in each volume,
  and the tracks with their hits (removed if `storeTracks` is false). Only the events with energy in the
  sensitive volumes inside `energyRangeStored` are stored, unless `saveAllEvents`.
- `AnalysisTree`: the analysis tree of the run, an entry per event.
- The metadata: the `TRestRun` (with its start and end times), the `TRestGeant4Metadata` (with the number of
  events processed, the simulation wall time, the Geant4 version and the geometry information) and the
  `TRestGeant4PhysicsLists`.
- `Geometry`: the geometry (`TGeoManager`), with its hash, so the geometry of the runs can be compared without
  reading it.

At the end, restG4 checks that the event, the metadata, the geometry and the analysis tree can be read from the
file.
