#pragma once

#include <atomic>
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
    /// The verbose levels of restv2 (silent, error, warning, info, debug)
    using Verbosity = TRestLogManager::REST_Verbose_Level;

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

    /// `verbose` of the TRestRawDAQMetadata; read by all the threads, changed by the `verbose` command
    std::atomic<Verbosity> verboseLevel{Verbosity::REST_Info};

    /// \brief `verbose` command: a verbose level of restv2 (silent, error, warning, info or debug); an
    /// unknown one is written and the level is not changed
    void SetVerboseLevel(const std::string& vFlag);

    /// \brief `addMetadata`: asks for the runTag, the runType and each runInfo key of the run and writes
    /// them in the configuration file
    void UpdateInfo();

    /// \brief A new run (`fopen`): a new run number if the configuration has `runNumber: auto`, subrun of
    /// the configuration
    void NewRun();
    /// \brief Name of the output file of the current run and subrun (outputFileName of the TRestRun)
    std::string GetOutputFileName() const;
    /// \brief outputFileName of the configuration, with its [parameters] and without mainDataPath
    const std::string& GetOutputPattern() const { return outputPattern; }

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
