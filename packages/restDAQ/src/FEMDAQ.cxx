#include "FEMDAQ.h"

#include <Compression.h>
#include <TFile.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "FEMDAQARCFEM.h"
#include "FEMDAQDCC.h"
#include "TCM.h"

std::atomic<bool> FEMDAQ::abrt(false);
std::atomic<bool> FEMDAQ::stopRun(false);
std::atomic<bool> FEMDAQ::stopEventBuilder(false);

namespace {
/// Tree of the events, read as a TRestRawSignalEvent
constexpr const char* kEventTree = "TRestRawSignalEvent";
}  // namespace

FEMDAQ::FEMDAQ(RunConfig& rC) : runConfig(rC) {
    std::cout << "------------ " << rC.electronics << " RUN ------------" << std::endl;

    // Built in place (a deque does not move its elements, FEMProxy cannot be copied nor moved)
    if (runConfig.isTCM) {
        auto& FEM = FEMArray.emplace_back();
        FEM.Open(runConfig.daq->fTCM);
        FEM.femID = -1;
    } else {
        for (const auto& fem : runConfig.daq->fFEMs) {
            auto& FEM = FEMArray.emplace_back();
            FEM.Open(fem.fIP);
            FEM.femID = fem.fId;
        }
        if (!runConfig.daq->fPrometheusAddress.empty())
            prometheus.StartServer(runConfig.daq->fPrometheusAddress);
    }

    if (FEMArray.empty()) {
        throw std::runtime_error(
            "No valid FEM/TCM configured for this client, "
            "please check your config file");
    }
}

std::unique_ptr<FEMDAQ> FEMDAQ::Create(RunConfig& cfg) {
    if (cfg.electronics == "ARC" || cfg.electronics == "FEMINOS") return std::make_unique<FEMDAQARCFEM>(cfg);
    if (cfg.electronics == "DCC") return std::make_unique<FEMDAQDCC>(cfg);
    if (cfg.electronics == "TCM") return std::make_unique<TCM>(cfg);
    throw std::runtime_error("electronics \"" + cfg.electronics + "\" not implemented");
}

void FEMDAQ::setActiveFEM(const std::string& FEMID) {
    if (FEMID == "*") {
        for (auto& FEM : FEMArray) {
            FEM.active = true;
        }
    } else {
        int fID;
        auto result = std::from_chars(FEMID.data(), FEMID.data() + FEMID.size(), fID);
        if (result.ec == std::errc()) {
            bool found = false;
            for (auto& FEM : FEMArray) {
                if (FEM.femID == fID) {
                    FEM.active = true;
                    found = true;
                } else {
                    FEM.active = false;
                }
            }
            if (!found)
                std::cout << "Warning FEMID " << FEMID << "/" << fID
                          << " not found, all FEMs will be disabled!! " << std::endl;
        } else {
            std::cout << "Warning cannot decode FEM ID from string " << FEMID << " doing nothing!!!"
                      << std::endl;
        }
    }
}

void FEMDAQ::WriteRunStartTime(const double startTime) {
    if (!HasRootFile()) return;
    runConfig.run->SetStartTimeStamp(startTime);
}

void FEMDAQ::WriteRunEndTime(const double endTime) {
    if (!HasRootFile()) return;
    runConfig.run->SetEndTimeStamp(endTime);
}

void FEMDAQ::OpenFiles(const std::vector<std::string>& flags) {
    if (runConfig.isTCM) {
        // The TCM only writes its log, in the current directory
        CloseLogFiles();
        baseFileName.clear();
        OpenFileLogs();
        return;
    }

    const std::string dataPath = runConfig.run->fMainDataPath;
    if (!dataPath.empty() && !std::filesystem::exists(dataPath)) {
        throw std::runtime_error("Data path: " + dataPath + " does not exist");
    }

    fileNameRoot.clear();
    baseFileName.clear();

    CloseLogFiles();
    CloseRootFile();

    bool openRootFile = false;
    bool openLogFile = false;

    if (flags.empty()) {
        openRootFile = true;
        openLogFile = true;
    } else {
        for (const auto& flag : flags) {
            if (flag == "all") {
                openRootFile = true;
                openLogFile = true;
            } else if (flag == "root") {
                openRootFile = true;
            } else if (flag == "log") {
                openLogFile = true;
            } else {
                throw std::runtime_error("Invalid fopen flag: " + flag +
                                         "; valid flags are: all, root or log");
            }
        }
    }

    if (openRootFile) {
        runConfig.NewRun();
        baseFileName = runConfig.GetOutputFileName();
        prometheus.setRunNumber(runConfig.run->GetRunNumber());
        OpenRootFile();
    }
    if (openLogFile) OpenFileLogs();
}

