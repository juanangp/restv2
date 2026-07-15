/// test.cxx - Step 1 smoke test: YAML config load + TRestRun file I/O
///
/// Usage: test --c|--config <config.yaml> --i|--input <input.root>

#include <iostream>
#include <string>

#include "TRestManager.h"
#include "TRestRun.h"
#include "TRestTools.h"

int main(int argc, char** argv) {
    std::string configPath;
    std::string inputPath;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--c" || arg == "--config") && i + 1 < argc) {
            configPath = argv[++i];
        } else if ((arg == "--i" || arg == "--input") && i + 1 < argc) {
            inputPath = argv[++i];
        } else {
            std::cerr << "Usage: " << argv[0] << " --c|--config <config.yaml> --i|--input <input.root>\n";
            return 1;
        }
    }

    if (configPath.empty() || inputPath.empty()) {
        std::cerr << "Usage: " << argv[0] << " --c|--config <config.yaml> --i|--input <input.root>\n";
        return 1;
    }

    try {
        YAML::Node raw = YAML::LoadFile(configPath);
        YAML::Node cfg = TRestTools::ResolveAllRefs(raw);

        if (cfg["manager"]["run"]) {
            YAML::Node run = cfg["manager"]["run"];
            TRestTools::OverrideYAMLParam(run, "inputFileName", inputPath);
        }

        RESTLog << "\n--- TRestManager ---" << RESTendl;
        TRestManager mgr("manager",cfg["manager"]);
        mgr.PrintMetadata();
        mgr.Run();
        mgr.SaveMetadata();


    } catch (const std::exception& ex) {
        std::cerr << "[ERROR] " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
