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
    int  fEventsToProcess       = 0;
    bool fInputAnalysisStorage  = true;
    bool fInputEventStorage     = true;
    bool fOutputEventStorage    = false;

    YAML::Node fParams;

    std::vector<std::unique_ptr<TRestMetadata>>      fMetaObjects;
    std::vector<std::unique_ptr<TRestEventProcess>>  fProcessChain;

   public:
    explicit TRestManager(const std::string& sectionName, const YAML::Node& node);
    explicit TRestManager(const std::string& fileName,
                          const std::string& sectionName = "");

    std::string GetClassName() const override { return "TRestManager"; }

    void LoadConfig()    override;
    void Initialize()    override {}
    void PrintMetadata() override;

   private:
    void LoadPipeline(YAML::Node& pipeline);
    void LoadParams();
};
