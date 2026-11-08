# REST

REST (Rare Event Searches Toolkit) is a C++/ROOT framework to simulate, process and analyse the data of rare event
experiments. The events of a run are processed by chains of processes configured in YAML files (`restManager`), and
the results are stored in REST files (ROOT files with the events, the analysis tree and the metadata).

## Framework (`source`)

- **tools**: generic functions (strings, files, YAML configuration, units, logging).
- **core**: events, metadata, runs, processes and their managers, analysis tree, datasets, browser.
- **analysis**: pulse shape analysis and other generic analysis functions.
- **macros** and **bin**: the macros of restRoot and the executables (restManager, restRoot...).

## Libraries (`libraries`)

- **raw**: raw signal events (DAQ data), their processes and viewers.
- **detector**: hits and detector signals, readouts, medium, response and their processes.
- **geant4**: Geant4 simulation events and metadata, and their processes.
- **track**: track events and their processes.
- **connectors**: what needs several libraries: processes converting events between libraries, the DAQ viewer.

## Packages (`packages`)

- **restG4**: Geant4 simulation.
- **restGarfield**: Garfield++ transport and gain tables of the detector media.
- **restDAQ**: data taking of AFTER/AGET electronics (femclient).

Each directory has a README.md with its contents (see the related pages).
