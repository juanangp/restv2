#include "FEMDAQ.h"

#include <Compression.h>
#include <TFile.h>

#include <charconv>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>

std::atomic<bool> FEMDAQ::abrt(false);
std::atomic<bool> FEMDAQ::stopRun(false);
std::atomic<bool> FEMDAQ::stopEventBuilder(false);

namespace {
/// Tree of the events, read as a TRestRawSignalEvent
constexpr const char* kEventTree = "TRestRawSignalEvent";
}  // namespace

FEMDAQ::FEMDAQ(RunConfig& rC) : runConfig(rC) {
    std::cout << "------------ " << rC.electronics << " RUN ------------" << std::endl;

    if (runConfig.isTCM) {
        FEMProxy FEM;
        FEM.Open(runConfig.daq->fTCM);
        FEM.femID = -1;
        FEMArray.emplace_back(std::move(FEM));
    } else {
        for (const auto& fem : runConfig.daq->fFEMs) {
            FEMProxy FEM;
            FEM.Open(fem.fIP);
            FEM.femID = fem.fId;
            FEMArray.emplace_back(std::move(FEM));
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

double FEMDAQ::getCurrentTime() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::system_clock::now().time_since_epoch())
               .count() /
           1000000.0;
}

std::string FEMDAQ::FormatElapsedTime(const double seconds) {
    double value;
    const char* unit;

    if (seconds >= 604800) {  // 1 week
        value = seconds / 604800.0;
        unit = " weeks";
    } else if (seconds >= 86400) {  // 1 day
        value = seconds / 86400.0;
        unit = " days";
    } else if (seconds >= 3600) {  // 1 hour
        value = seconds / 3600.0;
        unit = " hours";
    } else if (seconds >= 60) {
        value = seconds / 60.0;
        unit = " minutes";
    } else {
        value = seconds;
        unit = " seconds";
    }

    char timeStr[32];
    int len = std::snprintf(timeStr, sizeof(timeStr), "%.2f", value);

    return std::string(timeStr, len) + unit;
}

std::string FEMDAQ::GetTimeStampFromUnixTime(const double tm) {
    char tmpstm[20];  //"YYYY-MM-DD_HH:MM:SS" + \0
    std::time_t time = static_cast<std::time_t>(tm);

    std::strftime(tmpstm, sizeof(tmpstm), "%Y-%m-%d_%H:%M:%S", std::localtime(&time));

    return std::string(tmpstm);
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

    fileNameRoot = runConfig.GetOutputFileName();
    std::cout << "New file " << fileNameRoot << " " << std::endl;

    run.SetOutputFileName(fileNameRoot);
    run.SetStartTimeStamp(0);
    run.SetEndTimeStamp(0);
    run.SetEntriesSaved(0);
    run.FormOutputFile();
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
    const std::string tmstmp = GetTimeStampFromUnixTime(getCurrentTime());
    // The log files are named as the first file of the run, otherwise with the time
    std::string base;
    if (!baseFileName.empty()) {
        base = baseFileName;
        if (base.size() > 5 && base.substr(base.size() - 5) == ".root") base.erase(base.size() - 5);
    } else if (runConfig.run) {
        base = runConfig.run->PrefixMainDataPath(tmstmp);
    } else {
        base = tmstmp;
    }

    for (auto& FEM : FEMArray) {
        std::string fileName;
        if (runConfig.isTCM)
            fileName = base + "_TCM.log";
        else
            fileName = base + "_FEM" + std::to_string(FEM.femID) + ".log";
        std::cout << fileName << std::endl;
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

    std::string ts = GetTimeStampFromUnixTime(getCurrentTime());
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
    uint64_t fileSize = std::filesystem::file_size(fileNameRoot);

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

void FEMDAQ::stopDAQ() {
    runEndTime = getCurrentTime();

    cv.notify_all();

    if (UpdateRunThread.joinable()) UpdateRunThread.join();

    stopEventBuilder = true;
    if (eventBuilderThread.joinable()) eventBuilderThread.join();
}

void FEMDAQ::UpdateThread() {
    double prevRateTime = getCurrentTime();
    int prevEvCount = storedEvents.load();
    const int updateTime = runConfig.updateRateTime;

    while (!abrt && !stopRun) {
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait_for(lk, std::chrono::seconds(updateTime), [this] { return abrt || stopRun; });

        double rateTime = getCurrentTime();
        const double elapsed = rateTime - prevRateTime;
        const double runElapsedTime = rateTime - runStartTime;
        auto tmpstm = GetTimeStampFromUnixTime(rateTime);
        const int evCount = storedEvents.load();
        const double rate = (evCount - prevEvCount) / elapsed;

        prometheus.setEvents(evCount);
        prometheus.setDesyncEvents(nOutOfSync);
        prometheus.setRate(rate);

        if (runConfig.verboseLevel >= RunConfig::Verbosity::Info && (!abrt && !stopRun))
            std::cout << tmpstm << " Total events: " << evCount << " Rate: " << rate << " Hz "
                      << "Run time " << FormatElapsedTime(runElapsedTime) << std::endl;
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
