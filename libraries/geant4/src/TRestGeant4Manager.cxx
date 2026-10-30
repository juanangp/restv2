#include "TRestGeant4Manager.h"

#include "TRestGeant4Metadata.h"
#include "TRestGeant4PhysicsLists.h"
#include "TRestLogManager.h"
#include "TRestRun.h"

using namespace TRestTools;

// ---------------------------------------------------------------------------
// Self-registration
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    // The run, the Geant4 metadata and the physics lists
    TRestMetadataFieldRegistry::Instance().RegisterChildMetadata<TRestGeant4Manager>();
    MetadataClassRegistry::Instance().Register(
        "TRestGeant4Manager", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestGeant4Manager>(instanceName, params);
        });
    return true;
}();
}  // namespace

/// \brief Builds Geant4 manager metadata from an in-memory YAML node.
TRestGeant4Manager::TRestGeant4Manager(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

/// \brief Builds Geant4 manager metadata from a file section.
TRestGeant4Manager::TRestGeant4Manager(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

/// \brief Resolves and stores nested metadata blocks declared under the manager node.
void TRestGeant4Manager::LoadConfig() {
    fG4Metadata = nullptr;
    fRunInfo = nullptr;
    fG4PhysicsLists = nullptr;

    for (const auto& element : fNode) {
        const auto key = element.first.as<std::string>();
        auto value = element.second;

        if (!value || value.IsScalar() || !value.IsMap()) continue;

        auto meta = MetadataClassRegistry::Instance().Create(key, value);
        if (!meta) continue;

        if (auto* run = dynamic_cast<TRestRun*>(meta.get())) {
            fRunInfo = run;
            meta.release();
        }
        if (auto* pL = dynamic_cast<TRestGeant4PhysicsLists*>(meta.get())) {
            fG4PhysicsLists = pL;
            meta.release();
        }

        if (auto* G4Meta = dynamic_cast<TRestGeant4Metadata*>(meta.get())) {
            fG4Metadata = G4Meta;
            meta.release();
        }
    }
}

/// \brief Persists configured Geant4 metadata blocks into the associated run metadata.
void TRestGeant4Manager::SaveMetadata() {
    if (fRunInfo == nullptr) {
        throw std::runtime_error("TRestGeant4Manager::Run - no TRestRun configured under manager section.");
    }

    fG4Metadata->SyncMetadataMapsFromRuntime();
    fRunInfo->AddMetadata(fRunInfo);
    if (fG4PhysicsLists) fRunInfo->AddMetadata(fG4PhysicsLists);
    if (fG4Metadata) fRunInfo->AddMetadata(fG4Metadata);
}

/// \brief Prints manager-level metadata and its effective YAML configuration.
void TRestGeant4Manager::PrintMetadata() const {
    RESTMetadata << "=== TRestGeant4Manager ===" << RESTendl;
    if (fRunInfo) fRunInfo->PrintMetadata();
    if (fG4Metadata) fG4Metadata->PrintMetadata();
    if (fG4PhysicsLists) fG4PhysicsLists->PrintMetadata();
}
