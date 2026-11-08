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

/// \brief Data taking of a set of FEMs: commands, event builder and output files. The derived classes
/// implement each electronics (FEMDAQARCFEM: ARC and FEMINOS, FEMDAQDCC: DCC, TCM).
///
/// The files are REST run files (TRestRun of the configuration): the events in the tree
/// TRestRawSignalEvent (written as TRestRawDAQSignalEvent, with the information of each FEM), and the
/// TRestRun and TRestRawDAQMetadata as metadata.
class FEMDAQ {
   public:
    /// The FEMs (or the TCM); a deque so they are never moved (their threads hold references to them)
    std::deque<FEMProxy> FEMArray;

    virtual ~FEMDAQ();

    void setActiveFEM(const std::string& FEMID);

    virtual void startDAQ(const std::vector<std::string>& flags) = 0;
    /// \brief Stops the event builder and the update thread (waits for them); nothing if they are not
    /// running. startDAQ calls it when the run ends, so the files can be closed after it.
    void stopDAQ();
    virtual void SendCommand(const char* cmd) = 0;
    virtual void Pedestals(const std::vector<std::string>& flags) {
        std::cout << "Not implemented in this electronics" << std::endl;
    }

    static std::atomic<bool> abrt;
    static std::atomic<bool> stopRun;
    static std::atomic<bool> stopEventBuilder;
    std::atomic<uint32_t> storedEvents{0};

    /// \brief Whether a REST file is open (TRestRun output file)
    bool HasRootFile() const { return runConfig.run && runConfig.run->HasOutputFileOpen(); }

    void OpenRootFile();
    void CloseRootFile();

    void OpenFileLogs();
    void DumpExecFileToFEMLog(FEMProxy& FEM);
    void CloseLogFiles();

    void OpenFiles(const std::vector<std::string>& flags);
    void CloseFiles();

    void CheckFileSize(const double eventTime);

    inline void SetExecFile(const std::string& execF) { execFile = execF; }

    void WriteRunStartTime(const double startTime);
    void WriteRunEndTime(const double endTime);
    void FillTree(const double eventTime, double& lastTimeSaved);
    void UpdateThread();
    void UpdateRunConfigInfo() { runConfig.UpdateInfo(); }

    virtual void CreditsRestore() {
        std::cout << "WARNING credits restore not implemented in this electronics" << std::endl;
    }

    virtual void CreditsRestore(uint32_t reqFrames, uint32_t frameThr) {
        std::cout << "WARNING credits restore not implemented in this electronics" << std::endl;
    }

    virtual void CreditsShow() {
        std::cout << "WARNING credits not implemented in this electronics" << std::endl;
    }

    bool isReadOnly() const { return runConfig.readOnly; }

    std::thread UpdateRunThread;
    std::thread eventBuilderThread;

    /// \brief Runs the function of a thread: an exception stops the run and is written, instead of
    /// ending the program (std::terminate)
    static void RunThread(const std::string& name, const std::function<void()>& function);
    /// \brief Writes a message in the log file of a FEM (guarded by its logMutex)
    static void WriteLog(FEMProxy& FEM, const std::string& message);

    /// \brief The DAQ of the electronics of the configuration (RunConfig::electronics): FEMDAQARCFEM (ARC,
    /// FEMINOS), FEMDAQDCC (DCC) or TCM. Throws for another one.
    static std::unique_ptr<FEMDAQ> Create(RunConfig& cfg);

   protected:
    explicit FEMDAQ(RunConfig& rC);
    RunConfig& runConfig;
    double runStartTime = 0;
    double runEndTime = 0;
    std::string fileNameRoot = "";
    /// Output file of the first subrun of the run, for the names of the log files
    std::string baseFileName = "";
    std::string execFile = "";
    /// Event being built, written in the tree TRestRawSignalEvent
    TRestRawDAQSignalEvent sEvent;
    std::atomic<int> nOutOfSync{0};

    PrometheusManager prometheus;

    std::condition_variable cv;
    std::mutex cv_m;
};
