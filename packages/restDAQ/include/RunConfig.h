#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "TRestRawDAQMetadata.h"
#include "TRestRun.h"

/// \brief Configuration of restDAQ: the TRestRun (run numbers, tags, runInfo, output files) and the
/// TRestRawDAQMetadata (electronics, FEMs, limits of the run) of a YAML file, plus the options of the
/// command line.
class RunConfig {
   public:
    enum class Verbosity { Silent = 0, Info = 1, Debug = 2 };

    /// \brief Reads the TRestRun and the TRestRawDAQMetadata of the file (`parameters`: the --set of the
    /// command line). With isTCM only the TCM IP of the TRestRawDAQMetadata is used.
    RunConfig(const std::string& fileName, const std::map<std::string, std::string>& parameters,
              bool isTCM = false);

    std::unique_ptr<TRestRun> run;
    std::unique_ptr<TRestRawDAQMetadata> daq;

    /// ARC, FEMINOS, DCC, or TCM with isTCM
    std::string electronics = "";
    bool readOnly = false;
    bool isTCM = false;

    int nEvents = 0;
    double maxTimeSeconds = 0;
    uint64_t fileSize = 1024ull * 1024ull * 1024ull;
    int updateRateTime = 5;

    Verbosity verboseLevel = Verbosity::Info;

    void SetVerboseLevel(const std::string& vFlag);

    /// \brief `addMetadata`: asks for the runTag, the runType and each runInfo key of the run and writes
    /// them in the configuration file
    void UpdateInfo();

    /// \brief A new run (`fopen`): a new run number if the configuration has `runNumber: auto`, subrun of
    /// the configuration
    void NewRun();
    /// \brief Name of the output file of the current run and subrun (outputFileName of the TRestRun)
    std::string GetOutputFileName() const;

    /// \brief Configuration of the run and the DAQ, as stored in the files
    std::string Dump() const;
    std::string GetFileName() const { return fileName; }

   private:
    std::string fileName;
    /// Key of the TRestRun section in the configuration file
    std::string runSection;
    /// runNumber: auto, a new number in each run
    bool autoRunNumber = false;
    /// outputFileName of the configuration, with its [parameters]
    std::string outputPattern;
    int firstSubRun = 0;
};
