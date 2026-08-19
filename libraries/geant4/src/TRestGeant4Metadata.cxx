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
static const bool TRestGeant4Metadata_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();

    // Core parameters mapping
    reg.RegisterField<TRestGeant4Metadata>("geant4Version", &TRestGeant4Metadata::fGeant4Version);
    reg.RegisterField<TRestGeant4Metadata>("geometryPath", &TRestGeant4Metadata::fGeometryPath);
    reg.RegisterField<TRestGeant4Metadata>("gdmlFilename", &TRestGeant4Metadata::fGdmlFilename);
    reg.RegisterField<TRestGeant4Metadata>("gdmlReference", &TRestGeant4Metadata::fGdmlReference);
    reg.RegisterField<TRestGeant4Metadata>("materialsReference", &TRestGeant4Metadata::fMaterialsReference);

    // Standard Library components (std::pair handles seamlessly via reflection)
    reg.RegisterField<TRestGeant4Metadata>("energyRangeStored", &TRestGeant4Metadata::fEnergyRangeStored);

    // MathCore components
    reg.RegisterField<TRestGeant4Metadata>("magneticField", &TRestGeant4Metadata::fMagneticField);

    // Physics and simulation chains
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

    return true;
}();

/// \brief Default constructor.
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
    fActiveVolumes.clear();
    fChance.clear();
    fMaxStepSize.clear();
    fBiasingVolumes.clear();
    fSensitiveVolumes.clear();
    fFullChainStopIsotopes.clear();
    fRemoveUnwantedTracksVolumesToKeep.clear();
    fKillVolumes.clear();
    fActiveVolumesSet.clear();
    fGeant4PrimaryGeneratorInfo.RemoveParticleSources();
}

/// \brief Modern framework configuration entry point.
void TRestGeant4Metadata::LoadConfig() {
    // If the generator type is a pure zero-size point, we explicitly inject safe fallback fields
    // so the internal StringToSpatialGeneratorShapes parser doesn't trigger an out-of-range memory collision.
    if (fNode["generator"] && fNode["generator"].IsMap()) {
        auto mutableGenerator = fNode["generator"];
        if (mutableGenerator["type"] && mutableGenerator["type"].as<std::string>() == "point") {
            if (!mutableGenerator["shape"]) {
                mutableGenerator["shape"] = "box"; // Inject safe enum string template
            }
            if (!mutableGenerator["size"]) {
                mutableGenerator["size"] = std::vector<double>{0.0, 0.0, 0.0}; // Consolidated spatial footprint
            }
        }
    }

    UpdateParamsFromYAML<TRestGeant4Metadata>(fNode);

    if (fNode["generator"] && fNode["generator"].IsMap()) {
        fGeant4PrimaryGeneratorInfo.SetYAMLNode(fNode["generator"]);
        fGeant4PrimaryGeneratorInfo.LoadConfig();
    }

    if (fNode["activeVolumes"] && fNode["activeVolumes"].IsSequence()) {
        for (const auto& volNode : fNode["activeVolumes"]) {
            if (volNode.IsMap() && volNode["name"]) {
                std::string vName = volNode["name"].as<std::string>();
                double chance = volNode["chance"] ? volNode["chance"].as<double>() : 1.0;
                double maxStep = volNode["maxStep"] ? volNode["maxStep"].as<double>() : 0.0;
                SetActiveVolume(vName, chance, maxStep);
            } else if (volNode.IsScalar()) {
                SetActiveVolume(volNode.as<std::string>(), 1.0, 0.0);
            }
        }
    }

    if (fNode["sensitiveVolumes"] && fNode["sensitiveVolumes"].IsSequence()) {
        for (const auto& sNode : fNode["sensitiveVolumes"]) {
            InsertSensitiveVolume(sNode.as<std::string>());
        }
    }

    if (fNode["killVolumes"] && fNode["killVolumes"].IsSequence()) {
        for (const auto& kNode : fNode["killVolumes"]) {
            fKillVolumes.insert(kNode.as<std::string>());
        }
    }

    if (fNode["fullChainStopIsotopes"] && fNode["fullChainStopIsotopes"].IsSequence()) {
        for (const auto& isoNode : fNode["fullChainStopIsotopes"]) {
            fFullChainStopIsotopes.insert(isoNode.as<std::string>());
        }
    }

    ReadYAMLVerbose(fNode);

    // Dynamic field state synchronization back onto the nodes
    UpdateYAMLFromParams<TRestGeant4Metadata>(fNode);

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

/// \brief Deep copies properties from an external metadata reference instance.
void TRestGeant4Metadata::Merge(const TRestGeant4Metadata& other) {
    fIsMerge = true;
    fGeant4GeometryInfo = other.fGeant4GeometryInfo;
    fGeant4PhysicsInfo = other.fGeant4PhysicsInfo;
    fGeant4PrimaryGeneratorInfo = other.fGeant4PrimaryGeneratorInfo;
    fGeant4Version = other.fGeant4Version;
    fGeometryPath = other.fGeometryPath;
    fGdmlFilename = other.fGdmlFilename;
    fGdmlReference = other.fGdmlReference;
    fMaterialsReference = other.fMaterialsReference;
    fEnergyRangeStored = other.fEnergyRangeStored;
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
    fSensitiveVolumes = other.fSensitiveVolumes;
    fRemoveUnwantedTracksVolumesToKeep = other.fRemoveUnwantedTracksVolumesToKeep;
    fKillVolumes = other.fKillVolumes;
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
    if (src) fGeant4PrimaryGeneratorInfo.fParticleSources.push_back(src);
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

/// \brief Computes cosmic tracking exposure parameters (Placeholder implementations).
double TRestGeant4Metadata::GetGeneratorSurfaceCm2() const { return 1.0; }
double TRestGeant4Metadata::GetCosmicFluxInCountsPerCm2PerSecond() const { return 0.0; }
double TRestGeant4Metadata::GetCosmicIntensityInCountsPerSecond() const { return 0.0; }
double TRestGeant4Metadata::GetEquivalentSimulatedTime() const { return 0.0; }

/// \brief Outputs the active parameters onto the standard framework streams log.
void TRestGeant4Metadata::PrintMetadata() {
    RESTMetadata << "=== TRestGeant4Metadata ===" << RESTendl;
    RESTMetadata << " - Geant4 Version: " << fGeant4Version << RESTendl;
    RESTMetadata << " - Generated Events: " << fNEvents << RESTendl;
    RESTMetadata << " - Total Simulation Time (Wall): " << fSimulationTime << " s" << RESTendl;
    if (fNode && !fNode.IsNull()) {
        RESTMetadata << "=== Detailed Configuration Nodes ===" << RESTendl;
        RESTMetadata << YAML::Dump(fNode) << RESTendl;
    }
}
