/// test.cxx – minimal smoke test for the REST framework
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
        // --- Load and resolve the YAML config ---
        YAML::Node raw = YAML::LoadFile(argv[1]);
        YAML::Node cfg = TRestTools::ResolveAllRefs(raw);

        // --- Build the run object via the registry (no hard-coded class name) ---
        const auto& runNode   = cfg["run"];
        const auto  className = TRestTools::ReadYAMLParam<std::string>(runNode["class"]);
        const auto  name      = TRestTools::ReadYAMLParam<std::string>(runNode["name"]);
        auto        params    = runNode["params"];

        auto run = MetadataRegistry::Instance().Create(className, name, "run_section", params);
        run->PrintMetadata();

        // --- Alternative: construct TRestRun directly from file ---
        RESTLog << "\n--- TRestRun via direct constructor ---" << RESTendl;
        TRestRun run2(argv[1], "run");
        run2.PrintMetadata();

        // --- Build and run the full manager ---
        RESTLog << "\n--- TRestManager ---" << RESTendl;
        TRestManager mgr(argv[1],"manager");
        mgr.PrintMetadata();

    } catch (const std::exception& ex) {
        std::cerr << "[ERROR] " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
