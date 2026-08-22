#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// ROOT MathCore GenVector headers
#include <Math/Vector3D.h>

#include "TRestGeant4BiasingVolume.h"
#include "TRestGeant4GeometryInfo.h"
#include "TRestGeant4PhysicsInfo.h"
#include "TRestGeant4PrimaryGeneratorInfo.h"
#include "TRestGeant4ParticleSource.h"
#include "TRestMetadata.h"
#include "TRestTools.h"

// Forward declarations for framework friend classes
class SteppingAction;
class DetectorConstruction;

/// \class TRestGeant4ActiveVolumeMetadata
class TRestGeant4ActiveVolumeMetadata : public TRestMetadata {
   public:
    std::string fVolumeName;
    double fChance = 1.0;
    TRestWithUnits fMaxStep = 0.0;

    TRestGeant4ActiveVolumeMetadata();
    TRestGeant4ActiveVolumeMetadata(const std::string& instanceName, const YAML::Node& node);
    std::string GetClassName() const override { return "TRestGeant4ActiveVolumeMetadata"; }
    void LoadConfig() override;
    void Initialize() override {}

    friend class TRestMetadataFieldRegistry;
};

/// \brief The main class to store the *Geant4* simulation conditions that will be used by *restG4*.
class TRestGeant4Metadata : public TRestMetadata {
    DECLARE_LOG_CLASS(TRestGeant4Metadata)

   public:
    /// Defines if metadata is the result of a merge of other metadata
    bool fIsMerge = false;

    /// Class used to store and retrieve geometry info
    TRestGeant4GeometryInfo fGeant4GeometryInfo;

    /// Class used to store and retrieve physics info such as process names or particle names
    TRestGeant4PhysicsInfo fGeant4PhysicsInfo;

    /// Class used to store and retrieve Geant4 primary generator info
    TRestGeant4PrimaryGeneratorInfo fGeant4PrimaryGeneratorInfo;

    /// The version of Geant4 used to generate the data
    std::string fGeant4Version;

    /// The local path to the GDML geometry
    std::string fGeometryPath;  //!

    /// The filename of the GDML geometry
    std::string fGdmlFilename;  //!

    /// A pair storing the energy range, in keV, to decide if a particular event should be written to disk
    std::pair<TRestWithUnits, TRestWithUnits> fEnergyRangeStored = {0.0, 1.0E20};

    /// A vector to store the names of the active volumes
    std::vector<TRestGeant4ActiveVolumeMetadata> fActiveVolumesMetadata;
    std::vector<std::string> fActiveVolumes;

    /// A vector to store the probability value to write to disk the hits in a particular event
    std::vector<double> fChance;

    /// A vector to store the maximum step size at a particular volume
    std::vector<TRestWithUnits> fMaxStepSize;

    /// The number of biasing volumes used in the simulation. If zero, no biasing technique is used.
    int fNBiasingVolumes = 0;

    /// A vector containing the biasing volume properties
    std::vector<TRestGeant4BiasingVolume> fBiasingVolumes;

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

    /// The volume that serves as trigger for data storage
    std::vector<std::string> fSensitiveVolumes;

    /// A container related to fRemoveUnwantedTracks
    std::set<std::string> fRemoveUnwantedTracksVolumesToKeep;

    /// A container to store volumes where particles are killed instantly
    std::set<std::string> fKillVolumes;
    std::vector<std::string> fKillVolumesRegistry;  //! YAML adapter for registry

    /// The number of events simulated, or to be simulated
    int64_t fNEvents = 0;

    /// The number of events the user requested to be on the file
    int64_t fNRequestedEntries = 0;

    /// Time before simulation is ended and saved
    double fSimulationMaxTimeSeconds = 0.0;

    /// The time, in seconds, that the simulation took to complete (wall time)
    double fSimulationTime = 0.0;

    /// The seed value used for Geant4 random event generator
    long fSeed = 0;

    /// If set to true it will save all events even if they leave no energy in the sensitive volume
    bool fSaveAllEvents = false;

    /// Option to store target isotope information on hadronic processes (disabled by default)
    bool fStoreHadronicTargetInfo = false;

    /// Sets all volumes as active without having to explicitly list them
    bool fActivateAllVolumes = false;  //!

    /// If activated will remove tracks not present in volumes marked as "keep" or "sensitive"
    bool fRemoveUnwantedTracks = false;

    /// Store event tracks (default is false)
    bool fStoreTracks = false;

    /// Option for removeUnwantedTracks to keep zero energy tracks such as neutron captures
    bool fRemoveUnwantedTracksKeepZeroEnergyTracks = false;

    /// If set to true it will print out on screen every time 10k events are reached
    bool fPrintProgress = false;  //!

