#ifndef TRESTGEANT4METADATA_H
#define TRESTGEANT4METADATA_H

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

// ROOT MathCore GenVector headers
#include <Math/Vector3D.h>

#include "TRestGeant4BiasingVolume.h"
#include "TRestGeant4GeometryInfo.h"
#include "TRestGeant4PhysicsInfo.h"
#include "TRestGeant4PrimaryGeneratorInfo.h"
#include "TRestMetadata.h"

// Forward declarations for framework friend classes

class SteppingAction;
class DetectorConstruction;

/// \class TRestGeant4ActiveVolumeMetadata
/// \brief An entry of `activeVolumes`: the hits of the active volumes are stored. Since only active
/// volumes can be sensitive, the sensitive ones are flagged here (there is no `sensitiveVolumes` list):
///
///     activeVolumes:
///       - name: "gas"          # regular expression of physical (or logical) volume names
///         chance: 1.0          # probability to store the hits of an event in the volume
///         maxStep: 10 um       # Geant4 maximum step in the volume (0: no limit)
///         sensitive: true      # an event is stored only with energy in a sensitive volume
///
/// `chance` (0 to 1, default 1): restG4 draws, for each Geant4 event, whether the hits in the volume are
/// stored (the same for all its sub-events); when not, the volume is as a non active one for that event
/// (e.g. `chance: 0.1` in a vessel stores its hits in 10% of the events, to save space). The sensitive
/// energy, which decides if the event is stored, is not affected.
class TRestGeant4ActiveVolumeMetadata : public TRestMetadata {
   public:
    std::string fVolumeName;  ///< `name`: regular expression of physical (or logical) volume names
    double fChance = 1.0;     ///< `chance`: probability (0 to 1) to store the hits of an event in the volume
    TRestWithUnits fMaxStep = 0.0;  ///< `maxStep`: Geant4 maximum step in the volume (mm, 0: no limit)
    /// If true, the volume is also sensitive (it triggers the event storage)
    bool fSensitive = false;

    /// \brief Entry with the default values.
    TRestGeant4ActiveVolumeMetadata();
    /// \brief Entry configured from an element of `activeVolumes`.
    TRestGeant4ActiveVolumeMetadata(const std::string& instanceName, const YAML::Node& node);
    /// \brief Returns "TRestGeant4ActiveVolumeMetadata".
    std::string GetClassName() const override { return "TRestGeant4ActiveVolumeMetadata"; }
    /// \brief Reads the fields from the node and writes them back.
    void LoadConfig() override;
    /// \brief No-op.
    void Initialize() override {}

    friend class TRestMetadataFieldRegistry;
};

