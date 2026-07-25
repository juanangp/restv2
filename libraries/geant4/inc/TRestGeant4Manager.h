#pragma once

#include <memory>
#include <vector>

#include "TRestGeant4Metadata.h"
#include "TRestGeant4PhysicsLists.h"
#include "TRestLogManager.h"
#include "TRestRun.h"

/// \class TRestGeant4Manager
/// \brief Top-level orchestrator that loads configured metadata classes.
///
/// `TRestGeant4Manager` parses configuration and instantiates metadata objects.
class TRestGeant4Manager : public TRestMetadata {
   public:
    /// \brief Pointer to Geant4 run metadata loaded from configuration.
    TRestGeant4Metadata* fG4Metadata = nullptr;
    /// \brief Pointer to configured Geant4 physics list collection.
    TRestGeant4PhysicsLists* fG4PhysicsLists = nullptr;
    /// \brief Pointer to the run metadata section owning this setup.
    TRestRun* fConfiguredRun = nullptr;

    TRestGeant4Manager(const std::string& instanceName, const YAML::Node& node);
    TRestGeant4Manager(const std::string& fileName, const std::string& sectionName);
    ~TRestGeant4Manager() override = default;

    std::string GetClassName() const override { return "TRestGeant4Manager"; }

    /// \brief Resolves linked metadata objects from the YAML section.
    void LoadConfig() override;
    /// \brief No-op for this manager: initialization is done during config loading.
    void Initialize() override {}
    /// \brief Prints discovered Geant4 metadata and physics list associations.
    void PrintMetadata() override;
    /// \brief Persists loaded metadata state back to the configured output run.
    void SaveMetadata();
};
