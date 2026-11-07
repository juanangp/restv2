#ifndef TRESTEVENTPROCESSMANAGER_H
#define TRESTEVENTPROCESSMANAGER_H

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "TRestEventProcess.h"
#include "TRestMetadata.h"

class TRestRun;

/// \class TRestEventProcessManager
/// \brief Handles event-process pipeline loading and execution.
class TRestManager;

class TRestEventProcessManager : public TRestMetadata {
   private:
    std::vector<std::unique_ptr<TRestMetadata>> fMetaObjects;
    std::vector<std::unique_ptr<TRestEventProcess>> fProcessChain;

    std::vector<std::pair<std::string, std::string>> fPipelineConnections;
    std::vector<std::string> fPipelineOutputClasses;
    /// Output event of each stage: each stage has its own, so input and output are never the same event
    /// (processes of the same event type do not need to copy their input). Only the last stage of each
    /// event name is stored. Created in Run().
    std::vector<std::unique_ptr<TRestEvent>> fStageOutputs;
    /// Input event of each stage: output of the latest previous stage with that name, otherwise the
    /// input file event (nullptr for generators). Resolved in Run().
    std::vector<TRestEvent*> fStageInputs;

    TRestManager* fManager = nullptr;

    long long fEntryStart = -1;
    long long fEntryEnd = -1;

   public:
    int fEventsToProcess = 0;
    unsigned int fNThreads = 1;
    /// inputAnalysisStorage: the output analysis tree copies the observables of the input file (default).
    /// With false it only has the event info and the observables of the processes of this pipeline, as
    /// without input file; then TRestRun::GetObservable only finds the observables of the processes.
    bool fInputAnalysisStorage = true;
    bool fInputEventStorage = false;
    bool fOutputEventStorage = true;

    TRestEventProcessManager();
    TRestEventProcessManager(const std::string& instanceName, const YAML::Node& node);
    TRestEventProcessManager(const std::string& fileName, const std::string& sectionName);
    ~TRestEventProcessManager() override = default;

    std::string GetClassName() const override { return "TRestEventProcessManager"; }

    void LoadConfig() override;
    void LoadProcesses();

    void SetManager(TRestManager* mgr) { fManager = mgr; }
    void SetEntryRange(long long start, long long end) {
        fEntryStart = start;
        fEntryEnd = end;
    }

    /// \brief True if a process creates the events without input event (e.g. from a DAQ file): it reads
    /// them sequentially from the start, so the entries cannot be split among threads.
    bool HasGenerator() const {
        for (const auto& [input, output] : fPipelineConnections)
            if (input == "None") return true;
        return false;
    }

    /// \brief Output event of the last process of the chain (always stored), empty without processes.
    std::string GetOutputEvent() const {
        if (fPipelineConnections.empty()) return "";
        return fPipelineConnections.back().second;
    }

    void Run();
    void Run(TRestRun& restRun) {
        SetRunInfo(&restRun);
        Run();
    }

    void Initialize() override {}
};
#endif
