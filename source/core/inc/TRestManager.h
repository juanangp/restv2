#pragma once

#include <memory>
#include <vector>

#include "TRestEventProcess.h"
#include "TRestLogManager.h"
#include "TRestRun.h"
#include "TRestTools.h"

/// \class TRestManager
/// \brief Top-level orchestrator for metadata loading and pipeline execution.
///
/// `TRestManager` parses configuration, instantiates metadata objects and event
/// processes, and drives the run event loop through configured connections.
class TRestManager : public TRestMetadata {
    DECLARE_LOG_CLASS(TRestManager)

   private:
    int fEventsToProcess = 0;
    bool fInputAnalysisStorage = true;
    bool fInputEventStorage = true;
    bool fOutputEventStorage = false;

    YAML::Node fParams;

    std::vector<std::unique_ptr<TRestMetadata>> fMetaObjects;
    std::vector<std::unique_ptr<TRestEventProcess>> fProcessChain;

    std::map<std::string, std::unique_ptr<TRestEvent>> fEventPool;
    std::vector<std::pair<std::string, std::string>> fPipelineConnections;
    std::vector<std::string> fPipelineOutputClasses;

   public:
    /// \brief Constructor from YAML node.
    /// \param instanceName Instance name.
    /// \param node YAML node with manager configuration.
    TRestManager(const std::string& instanceName, const YAML::Node& node);

    /// \brief Constructor from YAML file and section.
    /// \param fileName YAML file path.
    /// \param sectionName YAML section name.
    TRestManager(const std::string& fileName, const std::string& sectionName);

    /// \brief Destructor.
    ~TRestManager() override = default;

    /// \brief Returns class name for metadata introspection.
    /// \return Class name string.
    std::string GetClassName() const override { return "TRestManager"; }

    /// \brief Executes configured process chain using a run context.
    /// \param restRun Run context.
    void Run(TRestRun& restRun);

    /// \brief Loads manager configuration from YAML node.
    void LoadConfig() override;

    /// \brief No-op initialize hook.
    void Initialize() override {}

    /// \brief Loads and instantiates pipeline stages from YAML section.
    /// \param pipeline Pipeline YAML node.
    void LoadPipeline(YAML::Node& pipeline);

    /// \brief Loads manager parameters from YAML.
    void LoadParams();

    /// \brief Prints manager metadata summary.
    void PrintMetadata() override;
};