/// \class TRestGeant4Metadata
/// \brief The main class to store the *Geant4* simulation conditions that will be used by *restG4*.
///
/// It is read by restG4 (inside a TRestGeant4Manager) and stored in the output file with the values used
/// and the tables of the simulation: the volume, particle and process names of the IDs stored in the
/// TRestGeant4Event (`volumeNameMap`, `particleNamesMap`, `processNamesMap`, `processTypesMap`), the
/// Geant4 version and the number of events. When written to a file, the geometry is stored as well
/// (`Geometry`), and it is loaded again when the metadata is read (any run made from a simulation file
/// keeps it). Units: keV, mm, us.
///
///     geant4Metadata:
///       class: "TRestGeant4Metadata"
///       gdmlFilename: "geometry/setup.gdml"  # GDML geometry
///       nEvents: 1000              # events to simulate
///       nRequestedEntries: 0       # stop when this number of events is stored (0: no limit)
///       simulationMaxTimeSeconds: 0  # stop after this wall time (0: no limit)
///       seed: 17021981             # 0: a random one, written back
///       subEventTimeDelay: 100 us  # a hit later than this starts a new sub-event
///       fullChain: true            # full radioactive decay chain or a single decay
///       fullChainStopIsotopes: ["Pb210"]  # the chain stops at these isotopes
///       resetGlobalTime: true      # reset the global time after a long radioactive decay
///       resetTimePrecision: 1.0    # us
///       energyRangeStored: [0, 5 MeV]  # events stored with sensitive energy in this range
///       saveAllEvents: false       # store also the events without energy in the sensitive volumes
///       storeTracks: true          # false: only the event summary (energies, primaries)
///       storeHadronicTargetInfo: false  # target nucleus of the hadronic processes in the hits
///       removeUnwantedTracks: false     # keep only the tracks with energy in the active volumes
///       removeUnwantedTracksKeepZeroEnergyTracks: false
///       printProgress: false
///       magneticField: [0, 0, 0]   # world magnetic field (T)
///       activeVolumes:             # TRestGeant4ActiveVolumeMetadata
///         - name: "gas"
///           maxStep: 1 mm
///           sensitive: true
///       killVolumes: ["shielding"] # particles entering them are killed
///       biasingVolumes: []         # TRestGeant4BiasingVolume
///       generator:                 # TRestGeant4PrimaryGeneratorInfo, see TRestGeant4PrimaryGenerator
///         type: "volume"
///         from: "gas"
///         source:
///           use: "Xe136bb0n.dat"
///
/// Also:
/// - `activateAllVolumes: true`: all the physical volumes of the geometry are active (their hits are
///   stored), with chance 1 and no step limit; the `activeVolumes` entries keep their values (and only
///   they and the sensitive ones are kept by removeUnwantedTracks). Default false.
/// - `registerEmptyTracks: false`: the tracks without hits in the stored volumes (only their initial hit,
///   always stored, outside the active volumes or the ones not stored in the event by their chance) are
///   removed from the stored events, to save space; their children keep their parent ID (the parent is not
///   stored). Default true (all the tracks are stored).
/// - `biasingVolumes` (TRestGeant4BiasingVolume): volume biasing in stages, see below.
///
/// Also registered: `geant4Version`, `geometryPath`, the ID tables and the biasing results
/// (`biasingTotalFactor`, `biasingStageEvents`, `biasingStageParticles`), written by restG4.
///
/// ### Volume biasing
///
/// With N `biasingVolumes` (nested virtual volumes, from the outermost to the innermost), restG4 runs N + 1
/// stages in one execution. Stage 0 simulates `nEvents` events of the `generator`; in stage k (k < N) a
/// particle entering volume k is stopped and, if its kinetic energy is in the `energyRange` of the volume,
/// its state at the crossing point is stored; stage k + 1 simulates each stored particle `biasingFactor` (of
/// volume k) times as the primary of a new event. The last stage (N) is a normal simulation, and only its
/// events are written to the output. Every output event has the same weight 1 / `biasingTotalFactor`, the
/// product of the factors of the volumes. The results of the stages are stored:
/// - `biasingTotalFactor`: product of the `biasingFactor` of the volumes (1 without biasing).
/// - `biasingStageEvents`: events simulated in each stage (N + 1 values, the first one is `nEvents`).
/// - `biasingStageParticles`: particles stored in stage k when entering volume k (N values).
///
/// The normalization getters: GetNumberOfEvents is the number of primaries of the `generator` simulated (in
/// stage 0); GetEquivalentNumberOfEvents is the number of primaries the output represents, `nEvents` times
/// the total factor (times GetBiasingCompletedFraction if the simulation ended early); GetBiasingEventWeight
/// is the weight of each output event. Without `biasingVolumes` the equivalent number is `nEvents` and the
/// weight 1.
///
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
class TRestGeant4Metadata : public TRestMetadata {
   public:
    /// Defines if metadata is the result of a merge of other metadata
    bool fIsMerge = false;

    /// Class used to store and retrieve geometry info
    TRestGeant4GeometryInfo fGeant4GeometryInfo;

    /// Class used to store and retrieve physics info such as process names or particle names
    mutable TRestGeant4PhysicsInfo fGeant4PhysicsInfo;

    /// Class used to store and retrieve Geant4 primary generator info
    TRestGeant4PrimaryGeneratorInfo fGeant4PrimaryGeneratorInfo;

    /// The version of Geant4 used to generate the data
    std::string fGeant4Version;

