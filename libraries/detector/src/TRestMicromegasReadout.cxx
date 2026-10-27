#include "TRestMicromegasReadout.h"

#include <map>
#include <stdexcept>

#include "TGeoBBox.h"
#include "TGeoManager.h"
#include "TGeoMatrix.h"
#include "TGeoNode.h"
#include "TGeoVolume.h"
#include "TRestConstants.h"

using namespace TRestConstants;

static const bool TRestMicromegasReadout_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();

    reg.RegisterNestedField<TRestMicromegasReadout>("readoutParameters", &TRestMicromegasReadout::fReadoutParams);

    return true;
}();


static const bool TRestMicromegasReadoutParameters_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestMicromegasReadoutParameters>("positionRelative", &TRestMicromegasReadoutParameters::fPositionRelative);
    reg.RegisterField<TRestMicromegasReadoutParameters>("globalRotation", &TRestMicromegasReadoutParameters::fGlobalRotation);
    reg.RegisterField<TRestMicromegasReadoutParameters>("nChannels", &TRestMicromegasReadoutParameters::fNChannels);
    reg.RegisterField<TRestMicromegasReadoutParameters>("pitch", &TRestMicromegasReadoutParameters::fPitch);
    reg.RegisterField<TRestMicromegasReadoutParameters>("thickness", &TRestMicromegasReadoutParameters::fThickness);
    reg.RegisterField<TRestMicromegasReadoutParameters>("gasThickness", &TRestMicromegasReadoutParameters::fGasThickness);
    reg.RegisterField<TRestMicromegasReadoutParameters>("normal", &TRestMicromegasReadoutParameters::fNormal);
    reg.RegisterField<TRestMicromegasReadoutParameters>("checkOverlaps", &TRestMicromegasReadoutParameters::fCheckOverlaps);
    return true;
}();

TRestMicromegasReadoutParameters::TRestMicromegasReadoutParameters() : TRestMetadata() {
    fName = "TRestMicromegasReadoutParameters";
}

TRestMicromegasReadoutParameters::TRestMicromegasReadoutParameters(
    const std::string& name, const YAML::Node& node) : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestMicromegasReadoutParameters::LoadConfig() {
    UpdateParamsFromYAML<TRestMicromegasReadoutParameters>(fNode);
    if (fNormal[0] != 0 || fNormal[1] != 0 || fNormal[2] == 0) {
        throw std::runtime_error("TRestMicromegasReadoutParameters: normal must be [0, 0, 1] or [0, 0, -1]");
    }
    UpdateYAMLFromParams<TRestMicromegasReadoutParameters>(fNode);
}


namespace {
/// \brief Registers this metadata type in the REST metadata registry.
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestMicromegasReadout", [](const std::string& instanceName, const YAML::Node& params) {
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

    UpdateParamsFromYAML<TRestMicromegasReadout>(fNode);
    // Sync resolved parameters to the node
    UpdateYAMLFromParams<TRestMicromegasReadout>(fNode);
}

