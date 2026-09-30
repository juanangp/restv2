#ifndef TRESTPROCESSMANAGER_H
#define TRESTPROCESSMANAGER_H

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
class TRestManager;

class TRestProcessManager : public TRestMetadata {
    DECLARE_LOG_CLASS(TRestProcessManager)

   private:
    std::vector<std::unique_ptr<TRestMetadata>> fMetaObjects;
    std::vector<std::unique_ptr<TRestEventProcess>> fProcessChain;

    std::map<std::string, std::unique_ptr<TRestEvent>> fEventPool;
    std::vector<std::pair<std::string, std::string>> fPipelineConnections;
    std::vector<std::string> fPipelineOutputClasses;

    TRestRun* fRunInfo = nullptr;
    TRestManager* fManager = nullptr;

    Long64_t fEntryStart = -1;
    Long64_t fEntryEnd = -1;

   public:
    int fEventsToProcess = 0;
    unsigned int fNThreads = 1;
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
    void SetManager(TRestManager* mgr) { fManager = mgr; }
    void SetEntryRange(Long64_t start, Long64_t end) { fEntryStart = start; fEntryEnd = end; }

    void Run();
    void Run(TRestRun& restRun) {
        SetRunInfo(&restRun);
        Run();
    }

    void Initialize() override {}
};
#endif