    /// The local path to the GDML geometry
    std::string fGeometryPath;  //!

    /// The filename of the GDML geometry
    std::string fGdmlFilename;  //!

    std::map<int, std::string> fVolumeNameMap;            ///< `volumeNameMap`: volume ID -> physical name
    std::map<int, std::string> fProcessNamesMap;          ///< `processNamesMap`: process ID -> name
    std::map<int, std::string> fParticleNamesMap;         ///< `particleNamesMap`: PDG encoding -> name
    std::map<std::string, std::string> fProcessTypesMap;  ///< `processTypesMap`: process name -> type

    /// A pair storing the energy range, in keV, to decide if a particular event should be written to disk
    std::pair<TRestWithUnits, TRestWithUnits> fEnergyRangeStored = {0.0, 1.0E20};

    /// A vector to store the names of the active volumes
    std::vector<TRestGeant4ActiveVolumeMetadata> fActiveVolumesMetadata;
    /// The physical volumes matching the `activeVolumes` expressions (filled by
    /// SyncActiveVolumesFromMetadata)
    std::vector<std::string> fActiveVolumes;

    /// A vector to store the probability value to write to disk the hits in a particular event
    std::vector<double> fChance;

    /// A vector to store the maximum step size at a particular volume
    std::vector<TRestWithUnits> fMaxStepSize;

    /// A vector containing the biasing volume properties (`biasingVolumes`), from the outermost to the
    /// innermost
    std::vector<TRestGeant4BiasingVolume> fBiasingVolumes;

    /// `biasingTotalFactor` (written by restG4): product of the biasing factors of the volumes, the inverse
    /// of the weight of each output event (1 without biasing)
    double fBiasingTotalFactor = 1.0;
    /// `biasingStageEvents` (written by restG4): events simulated in each biasing stage (N + 1 values for N
    /// volumes, the first one is the number of primaries of the generator, `nEvents`); empty without biasing
    std::vector<int64_t> fBiasingStageEvents;
    /// `biasingStageParticles` (written by restG4): particles stored in stage k when entering biasing volume
    /// k, in its energy range (N values); empty without biasing
    std::vector<int64_t> fBiasingStageParticles;

    /// The maximum target step size, in mm, allowed in Geant4 for the target volume (Obsolete)
    TRestWithUnits fMaxTargetStepSize = 0.0;  //!

    /// A time gap, in us, determining if an energy hit should be considered (and stored) as an independent
    /// event
    TRestWithUnits fSubEventTimeDelay = 100.0;

    /// Defines if a radioactive isotope decay is simulated in full chain (true) or just a single decay
    /// (false)
    bool fFullChain = true;

    /// Reset global time in case of a long radioactive decay
    bool fResetGlobalTime = true;

    /// Time precision, in us, to determine if the global time has to be reset or not
    double fResetTimePrecision = 1.0;

    /// If defined, it will stop the full chain decay simulation when one of these isotopes appears
    std::set<std::string> fFullChainStopIsotopes;
    std::vector<std::string> fFullChainStopIsotopesRegistry;  //! YAML adapter for registry

    /// The volumes that serve as trigger for data storage, the physical volumes matching the active
    /// volumes with sensitive: true (filled by SyncActiveVolumesFromMetadata)
    std::vector<std::string> fSensitiveVolumes;

    /// A container related to fRemoveUnwantedTracks
    std::set<std::string> fRemoveUnwantedTracksVolumesToKeep;

    /// A container to store volumes where particles are killed instantly: the physical volumes matching the
    /// `killVolumes` expressions (filled by SyncActiveVolumesFromMetadata). They do not need to be active
    std::set<std::string> fKillVolumes;
    std::vector<std::string> fKillVolumesRegistry;  //! YAML adapter for registry

    /// The number of events simulated, or to be simulated
    int64_t fNEvents = 0;

    /// The number of events the user requested to be on the file
    int64_t fNRequestedEntries = 0;

    /// Time before simulation is ended and saved (wall time in seconds, 0: no limit)
    double fSimulationMaxTimeSeconds = 0.0;