/// \brief Builds a rectangular pixel plane for a Micromegas readout.
/// \param readoutNode YAML node containing `geometry_parameters`.
///
/// The method creates one reusable pixel volume and instantiates nodes arranged
/// in a regular `rows x cols` lattice. Every node receives a monotonically
/// increasing physical identifier stored as `TGeoNode::UniqueID`.
void TRestMicromegasReadout::BuildGeometry() {
    if (!fNode || fNode.IsNull() ) {
        throw std::runtime_error(
            "TRestMicromegasReadout: 'readoutParameters' section is missing or fNode is not initialized!");
    }

    InitializeReadout();
    if (!LoadDecoding(fDecodingFile)) RESTError << "Decoding file not found " << RESTendl;

    const auto pitch = fReadoutParams.fPitch;
    const auto nChannels = fReadoutParams.fNChannels;
    double visibleThickness = fReadoutParams.fThickness;
    const double posRel[3] = {fReadoutParams.fPositionRelative[0], fReadoutParams.fPositionRelative[1],
                              fReadoutParams.fPositionRelative[2]};

    double pixelSize = pitch / std::sqrt(2.0);

    TGeoRotation* rot45 = new TGeoRotation("rot45");
    rot45->RotateZ(45.0);
    TGeoRotation* rotMinus135 = new TGeoRotation("rotMinus135");
    rotMinus135->RotateZ(-135.0);

    TGeoMedium* copperMedium = fGeoManager->GetMedium("copper_medium");
    if (!copperMedium) {
        TGeoMaterial* matCopper = new TGeoMaterial("Copper", 29.0, 63.546, 8.96);
        copperMedium = new TGeoMedium("copper_medium", 1, matCopper);
    }

    TGeoBBox* pixelShape =
        new TGeoBBox("pixel_shape", pixelSize / 2.0, pixelSize / 2.0, visibleThickness / 2.0);

    double moduleSizeX = (fReadoutParams.fNChannels + 1) * pitch - 0.5 * pitch;
    double moduleSizeY = (fReadoutParams.fNChannels + 1) * pitch - 0.75 * pitch;
    double offsetX = -moduleSizeX / 2.0;
    double offsetY = -moduleSizeY / 2.0;

    int nodeCounter = 0;

    TGeoVolumeAssembly* readoutGeom = new TGeoVolumeAssembly("readout_geom_rect");

    double zGlobal = 0.0;

    // Pixels are grouped in one assembly per strip: a single volume holding all 2*N*N pixels overflows
    // the Int_t buffer size computed in TGeoVoxelFinder::SortAll (heap corruption for N >~ 215)
    std::map<int, TGeoVolumeAssembly*> stripAssemblies;
    std::map<int, int> stripPixelCounter;
    const UInt_t kNoPhysicalID = static_cast<UInt_t>(-1);

    auto addPixel = [&](double localX, double localY, TGeoRotation* localRot, int channelID,
                        bool isChannelX) {
        double posX = localX + offsetX + posRel[0];
        double posY = localY + offsetY + posRel[1];
        double posZ = zGlobal + posRel[2];

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

        auto& strip = stripAssemblies[channelID];
        if (!strip) {
            strip = new TGeoVolumeAssembly((volName + "_ASSEMBLY").c_str());
            TGeoNode* stripNode = readoutGeom->AddNode(strip, channelID);
            stripNode->SetUniqueID(kNoPhysicalID);
        }

        TGeoNode* node = strip->AddNode(vol, stripPixelCounter[channelID]++, finalMatrix);
        node->SetUniqueID(channelID);
        nodeCounter++;
    };

    int chX0 = nChannels;
    for (int nPix = 0; nPix < nChannels; ++nPix) {
        addPixel((0.5 + nPix) * pitch, pitch - pitch / 4.0, rotMinus135, chX0, true);
    }

    for (int nCh = 1; nCh <= nChannels - 2; ++nCh) {
        int chID = nChannels + nCh;
        for (int nPix = 0; nPix < nChannels; ++nPix) {
            addPixel((0.5 + nPix) * pitch, nCh * pitch - pitch / 4.0, rot45, chID, true);
        }
    }

    int chXLast = nChannels + nChannels - 1;
    for (int nPix = 0; nPix < nChannels; ++nPix) {
        addPixel((0.5 + nPix) * pitch, (nChannels - 1) * pitch - pitch / 4.0, rot45, chXLast, true);
    }

    int chY0 = 0;
    for (int nPix = 0; nPix < nChannels; ++nPix) {
        addPixel((1.0) * pitch, pitch / 4.0 + nPix * pitch, rot45, chY0, false);
    }

    for (int nCh = 1; nCh <= nChannels - 2; ++nCh) {
        int chID = nCh;
        for (int nPix = 0; nPix < nChannels; ++nPix) {
            addPixel((1.0 + nCh) * pitch, pitch / 4.0 + nPix * pitch, rot45, chID, false);
        }
    }

    int chYLast = nChannels - 1;
    for (int nPix = 0; nPix < nChannels; ++nPix) {
        addPixel(nChannels * pitch, pitch / 4.0 + nPix * pitch, rot45, chYLast, false);
    }

    const int gasID = 2 * nChannels;
    const double gasH = fReadoutParams.fGasThickness;

    if (gasH > 0.0) {
        TGeoMedium* gasMedium = fGeoManager->GetMedium("gas_medium");
        if (!gasMedium) {
            TGeoMaterial* matGas = new TGeoMaterial("Argon", 39.948, 18.0, 1.662e-3);
            gasMedium = new TGeoMedium("gas_medium", 2, matGas);
        }

        const double gasRadius = 0.5 * std::sqrt(moduleSizeX * moduleSizeX + moduleSizeY * moduleSizeY);

        TGeoVolume* gasVol = fGeoManager->MakeTube("GAS", gasMedium, 0.0, gasRadius, gasH / 2.0);
        gasVol->SetLineColor(kAzure);
        gasVol->SetTransparency(65);

        // The gas (drift volume) is on the cathode side of the plane
        const double gasZ =
            posRel[2] + fReadoutParams.GetNormalSign() * (visibleThickness / 2.0 + gasH / 2.0);
        auto* gasMatrix = new TGeoTranslation(posRel[0],
                                              posRel[1], gasZ);

        TGeoNode* gasNode = readoutGeom->AddNode(gasVol, 0, gasMatrix);
        gasNode->SetUniqueID(gasID);
    }


    TGeoHMatrix* globalMatrix = new TGeoHMatrix();
    if (fReadoutParams.fGlobalRotation != 0.0) {
        TGeoRotation* globalRot = new TGeoRotation("globalRot");
        globalRot->RotateZ(fReadoutParams.fGlobalRotation * TMath::RadToDeg());
        globalMatrix->MultiplyLeft(globalRot);
    }

    TGeoVolume* top = fGeoManager->GetTopVolume();
    TGeoNode* readoutNode = top->AddNode(readoutGeom, 0, globalMatrix);
    readoutNode->SetUniqueID(kNoPhysicalID);

    fGeoManager->CloseGeometry();
    std::cout << "[+] Geometry built: " << nodeCounter << " pixels in a single unified Z plane." << std::endl;

    if (fReadoutParams.fCheckOverlaps) {
        // Only the direct daughters of readoutGeom (strips + gas) are checked, which covers X vs Y pixel
        // overlaps: pixels inside a strip cannot overlap by construction. Sampling ("s<npoints>") since the
        // full pairwise check is unaffordable
        std::cout << "[*] Checking geometry overlaps (sampling)..." << std::endl;
        fGeoManager->ClearOverlaps();
        readoutGeom->CheckOverlaps(0.001, "s1000");
        std::cout << "[+] Overlaps found: " << fGeoManager->GetListOfOverlaps()->GetEntriesFast() << std::endl;
    }
}

