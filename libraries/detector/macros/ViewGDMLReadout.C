#include <iostream>

#include "TFile.h"
#include "TSystem.h"

// Directly include your class headers
#include "TRestGDMLReadout.h"
#include "TRestTools.h"

TRestGDMLReadout *gmdlReadout = nullptr;

using namespace TRestTools;

void ViewGDMLReadout(const std::string& instanceName = "Veto_Readout",
                 const std::string& decodingName = "veto.dec",
                 const std::string inputFile = "veto_readout_output.root") {
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

    std::unique_ptr<TRestDetectorReadout> readout(
        dynamic_cast<TRestDetectorReadout*>(genericMetadata.release()));

    if (!readout) {
        throw std::runtime_error("Cannot cast to TRestDetectorReadout.");
    }

    readout->Import(fIn, instanceName, decodingName);
    readout->PrintMetadata();

    std::cout << "\n--- TESTING SPATIAL LOOKUP ---" << std::endl;

    int daqChannel = readout->GetChannelFromPosition(0, 327, 189.5);
    std::cout << "Hit at (" << 0 << ", " << 327 << ", " << 189.5
              << ") mm maps to DAQ ID: " << daqChannel << std::endl;

    std::vector ev = {289,290,291,292};

        for (const auto& ch : ev) {
            ROOT::Math::XYZVector centroid = readout->GetPositionFromChannel(ch);
            std::cout << "DAQ ID " << ch << " back-projects to centroid: (" << centroid.X() << ", "
                      << centroid.Y() << ", " << centroid.Z() << ") mm" << std::endl;
        }

    // =========================================================================
    // VISUALIZE GRAPHICALLY (Universal TGeo method inherited from base class)
    // =========================================================================
    std::cout << "\n[+] Spawning interactive 3D geometry viewer window..." << std::endl;
    readout->ViewActiveEvent(ev);  // Spawns ROOT high-speed OpenGL window
}