    /// The time, in seconds, that the simulation took to complete (wall time)
    double fSimulationTime = 0.0;

    /// The seed value used for Geant4 random event generator (0 in the YAML: a random one)
    long fSeed = 0;

    /// If set to true it will save all events even if they leave no energy in the sensitive volume
    bool fSaveAllEvents = false;

    /// Option to store target isotope information on hadronic processes (disabled by default)
    bool fStoreHadronicTargetInfo = false;

    /// `activateAllVolumes`: all the physical volumes are active, with chance 1 and no step limit
    bool fActivateAllVolumes = false;  //!

    /// If activated will remove tracks not present in volumes marked as "keep" or "sensitive"
    bool fRemoveUnwantedTracks = false;

    /// Store the tracks and hits of the events (true by default, as legacy); false: only the event summary
    bool fStoreTracks = true;

    /// Option for removeUnwantedTracks to keep zero energy tracks such as neutron captures
    bool fRemoveUnwantedTracksKeepZeroEnergyTracks = false;

    /// If set to true it will print out on screen every time 10k events are reached
    bool fPrintProgress = false;  //!

    /// `registerEmptyTracks`: if false the tracks without hits in the stored volumes are removed from the
    /// stored events (restG4), to save space
    bool fRegisterEmptyTracks = true;

    /// The world magnetic field (`magneticField`) in Tesla
    std::array<TRestWithUnits, 3> fMagneticField = {0.0, 0.0, 0.0};

    /// Used for faster lookup (non-persisted)
    std::set<std::string> fActiveVolumesSet = {};  //!

    // --- Constructors and Destructors ---
    /// \brief Metadata with the default values.
    TRestGeant4Metadata();
    /// \brief Metadata configured from the section `name` of a YAML file.
    TRestGeant4Metadata(const char* configFilename, const std::string& name = "");
    /// \brief Metadata configured from a YAML node.
    TRestGeant4Metadata(const std::string& instanceName,
                        const YAML::Node& node);  // Modern REST v3 constructor
    virtual ~TRestGeant4Metadata();

    // Copy constructor and assignment operator
    /// \brief Plain copy of all the values (use Merge to add the event counts of combined simulations).
    TRestGeant4Metadata(const TRestGeant4Metadata& metadata);
    /// \brief Plain assignment of all the values (use Merge to add the event counts of combined
    /// simulations).
    TRestGeant4Metadata& operator=(const TRestGeant4Metadata& metadata);

    /// Returns the class name used by REST metadata introspection
    std::string GetClassName() const override { return "TRestGeant4Metadata"; }

    // --- Life Cycle and Configuration Methods ---
    /// \brief Reads the fields from YAML, synchronizes the ID tables, draws a seed if it is 0, writes the
    /// values back, loads the geometry of the open file (or gGeoManager) and resolves the active,
    /// sensitive and kill volumes.
    void LoadConfig() override;
    /// \brief No-op.
    void Initialize() override {}
    using TRestMetadata::WriteMetadata;
    /// \brief Writes the YAML and, if loaded, the geometry as `Geometry` in the file, so every run made from
    /// a simulation one (e.g. an analysis) keeps it
    void WriteMetadata(TFile* file) override;
    /// \brief Loads the geometry (Geometry) of the file it is read from (a run or a dataset file)
    void LoadFromFile(TFile* file) override;
    /// \brief Resets all metadata containers and cached lookup structures.
    void Clear();
    /// \brief Resolves the `activeVolumes`, sensitive and `killVolumes` expressions to the physical volumes
    /// of the geometry (kept as written if none matches), and fills the volumes kept by
    /// removeUnwantedTracks (the active and sensitive ones) and the full chain stop isotopes.
    void SyncActiveVolumesFromMetadata();
    /// \brief Copies the stored ID tables (volume, process, particle names) to the geometry and physics
    /// information.
    void SyncRuntimeMapsFromMetadata();
    /// \brief Copies the ID tables of the geometry and physics information to the stored ones.
    void SyncMetadataMapsFromRuntime();
    /// \brief Merges another metadata object into this one for combined simulations: the values of `other`
    /// are copied, but the number of events, of requested entries, the simulation wall time, the cry
    /// showers and exposure and the counts of the biasing stages (if they have the same stages), which are
    /// added.
    void Merge(const TRestGeant4Metadata& other);

