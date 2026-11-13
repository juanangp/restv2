# The restGarfield package

This package is used to compute with Garfield++ the transport and gain properties of the detector media,
stored in tables that the detector library reads without depending on Garfield++.

Optional application that computes with [Garfield++](https://garfieldpp.web.cern.ch) the transport and gain
tables of a medium defined in a `TRestDetectorTransportManager` YAML configuration, and stores them in the
medium file (`mediumFile` of the `TRestDetectorMedium`, by default `<name>.root`). The REST libraries do not
depend on Garfield++: the tables are read by `TRestDetectorMedium` (detector library).

- **Transport tables** (`TRestDetectorTransportTable`): drift velocity, longitudinal and transversal
  diffusion, Townsend and attachment coefficients versus the electric field, for electrons, holes and ions
  (the carriers available in the Garfield medium). Gases use Magboltz (`MediumMagboltz`, up to 6
  components); solids of a single component `Si`, `GaAs`, `GaN`, `CdTe` or `diamond` use the Garfield solid
  media.
- **Gain tables** (`TRestDetectorGainTable`, gases only): mean gain, Polya theta and the avalanche size
  distributions, from microscopic avalanches of one electron in a uniform field across a gap (parallel
  plates).

Tables already in the medium file are skipped unless `overwrite: true`.

## Build

CMake option `REST_GARFIELD` (default `ON`). It needs Garfield++ (found with `GARFIELD_INSTALL` or
`GARFIELD_HOME`) and the detector library; otherwise the package is skipped with a message.

```
cmake -DREST_GARFIELD=ON ..
```

## Usage

```
restGarfield <config.yaml> [--set name=value ...] [--jobs N]
```

`--set` overrides a parameter of the `parameters` section for this execution (repeatable). `--jobs N`
splits the field nodes of each gas transport table in up to N processes, each computing a contiguous part
of the grid of at least 2 nodes (Magboltz is not thread safe, so processes instead of threads); the table is
the same. Several media can also run at once as separate `restGarfield` processes, even writing to the same
medium file (e.g. `Ar99_iC4H10_1`, `Ar98_iC4H10_2`... in `ArIso.root`, each one in `tables/<medium>/`): the
file is locked while it is read or written. Example based on
`examples/ArIso.yaml` (installed in `examples/restGarfield`; its `gain` section is commented out there):

```yaml
transportManager:
  class: "TRestDetectorTransportManager"
  medium:
    class: "TRestDetectorMedium"
    name: "ArIso"
    state: "gas"
    components: {Ar: 99, iC4H10: 1}
  transport:
    temperatures: [293.15]
    fieldRange: [30 V/cm, 500 V/cm]
    fieldNodes: 30
  gain:
    gap: 50 um
    reducedFieldRange: [50 kV/cm/bar, 70 kV/cm/bar]
```

## Configuration keys

`TRestDetectorTransportManager`: `medium` (a `TRestDetectorMedium`), `transport` and `gain` (each a map or a
list of maps, one per set of tables).

`transport`:

| Key | Default | Description |
|---|---|---|
| `pressure` | 1 bar | Pressure of the tables (gas tables are scaled to other pressures with E/p). |
| `temperatures` | medium temperature | One table per temperature (K). |
| `fieldRange` | [10 V/cm, 1000 V/cm] | Electric field range. |
| `fieldNodes` | 20 | Number of field nodes. |
| `logField` | true | Logarithmic field spacing. |
| `nCollisions` | 10 | Magboltz collisions (x 10^7). |
| `maxElectronEnergy` | 400 eV | Maximum electron energy in Magboltz. |
| `usePenning` | false | Penning transfer in the Townsend coefficient (gases). |
| `penningRate` | 0 | Penning transfer probability, (0, 1]. |
| `overwrite` | false | Compute again the tables already in the file. |

`gain`:

| Key | Default | Description |
|---|---|---|
| `gap` | (required) | Amplification gap. |
| `pressures` | medium pressure | One set of tables per pressure. |
| `temperatures` | medium temperature | One table per temperature (K). |
| `reducedFieldRange` | (required) | Reduced field range E/p; the fields are E = (E/p) * p. |
| `fieldNodes` | 5 | Number of equally spaced field nodes. |
| `avalanches` | 1000 | Avalanches (one primary electron each) per field node. |
| `usePenning` | false | Penning transfer. |
| `penningRate` | 0 | Penning transfer probability. |
| `overwrite` | false | Compute again the tables already in the file. |

See the TRestDetectorTransportManager and TRestDetectorMedium documentation for the validation of these
keys.

## Macros

- `REST_Garfield_DrawMedium(fileName, medium = "", pressure = 0)`: draws the transport tables (versus E,
  one color per table, one line style per carrier; scaled to `pressure` in bar if > 0) and the gain tables
  (mean gain and Polya theta) of a medium file.

```
restRoot
root [0] REST_Garfield_DrawMedium("ArIso.root")
root [1] REST_Garfield_DrawMedium("ArIso.root", "", 1.4)
```
