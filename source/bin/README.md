# REST executables (source/bin)

Each `.cxx` file in this directory is built as an executable of the same name, linked against RestCore and
every REST library built, and installed in `$REST_PATH/bin`.

## restManager

Runs a YAML configuration with a `TRestManager` section: event processing, dataset generation, dataset
processing and analysis plots.

```
restManager --c|--config <config.yaml> [--i|--input <input.root>] [--o|--output <output.root>]
            [--v|--verbose <level>] [--e|--events <n>] [--j|--threads <n>]
            [--r|--reference <process>=<dataset>] [--set <name>=<value>]
            [--maxRepeated <n>] [--repeatedInterval <s>]
```

| Option | Description |
|---|---|
| `--c`, `--config` | Configuration file (required). |
| `--i`, `--input` | Input: `inputFileName` of the TRestRun; with a TRestDataSet its `filePattern`; with a TRestDataSetProcessManager and no TRestDataSet its `inputFileName`; with no run nor dataset, the `inputFileName` of every TRestAnalysisPlot. |
| `--o`, `--output` | Output: `outputFileName` of the TRestRun, and of the TRestDataSetProcessManager if any, otherwise of the TRestDataSet. |
| `--v`, `--verbose` | Global verbose level. |
| `--e`, `--events` | `eventsToProcess` of the TRestEventProcessManager. |
| `--j`, `--threads` | `nThreads` of the TRestEventProcessManager. |
| `--r`, `--reference` | `referenceFileName` of a process of the TRestDataSetProcessManager (repeatable), e.g. `--r calib=D_00120_00122_Calibration.root`. |
| `--set` | Sets a parameter of the `parameters` section for this execution (repeatable; the file is not changed), e.g. `--set meshVoltage=300V`. A parameter not used is an error. |
| `--maxRepeated` | Similar messages (warning and info) from the same code line within the time window after which the next ones are suppressed (default 10, 0: no suppression; never with verbose debug). |
| `--repeatedInterval` | Time window of `--maxRepeated` in seconds (default 10, 0: a single window for the whole execution). |

SIGINT, SIGTERM, SIGHUP and SIGQUIT stop the processing cleanly. The exit code is 0 on success and 1 on error.

## restRoot

ROOT prompt with REST loaded.

```
restRoot [ROOT options] [file.root] [macro.C ...]
```

- Requires `REST_PATH`. Loads `$REST_PATH/lib/libRest*.so` (except `libRestG4.so`), adds
  `$REST_PATH/include` to the include path and loads the compiled macros of `$REST_PATH/macros`
  (see [source/macros](../macros/README.md)).
- A REST run file or dataset among the arguments is opened with `REST_OpenInputFile` (globals `_run`,
  `_ev` or `_dataSet`); if several are given only the last one is opened (with a warning). The other
  arguments are passed to ROOT (TRint).

Other executables are built by the packages (e.g. `restG4`, `restGarfield`, `restDAQ`).