    // --- Immutable Getters for Internal Structures ---
    inline long GetSeed() const { return fSeed; }  ///< `seed`
    /// \brief Geometry and volume ID map
    inline const TRestGeant4GeometryInfo& GetGeant4GeometryInfo() const { return fGeant4GeometryInfo; }
    /// \brief Particle and process ID maps
    inline const TRestGeant4PhysicsInfo& GetGeant4PhysicsInfo() const { return fGeant4PhysicsInfo; }
    /// \brief The `generator` section
    inline const TRestGeant4PrimaryGeneratorInfo& GetGeant4PrimaryGeneratorInfo() const {
        return fGeant4PrimaryGeneratorInfo;
    }
    /// \brief The `generator` section
    inline TRestGeant4PrimaryGeneratorInfo& GetGeant4PrimaryGeneratorInfo() {
        return fGeant4PrimaryGeneratorInfo;
    }

    // --- Modernized Getters (std::string) ---
    inline std::string GetGeant4Version() const { return fGeant4Version; }  ///< Geant4 version string
    /// \brief Extracts the Geant4 major version number from the version string (0 if not valid).
    size_t GetGeant4VersionMajor() const;
    /// \brief `storeHadronicTargetInfo`
    inline bool GetStoreHadronicTargetInfo() const { return fStoreHadronicTargetInfo; }
    inline std::string GetGeometryPath() const { return fGeometryPath; }  ///< `geometryPath`
    inline std::string GetGdmlFilename() const { return fGdmlFilename; }  ///< `gdmlFilename`

    // --- Analytical Logic Getters ---
    inline bool isFullChainActivated() const { return fFullChain; }     ///< `fullChain`
    inline bool isGlobalTimeReset() const { return fResetGlobalTime; }  ///< `resetGlobalTime`
    inline double GetResetTimePrecision() const {
        return fResetTimePrecision;
    }  ///< `resetTimePrecision` (us)
    /// \brief `fullChainStopIsotopes`
    inline std::set<std::string> GetFullChainStopIsotopes() const { return fFullChainStopIsotopes; }
    /// \brief True if the full chain stops at this isotope
    inline bool IsIsotopeFullChainStop(const std::string& isotope) const {
        return fFullChainStopIsotopes.count(isotope) > 0;
    }
    inline double GetMaxTargetStepSize() const { return fMaxTargetStepSize; }  ///< Obsolete (mm)
    inline double GetSubEventTimeDelay() const { return fSubEventTimeDelay; }  ///< `subEventTimeDelay` (us)
    inline bool GetSaveAllEvents() const { return fSaveAllEvents; }            ///< `saveAllEvents`
    inline bool PrintProgress() const { return fPrintProgress; }               ///< `printProgress`
    inline bool RegisterEmptyTracks() const { return fRegisterEmptyTracks; }   ///< `registerEmptyTracks`

    // --- Configuration Setters ---
    inline void SetSeed(long seed) { fSeed = seed; }                      ///< Sets `seed`
    inline void SetSaveAllEvents(bool value) { fSaveAllEvents = value; }  ///< Sets `saveAllEvents`
    /// \brief Sets the Geant4 version string
    inline void SetGeant4Version(const std::string& versionString) { fGeant4Version = versionString; }
    inline void SetFullChain(bool fullChain) { fFullChain = fullChain; }            ///< Sets `fullChain`
    inline void SetGeometryPath(const std::string& path) { fGeometryPath = path; }  ///< Sets `geometryPath`
    /// \brief Sets `gdmlFilename`
    inline void SetGdmlFilename(const std::string& gdmlFile) { fGdmlFilename = gdmlFile; }

