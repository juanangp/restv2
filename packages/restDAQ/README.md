## restDAQ

Data taking of AFTER/AGET electronics (ARC, FEMINOS, DCC and TCM), based on the `mclient` program of Denis Calvet.
It is FEMDAQ adapted to restv2 (same executable, `femclient`): it writes REST run files.

### Build

Optional package: `cmake -DREST_DAQ=ON ...`. It needs readline (`libreadline-dev`) and prometheus-cpp, found with
`find_package` (e.g. `-DCMAKE_PREFIX_PATH=<prometheus-cpp install>`), otherwise downloaded with FetchContent.

### Usage

```bash
femclient restDAQ.yaml                  # interactive console (history in `$REST_HOME/.rest/femclient_history`, or `~/.rest` without REST_HOME)
femclient restDAQ.yaml -e commands.txt  # commands of a file
femclient restDAQ.yaml --tcm            # commands to the TCM
femclient restDAQ.yaml --read-only      # fopen, fclose and addMetadata do nothing
femclient restDAQ.yaml --set name=value # parameters of the configuration
```

The commands are those of `mclient` (sent to the active FEMs) plus:

* `fopen [all|root|log]`: opens a new run (the REST file and/or a log file per FEM). Issue `addMetadata` before.
* `fclose`: closes the files.
* `addMetadata`: asks for the runTag, the runType and each `runInfo` key of the TRestRun, and writes them in the
  configuration file.
* `startDAQ`: acquisition loop, until `nEvents` or `time` of the TRestRawDAQMetadata (or CTRL-C); then the event
  builder writes the data left and stops, so the files can be closed. DCC: flags `internal` (trigger) and `ZS`
  (zero suppression).
* `stopDAQ`: stops the event builder (nothing if `startDAQ` already stopped it; kept for the scripts).
* `Pedestals [nTriggers=100] [mean=250] [stdDev=5.0]`: pedestal loop (DCC).
* `credits show`, `credits restore [frameThr reqFrames]`: request credits (ARC/FEMINOS).
* `fem <id>|*`: active FEMs. `verbose silent|error|warning|info|debug` (levels of restv2). `exec <file>`. `sleep <s>`. `quit`.

### Configuration

See `examples/restDAQ.yaml`: a TRestRawDAQMetadata (electronics, FEMs, limits of the run, Prometheus address) and a
TRestRun (`runNumber: auto`, a new number in each `fopen`; tags; `runInfo`; `mainDataPath`; `outputFileName`).
When a file reaches `maxFileSize` the next subrun is opened.

### Output

REST run files with the events in the tree `TRestRawSignalEvent`, written as TRestRawDAQSignalEvent: the signals
plus, for each FEM, its id, event counter, timestamp (10 ns ticks) and number of signals (branches `fFEMIDs`,
`fFEMEventCounts`, `fFEMTimestamps`, `fFEMNSignals`). They are read as plain TRestRawSignalEvent by restManager, the
browser and the macros. The TRestRun and the TRestRawDAQMetadata are stored as metadata.

The files are saved every 1000 events (or 60 s), so they can be followed while they are taken with the event
display of the connectors library: `restRoot` and `REST_DAQViewer("analysis.yaml", "file.root")` (TRestDAQViewer), with
the readouts and the pulse parameters of a restv2 configuration (template:
`libraries/connectors/examples/DAQViewer.yaml`). The signals of each event, a pad per ASIC:
`REST_Raw_PulsesViewer("file.root")` (TRestRawPulsesViewer, raw library).

### Monitoring

A Prometheus server (`prometheusAddress`, default `0.0.0.0:8080`) gives the run number, the rate, the events and the
events with the FEMs out of sync.
