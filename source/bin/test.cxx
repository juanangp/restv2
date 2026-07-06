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

        TRestRun run(configPath, "run");
        run.SetInputFileName(inputPath);
        run.PrintMetadata();

        RESTLog << "\n--- TRestManager ---" << RESTendl;
        TRestManager mgr(configPath, "manager");
        mgr.PrintMetadata();
        mgr.Run(run);

        RESTLog << "\n--- TRestRun Final ---" << RESTendl;
        run.PrintMetadata();
        run.CloseFiles();
        RESTLog << "Output written to: " << run.GetOutputFileName() << RESTendl;

    } catch (const std::exception& ex) {
        std::cerr << "[ERROR] " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