void FEMDAQ::OpenRootFile() {
    auto& run = *runConfig.run;

    // The pattern: FormOutputFile resolves it and adds mainDataPath (once)
    run.SetOutputFileName(runConfig.GetOutputPattern());
    run.SetStartTimeStamp(0);
    run.SetEndTimeStamp(0);
    run.SetEntriesSaved(0);
    run.FormOutputFile();
    fileNameRoot = run.GetOutputFileName();
    std::cout << "New file " << fileNameRoot << " " << std::endl;
    run.fOutputFile->SetCompressionAlgorithm(ROOT::RCompressionSetting::EAlgorithm::kZLIB);
    run.fOutputFile->SetCompressionLevel(1);
    run.RegisterEvent<TRestRawDAQSignalEvent>(kEventTree, sEvent);
    run.fOutputEventTrees[kEventTree]->SetAutoFlush(-30000000);  // 30 MB

    // Written now so the file is a valid REST file while it is taken, and again when it is closed
    run.AddMetadata(&run);
    run.AddMetadata(runConfig.daq.get());

    const std::string yamlDump = runConfig.Dump();
    prometheus.setRunName(fileNameRoot);
    prometheus.setMetadata(yamlDump);
}

void FEMDAQ::OpenFileLogs() {
    const std::string tmstmp = TRestTools::GetTimeStampFromUnixTime(TRestTools::GetCurrentTime());
    // The log files are named as the first file of the run, otherwise with the time
    std::string base;
    if (!baseFileName.empty()) {
        base = baseFileName;
        if (base.size() > 5 && base.substr(base.size() - 5) == ".root") base.erase(base.size() - 5);
    } else {
        // No spaces in the file name: YYYY-MM-DD_HH:MM:SS
        std::string time = tmstmp;
        std::replace(time.begin(), time.end(), ' ', '_');
        base = runConfig.run ? runConfig.run->PrefixMainDataPath(time) : time;
    }

    for (auto& FEM : FEMArray) {
        std::string fileName;
        if (runConfig.isTCM)
            fileName = base + "_TCM.log";
        else
            fileName = base + "_FEM" + std::to_string(FEM.femID) + ".log";
        std::cout << fileName << std::endl;
        std::lock_guard<std::mutex> lock(FEM.logMutex);
        FEM.logFile = fopen(fileName.c_str(), "a");
        if (!FEM.logFile) throw std::runtime_error("Cannot open the log file " + fileName);
        fprintf(FEM.logFile, "\n--- LOG FILE INITIALIZED AT %s ---\n", tmstmp.c_str());
        fflush(FEM.logFile);
        DumpExecFileToFEMLog(FEM);
    }
}

void FEMDAQ::DumpExecFileToFEMLog(FEMProxy& FEM) {
    if (!FEM.logFile) return;

    if (execFile.empty()) return;

    std::ifstream src(execFile);
    if (!src.is_open()) {
        std::cout << "Error: Cannot not open file: " << execFile << std::endl;
        return;
    }

    std::string ts = TRestTools::GetTimeStampFromUnixTime(TRestTools::GetCurrentTime());
    fprintf(FEM.logFile, "\n--- [DUMP EXEC FILE: %s] Filename: %s ---\n", ts.c_str(), execFile.c_str());
    fflush(FEM.logFile);

    std::string line;
    while (std::getline(src, line)) {
        fprintf(FEM.logFile, "%s\n", line.c_str());
    }

    fprintf(FEM.logFile, "--- [DUMP END] ---\n\n");
    std::fflush(FEM.logFile);
}

void FEMDAQ::CloseLogFiles() {
    for (auto& FEM : FEMArray) {
        std::lock_guard<std::mutex> lock(FEM.logMutex);
        if (FEM.logFile != nullptr) {
            fclose(FEM.logFile);
            FEM.logFile = nullptr;
        }
    }
}

void FEMDAQ::CloseFiles() {
    CloseRootFile();
    CloseLogFiles();
}