    // --- Event Counting and Max Simulation Times ---
    /// \brief `nEvents`: events simulated. With biasing volumes, the events (primaries) of the generator
    /// simulated in the first stage, not the statistics the output represents (GetEquivalentNumberOfEvents)
    inline int64_t GetNumberOfEvents() const { return fNEvents; }
    /// \brief `nRequestedEntries`
    inline int64_t GetNumberOfRequestedEntries() const { return fNRequestedEntries; }
    /// \brief `simulationMaxTimeSeconds`
    inline double GetSimulationMaxTimeSeconds() const { return fSimulationMaxTimeSeconds; }
    inline void SetNumberOfEvents(int64_t n) { fNEvents = n; }  ///< Sets `nEvents`
    /// \brief Sets `nRequestedEntries`
    inline void SetNumberOfRequestedEntries(int64_t n) { fNRequestedEntries = n; }
    /// \brief Sets `simulationMaxTimeSeconds`
    inline void SetSimulationMaxTimeSeconds(double seconds) { fSimulationMaxTimeSeconds = seconds; }

    // --- Primary Generator Sources ---
    /// \brief Number of sources of the generator (0 for cry)
    inline int GetNumberOfSources() const {
        return static_cast<int>(fGeant4PrimaryGeneratorInfo.GetNumberOfSources());
    }

    // --- Biasing Volumes Management ---
    inline size_t GetNumberOfBiasingVolumes() const { return fBiasingVolumes.size(); }  ///< Biasing volumes
    /// \brief The biasing volume n (throws std::out_of_range)
    inline TRestGeant4BiasingVolume GetBiasingVolume(int n) const { return fBiasingVolumes.at(n); }
    /// \brief The biasing volumes, from the outermost to the innermost
    inline const std::vector<TRestGeant4BiasingVolume>& GetBiasingVolumes() const { return fBiasingVolumes; }
    /// \brief Number of biasing volumes (non zero if biasing is active)
    inline int isBiasingActive() const { return static_cast<int>(fBiasingVolumes.size()); }
    /// \brief Throws std::runtime_error if a biasing volume is not inside the previous one (they must be
    /// nested and listed from the outermost to the innermost)
    void CheckBiasingVolumes() const;
    /// \brief Starts the biasing results (restG4): the total factor (product of the factors of the volumes,
    /// 1 without them) and the counts of the stages to 0 (empty without biasing volumes)
    void InitializeBiasingStages();
    /// \brief `biasingTotalFactor`: product of the factors of the biasing volumes (1 without biasing)
    inline double GetBiasingTotalFactor() const { return fBiasingTotalFactor; }
    /// \brief Weight of each output event, 1 / `biasingTotalFactor` (1 without biasing)
    inline double GetBiasingEventWeight() const { return 1.0 / fBiasingTotalFactor; }
    /// \brief Number of biasing stages simulated by restG4, the number of volumes + 1 (0 without biasing)
    inline size_t GetNumberOfBiasingStages() const { return fBiasingStageEvents.size(); }
    /// \brief `biasingStageEvents`: events simulated in the biasing stage (throws std::out_of_range)
    inline int64_t GetBiasingStageEvents(size_t stage) const { return fBiasingStageEvents.at(stage); }
    /// \brief `biasingStageParticles`: particles stored in the stage when entering the biasing volume of
    /// the same index (throws std::out_of_range)
    inline int64_t GetBiasingStageParticles(size_t stage) const { return fBiasingStageParticles.at(stage); }
    /// \brief Sets the events simulated in a biasing stage (restG4; ignored if out of range)
    void SetBiasingStageEvents(size_t stage, int64_t events);
    /// \brief Sets the particles stored in a biasing stage (restG4; ignored if out of range)
    void SetBiasingStageParticles(size_t stage, int64_t particles);
    /// \brief Fraction of the biasing stages completed: the product, over the stages after the first one, of
    /// the events simulated over the events planned (particles stored in the previous stage times its
    /// factor; a stage with nothing planned counts 1). It is 1 without biasing or when all the stages were
    /// completed (also when a stage stored no particle), below 1 when the last stage was stopped early
    /// (requested entries, time limit) and 0 if the simulation stopped (time limit, CTRL-C) before the last
    /// stage with stored particles not simulated (the output has no events). For merged
    /// metadata it is computed from the summed counts (exact when all the stages were completed)
    double GetBiasingCompletedFraction() const;
    /// \brief Number of primaries of the generator the output statistically represents, to normalize:
    /// `nEvents` * `biasingTotalFactor` * GetBiasingCompletedFraction (`nEvents` without biasing)
    double GetEquivalentNumberOfEvents() const;

