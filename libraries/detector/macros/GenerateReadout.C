#include <iostream>
#include "TSystem.h"
#include "TFile.h"

// Directly include your class headers
#include "TRestTools.h"
#include "TRestMicromegasReadout.h"

using namespace TRestTools;

TRestMicromegasReadout* mmReadout=nullptr;

void GenerateReadout(const std::string& yamlFile = "MicromegasReadout.yaml", const std::string outputFile = "iaxo_readout_output.root") {
    
    std::cout << "[+] ROOT Macro -> Instantiating TRestMicromegasReadout directly..." << std::endl;

    mmReadout = new TRestMicromegasReadout(yamlFile, "readout");

    try {
        mmReadout->BuildGeometry();
    } catch (const std::exception& e) {
        std::cerr << "[-] Error crítico en el ciclo de vida del Readout: " << e.what() << std::endl;
        delete mmReadout;
        return;
    }

    TFile* fOut = TFile::Open(outputFile.c_str(), "RECREATE");
    mmReadout->Export(fOut, mmReadout->GetName(), "nominal_decoding");

}
