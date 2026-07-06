#include <iostream>
#include "TSystem.h"
#include "TFile.h"
#include "TVector3.h"

// Directly include your class headers
#include "TRestTools.h"
#include "TRestMicromegasReadout.h"

using namespace TRestTools;

void ViewReadout(const std::string& instanceName = "IAXO_D1_Readout", const std::string &decodingName="iaxo_d0_nominal.dec", const std::string inputFile = "iaxo_readout_output.root") {
     // 1. Load the framework shared libraries
    gSystem->AddIncludePath("-I/home/juanan/restv2/source/core/inc");
    // Detector library headers (Where TRestMicromegasReadout.h lives)
    gSystem->AddIncludePath("-I/home/juanan/restv2/libraries/detector/inc");
    
    // Load the shared objects binaries compiled by CMake
    gSystem->Load("libRestCore.so");
    gSystem->Load("libRestDetector.so");

    TFile* fIn = TFile::Open(inputFile.c_str(), "READ");
    auto configNode = TRestMetadata::ReadMetadata(fIn, instanceName);

    if (!configNode["class"]) {
        throw std::runtime_error("Class node not found in metadata");
    }
    std::string className = configNode["class"].as<std::string>();

    std::unique_ptr<TRestMetadata> genericMetadata = 
        MetadataRegistry::Instance().Create(className, instanceName, configNode);

    if (!genericMetadata) {
        throw std::runtime_error(className + " not found in MetadataRegistry.");
    }

    std::unique_ptr<TRestDetectorReadout> readout(dynamic_cast<TRestDetectorReadout*>(genericMetadata.release()));

    if (!readout) {
        throw std::runtime_error("Cannot cast to TRestDetectorReadout.");
    }

    readout->Import(fIn,instanceName,decodingName);

    double testX = 0.5;
    double testY = 0.5;
    double testZ = -15.0; // Math matched to the YAML relative offset [-10 mm]

    std::cout << "\n--- TESTING SPATIAL LOOKUP ---" << std::endl;
    
    int daqChannel = readout->GetChannelFromPosition(testX, testY, testZ);
    std::cout << "Hit at (" << testX << ", " << testY << ", " << testZ << ") mm maps to DAQ ID: " << daqChannel << std::endl;

    if (daqChannel != -1) {
        TVector3 centroid = readout->GetPositionFromChannel(daqChannel);
        std::cout << "DAQ ID " << daqChannel << " back-projects to centroid: (" 
                  << centroid.X() << ", " << centroid.Y() << ", " << centroid.Z() << ") mm" << std::endl;
    }

    // =========================================================================
    // VISUALIZE GRAPHICALLY (Universal TGeo method inherited from base class)
    // =========================================================================
    std::cout << "\n[+] Spawning interactive 3D geometry viewer window..." << std::endl;
    std::vector ev = {200,201,202,daqChannel-1, daqChannel,daqChannel+1};
    readout->ViewActiveEvent(ev); // Spawns ROOT high-speed OpenGL window
    //mmReadout->ViewReadoutGeometry();


}