    // --- Sensitive Volumes Management ---
    /// \brief The sensitive physical volume n (throws std::out_of_range)
    inline std::string GetSensitiveVolume(int n) const { return fSensitiveVolumes.at(n); }
    /// \brief Number of sensitive physical volumes
    inline size_t GetNumberOfSensitiveVolumes() const { return fSensitiveVolumes.size(); }
    /// \brief The sensitive physical volumes
    inline const std::vector<std::string>& GetSensitiveVolumes() const { return fSensitiveVolumes; }
    /// \brief The `name` (expression) of each sensitive entry of `activeVolumes`, as written in the YAML
    inline std::vector<std::string> GetSensitiveVolumeExpressions() const {
        std::vector<std::string> expressions;
        for (const auto& volume : fActiveVolumesMetadata) {
            if (volume.fSensitive && !volume.fVolumeName.empty()) expressions.push_back(volume.fVolumeName);
        }
        return expressions;
    }
    /// \brief Adds a sensitive volume (if not already there)
    inline void InsertSensitiveVolume(const std::string& volume) {
        if (std::find(fSensitiveVolumes.begin(), fSensitiveVolumes.end(), volume) ==
            fSensitiveVolumes.end()) {
            fSensitiveVolumes.push_back(volume);
        }
    }

    /// \brief True if the physical volume is sensitive
    bool IsSensitiveVolume(const std::string& volumeName) {
        return std::find(fSensitiveVolumes.begin(), fSensitiveVolumes.end(), volumeName) !=
               fSensitiveVolumes.end();
    }

    // --- Fast Lookup and Modernized Volume Properties ---
    /// \brief Number of active physical volumes
    inline unsigned int GetNumberOfActiveVolumes() const {
        return static_cast<unsigned int>(fActiveVolumes.size());
    }
    /// \brief True if the physical volume is active (its hits are stored)
    inline bool IsActiveVolume(const std::string& volumeName) const {
        return fActiveVolumesSet.count(volumeName) > 0;
    }  //!
    /// \brief True if the tracks with hits in the volume are kept by removeUnwantedTracks
    inline bool IsKeepTracksVolume(const std::string& volumeName) const {
        return fRemoveUnwantedTracksVolumesToKeep.count(volumeName) > 0;
    }
    /// \brief True if the particles are killed in the volume
    inline bool IsKillVolume(const std::string& volumeName) const {
        return fKillVolumes.count(volumeName) > 0;
    }

    // Range-constructor based conversions from std::set to std::vector
    /// \brief The kill volumes
    inline std::vector<std::string> GetKillVolumes() const {
        return {fKillVolumes.begin(), fKillVolumes.end()};
    }
    /// \brief The volumes kept by removeUnwantedTracks
    inline std::vector<std::string> GetRemoveUnwantedTracksVolumesToKeep() const {
        return {fRemoveUnwantedTracksVolumesToKeep.begin(), fRemoveUnwantedTracksVolumesToKeep.end()};
    }

    /// \brief The active physical volume n (throws std::out_of_range)
    inline std::string GetActiveVolumeName(int n) const { return fActiveVolumes.at(n); }
    /// \brief The active physical volumes
    inline std::vector<std::string> GetActiveVolumes() const { return fActiveVolumes; }

    inline bool GetRemoveUnwantedTracks() const { return fRemoveUnwantedTracks; }  ///< `removeUnwantedTracks`
    inline bool GetStoreTracks() const { return fStoreTracks; }                    ///< `storeTracks`
    /// \brief `removeUnwantedTracksKeepZeroEnergyTracks`
    inline bool GetRemoveUnwantedTracksKeepZeroEnergyTracks() const {
        return fRemoveUnwantedTracksKeepZeroEnergyTracks;
    }

