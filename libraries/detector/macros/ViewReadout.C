#include <iostream>
#include "TSystem.h"
#include "TFile.h"

// Directly include your class headers
#include "TRestTools.h"
#include "TRestMicromegasReadout.h"

using namespace TRestTools;

void ViewReadout(const std::string& instanceName = "IAXO_D1_Readout", const std::string &decodingName="iaxo_d0_nominal.dec", const std::string inputFile = "iaxo_readout_output.root") {
   
    TFile* fIn = TFile::Open(inputFile.c_str(), "READ");
    auto configNode = TRestMetadata::ReadMetadata(fIn, instanceName);

    if (!configNode["class"]) {
        throw std::runtime_error("Class node not found in metadata");
    }
    std::string className = configNode["class"].as<std::string>();

    std::unique_ptr<TRestMetadata> genericMetadata = 
        MetadataClassRegistry::Instance().Create(className, instanceName, configNode);

    if (!genericMetadata) {
        throw std::runtime_error(className + " not found in MetadataRegistry.");
    }

    std::unique_ptr<TRestDetectorReadout> readout(dynamic_cast<TRestDetectorReadout*>(genericMetadata.release()));

    if (!readout) {
        throw std::runtime_error("Cannot cast to TRestDetectorReadout.");
    }

    readout->Import(fIn,instanceName,decodingName);
    readout->PrintMetadata();

    double testX = 0.5;
    double testY = 0.5;
    double testZ = -15.0; // Math matched to the YAML relative offset [-10 mm]

    std::cout << "\n--- TESTING SPATIAL LOOKUP ---" << std::endl;
    
    int daqChannel = readout->GetChannelFromPosition(testX, testY, testZ);
    std::cout << "Hit at (" << testX << ", " << testY << ", " << testZ << ") mm maps to DAQ ID: " << daqChannel << std::endl;

    std::vector ev = {200,201,202,daqChannel-1, daqChannel,daqChannel+1};

    if (daqChannel != -1) {
      for(const auto &ch : ev){
        ROOT::Math::XYZVector centroid = readout->GetPositionFromChannel(ch);
        std::cout << "DAQ ID " << ch << " back-projects to centroid: (" 
                  << centroid.X() << ", " << centroid.Y() << ", " << centroid.Z() << ") mm" << std::endl;
      }
    }

    // =========================================================================
    // VISUALIZE GRAPHICALLY (Universal TGeo method inherited from base class)
    // =========================================================================
    std::cout << "\n[+] Spawning interactive 3D geometry viewer window..." << std::endl;
    readout->ViewActiveEvent(ev); // Spawns ROOT high-speed OpenGL window
    //mmReadout->ViewReadoutGeometry();


}
