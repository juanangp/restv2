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

class TRestManager;

/// \class TRestEventProcessManager
/// \brief Handles event-process pipeline loading and execution.
///
/// Each map with `class` of its section is created as a metadata: the TRestEventProcess form the
/// chain in declaration order, the other metadata are kept (e.g. a readout used by the processes, also
/// reachable through the TRestManager). Each process reads the output event of the last previous process
/// with its input event name, otherwise the event of the input file.
///
/// \code{.yaml}
/// pipeline:
///   class: TRestEventProcessManager
///   eventsToProcess: 0            # 0: all the entries of the input
///   nThreads: 1                   # >1: the entries are split among threads (TRestManager)
///   inputAnalysisStorage: true    # copy the observables of the input file to the output analysis tree
///   inputEventStorage: false      # also store the events of the input file
///   outputEventStorage: true      # store the output event of every event type (false: only the last)
///   peaksFinder:
///     class: TRestDetectorSignalPeaksFinderProcess
///     ...
/// \endcode
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
    /// Maximum number of events to process (`eventsToProcess`), 0 for all.
    int fEventsToProcess = 0;
    /// Number of threads (`nThreads`); the split is done by TRestManager::Run.
    unsigned int fNThreads = 1;
    /// inputAnalysisStorage: the output analysis tree copies the observables of the input file (default).
    /// With false it only has the event info and the observables of the processes of this pipeline, as
    /// without input file; then TRestRun::GetObservable only finds the observables of the processes.
    bool fInputAnalysisStorage = true;
    /// inputEventStorage: the events of the input file are also stored in the output, unless a stored
    /// process output has the same event name.
    bool fInputEventStorage = false;
    /// outputEventStorage: the output of every event type of the chain is stored (its last stage); with
    /// false only the output event of the last process.
    bool fOutputEventStorage = true;

    TRestEventProcessManager();  ///< Default constructor.
    /// \brief Constructs from a YAML node and loads the processes (LoadConfig).
    TRestEventProcessManager(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructs from a section of a YAML file and loads the processes (LoadConfig).
    TRestEventProcessManager(const std::string& fileName, const std::string& sectionName);
    ~TRestEventProcessManager() override = default;  ///< Destructor.

    /// \brief Returns "TRestEventProcessManager".
    std::string GetClassName() const override { return "TRestEventProcessManager"; }

    /// \brief Reads the fields and creates the processes and metadata of the section (LoadProcesses).
    void LoadConfig() override;
    /// \brief Creates the metadata of the section (maps with `class`, inheriting the verbose of the
    /// manager) and builds the process chain and its event routes. Throws if the first process has no input
    /// event.
    void LoadProcesses();

    /// \brief Sets the TRestManager given to the processes (access to its metadata and cuts).
    void SetManager(TRestManager* mgr) { fManager = mgr; }
    /// \brief Restricts the event loop to the entries [start, end) (-1: not set), used for threads.
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

    /// \brief Runs the pipeline on the run set with SetRunInfo (throws if none): opens the output file if
    /// needed, initializes the processes and their cuts, creates the stage events, registers the stored
    /// events, and loops over the entries (or until a generator process finishes its input), filling the
    /// accepted events. Ends with EndProcess of every process. The loop stops on
    /// TRestManager::RequestStop.
    void Run();
    /// \brief Sets the run and runs the pipeline on it.
    /// \param restRun Run with the input and output files.
    void Run(TRestRun& restRun) {
        SetRunInfo(&restRun);
        Run();
    }

    /// \brief Nothing to initialize (the processes are initialized in Run).
    void Initialize() override {}
};
#endif
