#include <iostream>
#include <memory>

#include "TFile.h"
#include "TSystem.h"

// Directly include your class headers
#include "TRestMicromegasReadout.h"
#include "TRestTools.h"

using namespace TRestTools;

/// \brief Imports a readout (any TRestDetectorReadout, e.g. a TRestMicromegasReadout) from a ROOT file
/// written by REST_Detector_GenerateMicromegasReadout, prints it, tests the position lookup
/// (GetChannelFromPosition at (0.5, 0.5, 110) mm and GetPositionFromChannel of some DAQ channels) and
/// draws the geometry with those channels highlighted (ViewActiveEvent). Throws if the readout metadata is
/// not in the file; prints an error and returns if the file cannot be opened or the readout or its decoding
/// cannot be imported. The input file is closed after the import.
/// \param instanceName Name of the readout in the file.
/// \param decodingName Name of the decoding stored with the readout.
/// \param inputFile ROOT file with the readout.
///
/// Usage in restRoot (from the examples directory):
///
///    restRoot
///    [0] REST_Detector_ViewMicromegasReadout("IAXO_D1_Readout", "nominal_decoding",
///    "iaxo_readout_output.root")
void REST_Detector_ViewMicromegasReadout(const std::string& instanceName = "IAXO_D1_Readout",
                                         const std::string& decodingName = "nominal_decoding",
                                         const std::string inputFile = "iaxo_readout_output.root") {
    // Closed after the import: the readout keeps its own copy of the geometry, which is what is drawn
    std::unique_ptr<TFile> fIn(TFile::Open(inputFile.c_str(), "READ"));
    if (!fIn || fIn->IsZombie()) {
        std::cerr << "[-] Cannot open the input file " << inputFile << std::endl;
        return;
    }
    auto configNode = TRestMetadata::ReadMetadata(fIn.get(), instanceName);

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

    if (!readout->Import(fIn.get(), instanceName, decodingName)) {
        std::cerr << "[-] Cannot import the readout '" << instanceName << "' with the decoding '"
                  << decodingName << "' from " << inputFile << std::endl;
        return;
    }
    fIn->Close();
    readout->PrintMetadata();

    double testX = 0.5;
    double testY = 0.5;
    double testZ = 110.0;  // Math matched to the YAML relative offset [-10 mm]

    std::cout << "\n--- TESTING SPATIAL LOOKUP ---" << std::endl;

    int daqChannel = readout->GetChannelFromPosition(testX, testY, testZ);
    std::cout << "Hit at (" << testX << ", " << testY << ", " << testZ
              << ") mm maps to DAQ ID: " << daqChannel << std::endl;

    std::vector ev = {200, 201, 202, daqChannel - 1, daqChannel, daqChannel + 1};

    if (daqChannel != -1) {
        for (const auto& ch : ev) {
            ROOT::Math::XYZVector centroid = readout->GetPositionFromChannel(ch);
            std::cout << "DAQ ID " << ch << " back-projects to centroid: (" << centroid.X() << ", "
                      << centroid.Y() << ", " << centroid.Z() << ") mm" << std::endl;
        }
    }

    // =========================================================================
    // VISUALIZE GRAPHICALLY (Universal TGeo method inherited from base class)
    // =========================================================================
    std::cout << "\n[+] Spawning interactive 3D geometry viewer window..." << std::endl;
    readout->ViewActiveEvent(ev);  // Spawns ROOT high-speed OpenGL window
    // mmReadout->ViewReadoutGeometry();
}
