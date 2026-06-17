/// test.cxx – Step 1 smoke test: YAML config load + TRestRun file I/O
///
/// Usage:  test <config.yaml>

#include <iostream>

#include "TRestManager.h"
#include "TRestRun.h"
#include "TRestTools.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config.yaml>\n";
        return 1;
    }

    try {
        YAML::Node raw = YAML::LoadFile(argv[1]);
        YAML::Node cfg = TRestTools::ResolveAllRefs(raw);

        // --- TRestRun from YAML ---
        RESTLog << "--- TRestRun ---" << RESTendl;
        TRestRun run(argv[1], "run");
        run.PrintMetadata();

        // --- Open empty output file (validates tree creation + metadata write) ---
        run.OpenOutputFile();
        run.CloseFiles();
        RESTLog << "Output written to: " << run.GetOutputFileName() << RESTendl;

        // --- TRestManager (pipeline empty until Step 3) ---
        if (cfg["manager"]) {
            RESTLog << "\n--- TRestManager ---" << RESTendl;
            TRestManager mgr(argv[1], "manager");
            mgr.PrintMetadata();
        }

    } catch (const std::exception& ex) {
        std::cerr << "[ERROR] " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
