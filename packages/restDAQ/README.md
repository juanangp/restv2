# The restDAQ package

This package is used to take data with AFTER/AGET electronics and write it as REST run files, ready to be
processed with the raw library.

It supports ARC, FEMINOS, DCC and TCM, and is based on the `mclient` program of Denis Calvet: it is FEMDAQ
adapted to restv2 (same executable, `femclient`).

## Build

Optional package: `cmake -DREST_DAQ=ON ...`. It needs readline (`libreadline-dev`) and prometheus-cpp, found with
`find_package` (e.g. `-DCMAKE_PREFIX_PATH=<prometheus-cpp install>`), otherwise downloaded with FetchContent.

## Usage

```bash
femclient restDAQ.yaml                  # interactive console (history in `$REST_HOME/.rest/femclient_history`, or `~/.rest` without REST_HOME)
femclient restDAQ.yaml -e commands.txt  # commands of a file
femclient restDAQ.yaml --tcm            # commands to the TCM
femclient restDAQ.yaml --read-only      # fopen, fclose and addMetadata do nothing
femclient restDAQ.yaml --set name=value # parameters of the configuration
```

The configuration file can also be given with `-c`/`--config-file`. A command file (`-e`, `exec`) has one
command per line, as typed in the console; empty lines and lines starting with `#` are skipped (see
`examples/commands.txt`). A failed command (wrong arguments, FEM timeout...) writes its error and the session
continues. CTRL-C aborts the run in progress and ends the interactive console; the files are closed at exit.

The commands are those of `mclient` (sent to the active FEMs) plus:

* `fopen [all|root|log]`: opens a new run (the REST file and/or a log file per FEM, `<file>_FEM<id>.log`). Issue
  `addMetadata` before. The data path (`mainDataPath`) must exist.
* `fclose`: closes the files.
* `addMetadata`: asks for the runTag, the runType and each `runInfo` key of the TRestRun, and writes them in the
  configuration file.
* `startDAQ`: acquisition loop, until `nEvents` or `time` of the TRestRawDAQMetadata (or CTRL-C); then the event
  builder writes the data left and stops, so the files can be closed. DCC: flags `internal` (trigger) and `ZS`
  (zero suppression).
* `stopDAQ`: stops the event builder (nothing if `startDAQ` already stopped it; kept for the scripts).
* `Pedestals [nTriggers=100] [mean=250] [stdDev=5.0]`: pedestal loop (DCC).
* `credits show`, `credits restore [frameThr reqFrames]`: request credits (ARC/FEMINOS).
* `fem <id>|*`: active FEMs (`fem <id>` leaves only that FEM active). `verbose silent|error|warning|info|debug`
  (levels of restv2). `exec <file>`. `sleep <s>`. `quit` or `exit`.
* DCC: `asic <id> gain 0x<g> time 0x<t>` is translated into the write of the register of the ASIC.

## Configuration

See `examples/restDAQ.yaml`: a TRestRawDAQMetadata (electronics, FEMs, limits of the run, Prometheus address) and a
TRestRun (`runNumber: auto`, a new number in each `fopen`; tags; `runInfo`; `mainDataPath`; `outputFileName`).
When a file reaches `maxFileSize` the next subrun is opened.

## Output

REST run files with the events in the tree `TRestRawSignalEvent`, written as TRestRawDAQSignalEvent: the signals
plus, for each FEM, its id, event counter, timestamp (10 ns ticks) and number of signals (branches `fFEMIDs`,
`fFEMEventCounts`, `fFEMTimestamps`, `fFEMNSignals`). They are read as plain TRestRawSignalEvent by restManager, the
browser and the macros. The TRestRun and the TRestRawDAQMetadata are stored as metadata.

The files are saved every 1000 events (or 60 s), so they can be followed while they are taken with the event
display of the connectors library: `restRoot` and `REST_DAQViewer("analysis.yaml", "file.root")` (TRestDAQViewer), with
the readouts and the pulse parameters of a restv2 configuration (template:
`libraries/connectors/examples/DAQViewer.yaml`). The signals of each event, a pad per ASIC:
`REST_Raw_PulsesViewer("file.root")` (TRestRawPulsesViewer, raw library).

## Architecture

* `CommandFetcher` reads the commands (readline console or command file) and runs them on a `FEMDAQ`, created for
  the `electronics` of the configuration (`RunConfig`: the TRestRun and the TRestRawDAQMetadata of the YAML file):
  `FEMDAQARCFEM` (ARC, FEMINOS), `FEMDAQDCC` (DCC) or `TCM`.
* Each FEM is a `FEMProxy` (its UDP socket, `FEMSocket`, port 1122, plus the buffers and counters shared by the
  threads), kept in a `std::deque` so the threads can hold references to them.
* ARC/FEMINOS: a receiver thread per FEM, running all the session, puts the data frames in the buffer of the FEM
  and counts the command replies. During `startDAQ` a request thread per active FEM sends the `daq` requests
  (credits: `reqFrames` frames first, then a request of `frameThr` frames every `frameThr` frames received); for
  FEMINOS the sequence numbers of the frames give the frames lost.
* DCC: no receiver thread; the acquisition loop of `startDAQ` triggers and reads each ASIC (`areq`) and appends an
  end-of-event mark with a timestamp of the computer.
* The event builder thread extracts the next event of each active FEM from its buffer (`ARCPacket`,
  `FEMINOSPacket` or `DCCPacket`), joins them in one event (the signals of each FEM, with its event counter and
  timestamp) and writes the events with signals; every 100 events it checks the size of the file.
* Sync check between FEMs (ARC/FEMINOS): the timestamps of the events of the FEMs must agree within 1 us. The event
  ID and the time are the ones of the majority of the FEMs (without majority the ID is the number of the event).
  The changes of state (`[SYNC] ERROR FEMs out of sync`, `FEMs back in sync`) are written in stdout and in the logs
  of the FEMs, a warning is written when some FEMs have their event and others send nothing for 10 s, and a summary
  (events in sync, frames lost per FEM) at the end of the run. The events out of sync are counted in Prometheus.
* The update thread writes the number of events and the rate every `updateRate`, updates Prometheus and ends the
  run at `nEvents` or `time`. At the end of the run the event builder writes the data left and stops.

## Monitoring

A Prometheus server (`prometheusAddress`, default `0.0.0.0:8080`, empty for none; not with `--tcm`) gives the run
number (`run_number`), the rate (`event_rate`), the events (`events_total`) and the events with the FEMs out of
sync (`desync_events_total`), plus the file name (`run_filename_info`, label `filename`) and the YAML dump of the
configuration (`run_metadata_info`, label `metadata`) of the run as labels.