    /// If disabled then empty tracks will not be written to disk to save space
    bool fRegisterEmptyTracks = true;

    /// The world magnetic field in Tesla
    std::array<TRestWithUnits, 3> fMagneticField = {0.0, 0.0, 0.0};

    /// Used for faster lookup (non-persisted)
    std::set<std::string> fActiveVolumesSet = {};  //!

    // --- Constructors and Destructors ---
    TRestGeant4Metadata();
    TRestGeant4Metadata(const char* configFilename, const std::string& name = "");
    TRestGeant4Metadata(const std::string& instanceName,
                        const YAML::Node& node);  // Modern REST v3 constructor
    virtual ~TRestGeant4Metadata();

    // Copy constructor and assignment operator
    TRestGeant4Metadata(const TRestGeant4Metadata& metadata);
    TRestGeant4Metadata& operator=(const TRestGeant4Metadata& metadata);

    /// Returns the class name used by REST metadata introspection
    std::string GetClassName() const override { return "TRestGeant4Metadata"; }

    // --- Life Cycle and Configuration Methods ---
    void LoadConfig() override;
    void Initialize() override {}
    /// \brief Resets all metadata containers and cached lookup structures.
    void Clear();
    void SyncActiveVolumesFromMetadata();
    /// \brief Merges another metadata object into this one for combined simulations.
    void Merge(const TRestGeant4Metadata& other);

    // --- Immutable Getters for Internal Structures ---
    inline long GetSeed() const { return fSeed; }
    inline const TRestGeant4GeometryInfo& GetGeant4GeometryInfo() const { return fGeant4GeometryInfo; }
    inline const TRestGeant4PhysicsInfo& GetGeant4PhysicsInfo() const { return fGeant4PhysicsInfo; }
    inline const TRestGeant4PrimaryGeneratorInfo& GetGeant4PrimaryGeneratorInfo() const {
        return fGeant4PrimaryGeneratorInfo;
    }

    // --- Modernized Getters (std::string) ---
    inline std::string GetGeant4Version() const { return fGeant4Version; }
    /// \brief Extracts the Geant4 major version number from the version string.
    size_t GetGeant4VersionMajor() const;
    inline bool GetStoreHadronicTargetInfo() const { return fStoreHadronicTargetInfo; }
    inline std::string GetGeometryPath() const { return fGeometryPath; }
    inline std::string GetGdmlFilename() const { return fGdmlFilename; }

    // --- Analytical Logic Getters ---
    inline bool isFullChainActivated() const { return fFullChain; }
    inline bool isGlobalTimeReset() const { return fResetGlobalTime; }
    inline double GetResetTimePrecision() const { return fResetTimePrecision; }
    inline std::set<std::string> GetFullChainStopIsotopes() const { return fFullChainStopIsotopes; }
    inline bool IsIsotopeFullChainStop(const std::string& isotope) const {
        return fFullChainStopIsotopes.count(isotope) > 0;
    }
    inline double GetMaxTargetStepSize() const { return fMaxTargetStepSize; }
    inline double GetSubEventTimeDelay() const { return fSubEventTimeDelay; }
    inline bool GetSaveAllEvents() const { return fSaveAllEvents; }
    inline bool PrintProgress() const { return fPrintProgress; }
    inline bool RegisterEmptyTracks() const { return fRegisterEmptyTracks; }

    // --- Configuration Setters ---
    inline void SetSeed(long seed) { fSeed = seed; }
    inline void SetSaveAllEvents(bool value) { fSaveAllEvents = value; }
    inline void SetGeant4Version(const std::string& versionString) { fGeant4Version = versionString; }
    inline void SetFullChain(bool fullChain) { fFullChain = fullChain; }
    inline void SetGeometryPath(const std::string& path) { fGeometryPath = path; }
    inline void SetGdmlFilename(const std::string& gdmlFile) { fGdmlFilename = gdmlFile; }

    // --- Event Counting and Max Simulation Times ---
    inline int64_t GetNumberOfEvents() const { return fNEvents; }
    inline int64_t GetNumberOfRequestedEntries() const { return fNRequestedEntries; }
    inline double GetSimulationMaxTimeSeconds() const { return fSimulationMaxTimeSeconds; }
    inline void SetNumberOfEvents(int64_t n) { fNEvents = n; }
    inline void SetNumberOfRequestedEntries(int64_t n) { fNRequestedEntries = n; }
    inline void SetSimulationMaxTimeSeconds(double seconds) { fSimulationMaxTimeSeconds = seconds; }

