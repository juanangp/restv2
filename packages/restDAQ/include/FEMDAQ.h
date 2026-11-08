#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "FEMProxy.h"
#include "PrometheusManager.h"
#include "RunConfig.h"
#include "TRestRawDAQSignalEvent.h"
#include "TRestTools.h"

/// \file FEMDAQ.h
/// \brief Base class of the data taking of restDAQ (femclient).

/// \class FEMDAQ
/// \brief Data taking of a set of FEMs: commands, event builder and output files. The derived classes
/// implement each electronics (FEMDAQARCFEM: ARC and FEMINOS, FEMDAQDCC: DCC, TCM).
///
/// The files are REST run files (TRestRun of the configuration): the events in the tree
/// TRestRawSignalEvent (written as TRestRawDAQSignalEvent, with the information of each FEM), and the
/// TRestRun and TRestRawDAQMetadata as metadata.
///
/// During a run (startDAQ) two threads of this class run besides the ones of the electronics: the event
/// builder (eventBuilderThread, implemented by the derived class), which builds the events from the data
/// of the FEMs and writes them, and the update thread (UpdateRunThread, UpdateThread), which writes the
/// rate, updates the Prometheus metrics and ends the run at nEvents or time.
class FEMDAQ {
   public:
    /// The FEMs (or the TCM); a deque so they are never moved (their threads hold references to them)
    std::deque<FEMProxy> FEMArray;

    /// \brief Stops the threads and closes the files (with the end time of the run if not set)
    virtual ~FEMDAQ();

    /// \brief `fem` command: the FEM with this ID becomes the only active one, `*` activates all of them.
    /// An ID not found disables all the FEMs; an ID that is not a number does nothing
    void setActiveFEM(const std::string& FEMID);

    /// \brief `startDAQ` command: starts the event builder and the update thread and runs the acquisition
    /// until the end of the run (nEvents, time, CTRL-C or an error); then stops the threads (stopDAQ)
    /// \param flags options of the electronics (DCC: `internal`, `ZS`)
    virtual void startDAQ(const std::vector<std::string>& flags) = 0;
    /// \brief Stops the event builder and the update thread (waits for them); nothing if they are not
    /// running. startDAQ calls it when the run ends, so the files can be closed after it.
    void stopDAQ();
    /// \brief Sends a command (a line not handled by CommandFetcher) to the active FEMs (or the TCM) and
    /// waits for the reply; throws on an error or a timeout
    virtual void SendCommand(const char* cmd) = 0;
    /// \brief `Pedestals` command (DCC only): pedestal acquisition and thresholds
    virtual void Pedestals(const std::vector<std::string>& flags) {
        std::cout << "Not implemented in this electronics" << std::endl;
    }

    /// Abort (CTRL-C, CommandFetcher::requestShutdown): stops the acquisition and the threads
    static std::atomic<bool> abrt;
    /// End of the run: set by the update thread (nEvents, time), by stopDAQ or by a failed thread
    static std::atomic<bool> stopRun;
    /// The event builder writes the data left and ends
    static std::atomic<bool> stopEventBuilder;
    /// Events with signals of the current run (written if a file is open)
    std::atomic<uint32_t> storedEvents{0};

    /// \brief Whether a REST file is open (TRestRun output file)
    bool HasRootFile() const { return runConfig.run && runConfig.run->HasOutputFileOpen(); }

    /// \brief Opens the output file of the current run and subrun (outputFileName of the TRestRun),
    /// registers the event tree and writes the TRestRun and the TRestRawDAQMetadata
    void OpenRootFile();
    /// \brief Writes the metadata again and closes the output file, if open
    void CloseRootFile();

    /// \brief Opens (append) a log file per FEM, named as the first file of the run (`_FEM<id>.log`,
    /// `_TCM.log`) or with the current time, and dumps the command file being executed in it
    void OpenFileLogs();
    /// \brief Copies the command file being executed (exec) in the log file of the FEM
    void DumpExecFileToFEMLog(FEMProxy& FEM);
    /// \brief Closes the log files of the FEMs
    void CloseLogFiles();

    /// \brief `fopen` command: a new run (RunConfig::NewRun) and its files
    /// \param flags `all` or empty (REST file and logs), `root` (REST file), `log` (log files); throws for
    /// another flag or if the mainDataPath does not exist. The TCM only opens its log
    void OpenFiles(const std::vector<std::string>& flags);
    /// \brief `fclose` command: closes the REST file and the log files
    void CloseFiles();

