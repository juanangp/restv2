#include <iostream>
#include "TSystem.h"
#include "TFile.h"
#include "TVector3.h"

// Directly include your class headers
#include "TRestTools.h"
#include "TRestMicromegasReadout.h"

using namespace TRestTools;

TRestMicromegasReadout* mmReadout=nullptr;

void GenerateReadout(const std::string& yamlFile = "MicromegasReadout.yaml", const std::string outputFile = "iaxo_readout_output.root") {
    // 1. Load the framework shared libraries
    gSystem->AddIncludePath("-I/home/juanan/restv2/source/core/inc");
    // Detector library headers (Where TRestMicromegasReadout.h lives)
    gSystem->AddIncludePath("-I/home/juanan/restv2/libraries/detector/inc");
    
    // Load the shared objects binaries compiled by CMake
    gSystem->Load("libRestCore.so");
    gSystem->Load("libRestDetector.so");

    std::cout << "[+] ROOT Macro -> Instantiating TRestMicromegasReadout directly..." << std::endl;

    mmReadout = new TRestMicromegasReadout(yamlFile, "readout");

    try {
        mmReadout->BuildGeometry();
    } catch (const std::exception& e) {
        std::cerr << "[-] Error crítico en el ciclo de vida del Readout: " << e.what() << std::endl;
        delete mmReadout;
        return;
    }

    // =========================================================================
    // EXPORT TO EXPERIMENT RUN STORAGE
    // =========================================================================
    TFile* fOut = TFile::Open(outputFile.c_str(), "RECREATE");
    mmReadout->Export(fOut, mmReadout->GetName(), "nominal_decoding");

    //std::cout << "[+] Clean binary export complete. Geometry saved as: " << mmReadout->GetName() << std::endl;
    
    // Note: Do not delete mmReadout here if you want to keep the OpenGL viewer open 
    // inside the interactive ROOT session shell.
}
