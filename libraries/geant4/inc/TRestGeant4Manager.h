#pragma once

#include <memory>
#include <vector>

#include "TRestLogManager.h"
#include "TRestRun.h"
#include "TRestGeant4Metadata.h"
#include "TRestGeant4PhysicsLists.h"

/// \class TRestGeant4Manager
/// \brief Top-level orchestrator that loads configured metadata classes.
///
/// `TRestGeant4Manager` parses configuration and instantiates metadata objects.
class TRestGeant4Manager : public TRestMetadata {

   public:
    TRestGeant4Metadata* fG4Metadata = nullptr;
    TRestGeant4PhysicsLists* fG4PhysicsLists = nullptr;
    TRestRun* fConfiguredRun = nullptr;

    TRestGeant4Manager(const std::string& instanceName, const YAML::Node& node);
    TRestGeant4Manager(const std::string& fileName, const std::string& sectionName);
    ~TRestGeant4Manager() override = default;

    std::string GetClassName() const override { return "TRestGeant4Manager"; }

    void LoadConfig() override;
    void Initialize() override {}
    void PrintMetadata() override;
    void SaveMetadata();
};