void FEMDAQ::CloseRootFile() {
    if (!HasRootFile()) return;

    auto& run = *runConfig.run;
    run.AddMetadata(&run);
    run.AddMetadata(runConfig.daq.get());
    run.CloseFiles();
}

void FEMDAQ::CheckFileSize(const double eventTime) {
    // Bytes written so far (the file being written, without asking the filesystem)
    const uint64_t fileSize = runConfig.run->fOutputFile->GetEND();

    if (fileSize >= runConfig.fileSize) {
        WriteRunEndTime(eventTime);
        CloseRootFile();

        // Next subrun of the same run
        runConfig.run->SetSubRunNumber(runConfig.run->GetSubRunNumber() + 1);
        OpenRootFile();
        WriteRunStartTime(eventTime);
    }
}

void FEMDAQ::FillTree(const double eventTime, double& lastTimeSaved) {
    auto& run = *runConfig.run;
    sEvent.SetRunOrigin(run.GetRunNumber());
    sEvent.SetSubRunOrigin(run.GetSubRunNumber());
    run.Fill();
    const double elapsed = eventTime - lastTimeSaved;

    // The file can be read while it is taken (e.g. an online event display)
    if (storedEvents % 1000 == 0 || elapsed > 60) {
        run.fOutputFile->cd();
        for (auto& [name, tree] : run.fOutputEventTrees) tree->AutoSave("SaveSelf");
        if (run.fOutputAnalysisTree) run.fOutputAnalysisTree->AutoSave("SaveSelf");
        lastTimeSaved = eventTime;
    }
}

FEMDAQ::~FEMDAQ() {
    // The threads must not outlive the DAQ (e.g. CTRL-C in the console during a run)
    stopDAQ();
    // A file still open: its end time if the run did not set it
    if (HasRootFile() && runConfig.run->GetEndTime() == 0) WriteRunEndTime(TRestTools::GetCurrentTime());
    CloseFiles();
}

void FEMDAQ::RunThread(const std::string& name, const std::function<void()>& function) {
    try {
        function();
    } catch (const std::exception& e) {
        std::cout << "[CRITICAL] " << name << " stopped: " << e.what() << ". Stopping the run" << std::endl;
        stopRun = true;
    }
}

void FEMDAQ::WriteLog(FEMProxy& FEM, const std::string& message) {
    std::lock_guard<std::mutex> lock(FEM.logMutex);
    if (!FEM.logFile) return;
    fprintf(FEM.logFile, "%s\n", message.c_str());
    fflush(FEM.logFile);
}

void FEMDAQ::stopDAQ() {
    if (!eventBuilderThread.joinable() && !UpdateRunThread.joinable()) return;

    runEndTime = TRestTools::GetCurrentTime();

    stopRun = true;
    cv.notify_all();

    if (UpdateRunThread.joinable()) UpdateRunThread.join();

    stopEventBuilder = true;
    if (eventBuilderThread.joinable()) eventBuilderThread.join();
}

void FEMDAQ::UpdateThread() {
    double prevRateTime = TRestTools::GetCurrentTime();
    int prevEvCount = storedEvents.load();
    const int updateTime = runConfig.updateRateTime;

    while (!abrt && !stopRun) {
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait_for(lk, std::chrono::seconds(updateTime), [this] { return abrt || stopRun; });

        double rateTime = TRestTools::GetCurrentTime();
        const double elapsed = rateTime - prevRateTime;
        const double runElapsedTime = rateTime - runStartTime;
        auto tmpstm = TRestTools::GetTimeStampFromUnixTime(rateTime);
        const int evCount = storedEvents.load();
        const double rate = (evCount - prevEvCount) / elapsed;

        prometheus.setEvents(evCount);
        prometheus.setDesyncEvents(nOutOfSync);
        prometheus.setRate(rate);

        if (runConfig.verboseLevel >= RunConfig::Verbosity::REST_Info && (!abrt && !stopRun))
            std::cout << tmpstm << " Total events: " << evCount << " Rate: " << rate << " Hz "
                      << "Run time " << TRestTools::ToTimeStringLong(runElapsedTime) << std::endl;
        prevEvCount = evCount;
        prevRateTime = rateTime;
        if (runConfig.nEvents > 0 && evCount >= runConfig.nEvents) {
            stopRun = true;
            break;
        } else if (runConfig.maxTimeSeconds > 0 && runElapsedTime >= runConfig.maxTimeSeconds) {
            stopRun = true;
            break;
        }
    }
}
