#pragma once

#include <atomic>
#include <condition_variable>
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

/// \brief Data taking of a set of FEMs: commands, event builder and output files. The derived classes
/// implement each electronics (FEMDAQARCFEM: ARC and FEMINOS, FEMDAQDCC: DCC, TCM).
///
/// The files are REST run files (TRestRun of the configuration): the events in the tree
/// TRestRawSignalEvent (written as TRestRawDAQSignalEvent, with the information of each FEM), and the
/// TRestRun and TRestRawDAQMetadata as metadata.
class FEMDAQ {
   public:
    std::vector<FEMProxy> FEMArray;

    virtual ~FEMDAQ() {
        if (HasRootFile()) WriteRunEndTime(getCurrentTime());
        CloseFiles();
    }

    void setActiveFEM(const std::string& FEMID);

    virtual void startDAQ(const std::vector<std::string>& flags) = 0;
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

    static double getCurrentTime();
    static std::string GetTimeStampFromUnixTime(const double tm);
    static std::string FormatElapsedTime(const double seconds);

    std::thread UpdateRunThread;
    std::thread eventBuilderThread;

    using FactoryFunc = std::function<std::unique_ptr<FEMDAQ>(RunConfig&)>;

    // Factory method using RunConfig.electronics
    static std::unique_ptr<FEMDAQ> Create(RunConfig& cfg) {
        auto it = GetRegistry().find(cfg.electronics);
        if (it != GetRegistry().end()) {
            return it->second(cfg);
        }
        std::cout << cfg.electronics << " not implemented, skipping..." << std::endl;
        return nullptr;
    }

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
    int nOutOfSync = 0;

    PrometheusManager prometheus;

    std::condition_variable cv;
    std::mutex cv_m;

    static bool RegisterType(const std::string& electronics, FactoryFunc func) {
        GetRegistry()[electronics] = func;
        return true;
    }

   private:
    static std::map<std::string, FactoryFunc>& GetRegistry() {
        static std::map<std::string, FactoryFunc> registry;
        return registry;
    }
};
