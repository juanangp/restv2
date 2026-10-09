/// restManager.cxx - CLI entry point for TRestManager (YAML-based config).
///
/// Usage: restManager --c|--config <config.yaml> [--i|--input <input.root>]
///                     [--o|--output <output.root>] [--v|--verbose <level>]
///                     [--e|--events <n>]

#include <TROOT.h>

#include <iostream>
#include <string>
#include <csignal>
#include <cstdlib>
#include <thread>

#include "TRestManager.h"
#include "TRestTools.h"
#include "TRestLogManager.h"

void SignalWaiterThread(sigset_t waitSet) {
    int signum = 0;
    sigwait(&waitSet, &signum);
    std::cout << "\n[SignalWaiter] signal " << signum << " captured, stopping..." << std::endl;
    TRestManager::RequestStop();
}

int main(int argc, char** argv) {
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
        }else {
            std::cerr << "Usage: " << argv[0]
                      << " --c|--config <config.yaml> [--i|--input <input.root>]"
                         " [--o|--output <output.root>] [--v|--verbose <level>]"
                         " [--e|--events <n>]\n";
            return 1;
        }
    }

    if (configPath.empty()) {
        std::cerr << "Usage: " << argv[0]
                  << " --c|--config <config.yaml> [--i|--input <input.root>]"
                     " [--o|--output <output.root>] [--v|--verbose <level>]"
                     " [--e|--events <n>]\n";
        return 1;
    }

    try {
        YAML::Node cfg = TRestTools::OpenConfigFile(configPath);

        auto [managerKey, managerNode] = TRestTools::GetMetadataClass(cfg, "TRestManager");
        if (!managerNode || managerNode.IsNull()) {
            std::cerr << "[ERROR] No 'TRestManager' section found in " << configPath << "\n";
            return 1;
        }

        auto [runKey, runNode] = TRestTools::GetMetadataClass(managerNode, "TRestRun");
        if (runNode && !runNode.IsNull()) {
            if (!inputPath.empty())
              runNode["inputFileName"] = inputPath;
            if (!outputPath.empty())
              runNode["outputFileName"] = outputPath;
        }

        auto [pipelineKey, pipelineNode] = TRestTools::GetMetadataClass(managerNode, "TRestProcessManager");
        if (pipelineNode && !pipelineNode.IsNull()) {
            if (!eventsToProcess.empty())
              pipelineNode["eventsToProcess"] = eventsToProcess;
            if (!threadNumber.empty())
              pipelineNode["nThreads"] = threadNumber;
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
         TRestLogManager::setGlobalVerboseLevel(
         TRestLogManager::GetVerboseLevelFromString(verboseLevel));
       }

       RESTLog << "\n--- TRestManager ---" << RESTendl;
       TRestManager mgr(managerKey, managerNode);
       mgr.PrintMetadata();
       mgr.Run();
       mgr.SaveMetadata();

    } catch (const std::exception& ex) {
        std::cerr << "[ERROR] " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