    // --- Primary Generator Sources Management ---
    inline int GetNumberOfSources() const {
        return static_cast<int>(fGeant4PrimaryGeneratorInfo.fParticleSources.size());
    }
    inline TRestGeant4ParticleSource* GetParticleSource(size_t n = 0) {
        return &fGeant4PrimaryGeneratorInfo.fParticleSources.at(n);
    }
    inline const TRestGeant4ParticleSource* GetParticleSource(size_t n = 0) const {
        return &fGeant4PrimaryGeneratorInfo.fParticleSources.at(n);
    }
    /// \brief Clears all configured primary particle sources.
    void RemoveParticleSources();
    /// \brief Copies one particle source into metadata and releases the supplied instance.
    void AddParticleSource(TRestGeant4ParticleSource* src);

    // --- Biasing Volumes Management ---
    inline size_t GetNumberOfBiasingVolumes() const { return fBiasingVolumes.size(); }
    inline TRestGeant4BiasingVolume GetBiasingVolume(int n) const { return fBiasingVolumes.at(n); }
    inline int isBiasingActive() const { return static_cast<int>(fBiasingVolumes.size()); }

    // --- Sensitive Volumes Management ---
    inline std::string GetSensitiveVolume(int n = 0) const { return fSensitiveVolumes.at(n); }
    inline size_t GetNumberOfSensitiveVolumes() const { return fSensitiveVolumes.size(); }
    inline const std::vector<std::string>& GetSensitiveVolumes() const { return fSensitiveVolumes; }
    inline void InsertSensitiveVolume(const std::string& volume) {
        if (std::find(fSensitiveVolumes.begin(), fSensitiveVolumes.end(), volume) ==
            fSensitiveVolumes.end()) {
            fSensitiveVolumes.push_back(volume);
        }
    }

    // --- Fast Lookup and Modernized Volume Properties ---
    inline unsigned int GetNumberOfActiveVolumes() const {
        return static_cast<unsigned int>(fActiveVolumes.size());
    }
    inline bool IsActiveVolume(const std::string& volumeName) const {
        return fActiveVolumesSet.count(volumeName) > 0;
    }  //!
    inline bool IsKeepTracksVolume(const std::string& volumeName) const {
        return fRemoveUnwantedTracksVolumesToKeep.count(volumeName) > 0;
    }
    inline bool IsKillVolume(const std::string& volumeName) const {
        return fKillVolumes.count(volumeName) > 0;
    }

    // Range-constructor based conversions from std::set to std::vector
    inline std::vector<std::string> GetKillVolumes() const {
        return {fKillVolumes.begin(), fKillVolumes.end()};
    }
    inline std::vector<std::string> GetRemoveUnwantedTracksVolumesToKeep() const {
        return {fRemoveUnwantedTracksVolumesToKeep.begin(), fRemoveUnwantedTracksVolumesToKeep.end()};
    }

    inline std::string GetActiveVolumeName(int n) const { return fActiveVolumes.at(n); }
    inline std::vector<std::string> GetActiveVolumes() const { return fActiveVolumes; }

    inline bool GetRemoveUnwantedTracks() const { return fRemoveUnwantedTracks; }
    inline bool GetStoreTracks() const { return fStoreTracks; }
    inline bool GetRemoveUnwantedTracksKeepZeroEnergyTracks() const {
        return fRemoveUnwantedTracksKeepZeroEnergyTracks;
    }

    /// \brief Returns event storage probability associated with a given active volume.
    double GetStorageChance(const std::string& volume) const;
    /// \brief Returns configured Geant4 max step size for a given active volume.
    double GetMaxStepSize(const std::string& volume) const;

    // --- Cosmic Ray Analysis Methods ---
    /// \brief Computes effective source generation surface in cm2 when available.
    double GetGeneratorSurfaceCm2() const;
    double GetCosmicFluxInCountsPerCm2PerSecond() const;
    double GetCosmicIntensityInCountsPerSecond() const;
    double GetEquivalentSimulatedTime() const;
    inline double GetSimulationWallTime() const { return fSimulationTime; }
    inline void SetSimulationWallTime(double time) { fSimulationTime = time; }

    int GetActiveVolumeID(const std::string& name);
    bool isVolumeStored(const std::string& volume) const;
    void SetActiveVolume(const std::string& name, double chance, double maxStep = 0.0);

    // --- Geometric and Field Getters ---
    inline double GetMinimumEnergyStored() const { return fEnergyRangeStored.first; }
    inline double GetMaximumEnergyStored() const { return fEnergyRangeStored.second; }
    inline ROOT::Math::XYZVector GetMagneticField() const {
        return ROOT::Math::XYZVector(fMagneticField[0], fMagneticField[1], fMagneticField[2]);
    }

    friend class SteppingAction;
    friend class DetectorConstruction;
    friend class TRestGeant4Hits;
};
