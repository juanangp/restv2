# The geant4 library

This library is used to store and analyse the events generated in a Geant4 simulation. It defines and
stores the particle generator and the simulation conditions, such as the geometry, the active volumes and
the physics list used during the Monte Carlo, and implements processes on the simulated events (energy
quenching, observables of volumes, particles and interactions).

The geant4 library (`RestGeant4`) holds the event and the metadata of the Geant4 simulations of restv2:
the event written by [restG4](../../packages/restG4/README.md) (primaries, energy per volume, tracks
and hits), the simulation conditions (geometry, physics lists, active volumes, primary generator) and the
processes that analyse the simulated events. It does not depend on Geant4: the parts that need it (filling
the event from Geant4 steps, the volume tables of the Geant4 world, the positions in GDML volumes) are
implemented in restG4. The conversion of the simulated hits into detector hits is done by the
[connectors library](../connectors/README.md).

Units: energies in keV, lengths in mm, times in us and angles in rad; any YAML value can be given with
units (e.g. `5.3 MeV`, `2 cm`, `30 deg`).

## Events

| Class | Description |
| --- | --- |
| `TRestGeant4Event` | Event of restG4: primaries, energy deposited in total, in the sensitive volumes, per volume and per volume, particle and process, and (with `storeTracks`) the tracks and their hits. Particles, processes and volumes are stored as IDs, named by the `TRestGeant4Metadata` of the run. |
| `TRestGeant4EventData` | Flat storage of a `TRestGeant4Event` (one TTree branch per vector). |
| `TRestGeant4Track` | Light view of one track of a `TRestGeant4Event`: IDs, parent and children, creator process, energies, hits per volume, processes. |
| `TRestGeant4EventViewer` | 3D TEve viewer of the tracks and hits of a `TRestGeant4Event`, used by `TRestBrowser`. |

## Simulation metadata

| Class | Description |
| --- | --- |
| `TRestGeant4Manager` | Top section of a restG4 configuration: creates the run, the `TRestGeant4Metadata` and the `TRestGeant4PhysicsLists`. |
| `TRestGeant4Metadata` | Simulation conditions (geometry, number of events, seed, stored energy range, active volumes with their storage `chance`, `activateAllVolumes`, sensitive and kill volumes, `registerEmptyTracks`, decay chain options, biasing volumes, generator) and the ID tables and biasing results (`biasingTotalFactor`, events and particles per stage) written by restG4; it keeps the geometry with the file. Normalize with `GetEquivalentNumberOfEvents` (`nEvents` without biasing). |
| `TRestGeant4ActiveVolumeMetadata` | An entry of `activeVolumes`: volume expression, maximum step and sensitive flag. |
| `TRestGeant4BiasingVolume` | An entry of `biasingVolumes`: a virtual box or sphere (not part of the geometry), an integer factor and an energy range. restG4 simulates in stages: the particles entering each volume (outermost first) are stopped and stored, and replayed `biasingFactor` times by the next stage; only the last stage is written, each event with weight 1 / `biasingTotalFactor` (see the restG4 README). |
| `TRestGeant4GeometryInfo` | Geometry of the simulation (TGeoManager), volume ID table and volume lookup by regular expression. |
| `TRestGeant4PhysicsInfo` | Particle and process ID tables (PDG encodings, Geant4 process type * 1000 + subtype). |
| `TRestGeant4CRYShowers` | Configuration and results of a CRY cosmic-ray shower simulation run by restCRY; the showers file is read by the `cry` generator. |

## Physics lists

| Class | Description |
| --- | --- |
| `TRestGeant4PhysicsLists` | Geant4 physics lists (with their options), production cuts and ions with step limit. A physics list name restG4 does not support is a configuration error (the error lists the valid ones). |
| `TRestPhysicsListItem` | An entry of `physicsList`: name and options. |
| `TRestPhysicsListOption` | An option (`name`, `value`) of a physics list. |

## Primary generator

The `generator` section of `TRestGeant4Metadata`; the options and examples are documented in
`TRestGeant4PrimaryGenerator`.

