#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "TFile.h"
#include "TRestFibreReadout.h"
#include "TRestGDMLReadout.h"
#include "TRestMicromegasReadout.h"
#include "TRestYAMLUtils.h"

/// \brief Builds every readout of a YAML file (each section whose class is TRestMicromegasReadout,
/// TRestGDMLReadout or TRestFibreReadout, e.g. the two planes of a detector, or a Micromegas and its veto)
/// and exports them all to the same ROOT file, each one under its name with its decoding named decodingName,
/// so a project has a single readout file. A TRestDetectorReadoutManager imports them from it, one entry per
/// readout (instanceName: the name of the readout, the same inputFileName). The placements of the fibres of
/// a TRestFibreReadout (TRestFibreReadout::WriteGDMLPlacements), to include in the GDML of the simulation as
/// an external entity, are written next to the output file as <readout name>_fibres.xml.
///
///    plane0:
///      class: "TRestMicromegasReadout"
///      name: "TREXDM_Plane0_Readout"
///      decodingFile: "TREXDM_Plane0.dec"
///      readoutParameters: {...}
///    plane1:
///      class: "TRestMicromegasReadout"
///      name: "TREXDM_Plane1_Readout"
///      ...
///
/// The paths of the YAML (decoding files, GDML) are relative to the working directory.
/// \param yamlFile YAML file with the readout sections (other sections are ignored).
/// \param outputFile ROOT file written (RECREATE): nothing is kept if a readout fails.
/// \param decodingName Name of the decoding stored with each readout.
/// \param checkOverlaps Checks the overlaps of each geometry (TRestDetectorReadout::CheckOverlaps) before
/// exporting it.
/// \return 0 on success, 1 on any error (the exit code of `restRoot -b -q`, e.g. in a CI); on an error the
/// output file is removed.
///
/// Usage in restRoot:
///
///    restRoot
///    [0] REST_Detector_GenerateReadouts("TREXDM.yaml", "TREXDM.root")
int REST_Detector_GenerateReadouts(const std::string& yamlFile, const std::string& outputFile,
                                   const std::string& decodingName = "nominal_decoding",
                                   bool checkOverlaps = false) {
    std::vector<std::pair<std::string, std::string>> sections;  // name, class
    try {
        const YAML::Node config = TRestYAMLUtils::LoadConfigFile(yamlFile);
        for (const auto& entry : config) {
            const YAML::Node section = entry.second;
            if (!section.IsMap() || !section["class"]) continue;
            const std::string className = section["class"].as<std::string>();
            if (className == "TRestMicromegasReadout" || className == "TRestGDMLReadout" ||
                className == "TRestFibreReadout") {
                sections.emplace_back(entry.first.as<std::string>(), className);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[-] Cannot read " << yamlFile << ": " << e.what() << std::endl;
        return 1;
    }
    if (sections.empty()) {
        std::cerr << "[-] No readout section in " << yamlFile << std::endl;
        return 1;
    }

    std::unique_ptr<TFile> output(TFile::Open(outputFile.c_str(), "RECREATE"));
    if (!output || output->IsZombie()) {
        std::cerr << "[-] Cannot open the output file " << outputFile << std::endl;
        return 1;
    }
    const auto fail = [&]() {
        output->Close();
        std::filesystem::remove(outputFile);
        return 1;
    };

    const auto directory = std::filesystem::path(outputFile).parent_path();
    for (const auto& [section, className] : sections) {
        std::unique_ptr<TRestDetectorReadout> readout;
        try {
            if (className == "TRestMicromegasReadout") {
                readout = std::make_unique<TRestMicromegasReadout>(yamlFile, section);
            } else if (className == "TRestGDMLReadout") {
                readout = std::make_unique<TRestGDMLReadout>(yamlFile, section);
            } else {
                readout = std::make_unique<TRestFibreReadout>(yamlFile, section);
            }
            std::cout << "[+] " << section << ": " << className << " " << readout->GetName() << std::endl;
            readout->BuildGeometry();
        } catch (const std::exception& e) {
            std::cerr << "[-] Cannot build the readout of " << section << ": " << e.what() << std::endl;
            return fail();
        }
        if (checkOverlaps && readout->CheckOverlaps() > 0) {
            std::cerr << "[-] Geometry overlaps in the readout " << readout->GetName() << std::endl;
            return fail();
        }
        if (!readout->Export(output.get(), readout->GetName(), decodingName)) {
            std::cerr << "[-] Cannot export the readout " << readout->GetName() << " to " << outputFile
                      << std::endl;
            return fail();
        }
        if (auto* fibres = dynamic_cast<TRestFibreReadout*>(readout.get())) {
            const std::string placements = (directory / (fibres->GetName() + "_fibres.xml")).string();
            if (!fibres->WriteGDMLPlacements(placements)) {
                std::cerr << "[-] Cannot write the fibre placements to " << placements << std::endl;
                return fail();
            }
            std::cout << "[+] " << fibres->GetNumberOfFibres() << " fibre placements written to "
                      << placements << std::endl;
        }
    }
    output->Close();
    std::cout << "[+] " << sections.size() << " readouts written to " << outputFile << std::endl;
    return 0;
}
