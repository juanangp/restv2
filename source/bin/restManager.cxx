/// restManager.cxx - CLI entry point for TRestManager (YAML-based config).
///
/// Usage: restManager --c|--config <config.yaml> [--i|--input <input.root>]
///                     [--o|--output <output.root>] [--v|--verbose <level>]
///                     [--e|--events <n>] [--r|--reference <process>=<dataset>]
///                     [--set <name>=<value>]
///
/// --set sets a parameter of the section `parameters` of the configuration for this execution (repeatable;
/// the file is not changed), e.g. --set meshVoltage=300V
/// --r sets the referenceFileName of a process of the TRestDataSetProcessManager (repeatable), e.g.
/// --r calib=D_00120_00122_Calibration.root

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

void SignalWaiterThread(sigset_t waitSet) {
    int signum = 0;
    sigwait(&waitSet, &signum);
    std::cout << "\n[SignalWaiter] signal " << signum << " captured, stopping..." << std::endl;
    TRestManager::RequestStop();
}

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
                         " [--e|--events <n>] [--r|--reference <process>=<dataset>]"
                         " [--set <name>=<value>]\n";
            return 1;
        }
    }

    if (configPath.empty()) {
        std::cerr << "Usage: " << argv[0]
                  << " --c|--config <config.yaml> [--i|--input <input.root>]"
                     " [--o|--output <output.root>] [--v|--verbose <level>]"
                     " [--e|--events <n>] [--r|--reference <process>=<dataset>]"
                     " [--set <name>=<value>]\n";
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