ROOT::Math::XYZVector TRestMicromegasReadout::GetPositionFromChannel(int daqID) const {
    if (!fGeoManager) {
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
    if (targetPhysicalID < 0) {
        RESTError << "DaqID " << daqID << " not found in Micromegas map" << RESTendl;
        return ROOT::Math::XYZVector(REST_nan, REST_nan, REST_nan);
    }

    double moduleSizeX = (fReadoutParams.fNChannels + 1) * fReadoutParams.fPitch - 0.5 * fReadoutParams.fPitch;
    double moduleSizeY = (fReadoutParams.fNChannels + 1) * fReadoutParams.fPitch - 0.75 * fReadoutParams.fPitch;
    double offsetX = -moduleSizeX / 2.0;
    double offsetY = -moduleSizeY / 2.0;

    double posX = REST_nan;
    double posY = REST_nan;
    double posZ = REST_nan;

    bool isChannelX = (targetPhysicalID >= fReadoutParams.fNChannels);

    if (isChannelX) {
        int nCh = targetPhysicalID - fReadoutParams.fNChannels;
        double localY_fixed = nCh * fReadoutParams.fPitch - fReadoutParams.fPitch / 4.0;
        posX = localY_fixed + offsetX + fReadoutParams.fPositionRelative[0];
    } else {
        int nCh = targetPhysicalID;
        double localX_fixed = (1.0 + nCh) * fReadoutParams.fPitch;
        posY = localX_fixed + offsetY + fReadoutParams.fPositionRelative[1];
    }

    return ROOT::Math::XYZVector(posX, posY, posZ);
}

double TRestMicromegasReadout::GetDistanceToReadout(const ROOT::Math::XYZVector& position) const {
    // Single readout plane at z = positionRelative[2]; the global rotation is around Z, so the normal is +-Z
    return fReadoutParams.GetNormalSign() * (position.Z() - fReadoutParams.fPositionRelative[2]);
}

double TRestMicromegasReadout::GetMaxDistanceToReadout(const ROOT::Math::XYZVector& position) const {
    (void)position;
    if (fReadoutParams.fGasThickness <= 0) return std::numeric_limits<double>::quiet_NaN();
    // Same extent as the GAS volume built in BuildGeometry
    return fReadoutParams.fThickness / 2.0 + fReadoutParams.fGasThickness;
}