    /// \brief Returns event storage probability associated with a given active volume (0 if not active).
    double GetStorageChance(const std::string& volume) const;
    /// \brief Returns configured Geant4 max step size for a given active volume (mm, 0 if not active).
    double GetMaxStepSize(const std::string& volume) const;

    /// \brief Computes effective source generation surface in cm2 when available (cosmic generator, 0
    /// otherwise).
    double GetCosmicGeneratorSurfaceCm2() const;
    /// \brief cry generator: showers simulated and their exposure (time simulated by CRY, us), to normalize
    /// to a rate (0 for another generator)
    inline int64_t GetCRYShowersUsed() const { return fGeant4PrimaryGeneratorInfo.GetCRYShowersUsed(); }
    /// \brief cry generator: exposure (time simulated by CRY, us) of the showers used (in the first biasing
    /// stage, see GetEquivalentCRYExposureTime)
    inline double GetCRYExposureTime() const { return fGeant4PrimaryGeneratorInfo.GetCRYExposureTime(); }
    /// \brief cry generator: exposure (us) the output represents, GetCRYExposureTime * `biasingTotalFactor` *
    /// GetBiasingCompletedFraction (GetCRYExposureTime without biasing)
    inline double GetEquivalentCRYExposureTime() const {
        return GetCRYExposureTime() * fBiasingTotalFactor * GetBiasingCompletedFraction();
    }
    /// \brief Wall time of the simulation (s)
    inline double GetSimulationWallTime() const { return fSimulationTime; }
    /// \brief Sets the wall time of the simulation (s)
    inline void SetSimulationWallTime(double time) { fSimulationTime = time; }

    /// \brief Index of an active physical volume in the active volume list (-1 if not active)
    int GetActiveVolumeID(const std::string& name);
    /// \brief True if the volume is active (IsActiveVolume)
    bool isVolumeStored(const std::string& volume) const;
    /// \brief Adds an active physical volume, or updates its chance and maximum step (mm)
    void SetActiveVolume(const std::string& name, double chance, double maxStep = 0.0);

    /// \brief Registers the resolved Geant4 physical volume name as an alias of a user-declared
    /// (active/sensitive) volume name, so runtime lookups by physical name succeed directly.
    inline void RegisterVolumeAlias(const std::string& originalName, const std::string& physicalName) {
        if (originalName == physicalName) return;
        if (IsActiveVolume(originalName)) {
            SetActiveVolume(physicalName, GetStorageChance(originalName), GetMaxStepSize(originalName));
        }
        if (std::find(fSensitiveVolumes.begin(), fSensitiveVolumes.end(), originalName) !=
            fSensitiveVolumes.end()) {
            InsertSensitiveVolume(physicalName);
        }
        if (fRemoveUnwantedTracksVolumesToKeep.count(originalName) > 0) {
            fRemoveUnwantedTracksVolumesToKeep.insert(physicalName);
        }
        if (fKillVolumes.count(originalName) > 0) {
            fKillVolumes.insert(physicalName);
        }
    }

    // --- Geometric and Field Getters ---
    /// \brief Minimum of `energyRangeStored` (keV)
    inline double GetMinimumEnergyStored() const { return fEnergyRangeStored.first; }
    /// \brief Maximum of `energyRangeStored` (keV)
    inline double GetMaximumEnergyStored() const { return fEnergyRangeStored.second; }
    /// \brief `magneticField` (T)
    inline ROOT::Math::XYZVector GetMagneticField() const {
        return ROOT::Math::XYZVector(fMagneticField[0], fMagneticField[1], fMagneticField[2]);
    }

    /// \brief Loads the geometry (`Geometry`) of the open ROOT file (gFile) or, if none, gGeoManager.
    void LoadGeometryFromActiveFile();

    friend class SteppingAction;
    friend class DetectorConstruction;
    friend class TRestGeant4Hits;
};

#endif
