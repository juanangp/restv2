# REST tools (source/tools)

`libRestTools` is the lowest layer of REST: generic helpers with no dependency on the REST classes
(only ROOT and yaml-cpp). Every other library (`RestCore`, `RestAnalysis`, `libraries/*`, `packages/*`)
links it. Its contents are free functions grouped in namespaces, plus the logging classes.

## Contents

### Generic utilities

- `TRestTools` (TRestTools.h): strings (Split, Trim, ReplaceAll, Format, NormalizeOption, ReplaceDelimited,
  ReplaceIdentifiers...), files (GetFilesMatchingPattern, SearchFileInPath, DownloadFile, GetRESTUserDirectory,
  GetRunNumberAuto, stored in `<GetRESTUserDirectory()>/runNumber`), REST/ROOT files (isRootFile,
  isValidTRestRun, isDataSet, CopyDirectory), times (GetCurrentTime, StringToTimeStamp, ToTimeStringLong) and
  geometries stored in the files (WriteGeometry, ReadGeometry, ReadGeometryCopy, DeleteGeometry, geometry
  hashes).
- `TRestConstants` (TRestConstants.h): mathematical and physical constants (NaN, c, e, k_B, hBar, unit
  conversions to natural units...).

### Configuration

- `TRestYAMLUtils` (TRestYAMLUtils.h): opening a YAML configuration with its `include` keys expanded (a
  section taken from another file, its own keys overriding it; LoadConfigFile) and its references resolved
  (`${ENV:VAR}`, `${PARAM:name}`, `${a.b}`), the `parameters` section and its `--set` overrides, access to
  the parameters of a node (GetParam, GetList, ReadParamOrDefault), editing YAML files in place
  (SetValuesInFile) and reading the metadata stored in a REST file (ReadMetadataStore).
- `REST_Units` and `TRestWithUnits` (TRestSystemOfUnits.h): the system of units (keV, us, mm, kg, V, T, bar,
  rad), parsing of values with units ("300 V", "1 kV/cm") and formatting with SI prefixes. A configuration
  field declared as `TRestWithUnits` accepts units in the YAML.
- `TRestGDMLParser` (TRestGDMLParser.h): GDML geometries with their entities expanded and the configuration
  parameters applied to their constants and entities; `--update-gdml` of restG4 writes the parameters
  back into the GDML files.

### Logging

- `TRestLogManager` (TRestLogManager.h): the singleton writing the messages to the console and an optional
  log file, thread safe, with verbose levels (`error`, `silent`, `warning`, `info`, `debug`; `silent` writes
  the same as `error`: the errors are always written) and suppression of repeated messages.
- `RESTError`, `RESTWarning`, `RESTInfo`, `RESTDebug`, `RESTMetadata`, `RESTLog` and `RESTendl`: the macros
  used everywhere to write messages (`RESTInfo << "text" << RESTendl;`). The verbose level is the one of the
  metadata writing the message (its `verbose` field) or the global one.
- `TRestLogScope` / `TRestLogContext`: verbose level and name of the messages written outside a metadata in
  a thread (e.g. by the event of the process running).
- `TRestProgressBar` (global `RESTProgress`): console progress bar of the executables.

## Use in the framework

- `TRestMetadata` (source/core) reads its configuration with `TRestYAMLUtils` and `TRestWithUnits`, and
  writes its messages with the logging macros.
- `restManager`, `restG4` and the other executables open the configuration with
  `TRestYAMLUtils::OpenConfigFile`, apply `--set` with `AddParameterOverride` and check the parameters with
  `CheckParametersUsed`.
- restG4, `TRestGeant4GeometryInfo`, `TRestGDMLReadout` and the macro `REST_Geant4_ViewGeometry` read the
  GDML through `TRestGDMLParser`.
- `TRestGeant4Metadata`, `TRestDetectorReadout` and `TRestDataSet` store and read the geometry with
  `TRestTools::WriteGeometry` and `ReadGeometry`.

There are no examples or macros in this directory; the configuration features (parameters, units,
references) are used by the YAML files of `examples/`, `libraries/*/examples` and `packages/*/examples`.
