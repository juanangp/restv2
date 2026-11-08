/// \file restManager.cxx
/// \brief restManager: command line entry point of TRestManager (YAML-based configuration).
///
/// Usage:
///
///    restManager --c|--config <config.yaml> [--i|--input <input.root>]
///                [--o|--output <output.root>] [--v|--verbose <level>]
///                [--e|--events <n>] [--j|--threads <n>] [--r|--reference <process>=<dataset>]
///                [--set <name>=<value>] [--maxRepeated <n>] [--repeatedInterval <s>]
///
/// Reads the TRestManager section of the configuration, applies the command line overrides, runs it
/// (TRestManager::Run), saves its metadata and draws its plots (TRestManager::RunPlots).
///
/// Options (each one takes a value; an unknown option prints the usage and exits with 1):
/// - `--c`, `--config`: YAML configuration file with a TRestManager section (required).
/// - `--i`, `--input`: input file. It sets the inputFileName of the TRestRun; with a TRestDataSet
///   (dataset generation) its filePattern; with a TRestDataSetProcessManager and no TRestDataSet its
///   inputFileName; with no run nor dataset, the inputFileName of every TRestAnalysisPlot.
/// - `--o`, `--output`: output file. It sets the outputFileName of the TRestRun, of the
///   TRestDataSetProcessManager if any, otherwise of the TRestDataSet.
/// - `--v`, `--verbose`: global verbose level (TRestLogManager::GetVerboseLevelFromString).
/// - `--e`, `--events`: eventsToProcess of the TRestEventProcessManager.
/// - `--j`, `--threads`: nThreads of the TRestEventProcessManager (ROOT thread safety is enabled with more
///   than one thread).
/// - `--set`: sets a parameter of the section `parameters` of the configuration for this execution
///   (repeatable; the file is not changed), e.g. `--set meshVoltage=300V`. A parameter not used by the
///   configuration (or the geometries it reads) is an error.
/// - `--maxRepeated`: number of similar messages (warning and info) from the same line of the code that,
///   arriving within a time window of `--repeatedInterval` seconds, make the next ones be suppressed for
///   ever (default 10, 0: no suppression); with verbose debug none is suppressed.
/// - `--repeatedInterval`: time window in seconds of `--maxRepeated` (default 10 s, 0: a single window for
///   the whole execution).
/// - `--r`, `--reference`: sets the referenceFileName of a process of the TRestDataSetProcessManager
///   (repeatable), e.g. `--r calib=D_00120_00122_Calibration.root`.
///
/// SIGINT, SIGTERM, SIGHUP and SIGQUIT request a clean stop (TRestManager::RequestStop).

#include <TROOT.h>

#include <csignal>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "TRestLogManager.h"
#include "TRestManager.h"
#include "TRestSystemOfUnits.h"
#include "TRestYAMLUtils.h"

/// \brief Waits (in its own thread) for one of the signals of waitSet, blocked in the other threads, and
/// requests the stop of the TRestManager (TRestManager::RequestStop) when it arrives.
/// \param waitSet Signals to wait for.
void SignalWaiterThread(sigset_t waitSet) {
    int signum = 0;
    sigwait(&waitSet, &signum);
    std::cout << "\n[SignalWaiter] signal " << signum << " captured, stopping..." << std::endl;
    TRestManager::RequestStop();
}

