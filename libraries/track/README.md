# The track library

This library defines a track event type, groups of hits clustered by a distance relation, and the
processes that reduce, order and analyse the tracks to identify and reconstruct the physical tracks
(topological observables: track length, blobs at the ends, straight-line fits).

The track library (`RestTrack`) holds the track event of rest-for-physics-toolkit and the processes that
reduce, order and analyse its tracks. A track is a group of hits of one type (e.g. XZ and YZ for strip
readouts, XYZ for 3D readouts); the processes work with any type, and the observables of strip readouts are
given by `projections` (hit types) instead of fixed X and Y tracks.

The tracks are built from a `TRestDetectorHitsEvent` by `TRestDetectorHitsToTrackProcess`, in the
[connectors library](../connectors/README.md).

## Event

| Class | Description |
| --- | --- |
| `TRestTrackEvent` | Event with the tracks of one processing stage: hits (position, energy, time, type and sigma) stored back-to-back, with track ID, parent ID and number of hits per track. |
| `TRestTrack` | Lightweight view of one track of a `TRestTrackEvent` (hits, energy, length, volume, ends). |

Each process writes its tracks to a new event (the output event holds only the tracks of the last stage);
the parent ID of a track is the ID of the track it comes from in the input event.

The getters of `TRestTrackEvent` with a projection option (`GetNumberOfTracks`, `GetEnergy`,
`GetMaxEnergyTrack`, `GetSecondMaxEnergyTrack`, `GetMaxEnergyTrackVolume`, `GetMaxEnergyTrackLength`) select
only the XZ tracks with `"X"`, only the YZ tracks with `"Y"` and only the XYZ (3D) tracks with `"XYZ"`. The
empty option `""` is the XYZ tracks in `GetMaxEnergyTrack` and the methods based on it, and all the tracks in
`GetNumberOfTracks` and `GetEnergy` (as without option).

## Processes

Track reconstruction (`TRestTrackEvent` to `TRestTrackEvent`, one output track per input track):

| Class | Description |
| --- | --- |
| `TRestTrackReductionProcess` | Reduces the hits of each track to nodes, merging the hits closer than a growing distance up to `maxNodes`, optionally refined with k-means. |
| `TRestTrackPathMinimizationProcess` | Orders the nodes of each track along the shortest path through them; observable `Branches` (jumps in the path). |
| `TRestTrackLinearizationProcess` | Reduces each track to nodes along the line that best fits its hits, for linear tracks such as alpha tracks. |

Track analysis (observables in the analysis tree):

| Class | Description |
| --- | --- |
| `TRestTrackAnalysisProcess` | Number of tracks, energies, positions, sizes, lengths and twists of the main track and the other tracks, per projection and balances between projections; optional cut on the number of tracks. |
| `TRestTrackBlobAnalysisProcess` | Positions, distance and energies in spheres of the two ends (blobs) of the main track, after reduction and path minimization. |
| `TRestTrackLineAnalysisProcess` | Origin, end, length, direction, energy asymmetry and residual of linear tracks; the output event keeps only the main tracks. |

The configuration keys, the observables and a YAML example of each process are given in its class
documentation.

## Macros

| Macro | Description |
| --- | --- |
| `REST_Track_ViewTrackLine(fileName, prefix = "")` | Event browser of a file analysed with `TRestTrackLineAnalysisProcess`: track and hits events with the origin and end of the line drawn over them, raw and detector signals split in X and Y strips. |

```
restRoot
[0] REST_Track_ViewTrackLine("R01234_LINEAR.root", "trackLineAnalysis")
```

## Examples

The examples are in `examples/`; the paths are relative to the repository root.

| File | Description |
| --- | --- |
| `TrackLinearization.yaml` | Linearization of the tracks, track observables and line analysis of a file with `TRestTrackEvent` (e.g. the output of `libraries/connectors/examples/HitsToTrack.yaml`). |
| `TrackLinePlots.yaml` | Plots (`TRestAnalysisPlot`) with cuts (`TRestCut`) of the line analysis of one or several files of `TrackLinearization.yaml`. |
| `TrackLineDataSet.yaml` | Dataset (`TRestDataSet`) of the line observables of several runs, generated with the macro `REST_GenerateDataSet`. |
| `TrackLineDataSetPlots.yaml` | Plots of the dataset of `TrackLineDataSet.yaml`. |

```
restManager --c libraries/track/examples/TrackLinearization.yaml --i <file with TRestTrackEvent>
restManager --c libraries/track/examples/TrackLinePlots.yaml --i "R*_LINEAR.root"

restRoot
[0] REST_GenerateDataSet("libraries/track/examples/TrackLineDataSet.yaml", "dataSet")

restManager --c libraries/track/examples/TrackLineDataSetPlots.yaml --i Dataset_TrackLine.root
```
