/// \file main.cxx
/// \brief restGarfield: computes with Garfield++ the tables of a TRestDetectorTransportManager configuration
///
///    restGarfield config.yaml [--set name=value ...] [--jobs N]
///
/// `--set` sets a parameter of the section `parameters` of the configuration for this execution
/// (repeatable; the file is not changed). `--jobs N` splits the field nodes of each gas transport table in up
/// to N processes (Magboltz is not thread safe; default 1). Any other argument is an error.
///
///    transportManager:
///      class: "TRestDetectorTransportManager"
///      medium:
///        class: "TRestDetectorMedium"
///        name: "ArIso"
///        state: "gas"
///        components: {Ar: 99, iC4H10: 1}
///      transport:
///        fieldRange: [10 V/cm, 1000 V/cm]
///      gain:
///        gap: 128 um
///        reducedFieldRange: [25 kV/cm/bar, 35 kV/cm/bar]
///
/// The tables go to the mediumFile of the medium (by default <name>.root): first the transport tables,
/// then the gain tables. The classes are in the detector library (they do not depend on Garfield++); this
/// package only computes the tables.

#include <cstdlib>
#include <exception>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

#include "GainSimulation.h"
#include "TRestDetectorTransportManager.h"
#include "TRestYAMLUtils.h"
#include "TransportSimulation.h"

/// \brief Reads the TRestDetectorTransportManager section of the configuration (argv[1]) and computes its
/// transport tables (RunTransportSimulation) and then its gain tables (RunGainSimulation).
/// \return 0 on success, 1 on a usage error or an exception (the reason is printed).
int main(int argc, char* argv[]) {
    if (argc < 2 || std::string(argv[1]).rfind("--", 0) == 0) {
        std::cerr << "Usage: restGarfield <config.yaml> [--set name=value ...] [--jobs N]" << std::endl;
        return 1;
    }

    try {
        // --set: the parameters of the configuration for this execution (the file is not changed)
        std::map<std::string, std::string> parameters;
        int jobs = 1;
        for (int i = 2; i < argc; i++) {
            const std::string argument = argv[i];
            if (argument == "--set" && i + 1 < argc) {
                TRestYAMLUtils::AddParameterOverride(parameters, argv[++i]);
            } else if (argument == "--jobs" && i + 1 < argc) {
                char* end = nullptr;
                jobs = std::strtol(argv[++i], &end, 10);
                if (*end != '\0' || jobs < 1) throw std::runtime_error("--jobs: a number >= 1 expected");
            } else {
                throw std::runtime_error("unknown argument '" + argument +
                                         "', usage: restGarfield <config.yaml> [--set name=value ...] "
                                         "[--jobs N]");
            }
        }
        const YAML::Node config = TRestYAMLUtils::OpenConfigFile(argv[1], parameters);
        const auto section = TRestYAMLUtils::GetMetadataClass(config, "TRestDetectorTransportManager");
        if (!section.second) {
            throw std::runtime_error("no section of class TRestDetectorTransportManager in " +
                                     std::string(argv[1]));
        }

        TRestDetectorTransportManager manager(section.first, section.second);
        manager.CheckConfig();
        if (manager.fMedium->fComponents.empty()) {
            throw std::runtime_error("the medium '" + manager.fMedium->GetName() + "' has no components");
        }
        TRestYAMLUtils::CheckParametersUsed();
        manager.PrintMetadata();
        const bool verbose = manager.GetVerboseLevel() >= TRestLogManager::REST_Verbose_Level::REST_Info;
        for (const auto& table : manager.fTransport)
            RunTransportSimulation(*manager.fMedium, *table, verbose, jobs);
        for (const auto& table : manager.fGain) RunGainSimulation(*manager.fMedium, *table, verbose);
    } catch (const std::exception& error) {
        std::cerr << "restGarfield: " << error.what() << std::endl;
        return 1;
    }
    return 0;
}