/// \brief Parses the command line options (see the file documentation), applies them to the TRestManager
/// section of the configuration and runs it.
/// \return 0 on success, 1 on a usage or configuration error or an exception.
int main(int argc, char** argv) {
    REST_Units::SetGeoManagerDefaultUnits();

    sigset_t blockSet;
    sigemptyset(&blockSet);
    sigaddset(&blockSet, SIGINT);
    sigaddset(&blockSet, SIGTERM);
    sigaddset(&blockSet, SIGHUP);
    sigaddset(&blockSet, SIGQUIT);

    pthread_sigmask(SIG_BLOCK, &blockSet, nullptr);

    std::thread signalThread(SignalWaiterThread, blockSet);
    signalThread.detach();

    std::string configPath;
    std::string inputPath;
    std::string outputPath;
    std::string verboseLevel;
    std::string eventsToProcess;
    std::string threadNumber;
    // --r process=dataset: referenceFileName of a dataset process (repeatable)
    std::vector<std::pair<std::string, std::string>> references;
    // --set name=value: parameters of the configuration (repeatable)
    std::map<std::string, std::string> parameters;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--c" || arg == "--config") && i + 1 < argc) {
            configPath = argv[++i];
        } else if ((arg == "--i" || arg == "--input") && i + 1 < argc) {
            inputPath = argv[++i];
        } else if ((arg == "--o" || arg == "--output") && i + 1 < argc) {
            outputPath = argv[++i];
        } else if ((arg == "--v" || arg == "--verbose") && i + 1 < argc) {
            verboseLevel = argv[++i];
        } else if ((arg == "--e" || arg == "--events") && i + 1 < argc) {
            eventsToProcess = argv[++i];
        } else if ((arg == "--j" || arg == "--threads") && i + 1 < argc) {
            threadNumber = argv[++i];
        } else if ((arg == "--r" || arg == "--reference") && i + 1 < argc) {
            const std::string reference = argv[++i];
            const size_t equal = reference.find('=');
            if (equal == std::string::npos || equal == 0 || equal + 1 == reference.size()) {
                std::cerr
                    << "[ERROR] --r needs <process>=<dataset>, e.g. --r calib=Dataset_Calibration.root\n";
                return 1;
            }
            references.emplace_back(reference.substr(0, equal), reference.substr(equal + 1));
        } else if (arg == "--maxRepeated" && i + 1 < argc) {
            const std::string value = argv[++i];
            size_t end = 0;
            int maxRepeated = -1;
            try {
                maxRepeated = std::stoi(value, &end);
            } catch (const std::exception&) {
            }
            if (maxRepeated < 0 || end != value.size()) {
                std::cerr << "[ERROR] --maxRepeated needs an integer >= 0, not '" << value << "'\n";
                return 1;
            }
            TRestLogManager::setMaxRepeated(maxRepeated);
        } else if (arg == "--repeatedInterval" && i + 1 < argc) {
            const std::string value = argv[++i];
            size_t end = 0;
            double interval = -1;
            try {
                interval = std::stod(value, &end);
            } catch (const std::exception&) {
            }
            if (!(interval >= 0) || end != value.size()) {
                std::cerr << "[ERROR] --repeatedInterval needs a number of seconds >= 0, not '" << value
                          << "'\n";
                return 1;
            }
            TRestLogManager::setRepeatedInterval(interval);
        } else if (arg == "--set" && i + 1 < argc) {
            try {
                TRestYAMLUtils::AddParameterOverride(parameters, argv[++i]);
            } catch (const std::exception& error) {
                std::cerr << "[ERROR] " << error.what() << "\n";
                return 1;
            }
        } else {
            std::cerr << "Usage: " << argv[0]
                      << " --c|--config <config.yaml> [--i|--input <input.root>]"
                         " [--o|--output <output.root>] [--v|--verbose <level>]"
                         " [--e|--events <n>] [--j|--threads <n>] [--r|--reference <process>=<dataset>]"
                         " [--set <name>=<value>] [--maxRepeated <n>] [--repeatedInterval <s>]\n";
            return 1;
        }
    }

    if (configPath.empty()) {
        std::cerr << "Usage: " << argv[0]
                  << " --c|--config <config.yaml> [--i|--input <input.root>]"
                     " [--o|--output <output.root>] [--v|--verbose <level>]"
                     " [--e|--events <n>] [--j|--threads <n>] [--r|--reference <process>=<dataset>]"
                     " [--set <name>=<value>] [--maxRepeated <n>] [--repeatedInterval <s>]\n";
        return 1;
    }

    try {
        YAML::Node cfg = TRestYAMLUtils::OpenConfigFile(configPath, parameters);

        auto [managerKey, managerNode] = TRestYAMLUtils::GetMetadataClass(cfg, "TRestManager");
        if (!managerNode || managerNode.IsNull()) {
            std::cerr << "[ERROR] No 'TRestManager' section found in " << configPath << "\n";
            return 1;
        }

        auto [runKey, runNode] = TRestYAMLUtils::GetMetadataClass(managerNode, "TRestRun");
        if (runNode && !runNode.IsNull()) {
            if (!inputPath.empty()) runNode["inputFileName"] = inputPath;
            if (!outputPath.empty()) runNode["outputFileName"] = outputPath;
        }

        // Dataset generation (no run): --i its filePattern; --o its output unless there is a dataset pipeline
        auto [generationKey, generationNode] = TRestYAMLUtils::GetMetadataClass(managerNode, "TRestDataSet");
        const bool dataSetGeneration = generationNode && !generationNode.IsNull();
        // Dataset pipeline (no run): its input (the generated dataset if there is a TRestDataSet) and output
        auto [dataSetKey, dataSetNode] =
            TRestYAMLUtils::GetMetadataClass(managerNode, "TRestDataSetProcessManager");
        const bool dataSetPipeline = dataSetNode && !dataSetNode.IsNull();
        if (dataSetGeneration) {
            if (!inputPath.empty()) generationNode["filePattern"] = inputPath;
            if (!outputPath.empty() && !dataSetPipeline) generationNode["outputFileName"] = outputPath;
        }
        if (dataSetPipeline) {
            if (!inputPath.empty() && !dataSetGeneration) dataSetNode["inputFileName"] = inputPath;
            if (!outputPath.empty()) dataSetNode["outputFileName"] = outputPath;
        }
        for (const auto& [process, dataSet] : references) {
            YAML::Node processNode = dataSetPipeline ? dataSetNode[process] : YAML::Node();
            if (!processNode || !processNode.IsMap() || !processNode["class"]) {
                std::cerr << "[ERROR] --r " << process << "=" << dataSet << ": no dataset process '"
                          << process << "' in the TRestDataSetProcessManager of " << configPath << "\n";
                return 1;
            }
            processNode["referenceFileName"] = dataSet;
        }

        // Only plots (no run): the input of every TRestAnalysisPlot (a run file, a dataset or a pattern)
        if ((!runNode || runNode.IsNull()) && !dataSetPipeline && !dataSetGeneration && !inputPath.empty()) {
            for (auto element : managerNode) {
                YAML::Node node = element.second;
                if (node.IsMap() && node["class"] && node["class"].as<std::string>() == "TRestAnalysisPlot")
                    node["inputFileName"] = inputPath;
            }
        }

        auto [pipelineKey, pipelineNode] =
            TRestYAMLUtils::GetMetadataClass(managerNode, "TRestEventProcessManager");
        if (pipelineNode && !pipelineNode.IsNull()) {
            if (!eventsToProcess.empty()) pipelineNode["eventsToProcess"] = eventsToProcess;
            if (!threadNumber.empty()) pipelineNode["nThreads"] = threadNumber;
        }

        unsigned int effectiveThreads = 1;
        if (pipelineNode && !pipelineNode.IsNull()) {
            YAML::Node nThreadsNode = pipelineNode["nThreads"];
            if (nThreadsNode && nThreadsNode.IsDefined() && !nThreadsNode.IsNull()) {
                try {
                    effectiveThreads = nThreadsNode.as<unsigned int>();
                } catch (const std::exception&) {
                    RESTWarning << "restManager: invalid thread number" << RESTendl;
                    effectiveThreads = 1;
                }
            }
        }

        if (effectiveThreads > 1) {
            ROOT::EnableThreadSafety();
        }

        if (!verboseLevel.empty()) {
            TRestLogManager::setGlobalVerboseLevel(TRestLogManager::GetVerboseLevelFromString(verboseLevel));
        }

        RESTLog << "\n--- TRestManager ---" << RESTendl;
        TRestManager mgr(managerKey, managerNode);
        mgr.CheckConfig();
        // The configuration and the geometries (e.g. GDML readouts) read: a parameter not used is an error
        TRestYAMLUtils::CheckParametersUsed();
        mgr.PrintMetadata();
        mgr.Run();
        mgr.SaveMetadata();
        mgr.RunPlots();

    } catch (const std::exception& ex) {
        std::cerr << "[ERROR] " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
