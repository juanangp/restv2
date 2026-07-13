#pragma once

#include "TRestEvent.h"
#include "TRestMetadata.h"

class TRestRun;

/// \class TRestProcessManager
/// \brief Abstract base for event-processing pipeline stages.
///
class TRestProcessManager : public TRestMetadata {
    DECLARE_LOG_CLASS(TRestProcessManager)
   private:
    std::vector<std::unique_ptr<TRestMetadata>> fMetaObjects;
    std::vector<std::unique_ptr<TRestEventProcess>> fProcessChain;

    std::map<std::string, std::unique_ptr<TRestEvent>> fEventPool;
    std::vector<std::pair<std::string, std::string>> fPipelineConnections;
    std::vector<std::string> fPipelineOutputClasses;

    TRestRun *fRunInfo = nullptr;

   public:
    int fEventsToProcess = 0;
    bool fInputAnalysisStorage = true;
    bool fInputEventStorage = false;
    bool fOutputEventStorage = true;

    TRestProcessManager();
    TRestProcessManager(const std::string& instanceName, const YAML::Node& node);
    TRestProcessManager(const std::string& fileName, const std::string& sectionName);
    ~TRestProcessManager() override = default;

    /// \brief No-op configuration loader for process instances.
    virtual void LoadConfig() override;

    void LoadProcesses();
    void Run();

    /// \brief Initializes this process by delegating to `InitProcess`.
    void Initialize() override {  }
};
