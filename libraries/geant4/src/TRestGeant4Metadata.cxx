#include "TRestGeant4Metadata.h"

#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

#include "TDirectory.h"
#include "TError.h"
#include "TFile.h"
#include "TObjString.h"
#include "TRestConstants.h"

using namespace TRestConstants;

// ---------------------------------------------------------------------------
// Self-registration
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestGeant4Metadata", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestGeant4Metadata>(instanceName, params);
        });
    return true;
}();
}  // namespace

// Modern REST v3 field registry hook for safe, reflection-based YAML initialization
static const bool TRestGeant4ActiveVolumeMetadata_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestGeant4ActiveVolumeMetadata>("name",
                                                       &TRestGeant4ActiveVolumeMetadata::fVolumeName);
    reg.RegisterField<TRestGeant4ActiveVolumeMetadata>("chance",
                                                       &TRestGeant4ActiveVolumeMetadata::fChance);
    reg.RegisterField<TRestGeant4ActiveVolumeMetadata>("maxStep",
                                                       &TRestGeant4ActiveVolumeMetadata::fMaxStep);
    return true;
}();

static const bool TRestGeant4Metadata_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();

    // Core parameters mapping
    reg.RegisterField<TRestGeant4Metadata>("geant4Version", &TRestGeant4Metadata::fGeant4Version);
    reg.RegisterField<TRestGeant4Metadata>("geometryPath", &TRestGeant4Metadata::fGeometryPath);
    reg.RegisterField<TRestGeant4Metadata>("gdmlFilename", &TRestGeant4Metadata::fGdmlFilename);

    // Standard Library components (std::pair handles seamlessly via reflection)
    reg.RegisterField<TRestGeant4Metadata>("energyRangeStored", &TRestGeant4Metadata::fEnergyRangeStored);

    // MathCore components
    reg.RegisterField<TRestGeant4Metadata>("magneticField", &TRestGeant4Metadata::fMagneticField);

    // Physics and simulation chains
    reg.RegisterField<TRestGeant4Metadata>("volumeNameMap", &TRestGeant4Metadata::fVolumeNameMap);
    reg.RegisterField<TRestGeant4Metadata>("processNamesMap", &TRestGeant4Metadata::fProcessNamesMap);
    reg.RegisterField<TRestGeant4Metadata>("particleNamesMap", &TRestGeant4Metadata::fParticleNamesMap);
    reg.RegisterField<TRestGeant4Metadata>("processTypesMap", &TRestGeant4Metadata::fProcessTypesMap);
    reg.RegisterField<TRestGeant4Metadata>("subEventTimeDelay", &TRestGeant4Metadata::fSubEventTimeDelay);
    reg.RegisterField<TRestGeant4Metadata>("fullChain", &TRestGeant4Metadata::fFullChain);
    reg.RegisterField<TRestGeant4Metadata>("resetGlobalTime", &TRestGeant4Metadata::fResetGlobalTime);
    reg.RegisterField<TRestGeant4Metadata>("resetTimePrecision", &TRestGeant4Metadata::fResetTimePrecision);

    // Event loop control
    reg.RegisterField<TRestGeant4Metadata>("nEvents", &TRestGeant4Metadata::fNEvents);
    reg.RegisterField<TRestGeant4Metadata>("nRequestedEntries", &TRestGeant4Metadata::fNRequestedEntries);
    reg.RegisterField<TRestGeant4Metadata>("simulationMaxTimeSeconds",
                                           &TRestGeant4Metadata::fSimulationMaxTimeSeconds);
    reg.RegisterField<TRestGeant4Metadata>("seed", &TRestGeant4Metadata::fSeed);

    // Structural boolean options
    reg.RegisterField<TRestGeant4Metadata>("saveAllEvents", &TRestGeant4Metadata::fSaveAllEvents);
    reg.RegisterField<TRestGeant4Metadata>("storeHadronicTargetInfo",
                                           &TRestGeant4Metadata::fStoreHadronicTargetInfo);
    reg.RegisterField<TRestGeant4Metadata>("removeUnwantedTracks",
                                           &TRestGeant4Metadata::fRemoveUnwantedTracks);
    reg.RegisterField<TRestGeant4Metadata>("storeTracks", &TRestGeant4Metadata::fStoreTracks);
    reg.RegisterField<TRestGeant4Metadata>("removeUnwantedTracksKeepZeroEnergyTracks",
                                           &TRestGeant4Metadata::fRemoveUnwantedTracksKeepZeroEnergyTracks);
    reg.RegisterField<TRestGeant4Metadata>("registerEmptyTracks", &TRestGeant4Metadata::fRegisterEmptyTracks);

    // Additional YAML-backed control and volume lists
    reg.RegisterField<TRestGeant4Metadata>("activateAllVolumes", &TRestGeant4Metadata::fActivateAllVolumes);
    reg.RegisterField<TRestGeant4Metadata>("printProgress", &TRestGeant4Metadata::fPrintProgress);
    reg.RegisterField<TRestGeant4Metadata>("activeVolumes", &TRestGeant4Metadata::fActiveVolumesMetadata);
    reg.RegisterNestedField<TRestGeant4Metadata>("generator", &TRestGeant4Metadata::fGeant4PrimaryGeneratorInfo);
    reg.RegisterField<TRestGeant4Metadata>("sensitiveVolumes", &TRestGeant4Metadata::fSensitiveVolumes);
    reg.RegisterField<TRestGeant4Metadata>("killVolumes", &TRestGeant4Metadata::fKillVolumesRegistry);
    reg.RegisterField<TRestGeant4Metadata>("fullChainStopIsotopes",
                                           &TRestGeant4Metadata::fFullChainStopIsotopesRegistry);

    return true;
}();

