#include <iostream>

#include "TFile.h"
#include "TSystem.h"

// Directly include your class headers
#include "TRestGDMLReadout.h"
#include "TRestTools.h"

using namespace TRestTools;

TRestGDMLReadout* gdmlReadout = nullptr;

void GenerateGDMLReadout(const std::string& yamlFile = "GDMLReadout.yaml",
                     const std::string outputFile = "veto_readout_output.root") {
    std::cout << "[+] ROOT Macro -> Instantiating TRestGDMLReadout directly..." << std::endl;

    gdmlReadout = new TRestGDMLReadout(yamlFile, "readout");
    gdmlReadout->PrintMetadata();

    try {
        gdmlReadout->BuildGeometry();
    } catch (const std::exception& e) {
        std::cerr << "[-] Error crítico en el ciclo de vida del Readout: " << e.what() << std::endl;
        delete gdmlReadout;
        return;
    }

    TFile* fOut = TFile::Open(outputFile.c_str(), "RECREATE");
    gdmlReadout->Export(fOut, gdmlReadout->GetName(), "veto_decoding.dec");
    fOut->Close();
}