| Class | Description |
| --- | --- |
| `TRestGeant4PrimaryGeneratorInfo` | The `generator` section: spatial generator, optional spatial density and sources. |
| `TRestGeant4PrimaryGenerator` | Samples the primaries of each event (one per thread, with its own random generator). |
| `TRestGeant4SpatialGenerator` | Base of the generator types, where the primaries start. |
| `TRestGeant4SpatialGeneratorPoint` | `type: point`: a fixed position. |
| `TRestGeant4SpatialGeneratorShape` | Base of the volume and surface generators: a GDML volume or a shape, positioned and rotated. |
| `TRestGeant4SpatialGeneratorVolume` | `type: volume`: uniform in a GDML volume, box, sphere or cylinder. |
| `TRestGeant4SpatialGeneratorSurface` | `type: surface`: on the surface of a GDML volume, sphere, cylinder, circle or wall. |
| `TRestGeant4SpatialGeneratorCosmic` | `type: cosmic`: origin on a sphere around the world from the direction, for a flux. |
| `TRestGeant4SpatialGeneratorCRY` | `type: cry`: the showers of a restCRY file, one per event. |
| `TRestGeant4ParticleSource` | A `source`: particle with energy and angular distributions, or a Decay0 file. |
| `TRestGeant4EnergyDistribution` | Base of the energy types: mono, flat, log, formula, TH1D and the joint formula2, TH2D, cosmic (`...Mono`, `...Flat`, `...Log`, `...Formula`, `...TH1D`, `...Joint`). |
| `TRestGeant4AngularDistribution` | Base of the angular types: flux, isotropic, formula, TH1D and the joint ones (`...Flux`, `...Isotropic`, `...Formula`, `...TH1D`, `...Joint`). |
| `TRestGeant4ParticleState` | A primary particle: name, excitation, charge, energy, direction, origin and time. |
| `TRestGeant4PrimaryGeneratorTypes` | Namespace with the named formulas (CosmicNeutrons, Cos2, CosmicMuons...) and helpers. |

## Processes

`TRestGeant4Event` to `TRestGeant4Event`:

| Class | Description |
| --- | --- |
| `TRestGeant4QuenchingProcess` | Quenches the hit energies only in the volumes given (`volume`, each with its model, lindhard or birks, required; `model`: the same model for all the other volumes of the geometry) and recomputes the event energies. The Lindhard model needs a simulation with `storeHadronicTargetInfo: true` (error otherwise). |
| `TRestGeant4AnalysisProcess` | Observables of the event: global and primary ones and, per selection of volumes, particles and processes, energy, mean position, entering particle, energy per volume and interaction points (e.g. neutron captures). |

The configuration keys, the observables and a YAML example of each process are given in its class
documentation.

## Macros

Loaded by `restRoot`:

| Macro | Description |
| --- | --- |
| `REST_Geant4_ReadEvents(file, first, last, energyMin, energyMax, maxTracks, maxHits)` | Prints the events of a range of entries, optionally only those with sensitive energy in a range. |
| `REST_Geant4_ListIsotopes(file, outputFile, gammaMin, gammaMax, bins)` | Counts the particles and isotopes produced (with their creation volumes) and draws the spectrum of the gammas. |
| `REST_Geant4_ViewEvent(file [, maxVolumeCopies])` | Opens the file in `TRestBrowser` with the 3D event viewer. Logical volumes placed more than `maxVolumeCopies` times (default 1000, e.g. fibres) are not drawn, 0 draws all. |
| `REST_Geant4_ViewGeometry(file, option)` | Draws the geometry of a restG4 file or a GDML file (OpenGL, or TEve with `"eve"`). |
| `REST_Geant4_GenerateResponseMatrix(filePattern, varX, varY, range, cut)` | Response matrix (varY versus varX per simulated event) of analysis files, written as a binary table. |

```
restRoot
REST_Geant4_ReadEvents("Run1_NLDBD_Test.root", 0, 10)
```

## Examples

The examples in `examples/` run on the output of a restG4 simulation (see the
[restG4 examples](../../packages/restG4/examples/README.md)), e.g. `Run00010_CosmicNeutrons_IAXO.root`
of `packages/restG4/examples/13.IAXO`:

| File | Description |
| --- | --- |
| `Quenching.yaml` | `TRestGeant4QuenchingProcess`: Lindhard quenching in the gas and Birks quenching in the scintillators; output `*_QUENCHING.root`. |
| `Geant4Analysis.yaml` | `TRestGeant4AnalysisProcess`: selections of the gas (split by particle, with the entering particle), its Compton and elastic hadronic deposits, the scintillators and the neutrons. |
| `Run00010_CosmicNeutrons_IAXO_QUENCHING.root` | Output of `Quenching.yaml` on the IAXO cosmic neutron simulation, input of `Geant4Analysis.yaml`. |

```
restManager --c libraries/geant4/examples/Quenching.yaml --i Run00010_CosmicNeutrons_IAXO.root
restManager --c libraries/geant4/examples/Geant4Analysis.yaml \
    --i libraries/geant4/examples/Run00010_CosmicNeutrons_IAXO_QUENCHING.root
```
