#pragma once

#include <memory>
#include <vector>

#include "TRestEventProcess.h"
#include "TRestLogManager.h"
#include "TRestRun.h"
#include "TRestTools.h"

// ============================================================
//  TRestManager
//  Top-level orchestrator:  reads YAML, builds metadata objects
//  and the process chain, then drives the event loop.
// ============================================================
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

   public:
    // Constructor de 3 parámetros sincronizado con la factoría global
    TRestManager(const std::string& instanceName, const std::string& sectionName, const YAML::Node& node);
    TRestManager(const std::string& fileName, const std::string& sectionName);
    ~TRestManager() override = default;

    std::string GetClassName() const override { return "TRestManager"; }

    void Run(TRestRun& restRun);

    void LoadConfig() override;
    void Initialize() override {}
    void LoadPipeline(YAML::Node& pipeline);
    void LoadParams();
    void PrintMetadata() override;
};
