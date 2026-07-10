#include "TRestConstants.h"
#include "TRestMicromegasReadout.h"

#include <stdexcept>
#include <vector>

#include "TGeoBBox.h"
#include "TGeoMatrix.h"
#include "TGeoNode.h"
#include "TGeoVolume.h"

using namespace TRestConstants;

static const bool TRestMicromegasReadout_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();

    reg.RegisterField<TRestMicromegasReadout>("positionRelative",  &TRestMicromegasReadout::fPositionRelative);
    reg.RegisterField<TRestMicromegasReadout>("globalRotation",    &TRestMicromegasReadout::fGlobalRotation);
    reg.RegisterField<TRestMicromegasReadout>("nChannels",         &TRestMicromegasReadout::fNChannels);
    reg.RegisterField<TRestMicromegasReadout>("pitch",             &TRestMicromegasReadout::fPitch);
    reg.RegisterField<TRestMicromegasReadout>("thickness",         &TRestMicromegasReadout::fThickness);

    return true;
}();

namespace {
/// \brief Registers this metadata type in the REST metadata registry.
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestMicromegasReadout",
        [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestMicromegasReadout>(instanceName, params);

        });
    return true;
}();
}  // namespace

TRestMicromegasReadout::TRestMicromegasReadout() : TRestDetectorReadout() {
    fName = "TRestMicromegasReadout";
}

TRestMicromegasReadout::TRestMicromegasReadout(const std::string& instanceName, const YAML::Node& node)
    : TRestDetectorReadout(instanceName, node) {
    LoadConfig();
}

TRestMicromegasReadout::TRestMicromegasReadout(const std::string& fileName, const std::string& sectionName)
    : TRestDetectorReadout(fileName, sectionName) {
    LoadConfig();
}

void TRestMicromegasReadout::LoadConfig() {
    TRestDetectorReadout::LoadConfig();

    if (!fNode || fNode.IsNull() ) {
        RESTError << "TRestMicromegasReadout::LoadConfig YAML node is missing" << RESTendl;
        return;
    }

    fReadoutNode = fNode["readoutParameters"];

    if (!fReadoutNode || fReadoutNode.IsNull() ) {
       RESTError << "TRestMicromegasReadout::LoadConfig - 'readoutParameters' section is missing" << RESTendl;
       return;
    }

    UpdateParamsFromYAML<TRestMicromegasReadout>(fReadoutNode);
    //Sync resolved parameters to the node
    UpdateYAMLFromParams<TRestMicromegasReadout>(fReadoutNode);
}

