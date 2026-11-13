# Migration status of restv2

Source of truth for the migration of the legacy REST framework (`/home/juanan/framework`) to restv2: which
legacy classes are migrated or not, what is still pending, and the log of the design decisions taken along
the way. It replaces `MIGRATION_PENDING.md` and `IMPROVEMENT_IDEAS.md`.

How to use it:

- Before picking new migration work, read the [Checklist](#checklist) and the [Pending work](#pending-work).
- When a decision is taken, add it to the [Decision log](#decision-log) as a new dated section (or a dated
  item in an existing one) and update the checklist row and the pending list.
- Annotations: "implemented" means the decision is in the code; "user" means the decision was taken by the
  maintainer (not proposed by the assistant); "P" in the pending list means priority.

## Checklist

Comparison of `/home/juanan/framework/source` with restv2 (2026-10-31), excluding `axion` and `wimp`.

Legend (an `x` in the column):

- **M**: migrated or to be migrated.
- **N**: not migrated.
- Neither M nor N: still to be decided (no row is in this state at the moment).
- **P**: priority.

"(migrated: X)" gives the restv2 class that covers the legacy one; "(migrated, same name)" keeps the name; a
note in parentheses in an N row says where its functionality went.

### framework/analysis

| M | N | P | Class | What it does |
|---|---|---|-------|--------------|
|   | x |   | TRestEventRateAnalysisProcess | Event time and rate observables (time since the previous event, mean rate, run time). |
|   | x |   | TRestEventTimeSelectionProcess | Processes only the events inside time intervals read from a file. |
|   | x |   | TRestSummaryProcess | Stores a run summary as metadata: entries, duration, mean rate, mean/sigma of observables. |
|   | x |   | TRestDataQualityProcess | Tags the run with quality bits from rules on metadata and observables. |
|   | x |   | TRestDataQualityRules | Rules (range of an observable or metadata) used by TRestDataQualityProcess. |
|   | x |   | TRestBenchMarkProcess | Monitors performance (events/s, read MB/s, CPU, memory) while processing. |
| x |   | x | TRestDataSetCalibration (migrated, same name) | Energy calibration of a TRestDataSet fitting known peaks; adds the calibrated column. |
| x |   | x | TRestDataSetGainMap (migrated, same name) | Computes and applies gain maps per module and spatial segment from a calibration dataset. |
| x |   | x | TRestDataSetOdds (migrated: TRestDataSetLogOdds) | Log-odds of observables from PDFs obtained from a reference dataset. |
| x |   | x | TRestDataSetTMVA and TRestDataSetTMVAClassification (framework PR #448, never merged; migrated: TRestDataSetTMVA) | Trains a TMVA classifier (BDT) with signal and background datasets and adds its score. |
|   | x |   | TRestRealTimeAddInputFileProcess | Listens (through TRestMessenger) for new files and adds them to the run while processing. |
|   | x |   | TRestRealTimeDrawingProcess | Draws and saves analysis plots periodically while processing. |
|   | x |   | TRestMySQLToAnalysisProcess | Reads fields of an SQL database and adds them as observables by event time. |

### framework/core and framework/tools

| M | N | P | Class | What it does |
|---|---|---|-------|--------------|
|   | x |   | TRestMesh | Uniform mesh filled with TRestHits storing only occupied nodes (fast clustering, gain maps). |
|   | x |   | TRestMetadataPlot | TGraph of two metadata members over a set of files (evolution per run). |
|   | x |   | TRestMessenger | Sends and receives messages between restManager processes through shared memory. |
| x |   |   | TRestDataBase (postponed: needs a real database; `runNumber: auto` already covers what legacy did) | Interface to a run database (run number, files, metadata). |
|   | x |   | TRestComplex | High-precision complex numbers (used by the axion optics). |

### framework/masks

| M | N | P | Class | What it does |
|---|---|---|-------|--------------|
|   | x |   | TRestPatternMask | Abstract base class of masks: returns the region (id) of a point (x, y). |
|   | x |   | TRestGridMask | Square grid mask with period and thickness. |
|   | x |   | TRestStrippedMask | Parallel strips mask with period and thickness. |
|   | x |   | TRestRadialStrippedMask | Radial strips mask with angular period. |
|   | x |   | TRestRingsMask | Concentric rings mask between given radii. |
|   | x |   | TRestSpiderMask | Radial arms mask (spider-like support) with fixed angular separation. |
|   | x |   | TRestCombinedMask | Combines several masks into one. |

### framework/sensitivity

| M | N | P | Class | What it does |
|---|---|---|-------|--------------|
|   | x |   | TRestComponent | Base class of a component (background or signal) as a density in several variables. |
|   | x |   | TRestComponentDataSet | Component built by histogramming a TRestDataSet. |
|   | x |   | TRestComponentFormula | Component defined by an analytic formula of the variables. |
|   | x |   | TRestResponse | Detector response matrix to transform a component. |
|   | x |   | TRestExperiment | An experiment: background, signal, exposure and data (simulated or real). |
|   | x |   | TRestExperimentList | List of combined experiments. |
|   | x |   | TRestSensitivity | Sensitivity (exclusion curve) from a list of experiments. |

### libraries/raw

| M | N | P | Class | What it does |
|---|---|---|-------|--------------|
| x |   |   | TRestRawSignalAnalysisProcess (migrated, same name) | Computes observables of raw signals. |
| x |   |   | TRestRawSignalAddNoiseProcess (migrated, same name) | Adds white and sinusoidal noise to raw signals. |
| x |   |   | TRestRawBaseLineCorrectionProcess (migrated, same name) | Corrects the signal baseline with a moving average. |
| x |   |   | TRestRawCommonNoiseReductionProcess (migrated, same name) | Subtracts common-mode noise per block of channels, bin by bin. |
| x |   |   | TRestRawSignalShapingProcess (migrated, same name) | Convolves signals with a shaping kernel to emulate electronics. |
| x |   |   | TRestRawSignalRecoverSaturationProcess (migrated, same name) | Recovers ADC-saturated pulses by fitting the AGET response. |
| x |   |   | TRestRawZeroSupressionToRawProcess (migrated, same name) | Removes the offset of zero-suppressed signals. |
| x |   |   | TRestRawSignalRemoveChannelsProcess (migrated: TRestRawSignalChannelSelectionProcess) | Keeps or removes channels by ID or range. |
|   | x |   | TRestRawSignalFittingProcess | Fits each signal to the AGET response (logistic function): amplitude, time, chi2. |
|   | x |   | TRestRawSignalConvolutionFittingProcess | Same, with the AGET response convolved with a gaussian. |
|   | x |   | TRestRawSignalGeneralFitProcess | Fits each signal to a user-defined function from the configuration. |
|   | x |   | TRestRawFindResponseSignalProcess | Extracts the dominant normalized signal to use it as system response. |
|   | x |   | TRestRawSignalChannelActivityProcess | Channel activity histograms (how often each channel is over threshold). |
|   | x |   | TRestRawSignalIdTaggingProcess | Tags the event by DAQ id ranges (detector, module or region). |
|   | x |   | TRestRawSignalRangeReductionProcess | Reduces signal resolution and range to emulate an ADC with fewer bits. |
|   | x |   | TRestRawVetoAnalysisProcess (`keepChannelRange` of TRestRawSignalChannelSelectionProcess + TRestRawSignalAnalysisProcess, `libraries/raw/examples/VetoAnalysis.yaml`) | Observables of the grouped veto channels (peak time and maximum amplitude). |
|   | x |   | TRestRawBiPoAnalysisProcess | BiPo-specific observables (time distance between the two signals). |
|   | x |   | TRestRawReadoutMetadata | Stores the DAQ id → readout channel/type map to use it without the full readout. |
|   | x |   | TRestRawDAQMetadata | Acquisition parameters (electronics, gains, shaping); probably obsolete. The name is reused by the new restDAQ configuration class (2026-11-07), unrelated to the legacy one. |
|   | x |   | TRestRawFFT | FFT utility on raw signals (covered by TRestFFTAnalysis?). |
|   | x |   | TRestRawToSignalProcess | Base class of the decoders from binary files to TRestRawSignalEvent. |
| x |   |   | TRestRawMultiFEMINOSToSignalProcess (migrated, same name) | Decodes binary files of several FEMINOS/TCM cards. |
| x |   |   | TRestRawFeminosRootToSignalProcess (migrated, same name) | Converts ROOT files of the Feminos DAQ (feminos-daq) to TRestRawSignalEvent. |
| x |   |   | TRestRawFemDAQToSignalProcess (migrated, same name) | Reads old FEMDAQ ROOT files (tree `SignalEvent`). |
|   | x |   | TRestRawAFTERToSignalProcess | Decodes binary files of the AFTER electronics. |
|   | x |   | TRestRawMultiCoBoAsAdToSignalProcess | Decodes CoBo/AsAd (GET) files; flagged as possibly obsolete. |
|   | x |   | TRestRawFEUDreamToSignalProcess | Decodes binary files of the FEU Dream electronics. |
|   | x |   | TRestRawTDSToSignalProcess | Decodes binary files of a Tektronix (TDS) oscilloscope. |
|   | x |   | TRestRawUSTCToSignalProcess | Decodes binary files of the USTC electronics. |
|   | x |   | TRestRawBiPoToSignalProcess | Decodes files of the BiPo acquisition (Matacq cards). |
|   | x |   | TRestRawMemoryBufferToSignalProcess | Reads events from a shared memory buffer written by an external process. |

### libraries/detector

| M | N | P | Class | What it does |
|---|---|---|-------|--------------|
| x |   |   | TRestDetectorHitsAnalysisProcess (migrated, same name) | Basic hit observables: energy, number of hits, mean position, sigmas, skew, fiducialization. |
|   | x |   | TRestDetectorHitsGaussAnalysisProcess (GaussSigmaX/Y/Z of TRestDetectorHitsAnalysisProcess with `gaussFit`) | Sigmas of the hit distribution in X, Y and Z from gaussian fits. |
|   | x |   | TRestDetectorHitsReadoutAnalysisProcess (TRestDetectorHitsAnalysisProcess in the readout pipeline, FiducialEnergy in a generic fiducial cylinder) | Hit observables of one readout plane/module (energy, mean position, fiducial energy). |
|   | x |   | TRestDetectorHitmapAnalysisProcess (TRestDetectorHitsTransformationProcess + MeanX/Y/Z of TRestDetectorHitsAnalysisProcess) | Applies rotation/translation/mirror to the mean hit positions and stores them as observables. |
|   | x |   | TRestDetectorHitsReductionProcess (TRestTrackReductionProcess, same algorithm) | Merges close hits iteratively up to a distance or a maximum number of hits. |
| x |   |   | TRestDetectorHitsNormalizationProcess (migrated: TRestDetectorHitsTransformationProcess) | Multiplies the energy of all hits by a factor. |
| x |   |   | TRestDetectorHitsTranslationProcess (migrated: TRestDetectorHitsTransformationProcess) | Translates all hits by a vector. |
| x |   |   | TRestDetectorHitsRotationProcess (migrated: TRestDetectorHitsTransformationProcess) | Rotates all hits around a center and an axis. |
| x |   |   | TRestDetectorHitsSpecularProcess (migrated: TRestDetectorHitsTransformationProcess) | Mirrors the hits with respect to a plane. |
|   | x |   | TRestDetectorHitsShuffleProcess | Shuffles the hits of the event randomly. |
|   | x |   | TRestDetectorHits3DReconstructionProcess | Reconstructs XYZ hits from XZ and YZ strip hits. |
| x |   |   | TRestDetectorAvalancheProcess (migrated, same name, new meaning) | Applies the avalanche gain fluctuation to the hit energy. |
| x |   |   | TRestDetectorLightAttenuationProcess (replaced 2026-11-09: TRestDetectorLightCollectionProcess, model `attenuation`, with photons) | Attenuates the energy of veto hits by the distance to the light readout. |
| x |   |   | TRestDetectorScintillationProcess, TRestDetectorLightCollectionProcess and TRestFibreReadout (new, LiquidO macro Geant4ToSignal.C of losimulations) | Photons, their collection by the channels (local capture or attenuation) and photoelectrons; readout of fibre lattices. |
|   | x |   | TRestDetectorPositionMappingProcess | Corrects the energy by position (gain map) and generates the map. |
|   | x |   | TRestDetectorGainMap | Metadata with the gain map per module/position and its application to events. |
|   | x |   | TRestDetectorSingleChannelAnalysisProcess (TRestDataSetGainMap) | Calibrates the (relative) gain of each channel and applies it to the signals. |
|   | x |   | TRestDetectorSignalChannelActivityProcess (plots of SignalsID, SignalsPhysicalID and SignalsMaxAmplitude of TRestRawToDetectorSignalProcess with TRestAnalysisPlot) | Channel activity histograms per readout channel of the detector signals. |
| x |   |   | TRestDetectorSignalRecoveryProcess (migrated, same name) | Recovers dead channels interpolating the charge of the neighbouring channels. |
|   | x |   | TRestDetectorTriggerAnalysisProcess | Emulates a fixed-window trigger on the signals; threshold/time observables. |
|   | x |   | TRestDetectorDaqChannelSwitchingProcess | Reassigns the DAQ ids of the channels of a readout module. |
|   | x |   | TRestDetector | Generic detector metadata (name, pressure, field, basic geometry). |

### libraries/track

| M | N | P | Class | What it does |
|---|---|---|-------|--------------|
| x |   |   | TRestTrackAnalysisProcess (migrated, same name) | Computes track and event observables, optionally by projection. |
| x |   |   | TRestTrackLineAnalysisProcess (migrated, same name) | Finds origin, end, length and direction of linear tracks. |
| x |   |   | TRestTrackReductionProcess (migrated, same name) | Merges the hits of each track into nodes up to a maximum number of nodes. |
| x |   |   | TRestTrackLinearizationProcess (migrated, same name) | Reduces each track to nodes along its best-fit line. |
| x |   |   | TRestTrackPathMinimizationProcess (migrated, same name) | Orders the hits/nodes of each track along the shortest path (TSP). |
|   | x |   | TRestTrackReconnectionProcess (its `branches` observable: Branches of TRestTrackPathMinimizationProcess) | Breaks and reconnects the segments of an ordered track where there are jumps. |
|   | x |   | TRestTrackDetachIsolatedNodesProcess | Detaches the isolated nodes of the track. |
| x |   |   | TRestTrackBlobAnalysisProcess (migrated, same name) | Observables of the track ends (blobs): energy in a sphere and distance between them. |
|   | x |   | TRestTrackPointLikeAnalysisProcess (TRestTrackAnalysisProcess: Size, MaxNodeSize and expressions) | Observables to identify point-like events (size, energy/length balance of clusters). |
|   | x |   | TRestTrack2DAnalysisProcess | XZ/YZ track observables (energy, length, width, balances of first and second track). |
|   | x |   | TRestTrack3DAnalysisProcess | XYZ track observables (energy, length, volume of first and second track). |

### libraries/connectors

| M | N | P | Class | What it does |
|---|---|---|-------|--------------|
|   | x |   | TRestRawReadoutAnalysisProcess | Per channel/module observables with raw signals and the readout (baseline, first/last strip, activity maps). |
|   | x |   | TRestRawReadoutMetadataProcess | Creates and stores TRestRawReadoutMetadata from the readout. |
|   | x |   | TRestDetectorHitsToTrackFastProcess | Groups hits into tracks with TRestMesh (faster than the distance-based version). |
|   | x |   | TRestTrackToDetectorHitsProcess | Converts the tracks (or the main one) back to a TRestDetectorHitsEvent. |

### libraries/geant4

| M | N | P | Class | What it does |
|---|---|---|-------|--------------|
|   | x |   | TRestGeant4VetoAnalysisProcess (TRestGeant4AnalysisProcess: selections with `perVolume`) | Energy deposited in the veto volumes (grouped by keywords). |
|   | x |   | TRestGeant4NeutronTaggingProcess (TRestGeant4AnalysisProcess: `perVolume`, `interactions` and TRestGeant4QuenchingProcess) | Observables to tag neutrons (captures, deposits in vetoes and moderators). |
|   | x |   | TRestGeant4BlobAnalysisProcess | Position and distance of the double beta blobs from the Monte Carlo truth. |
|   | x |   | TRestGeant4ParticleSourceCosmics | Cosmic source with energy and angle read from histograms. |
| x |   |   | TRestGeant4ParticleSourceCry | Cosmics generated with the CRY library. Migrated (2026-11-13) as the restG4 generator `cry` (CRY linked to restG4). |
|   | x |   | TRestGeant4ParticleSourceDecay0 | Decays (double beta, etc.) generated with BxDecay0. |

### Replaced or not needed

Legacy classes without an equivalent of the same name because they are already covered. Mark **M** to
migrate one anyway.

| M | Class | Reason |
|---|-------|--------|
|   | TRestAnalysisTree | Analysis trees are plain TTrees read with RDataFrame. |
|   | TRestProcessRunner, TRestThread | Replaced by TRestEventProcessManager and TRestManager (threads included). |
|   | TRestTask | Decided not to have it (macros and TRestAnalysisPlot). |
|   | TRestDataSetPlot | Replaced by TRestAnalysisPlot. |
|   | TRestStringHelper, TRestStringOutput, TRestReflector, TRestVersion, TRestPhysics | Covered by TRestTools, TRestYAMLUtils, TRestLogManager, the field registry and TRestSystemOfUnits. TRestGDMLParser keeps its name (namespace in tools, no environment variables: `parameters`). |
|   | TRestVolumeHits | Integrated in TRestHits. |
|   | TRestDetectorReadoutPlane/Module/Channel/Pixel/Mapping, TRestDetectorGeometry, TRestDetectorSetup | Replaced by TRestDetectorReadout (TRestMicromegasReadout, TRestGDMLReadout). |
|   | TRestDetectorGas, TRestDetectorDriftVolume | Replaced by TRestDetectorMedium (drift field: `driftField` of its `charge`). |
|   | TRestDetectorElectronDiffusionProcess, TRestDetectorGarfieldDriftProcess | Replaced by TRestDetectorHitsDriftProcess. |
|   | TRestDetectorFiducializationProcess | Replaced by TRestDetectorHitsToSignalProcess. |
|   | TRestEventSelectionProcess | Replaced by TRestSelectionProcess. |
|   | TRestRawPeaksFinderProcess | Replaced by TRestDetectorSignalPeaksFinderProcess. |
|   | TRestRawSignal, TRestDetectorSignal, TRestGeant4Hits, TRestGeant4Particle | Integrated in their events / TRestGeant4Track. |
|   | TRestRawSignalViewerProcess, TRestDetectorSignalViewerProcess, TRestTrackViewerProcess, TRestDetectorHitsEventViewer, TRestDetectorReadoutEventViewer | Covered by DrawEvent, TRestBrowser and TRestEventViewer. |
|   | TRestLegacyProcess, TRestRawSignalRecoverChannelsProcess, TRestRawZeroSuppresionProcess | Already obsolete in legacy (`libraries/legacy`). |

## Pending work

### From the checklist and the decisions

- **TRestDataBase** (M): postponed until there is a real database to connect to (backend to be decided;
  2026-10-31, 2026-11-06).
- **Pipeline tests** (P): tests running the YAML of `examples/` and of the libraries with restManager, once it
  is decided where to publish the input data outside the repository (2026-11-09).
- **Unit test coverage**: not covered yet (2026-11-09): TRestDataSetProcess (abstract),
  TRestEventProcessManager, viewers, TRestGeant4AnalysisProcess (needs an input file),
  TRestDetectorAvalancheProcess (gain tables), TRestGDMLReadout (GDML file), TRestDataSetGainMap,
  restGarfield and restDAQ.
- **New repository and CI**: the GitHub Actions workflows stay local until the maintainer creates the new
  repository outside rest-for-physics; the Docker image (`/home/juanan/restv2-docker`) is built and pushed by
  the maintainer (2026-11-09).
- **Light response of the medium**: lightYield, attenuationLength and effectiveLightSpeed; the decay time of
  the scintillator and the propagation along the fibres (TRestFibreReadout) are not simulated yet
  (2026-11-09).
- **Field model**: field maps will replace `driftField` of the `charge` response (2026-11-05).
- **Gain tables with a mesh**: restGarfield simulates avalanches in a uniform field (parallel plates); a
  geometry field for the mesh will be added later (2026-11-05).
### Ideas

- **Event format version conversions** (not needed yet, 2026-11-06). Done: a format version per event class
  (`TRestEvent::GetFormatVersion`, `GetMinimumFormatVersion`) in the UserInfo of its tree, checked by TRestRun
  when reading (2026-11-02); `SetBranchAddresses` binds every branch with `TRestEvent::BindBranch`, which
  throws if a branch is missing instead of ROOT's message and empty data, and any branch change increases the
  format version (2026-11-06). Not done: reading files with renamed or retyped branches (e.g. TRestGeant4Event
  before the XYZVector → X, Y, Z floats and names → metadata IDs change), either converting when reading (old
  branch in a temporary, converted after `GetEntry`, names to IDs with the reverse tables of the metadata) or
  with a one-time macro (`REST_ConvertRun.C`) that rewrites old files. Files without a version (written before
  it existed) are recognised by their old branches (e.g. `fTrackParticleNames`, `fPrimaryPosition` in
  TRestGeant4Event). Not needed while there is no deployment and no old files in use.
- **Simulation vs data tuning** (long term, 2026-11-06): tune the simulation chain parameters (shapingTime,
  gain, noise of TRestRawSignalAddNoiseProcess, resolution, triggerDelay of the vetoes) comparing simulations
  with real data (e.g. AlphaCAMM), with TRestAnalysisPlot overlaying both: pulse amplitude, width and rise
  time (TRestRawSignalAnalysisProcess), baseline sigma, energy spectra and veto peak times. The AlphaCAMM
  pulses already look right in the maintainer's tests (values in `/home/juanan/AlphaCAMM_restv2/Analysis.yaml`).
- **Build time** (2026-11-06): ccache is done (`REST_USE_CCACHE`, used when found). Precompiled headers
  (`target_precompile_headers` of ROOT/yaml-cpp) not done: risk of conflicts with the ROOT dictionaries and
  the cling modules (as with the macros).
- **Offline resynchronization of the FEMs** (restDAQ, 2026-11-07, not done): restDAQ does not resynchronize
  the FEMs online (overhead in the data taking); it only counts the events out of sync (Prometheus, log) and
  stores the counter and timestamp of each FEM (TRestRawDAQSignalEvent). If desynchronized runs appear: a
  process reading the input tree as TRestRawDAQSignalEvent (it must check HasFEMInfo) that keeps the signals of
  each FEM (fFEMNSignals) over several entries and rebuilds the events with the same timestamp. It needs to
  read the input tree with the derived class (TRestRun creates the class named by the tree).

## Decision log

Dated sections in chronological order, **oldest first**. Items added later to a section carry their own date.

### General decisions (2026-10-31)

- TRestMesh: not migrated.
- TRestDataBase (2026-11-06): will be migrated, postponed until there is a real database to connect to
  (backend to be decided). Legacy did not connect to any: it only read/wrote the run number in
  `$REST_USER_PATH/runNumber` (already covered by `runNumber: auto` of TRestRun, `$REST_HOME/.rest/runNumber`)
  and the URLs of `$REST_PATH/data/dataURL`; the real connection was in external packages inheriting from it.
- Gain maps: only TRestDataSetGainMap, plus a process applying the map; TRestDetectorGainMap and
  TRestDetectorPositionMappingProcess are not migrated.
- TRestDetectorAvalancheProcess: changes meaning. Extend restGarfield to obtain the avalanche parameters as a
  function of the voltage and have the process use them (designed and implemented 2026-11-05, see below).
- TRestGeant4ParticleSourceCry: the restG4 generator `cry`, CRY called during the simulation (2026-11-13,
  implemented, see below; the restCRY executable of 2026-11-01 was removed).
- TRestDataSetCalibration, TRestDataSetGainMap and TRestDataSetOdds: as processes of a new
  TRestDataSetProcessManager (design 2026-11-02, see below).
- TRestTrackToDetectorHitsProcess: not migrated (2026-11-06). Each track already stores its hits and
  TRestTrackAnalysisProcess gives the observables of the main track; there is no use case.
- Migrated (2026-11-01): TRestDetectorHitsAnalysisProcess, TRestTrackReductionProcess,
  TRestTrackPathMinimizationProcess (own shortest path: exact up to 16 nodes, otherwise nearest neighbour +
  2-opt, no Concorde).
- TRestDetectorHitsToTrackFastProcess: not migrated; TRestDetectorHitsToTrackProcess gives the same tracks as
  legacy and is already as fast.

### CRY in restG4 (2026-11-13, implemented)

Replaces the restCRY design below. The showers file of restCRY was too large to keep or share (10^8 showers
over 3 m: tens of GB) and restG4 takes much longer to simulate the showers than CRY to generate them, so CRY
is called during the simulation (the user chose it; the legacy design, rejected on 2026-11-01).

- The options of CRY in their own section of the TRestGeant4Manager, class `TRestGeant4CRY` (as the physics
  lists; the user found them too many for the generator, "equivalent to the readouts"): `particles` (a list
  of muons, neutrons, protons, gammas, electrons, pions, kaons; all by default; replaces the legacy
  returnMuons... flags), nParticlesMin/Max, date, latitude, altitude, `subBoxLength` (now needed), optional
  `maxExposureTime` (stops when CRY has simulated that time; with nEvents 0 only it). TRestGeant4Manager
  links it to the cry generator (an error if it is missing) and stores it in the output; with another
  generator the section is not used (the same configuration for several generators).
- Generator `type: "cry"` (TRestGeant4SpatialGeneratorCRY, libraries/geant4, no CRY dependency): only
  `position` and `verticalAxis` of the CRY square. Results `showersUsed` and `exposureTime` (the time
  simulated by CRY) in the generator section as before; `Merge` adds them.
- CRY is behind an interface (TRestGeant4CRYEngine) given by restG4: `packages/restG4/cry/CRYEngine.cxx`,
  compiled into the restG4 executable (non-PIE, `libCRY.a` has no -fPIC) when CRY is found (`REST_CRY`), and
  registered by main.cxx. Without CRY the cry generator is an error.
- A single CRY generator shared by the threads, called under a lock; its seed is the metadata seed.
- Removed: restCRY, TRestGeant4CRYShowers (TRestGeant4CRY keeps its CRY options), the showers file and
  `filename`;
  examples/12.Generators/CRYShowers.yaml.

### CRY (2026-11-01, implemented; replaced on 2026-11-13, see above)

- Executable `restCRY config.yaml` in the restG4 package (`packages/restG4/restCRY.cxx`), CMake flag, links
  `libCRY.a` from `/programas/titan/cry/cry_v1.7` (the .so is not needed). Only restCRY depends on CRY.
- Metadata class `TRestGeant4CRYShowers` in `libraries/geant4` (no CRY dependency): CRY parameters
  (returnNeutrons ... returnMuons, nParticlesMin/Max, date, latitude, altitude, subBoxLength), `nShowers` and
  `exposureTime` (stops at the first reached; error if none is given), seed and file. No
  xoffset/yoffset/zoffset: placement only in restG4.
- Output: TTree `showers` (one entry per shower: name, charge, energy, position, direction and time of each
  particle) plus YAML metadata as restGarfield (configuration, program, date, generated showers,
  timeSimulated of CRY). Always overwrites the file.
- restG4: generator `type: "cry"` (no `source`): `filename`, `position` (center of the CRY plane in the world)
  and `verticalAxis` (world axis that is +Z of CRY). CRY positions used as they are. Sequential reading with a
  counter shared between threads; stops with a warning when the showers run out.
- Time per particle: new time field in TRestGeant4ParticleState (relative to the first particle of the
  shower), passed to Geant4.
- TRestGeant4Metadata stores the used showers and the equivalent exposure
  (timeSimulated × used / generated): `showersUsed` and `exposureTime` in its `generator` section, with
  `GetCRYShowersUsed()`/`GetCRYExposureTime()` (us); `Merge` adds them.

### Dataset processes (2026-11-02, implemented)

- TRestDataSetCalibration and TRestDataSetLogOdds (in `source/core`, moved to RestAnalysis on 2026-11-03, see
  next section) and TRestDataSetGainMap (in `libraries/detector`, uses the readout) are dataset processes
  (TRestDataSetProcess) of a TRestDataSetProcessManager: input dataset (`inputFileName`, only already
  generated dataset files), processes in order (each adds columns to the RDataFrame) and output dataset
  (`outputFileName`) written once. Launched with restManager (TRestDataSetProcessManager section, without
  TRestRun or TRestEventProcessManager; `--i`/`--o` give input and output); TRestAnalysisPlot sections without
  inputFileName plot its output.
- Parameters: each process has `referenceFileName` (dataset where it is fitted; empty: the input one) and
  `cuts` (TRestCut of the manager, applied to the reference). If the reference has in its metadata a process
  with the same name and class, its parameters (its stored configuration) are reused without fitting. The
  reference is used as it is: the previous processes of the chain are not applied to it.
- Columns `<name>_<obs>`: calibration `<name>_<calibratedObservable>` (default `Energy`, calib_Energy as
  legacy); gain map `<name>_<calibratedObservable>` and `<name>_<calibratedObservable>NoSegmentation`;
  log-odds `<name>_<observable>` and `<name>_total`. The output has all the input columns plus the new ones,
  the runs, the configuration of the input dataset, of the pipeline and of each process with its results
  (YAML), and the objects (spectra, fits, PDFs) in RESTDataSetStore/<class>/<name> (2026-11-03; before, one
  folder per process at the root). Those of the input are copied; a process named as one of the input
  replaces it with a warning (error if of another class); another name is added; a pipeline with the same
  name replaces the configuration of the previous one.
- Two calibration classes with common code: TRestDataSetCalibration (peaks and linear fit of the whole
  dataset) and TRestDataSetGainMap (segments of a readout) share the peak fit
  (TRestDataSetCalibration::FitPeak/FitLine).
- TRestDataSetGainMap: Draw (spectra, gain map) and Refit in restRoot; the corrected map is saved updating the
  dataset (metadata and objects; its columns are stale until the pipeline is run again with that file as
  reference). No REST_RefitGainMap macro.

### Dataset metadata (2026-11-02, implemented)

- The dataset writes the metadata of the first input file with selected runs (processing, readouts,
  TRestGeant4Metadata...; not its TRestRun nor the TRestDataSet it comes from), its readouts (folder
  `Readouts`) and its geometry (`Geometry`): a TRestDetectorReadoutManager with the dataset as
  `inputFileName` loads its readouts.
- The essentials are checked, and that the runs were processed the same way: throw if the processes differ
  (each TRestEventProcessManager with the same processes, name and class, in the same order), the
  AnalysisTree branches (name and type), the readouts (configuration, decodings and geometry) or the geometry
  (volumes and positions of the children). The rest (e.g. the seed of a simulation) may change.
  - Different process parameters (2026-11-04; before, the whole TRestEventProcessManager had to be equal):
    allowed; each distinct processing is stored in `RESTDataSetProcessing` (YAML list) and each run of
    `RESTDataSetInfo` carries its index (`processing`); GetProcessings() and GetProcessing(runIndex); merging
    datasets joins the lists without duplicates; a RESTWarning lists each differing parameter with its values
    and runs (no field to silence it). Not used in runConditions or columns (to classify by a parameter, put
    it in runInfo). REST_MergeRuns stays strict.
- Geometries are compared by an MD5 hash of their structure, written next to them (`GeometryMD5`,
  TRestTools::WriteGeometry); they are only read for files older than the hash.
- Access to the metadata as objects: TRestMetadataStore (composition, no common base class or multiple
  inheritance) in TRestRun and TRestDataSet; TRestMetadata::LoadFromFile loads their objects (readouts of
  TRestDetectorReadoutManager with `self` = the file read, geometry of TRestGeant4Metadata), e.g. for the gain
  map.
- TRestDataSetProcessManager: `inputFormat` and `outputFileName` with [key] as TRestRun (labels of
  inputFormat, otherwise keys of the runs of the dataset).
- Dataset names (TRestDataSet::ResolveFilePattern, also in REST_GenerateDataSet): no own number; [runStart],
  [runEnd], [runRange] (00010_00012, or 00010 with a single run) and the run parameters if they are equal in
  all runs (error if they differ); `[runTag|Mixed]` gives the default value if they differ (or it is missing in
  some run). Preferred form: `D_[runStart]_[runEnd]_[runTag|Mixed].root`.
- Names (2026-11-02): TRestProcessManager → TRestEventProcessManager (symmetric with
  TRestDataSetProcessManager; no compatibility with the old name: there were only test runs);
  TRestCalibration, TRestGainMap, TRestLogOdds → TRestDataSetCalibration, TRestDataSetGainMap,
  TRestDataSetLogOdds (and TRestDataSetCalibrationPeak, TRestDataSetLogOddsObservable).
- Layers (2026-11-03): RestAnalysis is now above core (links RestCore; core no longer depends on analysis)
  and contains TRestDataSetCalibration and TRestDataSetLogOdds, high-level analysis processes (core only has
  the base: TRestDataSet, TRestDataSetProcess, TRestDataSetProcessManager). The only thing core used from
  analysis, GetMinimumDistance and GetBoundaries of TRestHitsAnalysis (binning for drawing), moves to
  TRestHits as statics: GetMinimumDistance and GetBinning (struct Binning {min, max, nBins}, as legacy
  TRestHits::GetBoundaries), also used by TRestTrackEvent. TRestHitsAnalysis stays generic (positions and
  energies, no TRestHits).
- TRestDataSetGainMap (2026-11-03): a single readout, no modules (planeId, moduleId and definitionCut removed,
  no moduleID column); moved to `libraries/detector`. The X and Y extent is the box of the channels of the
  `readout` of the reference dataset (TRestDetectorReadout::GetChannelsBoundingBox, through
  TRestDataSet::GetMetadataOfType<TRestDetectorReadoutManager>), divided in `segments` (manual splitX/splitY
  still valid); it calibrates all events (several readouts: one map per readout, each in its dataset).
  TRestDataSetProcess::Fit also receives the reference dataset.
- Several readouts (2026-11-03): TRestDetectorReadoutManager loads all of them (to see the event in the full
  readout) and its `readout` field is the readout of the data of the file (empty: the only one loaded). One
  file per detector: in simulation with `volumes` of TRestGeant4ToDetectorHitsProcess, in data taking with
  TRestRawSignalChannelSelectionProcess (ranges of the cards of the other detector; channels are not selected
  by readout because CommonNoiseReduction uses all the channels of the card). The processes with `readout`
  (HitsDrift, HitsToSignal, SignalToHits) and TRestDataSetGainMap keep it: empty = readout of the data, another
  name = exception with a warning (SelectReadout); the one used is stored.
- Dataset generation with restManager (2026-11-03, the REST_GenerateDataSet macro is kept): TRestDataSet
  section in the TRestManager, `--i` = filePattern, `--o` = outputFileName; chained with a
  TRestDataSetProcessManager in the same manager (the generated dataset is its input, `--o` its output). The
  generated dataset is processed in memory and only the processed one is written; the generated one only if
  its TRestDataSet section has outputFileName (2026-11-03).
- References of the dataset processes (2026-11-03): `referenceFileName` can be a pattern with
  `referenceSelection: closest` (the dataset closest in time to the processed one: smallest separation between
  their start and end times, then the closest centers), and `restManager --r <process>=<dataset>`
  (repeatable) forces the reference of a process (e.g. gainMap fixed in the YAML and calibration by argument).
  Result `referenceUsed`.
- Peak fit (2026-11-03): TSpectrum is a legacy ROOT interface; new TRestPeakFit in RestAnalysis (functions on
  TH1, no metadata), with RooFit: gaussian or Crystal Ball and optional background (constant or linear); no
  peak search (windows from the maximum of the spectrum or the ranges; autoRangePeaks removed). Used by
  TRestDataSetCalibration and TRestDataSetGainMap (fields peakModel, background, window). CMake: RooFitCore and
  RooFit instead of Spectrum.
- Optional peak search (2026-11-03): a single field `peakSearch` (false) in calibration and gain map; searches
  only inside the range (or the estimated window) of each peak, with smoothing (3 bins) and significance
  (prominence > 3σ) fixed in TRestPeakFit::FindPeak; without a significant peak it fits the whole range.

### restG4 examples and macros (2026-11-03)

- Examples of `packages/restG4/examples`: each `.rml` becomes `.yaml` (restG4; the analysis and plot ones go to
  restManager) and the `.rml` files are deleted, as well as `v2.1.7/`, `05.PandaXIII/geometry/Th232_SS_01.rml`
  (absolute path), `12.Generators/CryGenerator.rml` (CRYSimulation.yaml exists) and `SinCos2Circle.rml`
  (formula SinCos2 does not exist in restv2). fullChain per source → `fullChain` field of the metadata;
  `keepTracks` is not needed (the tracks of all active volumes are stored).
- Legacy macros of `libraries/geant4/macros`: MergeRestG4Files inside REST_MergeRuns (simulations with
  different seeds); ListIsotopes + FindIsotopes + FindGammasEmitted → REST_Geant4_ListIsotopes;
  ReadNEvents(_inROI) + GetROIEvents(_Fiducial) → REST_Geant4_ReadEvents; ViewGeometry and
  GenerateResponseMatrix migrated; GetBiasingError not (restG4 had no biasing; see biasingVolumes, 2026-11-08).
- Point by point decisions (2026-11-03):
  - PrintEvent.C deleted (covered by REST_Geant4_ReadEvents).
  - Data: Muons.root copied into 04.MuonScan; Xe136bb0n.dat in 01.NLDBD and a symlink in 05.PandaXIII;
    CosmicGeneratorBenchmarks and 12.Generators/geometry-sphere deleted (CosmicsCry.root not needed).
  - Paths: left as they are (filename/use relative to the launch directory, GDML relative to the YAML); the
    examples are launched from their directory, shared data with symlinks.
  - Parameters (2026-11-04, implemented), replacing the environment variables in GDML and YAML:
    - `parameters` section at the root of any REST YAML (generic, TRestYAMLUtils): name: value (with units)
      or `${ENV:VAR}`, no expressions nor parameters using others. Used in the YAML as `${PARAM:name}`
      (undefined: error).
    - `--set name=value` (repeatable) in restG4, restManager, restCRY and restGarfield: only for that run, the
      YAML is not modified; a name not in `parameters` is an error.
    - GDML: no `${VAR}`. Numeric parameters as `<constant>` and text ones (e.g. a material name) as internal
      XML entities (`<!ENTITY gas "XenonNeon">`, `&gas;`), with their default value in the GDML (visible with
      any viewer). A parameter named as a constant or entity of the GDML overrides it; one that is neither in
      the GDML nor used in the YAML is an error.
    - GDML preprocessing: common functions (TRestGDMLParser, in tools) used by all readers (restG4 with Geant4
      and TGeoManager, TRestGeant4GeometryInfo, TRestGDMLReadout, REST_Geant4_ViewGeometry): flattens the
      ENTITY (recursive, local and remote) into a single temporary GDML (system temporary directory, deleted
      after reading) and overrides constants and entities. No variable values in the metadata.
    - Output: only the resolved configuration (with the substituted values) and the resolved geometry; the
      parameters are not stored separately.
    - `restG4 config.yaml [--set ...] --update-gdml`: rewrites in the GDML of the YAML (and its local included
      files) only the value of the constants and entities with the parameter values, without flattening, and
      exits without simulating: viewers see the simulated geometry (it changes the GDML defaults).
    - Examples 02, 06, 07, 08, 14: environment variables → parameters with the legacy/README defaults (F20,
      U238, 5 MeV, 5 um, values of launchResponse.sh...), in the GDML as constant or entity; the sweep scripts
      (launchResponse.sh) use --set. 02.TREXDM had no defaults: Neon 1% isobutane at 10 bar (nominal TREX-DM,
      to be confirmed).
    - Implementation: TRestYAMLUtils::ResolveRefs (parameters, overrides, GetParameters, MarkParameterUsed,
      CheckParametersUsed, AddParameterOverride); TRestGDMLParser (namespace in `source/tools`, the common
      layer of RestGeant4 and RestDetector: Resolve, WriteResolved, Import, UpdateParameters);
      TRestTools::DownloadFile (generic, curl); readers: DetectorConstruction, Application,
      TRestGeant4GeometryInfo, TRestGDMLReadout, REST_Geant4_ViewGeometry. The numbers of a constant are in
      REST units (mm, keV, us, bar): in the GDML the lengths of the parameters use lunit="mm".
  - Materials (2026-11-04, implemented): common `packages/restG4/examples/materials.xml` = the one of
    12.Generators (NIST, pure gases, Ar-isobutane and XeNe-isobutane mixtures at fixed pressures, solids),
    without the parametrized materials of 03/06 (gasPressure, targetGasDensity...), plus the missing ones:
    ideal gas at 298.15 K with molar fractions in the name (NeonGas, Xenon1%TMA10bar, Neon1%Isobutane10bar,
    Xenon30%Neon1.5bar, NitrogenGas) and the solids of 05.PandaXIII (Mylar C10H8O4 1.39 g/cm3 (the one of 05
    had fluorine), Acrylic C5H8O2 (the one of 05 had H and O swapped), HDPE, Kapton, RuO2, Resistance). A
    missing mixture or material is added to that file.
    - Each GDML uses `<!ENTITY materials SYSTEM "materials.xml">` (or geometry/materials.xml) and that file is a
      symlink to the common one; the own materials.xml of 03, 05, 06 and 12 deleted. The variables of the
      parametrized materials (targetGasDensity, gasPressure...) removed from the GDML. 10.Geometries does not
      include the common file (defines all its materials).
    - Since 2026-11-12 the common file is the `materials.xml` of rest-common-materials (the same definitions,
      plus those of LiquidO): each GDML includes it by URL (release v1.0, downloaded once and cached); the local file
      and its symlinks were deleted.
    - Material changes: Ar_ISO → Argon1%Isobutane1bar (04, 08, 11), PureXenon → XenonGas (09), Xenon_TMA →
      Xenon1%TMA10bar (01, 05), Stainlesssteel → StainlessSteel and PureNitrogen → NitrogenGas (05);
      02.TREXDM gas = Neon1%Isobutane10bar and 14.DetectorResponse gas = Xenon30%Neon1.5bar, without the
      pressure, fraction and density parameters (mixture and pressure given by the material). In an entity
      value the % is written `&#37;` (XML); restG4 --update-gdml escapes it.
    - Remote ENTITY of external GDML files are downloaded to `$REST_HOME/.rest/download` and reused.
  - Validation (2026-11-04, implemented): gtest in `packages/restG4/test`; each test launches the restG4 (and
    restManager if needed) executables from the example directory with few events (-n/--entries) and checks
    the .root with the restv2 API, with statistical tolerances instead of the exact legacy references. CMake:
    option BUILD_TESTING (OFF), find_package(GTest) (error if missing), restG4 tests only with REST_G4. All the
    .C of the examples are deleted, also GetQE.C and RecoilMFP.C (analysis, not validations). There was no
    test infrastructure in restv2.
    - `packages/restG4/test/src/examples.cxx` (one TEST per example or configuration, gtest_discover_tests,
      label restG4, REST_HOME = build/.../test/work), each test on its copy of examples/ in work/<test>. Checks
      of the legacy Validate with fewer events: run metadata, entries, mean energies (±20%), distributions of
      the primaries (ranges and means), isotopes of the chain (16 of U238, --set isotope=Be7), alpha fractions,
      Fe55 peak, efficiency (0.91 ± 0.03), speed of the geantinos, geometry volumes and --update-gdml. Without
      the exact values depending on the Geant4 version (number of hits and tracks). CRY has no test (CRY
      optional).
    - New TRestGeant4Event::GetPrimaryEnergy(n).
    - Postponed (2026-11-04): compile and tune the tests (values and tolerances) when the gtests of the rest of
      the framework are written; meanwhile BUILD_TESTING stays OFF by default and they are not compiled.
      Superseded: the restG4 tests were validated with ctest on 2026-11-08 and the per-component tests were
      added on 2026-11-09 (BUILD_TESTING is still OFF by default; the CI builds with it ON).

### Raw decoders (2026-11-04, implemented)

- Only TRestRawMultiFEMINOSToSignalProcess (.aqs, one Feminos or several with TCM) and
  TRestRawFeminosRootToSignalProcess (ROOT of feminos-daq, trees `events` and `run`; different from the
  format of TRestRawFemDAQToSignalProcess). No gblink (only used by AFTER): the constants of the Feminos frame
  format as constexpr in the .cxx.
- No base class TRestRawToSignalProcess: MultiFEMINOS inherits from TRestEventProcess.
- MultiFEMINOS: no fields. No `electronics` (2026-11-05): the .aqs come in built events
  (START/END_OF_BUILT_EVENT) also with a single card; with SingleFeminos the pending END_OF_BUILT_EVENT gave
  an empty event (warning per event, also in legacy) and with several cards the event was split; now inside a
  built event the event ends at END_OF_BUILT_EVENT and outside at the End of Event of the card. Each frame is
  decoded without its prefix and size (legacy read them as data) and a SOBE (built event with size) is no
  longer read as one block (legacy stayed in its first frame). No `showSamples` (with verbose debug channels
  and frames are printed) nor `minPoints`: with zero suppression (PFX_TIME_BIN_IX, ignored by legacy) each
  signal has 512 bins with the samples in their time bin and 0 in those not sent, as FEMDAQ
  (TRestRawZeroSupressionToRawProcess); channels without samples are not added. A single file, the
  `inputFileName` of the TRestRun (the pieces of a run are joined with REST_MergeRuns). The start header of the
  next event at the end of a frame is kept for it (the intention of legacy, without its logic with ID 0).
  Time: start of the file header + 48-bit timestamp (2^32·t2 + 2^16·t1 + t0, Feminos manual)·20 ns; legacy
  used 2^31 and 2^15 (bug: compressed the times).
- Unknown number of events: TRestEventProcess::InputFinished() and GetInputFraction(); the manager reads until
  the end of the file (not counted as a rejected event), the progress bar follows the fraction of the file
  read. With a generator (input "None") the pipeline runs in a single thread (HasGenerator).
- FeminosRoot: `useFeminosDaqRunInfo` (true): number, tag, description and start from the `run` tree (the non
  empty ones) and in `runInfo` detector, driftField (V/cm/bar), meshVoltage (V) and pressure (bar) if the
  configuration does not give them; samples per signal = signal_values / signal_ids (not a fixed 512); no own
  file field. Legacy lost the last event.

### TRestDetectorSignalRecoveryProcess (2026-11-05, implemented)

- Neighbours from a readout method, `TRestDetectorReadout::GetAdjacentChannels(daqID)` (physicalID ± 1 if
  connected; TRestMicromegasReadout does not join the last Y strip with the first X one), instead of the
  legacy modules (`readoutChannel ± 1`). Fields `channelIds` and `readout`.
- Groups of k consecutive dead channels (legacy only 1 or 2) with linear weights; the neighbours keep half
  (a neighbour of two groups gives half to each). Recovered if at least one neighbour has signal (legacy
  required both); not recovered at the readout edge (warning in InitProcess).
- Fixes legacy bugs: it subtracted the neighbours' charge in the already copied input event (energy not
  conserved), with two dead channels it subtracted 25 % twice, `GetSignalIndex > 0` instead of `>= 0`, and it
  went out of range at the edge of a module.

### Avalanche, medium and field (2026-11-05, implemented)

- TRestDetectorAvalancheProcess (hits → hits, after hitsDrift): fields `amplificationVoltage` and `gap` (YAML
  only), `medium`, `table` (key, mandatory only if there are several candidates) and `seed`. E = V/gap; G and
  θ from the gain table of the medium with that gap, the pressure and temperature of the medium (exact: the
  gain is not scaled with pressure); outside the field range of the table, error. log G linear in E, θ linear
  in E. Each hit (n = E/W electrons) is multiplied by Gamma(n(θ+1), G/(θ+1))/n (sum of n Polya). Output:
  energy × gain (as legacy). Writes the G and θ used. No energyReference/resolutionReference/detectorGain
  (that is TRestDetectorHitsSmearingProcess).
- TRestDetectorAmplification, now TRestDetectorGainTable (new, detector, like the transport table:
  configuration run by restGarfield + a table + I/O of the medium file): `gap`, `reducedFieldRange` (E/p,
  e.g. kV/cm/bar) and equally spaced `fieldNodes` (2026-11-05: the useful range is almost constant in E/p,
  each pressure uses E = (E/p)·p; the table is still in field; voltageRange, which does not scale with
  pressure, and logVoltage discarded), `pressures`/`temperatures` (default those of the medium), `avalanches`
  per field, `usePenning`/`penningRate`, `overwrite`. restGarfield: microscopic avalanches
  (AvalancheMicroscopic) in a uniform field (ComponentConstant, parallel plates), 1 electron at the mesh with
  0.1 eV; mean G, θ by moments (σ²/G² = 1/(1+θ)) and a histogram per field. Meshing: later (a geometry field
  will be added).
- TRestDetectorMedium merged (restGarfield definition + conditions): name, state, components, mediumFile
  (restGarfield writes there; TRestDetectorTransport loses outputPath), pressure, temperature, wValue,
  fanoFactor and fixed transport values (driftVelocity, longDiff, transDiff, attachment, no file). Metadata of
  the manager, self-contained: in LoadConfig it loads the composition (from the file; if given in the YAML it
  must match) and all the transport and amplification tables at its pressure and temperature (gas transport:
  the closest pressure scaled with E/p, no warning). Get...(field): the narrowest transport table containing
  the field, outside all of them the one at the edge with a warning; `transportTable` forces a key. Writes
  file, composition and loaded keys; the processes write the values used.
- Generic medium (2026-11-05): TRestDetectorMedium is only the material (name, state, components, mediumFile,
  temperature; pressure only for gases, error for solids and liquids) and its responses are keys without
  `class`, each its own class defined in TRestDetectorMedium.h/.cxx (no new files) and created by the medium
  with its file and conditions: `charge` → TRestDetectorChargeResponse (wValue, fanoFactor, fixed transport,
  transportTable and the transport and gain tables). Future responses (scintillation...) will be other keys.
  The processes ask for medium->GetChargeResponse(user). restGarfield: the tables are keys of
  TRestDetectorTransportManager, siblings of the medium (see below).
- TRestDetectorTransport at a single pressure (`pressure`, 1 bar by default, no `pressures` list): gas
  transport scales with E/p and solids do not depend on pressure; the medium scales it without warning.
  Checking the scaling: REST_Garfield_DrawMedium(file, "", pressure) draws all the tables scaled to that
  pressure (they must overlap). No compatibility with old files (tables without `carriers`).
- TRestDetectorDrift → TRestDetectorElectricField → `driftField` of `charge` (2026-11-05): no field class (it
  was a light class with a single number); the drift field is a detector condition like the pressure, two
  drift regions are two media. No links between metadata: the processes take the medium from the manager
  (field `medium`, optional if there is only one; replaces driftMedium) and ask for
  medium->GetChargeResponse(user).GetDriftVelocity() (also diffusion and attachment at the drift field; error
  if there are transport tables and no driftField). A future field model (maps) will replace driftField.
- Tables without their own files (2026-11-05): TRestDetectorTransport → TRestDetectorTransportTable and
  TRestDetectorAmplification → TRestDetectorGainTable, defined in TRestDetectorTransportManager.h/.cxx (common
  code: index, reading and writing of the medium file). TRestDetectorTransportManager has the class-less keys
  `medium`, `transport` and `gain` (each a configuration or a list); restGarfield computes first the transport
  and then the gain (RunTransportSimulation, RunGainSimulation). Discarded: a TRestManager container (it is
  for analysis) and putting the tables in TRestDetectorMedium (it would grow too much). The medium file format
  does not change (`tables/`, `amplification/`).
- No Garfield++ table in the medium file (2026-11-05): restGarfield no longer stores the Magboltz `.gas`
  (`garfieldTable`): REST did not use it and it made the file depend on the Garfield++ version. The avalanche
  size histograms are kept (TH1D, to check the Polya fit).
- Macro REST_Garfield_DrawTransport → REST_Garfield_DrawMedium (also the gain tables).

### Tracks and hit transformation (2026-11-05, implemented)

- TRestTrackBlobAnalysisProcess: after Reduction + PathMinimization (ordered nodes). The spheres add the nodes
  of the track, not the original hits (the restv2 event stores a single stage; the core is not touched). Radii
  in `radii` (vector) and vector observables Q1, Q2, QHigh, QLow, QBalance, QRatio, one entry per radius.
  `projections` as TRestTrackAnalysisProcess (suffix _P); blob 1 = smaller |z|, as legacy.
- TRestTrackReconnectionProcess: not migrated (PathMinimization already gives the shortest path). Its
  observable `branches` moves to TRestTrackPathMinimizationProcess as Branches: steps longer than
  `branchFactor` (3) × the median step (mean + real sigma does not detect jumps with ~30 nodes; legacy used
  sigma = sqrt(mean)).
- TRestTrackDetachIsolatedNodesProcess: not migrated (no legacy rml used it).
- TRestDetectorHitsTransformationProcess: a single process for Normalization, Specular, Rotation and
  Translation, in that fixed order and only those given; it only moves XYZ hits (as legacy). Not Shuffle.

### Detector hits processes (2026-11-06, implemented)

- Not migrated: TRestDetectorHitsGaussAnalysisProcess, TRestDetectorHitsReadoutAnalysisProcess,
  TRestDetectorHitmapAnalysisProcess, TRestDetectorHitsReductionProcess,
  TRestDetectorHits3DReconstructionProcess and TRestDetectorSingleChannelAnalysisProcess. What their
  observables need goes into TRestDetectorHitsAnalysisProcess:
  - `gaussFit: true`: GaussSigmaX/Y/Z (TRestHitsAnalysis::GetGaussSigma, with the hits defining the
    coordinate; legacy used all). xy2SigmaGaus and xySigmaBalanceGaus are expressions.
  - Generic fiducial cylinder, without readout: `fiducialCenter` [X, Y, Z], `fiducialAxis` (default
    [0, 0, 1]), `fiducialRadius` (0: none) and `fiducialLength` (0: infinite, centered on the center):
    FiducialEnergy, energy of the hits inside (legacy readoutEnergyInFiducial: circle in XY,
    fiducialDiameter = 2 radius). The coordinates a hit does not define take the value of the center: with
    the axis along X, Y or Z it is equivalent to some value of them falling inside (XZ with axis Z:
    |X − centerX| ≤ radius). Selection by readout channel type is not needed: the vetoes go in their own
    pipeline.
  - Hitmap: TRestDetectorHitsTransformationProcess before TRestDetectorHitsAnalysisProcess (it also transforms
    the event). Hit reduction: TRestTrackReductionProcess after TRestDetectorHitsToTrackProcess. Gain per
    channel: TRestDataSetGainMap. 3D strip reconstruction: no (PandaX-III specific).
- GaussSigmaX/Y/Z and FiducialEnergy are only registered if active (`gaussFit`, `fiducialRadius` > 0). In
  general (TRestEventProcess), an `observables` name (except `all`, `none`) that the process does not give with
  its configuration is an error at initialization, with the list of the available ones.
- TRestDetectorLightAttenuationProcess: migrated (2026-11-06), see "Veto readout"; replaced on 2026-11-09 by
  TRestDetectorLightCollectionProcess (model `attenuation`), see "Fibre readout and scintillation".

### Veto readout (2026-11-06, implemented)

- TRestGDMLReadout separates medium and readout in each `readoutInfo` entry: `volume` (formerly
  `channelPrefix`, no compatibility: sensitive volumes, one channel each) and `readoutVolume` (optional: volume
  where the signal is collected, exactly one in the same parent volume, the assembly copy; error otherwise). In
  IAXO it is the light guide of each veto (the PMT is not in the GDML): the orientation of each PMT comes from
  the rotations of the layers, which rotate the whole assembly.
- GetReadoutPosition(daqID) in the base class (NaN by default); GDML: global origin of the readoutVolume of the
  channel. GetDistanceToReadout(daqID, pos) of the GDML: euclidean distance to that point; without channel it
  stays NaN (veto hits do not drift). GetPositionFromChannel is still the position of the channel (the
  scintillator).
- Light response of the medium (`light`, TRestDetectorLightResponse): attenuationLength and
  effectiveLightSpeed (legacy minimum); the rest (light yield, decay, quenching, quantum efficiency) when it is
  decided how to parametrize the light.
- TRestDetectorLightAttenuationProcess (hits → hits, `readout`, `medium`): E·exp(−d/λ), t + d/v per hit with
  the channel of its position; hits without channel are left as they are (NHitsWithoutChannel), a channel
  without distance is an error.

### Event format and simulation (2026-11-06, implemented)

- No conversions between event format versions: no deployment and no old files in use. To ensure
  compatibility, `TRestEvent::BindBranch` (used in the SetBranchAddresses of all event classes) throws an
  error if a branch is missing, instead of ROOT's message and empty data; any branch change increases
  GetFormatVersion.
- AlphaCAMM shaping: pulses checked by the maintainer; the systematic simulation/data comparison is a long
  term idea (see [Ideas](#ideas)).

### Observable trigger (2026-11-06, implemented)

- TRestDetectorSignalToRawSignalProcess: `triggerMode: "observable"` with `triggerObservable` (scalar branch of
  the analysis tree, from the input file or from a previous process), read as TRestCut (leaves of the output
  tree, without rebinding branches). It simulates the common DAQ trigger in the veto run, which is separate from
  the Micromegas one (TRestGeant4ToDetectorHitsProcess separates the volumes) and does not have its signals: it
  uses an observable of TRestGeant4AnalysisProcess of the same pipeline, e.g. g4Ana_gas_FirstHitTime
  (`libraries/connectors/examples/Geant4ToRawVeto.yaml`). The drift of the first electrons and the delay of
  the veto electronics go in triggerDelay, measured in data. Non-finite values: event rejected.
- TRestDataBase: postponed until there is a real database design.

### Geant4 analysis (2026-11-06, implemented)

- TRestGeant4VetoAnalysisProcess and TRestGeant4NeutronTaggingProcess are not migrated; their observables, in
  a generic way, in the selections of TRestGeant4AnalysisProcess:
  - `perVolume: true`: <name>_<volume>_Energy of each volume of the selection (geometry name without
    forbidden characters; error if two coincide) and <name>_MaxVolumeEnergy (threshold per veto panel). Groups
    and layers (legacy: from the volume name) are selections with their expression.
  - `interactions: true`: the selected hits with or without energy (interaction points; for discrete
    processes, e.g. `particles: ["neutron"]`, `processes: ["nCapture"]`): <name>_NInteractions and vectors
    <name>_InteractionX/Y/Z, _InteractionTime, _InteractionKineticEnergy and _InteractionSecondariesEnergy
    (initial kinetic energy of the secondaries created by that process, assigned to the closest interaction
    of the track), _InteractionTrackEdep and _InteractionTrackMaxVolumeEdep (energy deposited by the track of
    the interaction and all its descendants in `trackEdepVolumes`, expressions, all if not given; in total and
    in the volume with most). For a neutron captured with the vetoes: the energy in the vetoes of the neutron,
    its recoils before the capture and the capture products, i.e. whether the veto tags it (legacy
    neutronsCapturedEDepByNeutronAndChildrenInVeto and ...Max). The only vector observables of the process.
  - Secondary neutrons leaving the shielding: selection with `particles: ["neutron"]`,
    `processes: ["Transportation"]`, the shielding volumes and `interactions`.
  - Quenching with the fixed factors of NeutronTagging: TRestGeant4QuenchingProcess (Birks) before.
  - Not migrated: the initial energy of the track of the captured neutron, the energy of the neutron alone
    (EDepByNeutron), the minimum per veto and the list of capture gammas (their total energy is
    _InteractionSecondariesEnergy).
- TRestGeant4BlobAnalysisProcess not migrated (double beta specific; TRestTrackBlobAnalysisProcess on the
  reconstructed tracks, the MC ends of the primary electrons with `entering`).

### Point-like events (2026-11-06, implemented)

- TRestTrackPointLikeAnalysisProcess not migrated. TRestTrackAnalysisProcess adds to the track observables
  Size and MaxNodeSize: sum and maximum of the node sizes, sqrt(σx² + σy² + σz²) of the coordinates each node
  defines (σ from TRestTrackReductionProcess; 0 without reduction), as MaxTrackSize, TracksSize,
  MaxTrack_P_Size... Legacy: size → MaxTrackSize, sizeBalance → MaxTrackMaxNodeSize / MaxTrackSize,
  clusterEnergyBalance → 1 − MaxTrackEnergyRatio, nTotalHits → MaxTrackNHits + Sum(TracksNHits);
  clusterLengthBalance and hitsBalance (longest track and track with most hits, not the main one) as
  expressions with Max/Sum of MaxTrackLength/TracksLength and NHits.

### Channel activity (2026-11-06, implemented)

- TRestDetectorSignalChannelActivityProcess not migrated: its histograms are plots with TRestAnalysisPlot. The
  translation of the DAQ id to the readout channel goes in TRestRawToDetectorSignalProcess (the raw library does
  not depend on detector): vector observables SignalsPhysicalID (with TRestDetectorReadoutManager, field
  `readout`), SignalsID (DAQ channel) and SignalsMaxAmplitude, one per output signal (aligned; SignalsID of
  TRestRawSignalAnalysisProcess is not, because RawToDetectorSignal drops the signals without points).
- `libraries/connectors/examples/ChannelActivity.yaml` (plots only, `--i`): activity per DAQ and readout
  channel, and maximum amplitude. Without the legacy histograms per threshold (25, 50) and multiplicity (1, 2,
  3, 4-9): the zero suppression already sets the threshold; they can be added with TRestCut if needed.

### Baseline and veto (2026-11-06, implemented)

- TRestRawBaseLineCorrectionProcess: subtracts from each signal the moving average over `smoothingWindow` bins
  (75, the legacy value; odd), replacing the points beyond 3 sigma by the baseline (IQR over the whole signal
  except 5 bins per side), as legacy `GetBaseLineCorrected` (GetSignalSmoothed_ExcludeOutliers of
  TRestPulseShapeAnalysis). Only `smoothingWindow`: corrects all signals; no `channelType` nor `signalsRange`
  (the channels are selected before with TRestRawSignalChannelSelectionProcess).
- TRestRawSignalAnalysisProcess: `signalsRange` removed (same reason).
- TRestRawSignalRemoveChannelsProcess renamed TRestRawSignalChannelSelectionProcess: `removeChannelIds` and
  `removeChannelRange` (formerly `channelIds` and `channelRange`), and new `keepChannelIds` and
  `keepChannelRange` (only those are kept). A channel in keep and in remove is removed (e.g. the veto range
  minus a dead channel).
- TRestRawVetoAnalysisProcess not migrated: the veto is analysed in a separate run,
  `libraries/raw/examples/VetoAnalysis.yaml`: keepVeto (`keepChannelRange`) → baseLineCorrection (optional) →
  TRestRawSignalAnalysisProcess (SignalsID, AmpSgnMaxMethod = MaxPeakAmplitude, PeakTime, PointsOverThr). The
  detector in another one removing the veto (`removeChannelRange`; legacy removed it from the event); the
  trees are combined by event ID. The flags (VetoAboveThreshold, VetoInTimeWindow, their N) and the groups are
  RDF expressions (Any/Sum, masks by SignalsID).
- TRestRawBiPoAnalysisProcess not migrated: it only copied the auxiliary of the BiPo decoder (also N).

### Case-insensitive options (2026-11-06, implemented)

- `TRestTools::NormalizeOption(field, value, options)` in LoadConfig of the text fields with fixed values:
  returns the form of the list (e.g. "ROBUST" → "robust", "triplemaxaverage" → "tripleMaxAverage") and throws
  if it is not there; the YAML stored in TRestRun has it. Applied to baselineOption, maxOption
  (RawSignalAnalysis, RecoverSaturation, RawToDetectorSignal), shapingType (raw and detector shaping), method
  (AddNoise, SignalToHits), driftAxis, fluctuations, mode and triggerMode (SignalToRawSignal), state
  (TRestDetectorMedium), lineAnaMethod and projections (HitsAnalysis, TrackAnalysis, TrackBlobAnalysis,
  TrackLineAnalysis), peakModel and background (DataSetCalibration, DataSetGainMap), model (quenching) and type
  (biasing). The restG4 generator types were already lowercased. Only YAML fields: not function arguments nor
  names (files, volumes, particles).

### restDAQ: migration of FEMDAQ (2026-11-07)

FEMDAQ (`/home/juanan/FEMDAQ`, formerly femclient) moves to `packages/restDAQ`, with the executable
`femclient` (same name as in FEMDAQ) and the CMake option `REST_DAQ` (OFF by default).

- Electronics: all (ARC, FEMINOS, DCC and TCM). Decoders (ARCPacket, FEMINOSPacket, DCCPacket), readline
  console with history, log files per FEM, commands (`fopen`, `fclose`, `startDAQ`, `stopDAQ`, `credits`,
  `Pedestals`, `verbose`, `fem`, `exec`, `sleep`, `addMetadata`) unchanged.
- Dependencies: no CLI11 (arguments parsed by hand, as restManager). Prometheus is kept:
  `find_package(prometheus-cpp)`, and FetchContent only if not found. Compatible with prometheus-cpp 1.0
  (Debian package `prometheus-cpp-dev`, without `prometheus::Info`): run_metadata_info and run_filename_info
  are gauges = 1 with labels, the same series the Info of FEMDAQ exported.
- Configuration in restv2 style, registered fields: a TRestRun (mainDataPath, outputFileName with
  [subRunNumber], experimentName, runTag, runType, runInfo) and a TRestRawDAQMetadata (electronics, fems with
  id, IP and FECs, TCM, nEvents, time, maxFileSize, updateRate, prometheusAddress). Both are stored as metadata
  in each file (replacing the TObjString RunConfigYAML, startTime and endTime).
- Run number: `runNumber: auto` of restv2 (`$REST_HOME/.rest/runNumber`), a new one at each `fopen`.
  `maxFileSize`: when exceeded the next subrun is opened.
- `addMetadata`: asks for runTag, runType and each runInfo key of the TRestRun and rewrites the YAML.
- Data: REST files with the `TRestRawSignalEvent` tree, written with the derived class TRestRawDAQSignalEvent
  (libraries/raw), which adds per FEM (in the order of its signals) the id, the event counter, the timestamp
  and the number of signals. Reading creates a normal TRestRawSignalEvent, which ignores those branches:
  pipeline, browser and readout unchanged. Resynchronization, if needed, will be an offline process checking
  that the branches exist (no online resynchronization, it would be overhead in the data taking).
- TRestRawFemDAQToSignalProcess (old FEMDAQ files) is kept unchanged, without reading femID, femEventCount nor
  femTimestamp (2026-11-08): they were only in the last FEMDAQ files, without the number of signals of each
  FEM, and would only serve to resynchronize; new data come from restDAQ.
- Macros: EventDisplay.C is migrated as the class TRestDAQViewer of libraries/connectors (restv2 macros are
  compiled without dictionary and the GUI needs it for signals and slots), launched with the macro
  REST_DAQViewer(config, file) in restRoot (2026-11-08; before, TRestRawDAQViewer in raw with its own .dec):
  - Configuration: a restv2 YAML (e.g. the analysis one); it takes, at any depth, the
    TRestDetectorReadoutManager (restv2 readouts and decodings; the femclient IDs, channel + 72·chip +
    288·card, are those of TRestRawMultiFEMINOSToSignalProcess) and the TRestRawSignalAnalysisProcess
    (baselineRange, integralRange, thresholds, maxOption). No parameter or decoding buttons.
  - Each readout separately: spectrum (sum of amplitudes), hitmap and signals of the event; plus the rate.
  - Spectrum binning (2026-11-08, user): in the `parameters` section of the YAML, spectrumBins, spectrumMin,
    spectrumMax (default 2048, 0, 100000) and with suffix _<instanceName> for one readout; an unused parameter
    is warned. No "Spectra Max Range" box in the window (nor in the settings).
  - Live reading (2026-11-08): TFile + TTree::Refresh instead of the TChain of EventDisplay.C (the TChain fixes
    its entries when opened and did not see the new events of the file being written); no read cache.
    femclient still does AutoSave every 1000 events or 60 s (user decision).
  - Automatic hitmap (user decision, 2026-11-08, e.g. Micromegas + vetoes as two readouts): readouts with
    strips along two axes (GetPositionFromChannel with a single finite coordinate; not necessarily X and Y):
    amplitude-weighted mean of each axis, bins centered on the strips. The others (TRestGDMLReadout vetoes,
    pixels): XY, XZ and YZ projections of the channel volumes (TH2Poly, one bin per volume with its projected
    box, content = events with signal in the channel as in the spectrum; volumes overlapping in a projection
    are drawn on top of each other). For that ViewItem stores the physicalID of its volume.
  - Amplitude: the AmpSgnMaxMethod of TRestRawSignalAnalysisProcess with the functions of
    TRestPulseShapeAnalysis (CalculateBaselineAndSigma, GetPointsOverThreshold, GetMax), without changing the
    process.
  - DrawPulses.C (2026-11-08, user: own viewer): TRestRawPulsesViewer (raw) and the macro
    REST_Raw_PulsesViewer(file): the signals of each event in one pad per ASIC with signals (ASIC = ID/72,
    title card and ASIC, color per channel), open file, previous/next, jump to an entry, save PDF; follows a
    file being written (TTree::Refresh). AnalyzePulses.C and FEMDaqRoot not migrated (covered by
    TRestRawSignalAnalysisProcess and restRoot).
- DAQ messages: still on the console and in the log of each FEM (not TRestLogManager); level with `verbose` of
  the TRestRawDAQMetadata or the `verbose` command.
- Code review (2026-11-08, all fixed): signal handling is the one of FEMDAQ (unchanged); CTRL-C during a
  command that throws (addMetadata) still terminates the program. CTRL-C in the interactive console (leaves
  the readline loop) or a script without stopDAQ destroyed the FEMDAQ with the std::threads still joinable
  (std::terminate, file not closed; with `-e` and stopDAQ it did not happen): startDAQ ends with stopDAQ,
  startDAQ stops a previous run and the destructors stop the threads; `credits restore <frameThr> <reqFrames>`
  passed the arguments swapped (and 0 < frameThr <= reqFrames is required); a relative mainDataPath was
  prepended twice; DCC: infinite loop with size 0 packets and reads outside the event; exceptions in the
  threads or in a command no longer terminate the program; logs protected by a mutex (the receivers write
  while fopen/fclose); incomplete event of the previous run; stopDAQ sets stopRun; DCC/TCM timeouts of ~1000 s
  instead of 10 s (common FEMSocket::Receive); ASCII characters of ARC; warning if a FEM does not send its
  event in 10 s; datagrams < 4 bytes; DCC overflows; closed sockets; Prometheus series that accumulated; debug
  prints (timestamp, deque, histogram bins); dead code (GetPedestalEvent, requestInterrupt, FEMSocket::Clear,
  GetUInt32FromBufferBE); format warnings. Non-data frames still count as the answer to a command (cmd_rcv;
  some commands answer with monitoring frames, e.g. pedestal histograms), but each command waits for a frame
  arrived after sending it (cmd_rcv above its previous value, 2026-11-08): before, cmd_rcv above cmd_sent
  (multi-frame answers, frames during the run) stopped waiting forever.
- Simplification (2026-11-08, user decisions): generic functions to TRestTools
  (TRestTools::GetTimeStampFromUnixTime instead of the copy with std::localtime, not thread-safe; messages come
  out with a space and the log names without file time keep '_'; FormatElapsedTime →
  TRestTools::ToTimeStringLong, which now gives weeks from 7 days on; getCurrentTime →
  TRestTools::GetCurrentTime); FEMArray is a std::deque (FEMProxy and FEMSocket are neither copied nor moved,
  no hand-written moves); FEMSocket::Send instead of the copied sendto calls; FEMDAQ::Create with an if
  instead of the static Registrar object registry; the command logs through WriteLog or with logMutex
  (before, without the mutex in ARC, DCC, TCM and CreditsShow). The manual mutex_mem.lock()/unlock() are left
  as they are.
- Verbose (2026-11-08, user): the restv2 one. RunConfig::Verbosity is TRestLogManager::REST_Verbose_Level,
  initialized with the `verbose` of the TRestRawDAQMetadata; the `verbose` command accepts silent, error,
  warning, info or debug (an unknown one does not change the level).
- addMetadata (2026-11-08, user): only writes the changed fields, in the text of the file
  (TRestYAMLUtils::SetValuesInFile: keeps comments, order and format; a plain value if it reads back the same,
  otherwise in double quotes; a missing key is added at the beginning of its map). Before, it rewrote the
  whole YAML with yaml-cpp, losing the comments.
- User files in .rest (2026-11-08, user): TRestTools::GetRESTUserDirectory (`$REST_HOME/.rest`, or
  `$HOME/.rest` without REST_HOME; also used by DownloadFile): femclient history `femclient_history` (formerly
  `~/.femdaq_history`) and viewer settings `DAQViewer_settings` (formerly `~/.restdaq_viewer`).
- Not compiled (2026-11-07): validation by the maintainer.

### Doxygen documentation and README (2026-11-08)

- Done (user decisions): Doxygen comments of the classes and the public/protected API of the whole repository
  (source, libraries, packages; comments only, checked that the code does not change), README.md in each
  library (source/tools, core, analysis, macros, bin; libraries/*) and package (restGarfield new, restDAQ and
  restG4 reviewed: restG4 no longer talks about .rml). Doxygen: `doc/Doxyfile.in` (reads .h, .cxx, .C and .md;
  the YAML examples of many processes are in the \class of the .cxx), main page `doc/mainpage.md`, optional
  target `doc` (find_package(Doxygen), dot optional).
- Limitation: the trailing ROOT `//!` comments (transient members) are Qt comments for Doxygen and document
  the next member (e.g. in TRestBrowser.h); they are not changed because they mark the streamer.
- Bugs found while documenting, fixed (2026-11-08, user decisions): restDAQ (limits in
  TryExtractNextEvent/ParseEventFromWords of ARC/FEMINOS, end of run of the DCC event builder, Prometheus
  label `metadata`); geant4/restG4 (copy of TRestGeant4Metadata without Merge, SaveMetadata, declarations
  without definition, GetProcessID, invalid physics lists and CosmicGammas give an error,
  TRestGeant4EventViewer, GeometryInfo without exit, quenching: only in the given volumes, mandatory model per
  volume (no `auto` nor fixed names), `model` of the process for all the others; lindhard without
  storeHadronicTargetInfo gives an error, help without RML, restCRY with options in any order; applied
  `chance`, `activateAllVolumes` and `registerEmptyTracks` = false removes the tracks with only their initial
  hit outside the stored volumes); core (TRestHits: Sort descending by default, Add/Remove only in the last
  view; TRestBrowser owns viewer and run, no exit; DeleteCurrentEvent does not delete; GetEntry with only
  AnalysisTree; REST_VERSION for [version]); tools/analysis (TRestFFTAnalysis according to its names, hBar,
  GetRunNumberAuto with GetRESTUserDirectory, GetSinNoise with double period); track ("X" = XZ, "Y" = YZ,
  "XYZ" = 3D; "" = XYZ in GetMaxEnergyTrack, all in GetNumberOfTracks/GetEnergy); raw/detector/connectors
  (shaping lets empty events through, decimal sizes, `//!` pointers in TRestDetectorSignalEvent, macros,
  example paths from the root, Reload Run of the viewer); messages in English.
- biasingVolumes (user decisions 2026-11-08, implemented; volumes from outside to inside in the YAML, integer
  factor >= 1, nEvents mandatory; results in biasingTotalFactor/biasingStageEvents/biasingStageParticles;
  GetEquivalentNumberOfEvents; example 09.Pb210_Shield/Biasing.yaml and test restG4Examples.Biasing): chained
  simulations in a single restG4 run, from outside to inside; at each stage the particles entering the next
  volume are stopped (stored if in energyRange) and the next stage simulates them again biasingFactor times
  with their exact state; only the events of the last stage are written; equal weight for all (total factor
  in the metadata, equivalent time/events corrected). Not even legacy restG4 2.4 applied it.
- registerEmptyTracks: false (user, 2026-11-08): empty track = only its initial hit, outside the stored
  volumes; they are removed and their daughters keep their parent ID.

### `include` key in the YAML (2026-11-08, user decisions)

- Done: a map with `include: "file.yaml"` takes the content of that file (the section directly) or
  `include: "file.yaml:key"` a section of a file with several; local keys override (maps deep-merged, lists
  and scalars replaced); paths relative to the including file; nestable, cycles detected.
  TRestYAMLUtils::LoadConfigFile (used by OpenConfigFile and by TRestMetadata(file, section)); `parameters`
  only from the main file, resolved afterwards. SetValuesInFile adds the missing parent keys (addMetadata with
  an included TRestRun). Not standard YAML: a key convention as GitLab CI / Docker Compose (the maintainer
  chose it over an `!include` tag).
- Example: `packages/restG4/examples/04.MuonScan/physicsLists.yaml`, included by CosmicMuons.yaml,
  CosmicMuonsFromWall.yaml and CosmicMuonsFromCircle.yaml (their physicsG4 were identical; the tests copy the
  whole folder).

### restG4 tests (2026-11-08, validated by the maintainer with ctest)

- Fixed: Decay0 reader with the old format (Xe136bb0n.dat) besides bxdecay0; mono energy 0 (ion at rest);
  TH1F/TH2F histograms accepted (converted with their axis titles, which give the unit); energy formulas
  compiled (no cling) and own sampling of formula2 (TF2::GetRandom2 integrated 2.5 million cells);
  TRestRun(file) takes the parameters of its run (before, runNumber -1); XML comments with `--` (Alphas,
  TREXDM, DetectorResponse) and attributes with spaces around `=` (PandaXIII) in the GDML; IonRecoils with
  NeonGas (Neon_ISO of the remote rest.xml does not exist); Alphas with runNumber auto (the dataset of
  plots.yaml needs distinct runs).
- Test references (user): the restv2 ones when the material changes (legacy Ar_ISO → Argon1%Isobutane1bar:
  MuonsFromPoint, CosmicMuons, Xrays); IAXO with the TGeo counts (97 physical with unique name, 46 logical
  with assemblies); FullChainDecay checks the main chain (16 to 18 isotopes with the rare branches);
  IonRecoils up to 30 keV (the beta decay of the primary F20 goes in the same event, as in legacy).
- PandaXIII: activeVolumes `^Gas$` (the expressions are searched inside the name: "Gas" also took
  OtherXeTMAGas and GasInbetween). Alphas: plots.yaml only with the 5 um foil (user: a dataset requires the
  same geometry in all its runs, and the thickness is a GDML parameter).

### Micromegas with the normal along any axis (2026-11-08, user)

- TRestMicromegasReadout: `normal` along X, Y or Z (before only ±Z); the X and Y strips measure the other two
  axes in cyclic order (normal Z: X, Y; normal X: Y, Z; normal Y: Z, X); positionRelative in global
  coordinates (its component along the normal is the plane); globalRotation around the normal. The geometry
  is built in the local frame and placed with the axis permutation; GetPositionFromChannel,
  GetDistanceToReadout and GetChannelsBoundingBox in those axes. TRestDetectorSignalToHitsProcess takes the
  drift axis from the normal of the readout (driftAxis only for readouts without distance). No changes in
  HitsToSignal/HitsDrift (they use the generic distance and normal) nor in the DAQ viewer (hitmap of any pair
  of axes); GainMap takes the two axes of the plane (the normal is the axis of smallest extent of the channel
  box). Tilted normal discarded (it would need local coordinates in the API).

### TRestDataSetTMVA (2026-11-09, implemented)

- Origin: framework PR #448 (TRestDataSetTMVA to train and TRestDataSetTMVAClassification to classify, never
  merged). TMVA is kept (no new dependencies; XGBoost + RBDT discarded to keep training inside restManager).
- A single class, TRestDataSetTMVA (RestAnalysis), a dataset process like TRestDataSetLogOdds:
  ComputeParameters trains and Apply adds `<name>_score`. Applying a trained model = referenceFileName to an
  already processed dataset (weights stored in its configuration).
- Signal = the common reference (referenceFileName, cuts); background = `backgroundFileName` (a single
  dataset) and `backgroundCuts`. TRestDataSetProcess gains GetCutExpression (and fManager) for the background
  cuts.
- One method per process (`method`, BDT by default, and `options`); `splitOptions` for the training/test
  split. To compare methods, several processes.
- Generic results: weights (TMVA XML), rocIntegral, separation, ksSignal/ksBackground (training vs test),
  variableSeparation (per observable, independent of the method); BDT only: variableImportance (TMVA::Ranking
  gives no access to the values of other methods). Objects: roc, score_signal/score_background (test) and
  *_train. No TMVA.root nor weights/ (in-memory file and temporary directory).
- Visualization: TRestDataSetTMVA::DrawResults (ROC; test scores filled and training ones as points,
  normalized, with the KS; separation and importance bars per observable) and the macro
  `REST_ViewTMVA(dataset, name, pdf)` in source/macros (ReadFromFile + DrawResults).
- High-level analysis examples (DataSetLogOdds.yaml, DataSetTMVA.yaml) in `examples/` at the root: they do not
  belong to any library; DataSetCalibration.yaml stays in detector (uses TRestDataSetGainMap).

### Fibre readout and scintillation (2026-11-09, implemented, user decisions)

- Origin: the macro `Geant4ToSignal.C` of the LiquidO simulations (losimulations), which emulated the light
  (photoelectrons per keV, Gaussian light ball, closest fibre), the electronics and the tracking in a single
  macro. Only the light and the readout are new: the rest is the existing chain (TRestGeant4QuenchingProcess,
  TRestGeant4ToDetectorHitsProcess, TRestDetectorHitsToSignalProcess, TRestDetectorSignalShapingProcess,
  TRestDetectorSignalToRawSignalProcess, TRestRawSignalAddNoiseProcess, TRestRawToDetectorSignalProcess,
  TRestDetectorSignalToHitsProcess, TRestDetectorHitsToTrackProcess).
- TRestFibreReadout: a parametric readout (not fibres in the GDML read with TRestGDMLReadout): a box and a
  list of fibre families (`fibres`: axis, pitch, offset), so any lattice (two or three directions, different
  pitches) is described. Channel = the closest fibre (computed, not a geometry search:
  TRestDetectorReadout::GetChannelFromPosition is virtual); a fibre gives its two transverse coordinates
  (XZ / YZ hits as strips). Without decodingFile the DAQ ID is the physical ID.
- The light in two processes (user, revised the same day: a single process with the light ball was not
  generic, it is a particular case of opaque scintillators):
  - TRestDetectorScintillationProcess: only the primary scintillation, photons = Poisson(E * lightYield of
    the `light` of the medium) per hit, same position and time (no decay time yet).
  - TRestDetectorLightCollectionProcess with a `model` per kind of scintillator (user): `localCapture`
    (opaque: the channels within maxDistance, TRestDetectorReadout::GetChannelsInRange, capture the photons
    with captureProbability(r), a formula or a graph from an optical simulation of Geant4; multinomial, a
    photon goes to one channel at most) and `attenuation` (transparent: Binomial(N, exp(-d/λ)) to the readout
    of the channel, t + d/v, light response of the medium). P(r) moved from the fibre families to the process:
    the readout keeps only the geometry (as agreed for the readouts).
  - The quantumEfficiency (binomial, moved from the readout) is a parameter of
    TRestDetectorLightCollectionProcess, which gives photoelectrons (user: a separate photosensor process
    was rejected, also its name).
  - TRestDetectorLightAttenuationProcess is removed (user): the vetoes use scintillation → lightCollection
    (attenuation, quantumEfficiency of the PMT), with signals in photoelectrons (Geant4ToRawVeto.yaml, gain in ADC per
    photoelectron).
  No attenuation nor delay along the fibres for now (user).
- Fibres in Geant4 (user: they are dead material): the placements are generated from the same readout
  parameters (TRestFibreReadout::WriteGDMLPlacements, macro REST_Detector_GenerateFibreReadout) and included
  as an external entity inside the scintillator volume; the solid and material of the fibres are written by
  hand in the GDML (logical volume `volume` of each family). Rejected: GDML divisions (shared names), C++ in
  restG4 (dependency on the detector library), homogenized material (no event by event effect). ROOT reads
  neither GDML `replicavol` nor `loop`.
- TRestDetectorHitsToSignalProcess and TRestDetectorSignalToHitsProcess accept a medium without `charge`
  (a scintillator with only `light`): no drift velocity.

### TRestDataSetCombine (2026-11-09, implemented, user decisions)

- Combination of two datasets of the same events processed apart (IAXO: vetoes and Micromegas), not the
  existing merge of datasets (concatenation of runs). A dataset process (user), in source/analysis; name
  chosen by the user (TRestDataSetJoin rejected).
- Matching by run (runNumber, subRunNumber; the runIndex of each dataset can differ), eventID and
  subEventID. The dataset processed is the base: entries only in combineFileName are dropped (user); entries
  without match are kept with default values (NaN, 0, empty) and `<name>_matched` 0, or filtered with
  keepUnmatched: false (user: configurable).
- Columns: `observables` patterns, `prefix`; an existing column or an event twice is an error. The columns
  of combineFileName are read in memory in one event loop.

### Conditional blocks in the YAML (2026-11-09, implemented, user decisions)

- Legacy RML `<if>` (e.g. the generators of the LiquidO simulations) becomes a `select` block: `select:
  value` (a parameter) and `cases` (name: content, optional `default`); the other keys of the block are
  common and the case is merged over them (TRestYAMLUtils::ResolveRefs, after the parameters, so `--set`
  changes the case). Rejected: if/then/else (several cases need nesting) and cases as siblings of `select`
  (no common keys, names colliding with real keys). Only the case chosen is stored in the metadata (user).
- The parameters of the cases not chosen are resolved too (they must be defined and count as used).

### TRestDataSetSignalSelection (2026-11-09, implemented)

- High-level analysis on LogOdds or TMVA: an efficiency is fixed on the calibration (the reference) and
  applied to the background. Generic: any observable (logOdds_total, bdt_score...), one observable per process
  (several selections: several chained processes).
- The processed dataset is filtered (no decision column); the events passing are in selectedEntries.
- The direction of the cut is given with `quantiles: [lower, upper]` of the reference (not "keep",
  "signalLike" nor "condition"): [0, 0.8] the low values (log odds), [0.2, 1] the high ones (BDT), [0.1, 0.9]
  central. Results `range` (-.inf/.inf without limit) and `efficiency` (actual fraction of the reference in
  the range). Example: `examples/DataSetSignalSelection.yaml`.

### Tests and validation (2026-11-09, implemented)

- Tests per component: `test/` in source/tools, core, analysis and in each library, GoogleTest executable
  `<component>Tests` with `rest_add_tests` (cmake/RestTests.cmake), label = component; `ctest` runs
  everything, `ctest -L <component>` one, `make check` builds and runs everything. Unit tests first (inputs
  in memory, no external data).
- Pending (P): pipeline tests (YAML of examples/ and of the libraries with restManager) once it is decided
  where to publish the input data outside the repository.
- CI (GitHub Actions, local until the maintainer creates the new repository outside rest-for-physics):
  `.github/workflows/validation.yml` (PR to master, push to master, releases, manual; jobs Build, Unit tests,
  restG4 examples, Format), `docs.yml` (Doxygen on GitHub Pages), PR template, CODEOWNERS, dependabot,
  pr-badge.yml, and the protection of master as an importable ruleset (`.github/rulesets/master.json`, only
  PRs with green checks). See CONTRIBUTING.md.
- CI Docker image in a separate repository, `/home/juanan/restv2-docker` (Ubuntu 24.04, ROOT 6.36.04 binary,
  Geant4 11.0.3, Garfield++ 4.0, CRY 1.7, prometheus-cpp), published by its workflow in
  `ghcr.io/<owner>/rest-env`; built and pushed by the maintainer.
- Unit tests (2026-11-09, all passing with ctest, validated by the maintainer): tools 44, core 36,
  analysis 66, raw 32, connectors 16, detector 25, track 12, geant4 11 (with the regression tests of the fixes
  below), plus the 22 restG4 examples; the 9 FFTAnalysis tests are skipped without FFTW in ROOT.
  Not covered: TRestDataSetProcess (abstract), TRestEventProcessManager, viewers, TRestGeant4AnalysisProcess
  (needs an input file), TRestDetectorAvalancheProcess (gain tables), TRestGDMLReadout (GDML file),
  TRestDataSetGainMap, restGarfield and restDAQ.
- Problems found while writing them, fixed (2026-11-09), with regression tests:
  - Event copy: TRestEvent copied the TPad it owns (double delete) and the branch pointer; the events with
    views (TRestDetectorHitsEvent, TRestTrackEvent, TRestRawSignalEvent, TRestDetectorSignalEvent) left the
    views of the copy on the storage of the original. Explicit copy constructor and assignment rebuilding the
    views.
  - Undefined coordinates: TRestHitsData::push_back stores NaN in the coordinates the hit type does not define
    (used by TRestHits::AddHit, TRestDetectorHitsEvent::AddHit, TRestTrackEvent::AddHitToTrack); before, the
    processes copying hits with GetPosition (projected, 0) stored 0.
  - TRestDataSet runConditions: a literal with units (`runInfo.meshVoltage > 300 V`) is converted to REST
    units, as the run parameters (before, the expression did not compile).
  - TRestDataSet runConditions gave a segmentation violation: RInterfaceBase::HasColumn of ROOT 6.36 fails on
    an RDataFrame without data source (the one of the runs, RDataFrame(n)); DefineRunParameters uses its set of
    columns instead of HasColumn.
  - TRestDetectorMedium: the check "the medium needs a name" never failed (TRestMetadata writes the instance
    name in the node); removed: the default name is the section one, as for all metadata.
- Reviewed, no change:
  - The TRestRunInfo constructors do not call LoadConfig on purpose (base of TRestRun, which reads its node
    with keys TRestRunInfo does not know); documented.
  - TRestDataSetCalibration throws std::invalid_argument with unknown peakModel/background (the rest,
    std::runtime_error).
  - TRestGeant4QuenchingProcess and TRestGeant4AnalysisProcess only read their metadata from the input file.
  - TRestHitsAnalysis::GetTwist and GetTwistWeighted measure the angle differently (as legacy).

### Shared resources by URL (2026-11-11, implemented, user decisions)

- Organization: one repository per project in rest-for-physics-toolkit (alphacamm, liquido...), grouped by a
  topic and the profile of the organization, and three repositories for the shared resources (topic
  `rest-common`), each one with its own versions, since they are generated and change differently:
  `rest-common-materials` (materials.xml of Geant4; CI with Python only), `rest-common-transport`
  (restGarfield YAML; its transport tables are computed locally) and `rest-common-readouts` (YAML and
  decoding per project; CI building the framework). They were a single `rest-common` until 2026-11-12.
  Only the sources are committed: on a tag, the CI generates the readout files (with the
  overlaps checked: the generation macros return 1 if TRestDetectorReadout::CheckOverlaps finds any, the
  exit code of `restRoot -b -q -e 'gSystem->Exit(<macro>(...));'`; a bare call is not run by restRoot)
  and `materials.xml` (merged from `definitions/`, as rest-for-physics/materials), and attaches them to the
  release; the transport tables (too slow for a CI runner) are computed locally (`restGarfield --jobs`) and
  attached by hand. A medium file can hold several
  media (`tables/<medium>/<key>`), e.g. `ArIso.root` with `Ar99_iC4H10_1`, `Ar98_iC4H10_2`...
- One readout file per project (e.g. `TREXDM.root` with its two planes): `REST_Detector_GenerateReadouts`
  builds every readout section of a YAML and exports them all to the same file (`Readouts/<name>/`); the
  readout manager has one entry per readout with the same `inputFileName`. A readout stores several
  decodings (`decodings`: name and decoding file, besides `decodingFile`), and a decoding can be added later
  to a readout file without rewriting its geometry (TRestDetectorReadout::AddDecoding, macro
  REST_Detector_AddDecoding); the manager chooses one with `decodingName`.
- The projects read them with versioned URLs (a tag), not GitHub Pages (only the latest version):
  `https://github.com/rest-for-physics-toolkit/rest-common-materials/releases/download/<tag>/materials.xml`.
- Read from a URL (TRestTools::GetLocalFile: downloaded once with curl and cached in
  `$REST_HOME/.rest/download`, so it works without network once cached, e.g. in the nodes of a cluster):
  the GDML entities (before), the YAML `include` (relative includes of a URL are relative to it) and the
  main YAML file, `mediumFile` of TRestDetectorMedium (read only: restGarfield needs a local file) and
  `inputFileName` of the readouts of TRestDetectorReadoutManager. The `gdmlFilename` of restG4 stays local
  (the projects keep their setup GDML, its entities point to rest-common-*).

### TREX-DM gas (2026-11-13, user)

- Neon with 1% isobutane at 10 bar is the nominal TREX-DM gas configuration, not the only mixture the
  experiment may use. It is not pending confirmation.

### Dataset event time differences (2026-11-13, user)

- `TRestDataSetTardis` (named as the other dataset processes, without `Process`) adds `<name>_timeDelay`,
  the time in seconds since the previous entry in chronological order across the runs, NaN for the first.
- The dataset is not reordered (user choice): the timestamps are read in memory and the delays matched to
  the entries by runIndex, eventID and subEventID, as TRestDataSetCombine. TRestDataSet is not changed (a
  first version wrote the dataset ordered with temporary files and a reordering in TRestDataSet::Write).
- `cuts` filter the dataset; the delays are those between the remaining entries. A reference is not used
  (warning). No results stored (user choice).
- Dataset generation keeps `timeNanoSecs` when present, regardless of `observables`. The process requires
  both timestamp components; datasets that omitted nanoseconds must be regenerated. Later filters do not
  recompute the delays.
- Options added later (user, 2026-11-13): `channelObservable`/`channelThreshold` (only the channels over a
  threshold, a value per channel; replaces a mask written in `channels`) and `previousObservable`/
  `previousThreshold` (only the events over a threshold, e.g. the energy, can be the previous event).
- Option `channels` (user, 2026-11-13): the previous event in the same place. The physical channels of each
  event (a vector column, e.g. rawToSignal_SignalsPhysicalID; not the hits, not the DAQ channels) and the
  readout of the dataset, which gives the neighbours (GetAdjacentChannels, `channelTolerance` 1) and the
  groups (finite coordinates of GetPositionFromChannel: X/Y strips, fibre families), so no columns per
  axis. Same place: a group in common and near channels in every group both events have. The class moved
  to libraries/detector (it needs the readout, as TRestDataSetGainMap); GetDAQID moved from
  TRestFibreReadout to TRestDetectorReadout.

### Gaussian and landau fits of the pulse maximum (2026-11-13, user)

- `gaussFit` and `landauFit` (TRestPulseShapeAnalysis::GetMaxGauss, GetMaxLandau; signalToHits, raw analysis)
  failed on TREX-DM data (Minuit2 "Initial matrix not pos.def.", fits escaping the pulse): the legacy window
  [0.9, 1.2] times the time of the maximum depends on where the pulse is, and the initial values were the
  automatic ones of ROOT. Now the fit covers the top of the pulse (the points over half the maximum and one
  more on each side), starts from the maximum and the width at half maximum, and the position is limited to
  the window. A failed fit (or less than 4 points) gives the maximum point instead of (-1, -1), so the charge
  of the signal is not lost.

### Readouts stored in a file (2026-11-13, user)

- With a data readout (`readout` of TRestDetectorReadoutManager), only that readout is written to the output
  file (and listed in the stored `readouts`); the others were loaded for the processing only. A dataset of
  the file keeps that readout, e.g. one plane of TREX-DM (examples/TREXDMToTracks.yaml loads both planes,
  `readout` selects one). Without `readout`, all the readouts are stored, as before. The analyses use that
  readout, not a parameter; the plane is also in runInfo (`plane`), only for the names of the datasets.