/// \brief Default constructor.
TRestGeant4ActiveVolumeMetadata::TRestGeant4ActiveVolumeMetadata() {
    fName = "TRestGeant4ActiveVolumeMetadata";
}

TRestGeant4ActiveVolumeMetadata::TRestGeant4ActiveVolumeMetadata(const std::string& instanceName,
                                                                 const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

void TRestGeant4ActiveVolumeMetadata::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4ActiveVolumeMetadata>(fNode);
    UpdateYAMLFromParams<TRestGeant4ActiveVolumeMetadata>(fNode);
}

TRestGeant4Metadata::TRestGeant4Metadata() : TRestMetadata() { fName = "TRestGeant4Metadata"; }

/// \brief Constructor using raw config file path.
TRestGeant4Metadata::TRestGeant4Metadata(const char* configFilename, const std::string& name)
    : TRestMetadata(configFilename, name) {
    LoadConfig();
}

/// \brief Modern constructor for internal REST pipeline execution.
TRestGeant4Metadata::TRestGeant4Metadata(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

/// \brief Destructor clean-up logic.
TRestGeant4Metadata::~TRestGeant4Metadata() { fGeant4PrimaryGeneratorInfo.RemoveParticleSources(); }

/// \brief Pre-allocation and basic environment state setups.
void TRestGeant4Metadata::Clear() {
    fVolumeNameMap.clear();
    fProcessNamesMap.clear();
    fParticleNamesMap.clear();
    fProcessTypesMap.clear();
    fActiveVolumesMetadata.clear();
    fActiveVolumes.clear();
    fChance.clear();
    fMaxStepSize.clear();
    fBiasingVolumes.clear();
    fSensitiveVolumes.clear();
    fFullChainStopIsotopes.clear();
    fFullChainStopIsotopesRegistry.clear();
    fRemoveUnwantedTracksVolumesToKeep.clear();
    fKillVolumes.clear();
    fKillVolumesRegistry.clear();
    fActiveVolumesSet.clear();
    fGeant4PrimaryGeneratorInfo.RemoveParticleSources();
}

void TRestGeant4Metadata::SyncRuntimeMapsFromMetadata() {
    fGeant4GeometryInfo.fVolumeNameMap = fVolumeNameMap;
    fGeant4GeometryInfo.fVolumeNameReverseMap.clear();
    for (const auto& [id, name] : fVolumeNameMap) fGeant4GeometryInfo.fVolumeNameReverseMap[name] = id;

    fGeant4PhysicsInfo.fProcessNamesMap = fProcessNamesMap;
    fGeant4PhysicsInfo.fParticleNamesMap = fParticleNamesMap;
    fGeant4PhysicsInfo.fProcessTypesMap = fProcessTypesMap;
    fGeant4PhysicsInfo.fProcessNamesReverseMap.clear();
    for (const auto& [id, name] : fProcessNamesMap) fGeant4PhysicsInfo.fProcessNamesReverseMap[name] = id;
    fGeant4PhysicsInfo.fParticleNamesReverseMap.clear();
    for (const auto& [id, name] : fParticleNamesMap) fGeant4PhysicsInfo.fParticleNamesReverseMap[name] = id;
}

void TRestGeant4Metadata::SyncMetadataMapsFromRuntime() {
    fVolumeNameMap = fGeant4GeometryInfo.fVolumeNameMap;
    fProcessNamesMap = fGeant4PhysicsInfo.fProcessNamesMap;
    fParticleNamesMap = fGeant4PhysicsInfo.fParticleNamesMap;
    fProcessTypesMap = fGeant4PhysicsInfo.fProcessTypesMap;
}

/// \brief Modern framework configuration entry point.
void TRestGeant4Metadata::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4Metadata>(fNode);
    SyncRuntimeMapsFromMetadata();
    ReadYAMLVerbose(fNode);
    if(fSeed==0)fSeed=TRestTools::GetRandomSeed();
    UpdateYAMLFromParams<TRestGeant4Metadata>(fNode);
    SyncActiveVolumesFromMetadata();
    LoadGeometryFromActiveFile();
}


