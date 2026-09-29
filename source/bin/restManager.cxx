/// restManager.cxx - CLI entry point for TRestManager (YAML-based config).
///
/// Usage: restManager --c|--config <config.yaml> [--i|--input <input.root>]
///                     [--o|--output <output.root>] [--v|--verbose <level>]
///                     [--e|--events <n>]

#include <iostream>
#include <string>

#include "TRestManager.h"
#include "TRestTools.h"
#include "TRestLogManager.h"

int main(int argc, char** argv) {
    std::string configPath;
    std::string inputPath;
    std::string outputPath;
    std::string verboseLevel;
    std::string eventsToProcess;

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
        } else {
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
        YAML::Node raw = YAML::LoadFile(configPath);
        YAML::Node cfg = TRestTools::ResolveAllRefs(raw);

        auto [managerKey, managerNode] = TRestTools::GetMetadataClass(cfg, "TRestManager");
        if (!managerNode || managerNode.IsNull()) {
            std::cerr << "[ERROR] No 'TRestManager' section found in " << configPath << "\n";
            return 1;
        }

        auto [runKey, runNode] = TRestTools::GetMetadataClass(managerNode, "TRestRun");
        if (runNode && !runNode.IsNull()) {
            if (!inputPath.empty()) TRestTools::OverrideYAMLParam(runNode, "inputFileName", inputPath);
            if (!outputPath.empty()) TRestTools::OverrideYAMLParam(runNode, "outputFileName", outputPath);
        }

        auto [pipelineKey, pipelineNode] = TRestTools::GetMetadataClass(managerNode, "TRestProcessManager");
        if (pipelineNode && !pipelineNode.IsNull()) {
            if (!eventsToProcess.empty())
                TRestTools::OverrideYAMLParam(pipelineNode, "eventsToProcess", eventsToProcess);
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
