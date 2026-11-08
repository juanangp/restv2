#include <iostream>
#include <memory>

#include "TFile.h"
#include "TSystem.h"

// Directly include your class headers
#include "TRestMicromegasReadout.h"
#include "TRestTools.h"

using namespace TRestTools;

TRestMicromegasReadout* mmReadout = nullptr;

/// \brief Builds a TRestMicromegasReadout from the `readout` section of a YAML file (BuildGeometry, with
/// the decoding file of the configuration) and exports it, with its decoding named decodingName
/// ("nominal_decoding" by default), to a ROOT file that a TRestDetectorReadoutManager can import
/// (instanceName: the name of the readout, decodingName: the same name). The readout is kept in the global
/// mmReadout (nullptr if it cannot be built).
/// \param yamlFile YAML file with a `readout` section of class TRestMicromegasReadout.
/// \param outputFile ROOT file written (RECREATE).
/// \param decodingName Name of the decoding stored with the readout.
///
/// Usage in restRoot (from the examples directory):
///
///    restRoot
///    [0] REST_Detector_GenerateMicromegasReadout("MicromegasReadout.yaml", "iaxo_readout_output.root")
///    [1] REST_Detector_GenerateMicromegasReadout("AlphaCAMM.yaml", "AlphaCAMM_readout.root")
void REST_Detector_GenerateMicromegasReadout(const std::string& yamlFile = "MicromegasReadout.yaml",
                                             const std::string outputFile = "iaxo_readout_output.root",
                                             const std::string& decodingName = "nominal_decoding") {
    std::cout << "[+] ROOT Macro -> Instantiating TRestMicromegasReadout directly..." << std::endl;

    mmReadout = new TRestMicromegasReadout(yamlFile, "readout");

    try {
        mmReadout->BuildGeometry();
    } catch (const std::exception& e) {
        std::cerr << "[-] Cannot build the readout: " << e.what() << std::endl;
        delete mmReadout;
        mmReadout = nullptr;
        return;
    }

    std::unique_ptr<TFile> fOut(TFile::Open(outputFile.c_str(), "RECREATE"));
    if (!fOut || fOut->IsZombie()) {
        std::cerr << "[-] Cannot open the output file " << outputFile << std::endl;
        return;
    }
    if (!mmReadout->Export(fOut.get(), mmReadout->GetName(), decodingName)) {
        std::cerr << "[-] Cannot export the readout to " << outputFile << std::endl;
    }
    fOut->Close();
}
