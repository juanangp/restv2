#include <iostream>
#include <memory>

#include "TFile.h"
#include "TSystem.h"

// Directly include your class headers
#include "TRestGDMLReadout.h"
#include "TRestTools.h"

using namespace TRestTools;

TRestGDMLReadout* gdmlReadout = nullptr;

/// \brief Builds a TRestGDMLReadout from the `readout` section of a YAML file (BuildGeometry: the GDML
/// file and the decoding file of the configuration) and exports it, with its decoding named decodingName
/// ("veto_decoding" by default), to a ROOT file that a TRestDetectorReadoutManager can import
/// (instanceName: the name of the readout, decodingName: the same name). The readout is kept in the global
/// gdmlReadout (nullptr if it cannot be built).
/// \param yamlFile YAML file with a `readout` section of class TRestGDMLReadout.
/// \param outputFile ROOT file written (RECREATE).
/// \param decodingName Name of the decoding stored with the readout.
/// \param checkOverlaps Checks the overlaps of the geometry (TRestDetectorReadout::CheckOverlaps) before
/// exporting it: nothing is written if there is any.
/// \return 0 on success, 1 on any error (the exit code of `restRoot -b -q`, e.g. in a CI).
///
/// Usage in restRoot (from the repository root, as the paths of GDMLReadout.yaml):
///
///    restRoot
///    [0] REST_Detector_GenerateGDMLReadout("libraries/detector/examples/GDMLReadout.yaml",
///                                          "libraries/detector/examples/veto_readout_output.root")
int REST_Detector_GenerateGDMLReadout(
    const std::string& yamlFile = "libraries/detector/examples/GDMLReadout.yaml",
    const std::string outputFile = "libraries/detector/examples/veto_readout_output.root",
    const std::string& decodingName = "veto_decoding", bool checkOverlaps = false) {
    std::cout << "[+] ROOT Macro -> Instantiating TRestGDMLReadout directly..." << std::endl;

    gdmlReadout = new TRestGDMLReadout(yamlFile, "readout");
    gdmlReadout->PrintMetadata();

    try {
        gdmlReadout->BuildGeometry();
    } catch (const std::exception& e) {
        std::cerr << "[-] Cannot build the readout: " << e.what() << std::endl;
        delete gdmlReadout;
        gdmlReadout = nullptr;
        return 1;
    }

    if (checkOverlaps && gdmlReadout->CheckOverlaps() > 0) {
        std::cerr << "[-] Geometry overlaps in the readout " << gdmlReadout->GetName() << std::endl;
        return 1;
    }

    std::unique_ptr<TFile> fOut(TFile::Open(outputFile.c_str(), "RECREATE"));
    if (!fOut || fOut->IsZombie()) {
        std::cerr << "[-] Cannot open the output file " << outputFile << std::endl;
        return 1;
    }
    if (!gdmlReadout->Export(fOut.get(), gdmlReadout->GetName(), decodingName)) {
        std::cerr << "[-] Cannot export the readout to " << outputFile << std::endl;
        return 1;
    }
    fOut->Close();
    return 0;
}
