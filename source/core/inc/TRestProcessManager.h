#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "TRestEventProcess.h"
#include "TRestMetadata.h"

class TRestRun;

/// \class TRestProcessManager
/// \brief Handles event-process pipeline loading and execution.
class TRestProcessManager : public TRestMetadata {
    DECLARE_LOG_CLASS(TRestProcessManager)

   private:
    std::vector<std::unique_ptr<TRestMetadata>> fMetaObjects;
    std::vector<std::unique_ptr<TRestEventProcess>> fProcessChain;

    std::map<std::string, std::unique_ptr<TRestEvent>> fEventPool;
    std::vector<std::pair<std::string, std::string>> fPipelineConnections;
    std::vector<std::string> fPipelineOutputClasses;

    TRestRun* fRunInfo = nullptr;

   public:
    int fEventsToProcess = 0;
    bool fInputAnalysisStorage = true;
    bool fInputEventStorage = false;
    bool fOutputEventStorage = true;

    TRestProcessManager();
    TRestProcessManager(const std::string& instanceName, const YAML::Node& node);
    TRestProcessManager(const std::string& fileName, const std::string& sectionName);
    ~TRestProcessManager() override = default;

    std::string GetClassName() const override { return "TRestProcessManager"; }

    void LoadConfig() override;
    void LoadProcesses();

    void SetRunInfo(TRestRun* runInfo) { fRunInfo = runInfo; }
    void Run();
    void Run(TRestRun& restRun) {
        SetRunInfo(&restRun);
        Run();
    }

    void Initialize() override {}
};