/// \brief Builds a rectangular pixel plane for a Micromegas readout.
/// \param readoutNode YAML node containing `geometry_parameters`.
///
/// The method creates one reusable pixel volume and instantiates nodes arranged
/// in a regular `rows x cols` lattice. Every node receives a monotonically
/// increasing physical identifier stored as `TGeoNode::UniqueID`.
void TRestMicromegasReadout::BuildGeometry( ) {
    
    if ((!fNode || fNode.IsNull()) || !fReadoutNode || fNode.IsNull() ) {
        throw std::runtime_error("TRestMicromegasReadout: 'readoutParameters' section is missing or fNode is not initialized!");
    }

    PrintMetadata();

    InitializeReadout();
    
    double pixelSize = fPitch / std::sqrt(2.0);
    double visibleThickness = fThickness; // Parámetro recuperado del YAML

    TGeoRotation* rot45 = new TGeoRotation("rot45"); rot45->RotateZ(45.0);
    TGeoRotation* rotMinus135 = new TGeoRotation("rotMinus135"); rotMinus135->RotateZ(-135.0);

    TGeoMedium* copperMedium = fGeoManager->GetMedium("copper_medium");
    if (!copperMedium) {
        TGeoMaterial* matCopper = new TGeoMaterial("Copper", 29.0, 63.546, 8.96);
        copperMedium = new TGeoMedium("copper_medium", 1, matCopper);
    }

    TGeoBBox* pixelShape = new TGeoBBox("pixel_shape", pixelSize / 2.0, pixelSize / 2.0, visibleThickness / 2.0);

    double moduleSizeX = (fNChannels + 1) * fPitch - 0.5 * fPitch;
    double moduleSizeY = (fNChannels + 1) * fPitch - 0.75 * fPitch;
    double offsetX = -moduleSizeX / 2.0;
    double offsetY = -moduleSizeY / 2.0;

    int nodeCounter = 0;

    TGeoVolumeAssembly* readoutGeom = new TGeoVolumeAssembly("readout_geom_rect");

    double zGlobal = 0.0;

    auto addPixel = [&](double localX, double localY, TGeoRotation* localRot, int channelID, bool isChannelX) {
        double posX = localX + offsetX + fPositionRelative[0];
        double posY = localY + offsetY + fPositionRelative[1];
        double posZ = zGlobal + fPositionRelative[2];

        TGeoCombiTrans* finalMatrix = new TGeoCombiTrans(posX, posY, posZ, localRot);

        std::string baseName = isChannelX ? "STRIP_X_" : "STRIP_Y_";
        std::string volName = baseName + std::to_string(channelID);
        
        TGeoVolume* vol = fGeoManager->GetVolume(volName.c_str());
        if (!vol) {
            vol = new TGeoVolume(volName.c_str(), pixelShape, copperMedium);
            
            if (isChannelX) {
                vol->SetLineColor(kBlack);
            } else {
                vol->SetLineColor(kGray);
            }
        }

        TGeoNode* node = readoutGeom->AddNode(vol, nodeCounter, finalMatrix);
        node->SetUniqueID(channelID); 
        nodeCounter++;
    };

    int chX0 = fNChannels;
    for (int nPix = 0; nPix < fNChannels; ++nPix) {
        addPixel((0.5 + nPix) * fPitch, fPitch - fPitch / 4.0, rotMinus135, chX0, true);
    }

    for (int nCh = 1; nCh <= fNChannels - 2; ++nCh) {
        int chID = fNChannels + nCh;
        for (int nPix = 0; nPix < fNChannels; ++nPix) {
            addPixel((0.5 + nPix) * fPitch, nCh * fPitch - fPitch / 4.0, rot45, chID, true);
        }
    }

    int chXLast = fNChannels + fNChannels - 1;
    for (int nPix = 0; nPix < fNChannels; ++nPix) {
        addPixel((0.5 + nPix) * fPitch, (fNChannels - 1) * fPitch - fPitch / 4.0, rot45, chXLast, true);
    }

    int chY0 = 0;
    for (int nPix = 0; nPix < fNChannels; ++nPix) {
        addPixel((1.0) * fPitch, fPitch / 4.0 + nPix * fPitch, rot45, chY0, false);
    }

    for (int nCh = 1; nCh <= fNChannels - 2; ++nCh) {
        int chID = nCh;
        for (int nPix = 0; nPix < fNChannels; ++nPix) {
            addPixel((1.0 + nCh) * fPitch, fPitch / 4.0 + nPix * fPitch, rot45, chID, false);
        }
    }

    int chYLast = fNChannels - 1;
    for (int nPix = 0; nPix < fNChannels; ++nPix) {
        addPixel(fNChannels * fPitch, fPitch / 4.0 + nPix * fPitch, rot45, chYLast, false);
    }

    TGeoHMatrix* globalMatrix = new TGeoHMatrix();
    if (fGlobalRotation != 0.0) {
        TGeoRotation* globalRot = new TGeoRotation("globalRot");
        globalRot->RotateZ(fGlobalRotation);
        globalMatrix->MultiplyLeft(globalRot);
    }

    fTopAssembly->AddNode(readoutGeom, 0, globalMatrix);

    fTopAssembly->GetShape()->ComputeBBox();
    fTopAssembly->Voxelize("");
    std::cout << "[+] Geometry built: " << nodeCounter << " pixels in a single unified Z plane." << std::endl;

    std::cout << "[*] Checking geometry overlaps..." << std::endl;
    fGeoManager->CheckOverlaps(0.001); 

    fGeoManager->CloseGeometry();
}

ROOT::Math::XYZVector TRestMicromegasReadout::GetPositionFromChannel(int daqID) const {
    if (!fTopAssembly) {
      RESTError << "Geometry not initialized in TRestMicromegasReadout" << RESTendl;
      return ROOT::Math::XYZVector(REST_nan, REST_nan, REST_nan);
    }

    int targetPhysicalID = -1;
    for (const auto& [physicalID, channelID] : fPhysicalToDAQMap) {
        if (channelID == daqID) {
            targetPhysicalID = physicalID;
            break;
        }
    }
    if (targetPhysicalID < 0){
      RESTError << "DaqID " << daqID << " not found in Micromegas map" << RESTendl;
      return ROOT::Math::XYZVector(REST_nan, REST_nan, REST_nan);
    }

    double moduleSizeX = (fNChannels + 1) * fPitch - 0.5 * fPitch;
    double moduleSizeY = (fNChannels + 1) * fPitch - 0.75 * fPitch;
    double offsetX = -moduleSizeX / 2.0;
    double offsetY = -moduleSizeY / 2.0;

    double posX = REST_nan;
    double posY = REST_nan;
    double posZ = REST_nan; 

    bool isChannelX = (targetPhysicalID >= fNChannels);

    if (isChannelX) {
        int nCh = targetPhysicalID - fNChannels; 
        double localY_fixed = nCh * fPitch - fPitch / 4.0; 
        posX = localY_fixed + offsetX + fPositionRelative[0];
    } else {
        int nCh = targetPhysicalID;
        double localX_fixed = (1.0 + nCh) * fPitch;
        posY = localX_fixed + offsetY + fPositionRelative[1];
    }

    return ROOT::Math::XYZVector(posX, posY, posZ);
}