    /// \brief When the output file reaches the maximum size, closes it and opens the next subrun
    /// \param eventTime time of the last event, end time of the file and start time of the next one
    void CheckFileSize(const double eventTime);

    /// \brief The command file being executed, copied in the log files (empty: none)
    inline void SetExecFile(const std::string& execF) { execFile = execF; }

    /// \brief Start time of the run in the TRestRun, if a file is open
    void WriteRunStartTime(const double startTime);
    /// \brief End time of the run in the TRestRun, if a file is open
    void WriteRunEndTime(const double endTime);
    /// \brief Writes the current event (sEvent) in the output file. The trees are saved (AutoSave)
    /// every 1000 events or 60 s, so the file can be read while it is taken
    /// \param eventTime time of the event (s)
    /// \param lastTimeSaved time of the last AutoSave, updated
    void FillTree(const double eventTime, double& lastTimeSaved);
    /// \brief Function of the update thread: every updateRate writes the number of events and the rate,
    /// updates the Prometheus metrics, and sets stopRun when nEvents or the time of the run are reached
    void UpdateThread();
    /// \brief `addMetadata` command (RunConfig::UpdateInfo)
    void UpdateRunConfigInfo() { runConfig.UpdateInfo(); }

    /// \brief `credits restore`: default request credits (ARC/FEMINOS)
    virtual void CreditsRestore() {
        std::cout << "WARNING credits restore not implemented in this electronics" << std::endl;
    }

    /// \brief `credits restore frameThr reqFrames`: request credits (ARC/FEMINOS)
    /// \param reqFrames frames requested to the FEM in the first request
    /// \param frameThr frames received that trigger a new request (of frameThr frames)
    virtual void CreditsRestore(uint32_t reqFrames, uint32_t frameThr) {
        std::cout << "WARNING credits restore not implemented in this electronics" << std::endl;
    }

    /// \brief `credits show`: writes the request credits (ARC/FEMINOS)
    virtual void CreditsShow() {
        std::cout << "WARNING credits not implemented in this electronics" << std::endl;
    }

    /// \brief Whether femclient runs with --read-only (fopen, fclose and addMetadata do nothing)
    bool isReadOnly() const { return runConfig.readOnly; }

    std::thread UpdateRunThread;     ///< Update thread (UpdateThread) of the current run
    std::thread eventBuilderThread;  ///< Event builder thread of the current run

    /// \brief Runs the function of a thread: an exception stops the run and is written, instead of
    /// ending the program (std::terminate)
    static void RunThread(const std::string& name, const std::function<void()>& function);
    /// \brief Writes a message in the log file of a FEM (guarded by its logMutex)
    static void WriteLog(FEMProxy& FEM, const std::string& message);

    /// \brief The DAQ of the electronics of the configuration (RunConfig::electronics): FEMDAQARCFEM (ARC,
    /// FEMINOS), FEMDAQDCC (DCC) or TCM. Throws for another one.
    static std::unique_ptr<FEMDAQ> Create(RunConfig& cfg);

   protected:
    /// \brief Opens a socket to each FEM of the TRestRawDAQMetadata (or to the TCM) and starts the
    /// Prometheus server (not for the TCM). Throws if there is no FEM
    explicit FEMDAQ(RunConfig& rC);
    RunConfig& runConfig;           ///< Configuration (not owned)
    double runStartTime = 0;        ///< Unix time of the start of the run (startDAQ)
    double runEndTime = 0;          ///< Unix time of the end of the run (stopDAQ)
    std::string fileNameRoot = "";  ///< Name of the output file open
    /// Output file of the first subrun of the run, for the names of the log files
    std::string baseFileName = "";
    std::string execFile = "";
    /// Event being built, written in the tree TRestRawSignalEvent
    TRestRawDAQSignalEvent sEvent;
    std::atomic<int> nOutOfSync{0};  ///< Events of the run with the FEMs out of sync

    PrometheusManager prometheus;  ///< Metrics of the run

    std::condition_variable cv;  ///< Wakes up the update thread at the end of the run
    std::mutex cv_m;             ///< Mutex of cv
};