/// \brief Copy constructor implementing full internal state replication.
TRestGeant4Metadata::TRestGeant4Metadata(const TRestGeant4Metadata& metadata) : TRestMetadata(metadata) {
    this->Merge(metadata);
}

/// \brief Assignment operator.
TRestGeant4Metadata& TRestGeant4Metadata::operator=(const TRestGeant4Metadata& metadata) {
    if (this != &metadata) {
        TRestMetadata::operator=(metadata);
        this->Merge(metadata);
    }
    return *this;
}

  /// \brief Synchronizes runtime active-volume caches from registry-managed entries.
void TRestGeant4Metadata::SyncActiveVolumesFromMetadata() {
    fActiveVolumes.clear();
    fChance.clear();
    fMaxStepSize.clear();
    fActiveVolumesSet.clear();
    for (const auto& volume : fActiveVolumesMetadata) {
        if (volume.fVolumeName.empty()) continue;
        SetActiveVolume(volume.fVolumeName, volume.fChance, volume.fMaxStep);
    }
    fKillVolumes.clear();
    fKillVolumes.insert(fKillVolumesRegistry.begin(), fKillVolumesRegistry.end());
    fFullChainStopIsotopes.clear();
    fFullChainStopIsotopes.insert(fFullChainStopIsotopesRegistry.begin(), fFullChainStopIsotopesRegistry.end());
}

  /// \brief Deep copies properties from an external metadata reference instance.
void TRestGeant4Metadata::Merge(const TRestGeant4Metadata& other) {
    fIsMerge = true;
    fGeant4GeometryInfo = other.fGeant4GeometryInfo;
    fGeant4PhysicsInfo = other.fGeant4PhysicsInfo;
    fVolumeNameMap = other.fVolumeNameMap;
    fProcessNamesMap = other.fProcessNamesMap;
    fParticleNamesMap = other.fParticleNamesMap;
    fProcessTypesMap = other.fProcessTypesMap;
    fGeant4PrimaryGeneratorInfo = other.fGeant4PrimaryGeneratorInfo;
    fGeant4Version = other.fGeant4Version;
    fGeometryPath = other.fGeometryPath;
    fGdmlFilename = other.fGdmlFilename;
    fEnergyRangeStored = other.fEnergyRangeStored;
    fActiveVolumesMetadata = other.fActiveVolumesMetadata;
    fActiveVolumes = other.fActiveVolumes;
    fChance = other.fChance;
    fMaxStepSize = other.fMaxStepSize;
    fNBiasingVolumes = other.fNBiasingVolumes;
    fBiasingVolumes = other.fBiasingVolumes;
    fMaxTargetStepSize = other.fMaxTargetStepSize;
    fSubEventTimeDelay = other.fSubEventTimeDelay;
    fFullChain = other.fFullChain;
    fResetGlobalTime = other.fResetGlobalTime;
    fResetTimePrecision = other.fResetTimePrecision;
    fFullChainStopIsotopes = other.fFullChainStopIsotopes;
		fFullChainStopIsotopesRegistry = other.fFullChainStopIsotopesRegistry;
    fSensitiveVolumes = other.fSensitiveVolumes;
    fRemoveUnwantedTracksVolumesToKeep = other.fRemoveUnwantedTracksVolumesToKeep;
    fKillVolumes = other.fKillVolumes;
		fKillVolumesRegistry = other.fKillVolumesRegistry;
    fNEvents += other.fNEvents;  // Cumulative update on events count
    fNRequestedEntries += other.fNRequestedEntries;
    fSimulationMaxTimeSeconds = other.fSimulationMaxTimeSeconds;
    fSimulationTime += other.fSimulationTime;
    fSeed = other.fSeed;
    fSaveAllEvents = other.fSaveAllEvents;
    fStoreHadronicTargetInfo = other.fStoreHadronicTargetInfo;
    fActivateAllVolumes = other.fActivateAllVolumes;
    fRemoveUnwantedTracks = other.fRemoveUnwantedTracks;
    fStoreTracks = other.fStoreTracks;
    fRemoveUnwantedTracksKeepZeroEnergyTracks = other.fRemoveUnwantedTracksKeepZeroEnergyTracks;
    fPrintProgress = other.fPrintProgress;
    fRegisterEmptyTracks = other.fRegisterEmptyTracks;
    fMagneticField = other.fMagneticField;
    fActiveVolumesSet = other.fActiveVolumesSet;

}

