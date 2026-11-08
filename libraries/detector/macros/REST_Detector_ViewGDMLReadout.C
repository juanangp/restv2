#include <iostream>
#include <memory>

#include "TFile.h"
#include "TSystem.h"

// Directly include your class headers
#include "TRestGDMLReadout.h"
#include "TRestTools.h"

TRestGDMLReadout* gmdlReadout = nullptr;

using namespace TRestTools;

/// \brief Imports a readout (any TRestDetectorReadout, e.g. the TRestGDMLReadout of the vetoes) from a
/// ROOT file written by REST_Detector_GenerateGDMLReadout, prints it, tests the position lookup
/// (GetChannelFromPosition at a fixed point, GetPositionFromChannel of the DAQ channels 289 to 292) and
/// draws the geometry with those channels highlighted (ViewActiveEvent). Throws if the readout is not in
/// the file; prints an error and returns if the file cannot be opened or the readout or its decoding cannot
/// be imported. The input file is closed after the import.
/// \param instanceName Name of the readout in the file.
/// \param decodingName Name of the decoding stored with the readout.
/// \param inputFile ROOT file with the readout.
///
/// Usage in restRoot (from the examples directory):
///
///    restRoot
///    [0] REST_Detector_ViewGDMLReadout("Veto_Readout", "veto_decoding", "veto_readout_output.root")
void REST_Detector_ViewGDMLReadout(const std::string& instanceName = "Veto_Readout",
                                   const std::string& decodingName = "veto_decoding",
                                   const std::string inputFile = "veto_readout_output.root") {
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

    std::cout << "\n--- TESTING SPATIAL LOOKUP ---" << std::endl;

    int daqChannel = readout->GetChannelFromPosition(0, 327, 189.5);
    std::cout << "Hit at (" << 0 << ", " << 327 << ", " << 189.5 << ") mm maps to DAQ ID: " << daqChannel
              << std::endl;

    std::vector ev = {289, 290, 291, 292};

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
