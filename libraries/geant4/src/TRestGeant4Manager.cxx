#include "TRestGeant4Manager.h"

using namespace TRestTools;

// ---------------------------------------------------------------------------
// Self-registration
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
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
    fConfiguredRun = nullptr;
    fG4PhysicsLists = nullptr;

    for (const auto& element : fNode) {
        const auto key = element.first.as<std::string>();
        auto value = element.second;

        if (!value || value.IsScalar() || !value.IsMap()) continue;

        auto meta = MetadataClassRegistry::Instance().Create(key, value);
        if (!meta) continue;

        if (auto* run = dynamic_cast<TRestRun*>(meta.get())) {
            fConfiguredRun = run;
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
    if (fConfiguredRun == nullptr) {
        throw std::runtime_error("TRestGeant4Manager::Run - no TRestRun configured under manager section.");
    }

    if (fG4PhysicsLists)
        fConfiguredRun->AddMetadata(fG4PhysicsLists->GetName(), fG4PhysicsLists->GetYAMLNode());
    if (fG4Metadata) fConfiguredRun->AddMetadata(fG4Metadata->GetName(), fG4Metadata->GetYAMLNode());
}

/// \brief Prints manager-level metadata and its effective YAML configuration.
void TRestGeant4Manager::PrintMetadata() {
    RESTMetadata << "=== TRestGeant4Manager ===" << RESTendl;
    if (fNode && !fNode.IsNull()) RESTMetadata << YAML::Dump(fNode) << RESTendl;
}
