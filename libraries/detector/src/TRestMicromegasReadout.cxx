#include "TRestMicromegasReadout.h"

#include <stdexcept>
#include <vector>

#include "TGeoBBox.h"
#include "TGeoMatrix.h"
#include "TGeoNode.h"
#include "TGeoVolume.h"

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

/// \brief Builds a rectangular pixel plane for a Micromegas readout.
/// \param readoutNode YAML node containing `geometry_parameters`.
///
/// The method creates one reusable pixel volume and instantiates nodes arranged
/// in a regular `rows x cols` lattice. Every node receives a monotonically
/// increasing physical identifier stored as `TGeoNode::UniqueID`.
void TRestMicromegasReadout::BuildGeometry( ) {
    
    if (!fNode || !fNode["geometry_parameters"]) {
        throw std::runtime_error("TRestMicromegasReadout: 'geometry_parameters' section is missing or fNode is not initialized!");
    }

    auto params = fNode["geometry_parameters"];

    if (!params["n_channels"]) {
        throw std::runtime_error("TRestMicromegasReadout: Required parameter 'n_channels' is missing in geometry_parameters!");
    }
    if (!params["pitch"]) {
        throw std::runtime_error("TRestMicromegasReadout: Required parameter 'pitch' is missing in geometry_parameters!");
    }

    InitializeReadout();

    int nChannels = TRestTools::ReadYAMLParam<int>(params["n_channels"]);
    double pitch = TRestTools::ReadYAMLParam<double>(params["pitch"]);     
    
    double pixelSize = pitch / std::sqrt(2.0);
    double visibleThickness = 0.001;

    std::vector<double> positionRelative = {0.0, 0.0, 0.0};
    if (params["position_relative"]) {
        positionRelative = TRestTools::ReadYAMLParamVector<double>(params["position_relative"]);
    }

    auto globalRotAngle = TRestTools::ReadYAMLParamOrDefault<double>(params, "global_rotation", 0.0);    TGeoRotation* globalRot = new TGeoRotation();
    globalRot->RotateZ(globalRotAngle);

    // Definición de Cobre real para evitar warnings
    TGeoMedium* copperMedium = fGeoManager->GetMedium("copper_medium");
    if (!copperMedium) {
        TGeoMaterial* matCopper = new TGeoMaterial("Copper", 29.0, 63.546, 8.96);
        copperMedium = new TGeoMedium("copper_medium", 1, matCopper);
    }

    // Única forma geométrica base: El diamante cuadrado
    TGeoBBox* pixelShape = new TGeoBBox("pixel_shape", pixelSize / 2.0, pixelSize / 2.0, visibleThickness / 2.0);

    // Rotaciones intrínsecas dictadas estrictamente por tu RML
    TGeoRotation* rot45 = new TGeoRotation(); rot45->RotateZ(45.0);
    TGeoRotation* rotMinus135 = new TGeoRotation(); rotMinus135->RotateZ(-135.0);

    // Centrado simétrico exacto del módulo en base al tamaño definido en tu RML
    double moduleSizeX = (nChannels + 1) * pitch - 0.5 * pitch;
    double moduleSizeY = (nChannels + 1) * pitch - 0.75 * pitch;
    double offsetX = -moduleSizeX / 2.0;
    double offsetY = -moduleSizeY / 2.0;

    int nodeCounter = 0;

    // Función auxiliar unificada: Crea volúmenes dedicados por canal y gestiona el color inicial
    auto addPixel = [&](double localX, double localY, double localZ, TGeoRotation* localRot, int channelID, bool isChannelX) {
        double posX = localX + offsetX + positionRelative[0];
        double posY = localY + offsetY + positionRelative[1];
        double posZ = localZ + positionRelative[2];

        TGeoCombiTrans localTrans(posX, posY, posZ, localRot);
        TGeoCombiTrans globalTrans(0, 0, 0, globalRot);
        TGeoHMatrix* finalMatrix = new TGeoHMatrix(globalTrans * localTrans);

        // Nombres de volumen independientes para separar pistas X de pistas Y
        std::string baseName = isChannelX ? "STRIP_X_" : "STRIP_Y_";
        std::string volName = baseName + std::to_string(channelID);
        
        TGeoVolume* vol = fGeoManager->GetVolume(volName.c_str());
        if (!vol) {
            vol = new TGeoVolume(volName.c_str(), pixelShape, copperMedium);
            
            // ASIGNAR COLORES DIFERENTES POR DEFECTO PARA EL VISOR
            if (isChannelX) {
                vol->SetLineColor(kBlack);
            } else {
                vol->SetLineColor(kGray);
            }
            fGeoManager->AddVolume(vol);
        }

        TGeoNode* node = fTopAssembly->AddNode(vol, nodeCounter, finalMatrix);
        node->SetUniqueID(channelID); 
        nodeCounter++;

    };

    double zX = -visibleThickness;

    // First strip is special (X0) -> ID: nChannels
    int chX0 = nChannels;
    for (int nPix = 0; nPix < nChannels; ++nPix) {
        addPixel((0.5 + nPix) * pitch, pitch - pitch / 4.0, zX, rotMinus135, chX0, true);
    }

    // Intermediate X-strips -> IDs: nChannels + 1 a 2*nChannels - 2
    for (int nCh = 1; nCh <= nChannels - 2; ++nCh) {
        int chID = nChannels + nCh;
        for (int nPix = 0; nPix < nChannels; ++nPix) {
            addPixel((0.5 + nPix) * pitch, nCh * pitch - pitch / 4.0, zX, rot45, chID, true);
        }
    }

    // Last strip is special (X_nChannels-1) -> ID: 2*nChannels - 1
    int chXLast = nChannels + nChannels - 1;
    for (int nPix = 0; nPix < nChannels; ++nPix) {
        addPixel((0.5 + nPix) * pitch, (nChannels - 1) * pitch - pitch / 4.0, zX, rot45, chXLast, true);
    }


    double zY = 0.0;

    // First strip is special (Y0) -> ID: 0
    int chY0 = 0;
    for (int nPix = 0; nPix < nChannels; ++nPix) {
        addPixel((1.0) * pitch, pitch / 4.0 + nPix * pitch, zY, rot45, chY0, false);
    }

    // Intermediate Y-strips -> IDs: 1 a nChannels-2
    for (int nCh = 1; nCh <= nChannels - 2; ++nCh) {
        int chID = nCh;
        for (int nPix = 0; nPix < nChannels; ++nPix) {
            addPixel((1.0 + nCh) * pitch, pitch / 4.0 + nPix * pitch, zY, rot45, chID, false);
        }
    }

    // Last strip is special (Y_nChannels-1) -> ID: nChannels-1
    int chYLast = nChannels - 1;
    for (int nPix = 0; nPix < nChannels; ++nPix) {
        addPixel(nChannels * pitch, pitch / 4.0 + nPix * pitch, zY, rot45, chYLast, false);
    }

    fTopAssembly->GetShape()->ComputeBBox();
    fTopAssembly->Voxelize("");
    std::cout << "[+] Geometry built: " << nodeCounter << " pixels separated into X (Brown) and Y (Cyan)." << std::endl;

    fGeoManager->CloseGeometry();
}
