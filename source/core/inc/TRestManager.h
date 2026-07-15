#pragma once

#include <memory>
#include <vector>

#include "TRestLogManager.h"
#include "TRestRun.h"
#include "TRestTools.h"

class TRestProcessManager;

/// \class TRestManager
/// \brief Top-level orchestrator that loads configured metadata classes.
///
/// `TRestManager` parses configuration and instantiates metadata objects.
/// Event pipeline execution is delegated to `TRestProcessManager`.
class TRestManager : public TRestMetadata {
    DECLARE_LOG_CLASS(TRestManager)

   private:
    std::vector<std::unique_ptr<TRestMetadata>> fMetaObjects;
    TRestProcessManager* fProcessManager = nullptr;
    TRestRun* fConfiguredRun = nullptr;

   public:
    TRestManager(const std::string& instanceName, const YAML::Node& node);
    TRestManager(const std::string& fileName, const std::string& sectionName);
    ~TRestManager() override = default;

    std::string GetClassName() const override { return "TRestManager"; }

    void Run();

    void LoadConfig() override;
    void Initialize() override {}
    void SaveMetadata();
};