void TRestGeant4Metadata::RemoveParticleSources() {
    fGeant4PrimaryGeneratorInfo.RemoveParticleSources();
}

/// \brief Append a newly declared target particle source generator.
void TRestGeant4Metadata::AddParticleSource(TRestGeant4ParticleSource* src) {
    if (src) {
        fGeant4PrimaryGeneratorInfo.fParticleSources.push_back(*src);
        delete src;
    }
}

/// \brief Parses the local Major configuration value from the semantic version string.
size_t TRestGeant4Metadata::GetGeant4VersionMajor() const {
    if (fGeant4Version.empty()) return 0;
    try {
        size_t dotPos = fGeant4Version.find('.');
        if (dotPos != std::string::npos) {
            return std::stoul(fGeant4Version.substr(0, dotPos));
        }
        return std::stoul(fGeant4Version);
    } catch (...) {
        return 0;
    }
}

/// \brief Returns the active ID for an active geometric element volume.
int TRestGeant4Metadata::GetActiveVolumeID(const std::string& name) {
    auto it = std::find(fActiveVolumes.begin(), fActiveVolumes.end(), name);
    if (it != fActiveVolumes.end()) {
        return std::distance(fActiveVolumes.begin(), it);
    }
    return -1;
}

/// \brief Checks whether a given volume layout exists inside storage tracking.
bool TRestGeant4Metadata::isVolumeStored(const std::string& volume) const { return IsActiveVolume(volume); }

/// \brief Registers or updates tracking states on a structural simulation volume.
void TRestGeant4Metadata::SetActiveVolume(const std::string& name, double chance, double maxStep) {
    int id = GetActiveVolumeID(name);
    if (id >= 0) {
        fChance[id] = chance;
        fMaxStepSize[id] = maxStep;
    } else {
        fActiveVolumes.push_back(name);
        fChance.push_back(chance);
        fMaxStepSize.push_back(maxStep);
        fActiveVolumesSet.insert(name);
    }
}

/// \brief Extracts the tracking interaction registration probability chance.
double TRestGeant4Metadata::GetStorageChance(const std::string& volume) const {
    auto it = std::find(fActiveVolumes.begin(), fActiveVolumes.end(), volume);
    if (it != fActiveVolumes.end()) {
        return fChance.at(std::distance(fActiveVolumes.begin(), it));
    }
    return 0.0;
}

/// \brief Extracts the target max tracking step length limit size constraint.
double TRestGeant4Metadata::GetMaxStepSize(const std::string& volume) const {
    auto it = std::find(fActiveVolumes.begin(), fActiveVolumes.end(), volume);
    if (it != fActiveVolumes.end()) {
        return fMaxStepSize.at(std::distance(fActiveVolumes.begin(), it));
    }
    return 0.0;
}

//Automatic load of Geometry info in case a root file is opened
void TRestGeant4Metadata::LoadGeometryFromActiveFile() {
    if (gFile && !gFile->IsZombie()) {
        TGeoManager* geo = nullptr;
        
        gFile->GetObject("Geometry", geo);
        
        if (geo) {
            fGeant4GeometryInfo.LoadGeometry(geo);
            RESTInfo << "Geometry automatically loaded from ROOT file" << RESTendl;
            return;
        }
    }
    
    if (gGeoManager) {
        fGeant4GeometryInfo.LoadGeometry(gGeoManager);
    }
}

// Returns the computed generation surface area for cosmic run in cm^2
double TRestGeant4Metadata::GetCosmicGeneratorSurfaceCm2() const {
    return fGeant4PrimaryGeneratorInfo.GetSpatialGeneratorCosmicSurfaceTermCm2();
}
