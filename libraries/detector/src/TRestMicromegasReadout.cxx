#include "TRestMicromegasReadout.h"

#include <algorithm>
#include <cmath>
#include <limits>
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

    reg.RegisterNestedField<TRestMicromegasReadout>("readoutParameters",
                                                    &TRestMicromegasReadout::fReadoutParams);

    return true;
}();

static const bool TRestMicromegasReadoutParameters_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestMicromegasReadoutParameters>("positionRelative",
                                                        &TRestMicromegasReadoutParameters::fPositionRelative);
    reg.RegisterField<TRestMicromegasReadoutParameters>("globalRotation",
                                                        &TRestMicromegasReadoutParameters::fGlobalRotation);
    reg.RegisterField<TRestMicromegasReadoutParameters>("nChannels",
                                                        &TRestMicromegasReadoutParameters::fNChannels);
    reg.RegisterField<TRestMicromegasReadoutParameters>("pitch", &TRestMicromegasReadoutParameters::fPitch);
    reg.RegisterField<TRestMicromegasReadoutParameters>("thickness",
                                                        &TRestMicromegasReadoutParameters::fThickness);
    reg.RegisterField<TRestMicromegasReadoutParameters>("gasThickness",
                                                        &TRestMicromegasReadoutParameters::fGasThickness);
    reg.RegisterField<TRestMicromegasReadoutParameters>("normal", &TRestMicromegasReadoutParameters::fNormal);
    reg.RegisterField<TRestMicromegasReadoutParameters>("checkOverlaps",
                                                        &TRestMicromegasReadoutParameters::fCheckOverlaps);
    return true;
}();

TRestMicromegasReadoutParameters::TRestMicromegasReadoutParameters() : TRestMetadata() {
    fName = "TRestMicromegasReadoutParameters";
}

TRestMicromegasReadoutParameters::TRestMicromegasReadoutParameters(const std::string& name,
                                                                   const YAML::Node& node)
    : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestMicromegasReadoutParameters::LoadConfig() {
    UpdateParamsFromYAML<TRestMicromegasReadoutParameters>(fNode);
    const int nonZero = (fNormal[0] != 0) + (fNormal[1] != 0) + (fNormal[2] != 0);
    if (nonZero != 1) {
        throw std::runtime_error(
            "TRestMicromegasReadoutParameters: normal must be along an axis ([+-1, 0, 0], [0, +-1, 0] or "
            "[0, 0, +-1])");
    }
    // A unit vector
    for (auto& component : fNormal) component = component > 0 ? 1 : component < 0 ? -1 : 0;
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

/// \brief Builds the strip plane of a Micromegas readout from `readoutParameters`.
///
/// Each strip is an assembly of nChannels pixels (one volume per strip); every pixel node stores the
/// physical ID of its strip as `TGeoNode::UniqueID` (Y strips 0 to nChannels - 1, X strips nChannels to
/// 2 nChannels - 1).
void TRestMicromegasReadout::BuildGeometry() {
    if (!fNode || fNode.IsNull()) {
        throw std::runtime_error(
            "TRestMicromegasReadout: 'readoutParameters' section is missing or fNode is not initialized!");
    }

    InitializeReadout();
    if (!LoadDecoding(fDecodingFile)) {
        throw std::runtime_error("TRestMicromegasReadout::BuildGeometry - cannot load the decoding file '" +
                                 fDecodingFile + "' (not found or without channels)");
    }

    const auto pitch = fReadoutParams.fPitch;
    const auto nChannels = fReadoutParams.fNChannels;
    double visibleThickness = fReadoutParams.fThickness;
    // The plane is built in its local frame (X and Y strips along local X and Y, normal along local Z) and
    // placed with the rotation of its axes (globalMatrix): positionRelative in that frame
    const int axisX = fReadoutParams.GetStripAxisX(), axisY = fReadoutParams.GetStripAxisY();
    const int axisNormal = fReadoutParams.GetNormalAxis();
    const double posRel[3] = {fReadoutParams.fPositionRelative[axisX],
                              fReadoutParams.fPositionRelative[axisY],
                              fReadoutParams.fPositionRelative[axisNormal]};

    double pixelSize = pitch / std::sqrt(2.0);

    TGeoRotation* rot45 = new TGeoRotation("rot45");
    rot45->RotateZ(45.0);
    TGeoRotation* rotMinus135 = new TGeoRotation("rotMinus135");
    rotMinus135->RotateZ(-135.0);

    TGeoMedium* copperMedium = fGeoManager->GetMedium("copper_medium");
    if (!copperMedium) {
        TGeoMaterial* matCopper = new TGeoMaterial("Copper", 63.546, 29.0, 8.96);
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
    // the int buffer size computed in TGeoVoxelFinder::SortAll (heap corruption for N >~ 215)
    std::map<int, TGeoVolumeAssembly*> stripAssemblies;
    std::map<int, int> stripPixelCounter;
    const unsigned int kNoPhysicalID = static_cast<unsigned int>(-1);

    // Pixel centred at (x, y) in the module frame (before the offsets and positionRelative)
    auto addPixel = [&](double x, double y, TGeoRotation* localRot, int channelID, bool isChannelX) {
        double posX = x + offsetX + posRel[0];
        double posY = y + offsetY + posRel[1];
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

    // The positions are the nominal ones of GetPositionFromChannel (those of the DAQ).
    // X channel nCh (physical ID nChannels + nCh): column of pixels at x = nCh * pitch - pitch / 4
    for (int nCh = 0; nCh < nChannels; ++nCh) {
        TGeoRotation* rot = nCh == 0 ? rotMinus135 : rot45;
        for (int nPix = 0; nPix < nChannels; ++nPix) {
            addPixel(nCh * pitch - pitch / 4.0, (0.5 + nPix) * pitch, rot, nChannels + nCh, true);
        }
    }

    // Y channel nCh (physical ID nCh): row of pixels at y = (1 + nCh) * pitch, between the X pixels
    for (int nCh = 0; nCh < nChannels; ++nCh) {
        for (int nPix = 0; nPix < nChannels; ++nPix) {
            addPixel(pitch / 4.0 + nPix * pitch, (1.0 + nCh) * pitch, rot45, nCh, false);
        }
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
        auto* gasMatrix = new TGeoTranslation(posRel[0], posRel[1], gasZ);

        TGeoNode* gasNode = readoutGeom->AddNode(gasVol, 0, gasMatrix);
        gasNode->SetUniqueID(gasID);
    }

    TGeoHMatrix* globalMatrix = new TGeoHMatrix();
    if (fReadoutParams.fGlobalRotation != 0.0) {
        TGeoRotation* globalRot = new TGeoRotation("globalRot");
        globalRot->RotateZ(fReadoutParams.fGlobalRotation * TMath::RadToDeg());
        globalMatrix->MultiplyLeft(globalRot);
    }
    // Local axes X, Y, Z to the global axes of the X strips, Y strips and normal (a cyclic permutation:
    // a proper rotation)
    if (axisNormal != 2) {
        double axes[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};  // Row major: global = axes * local
        axes[axisX * 3 + 0] = 1;
        axes[axisY * 3 + 1] = 1;
        axes[axisNormal * 3 + 2] = 1;
        TGeoRotation* axesRotation = new TGeoRotation("axesRotation");
        axesRotation->SetMatrix(axes);
        globalMatrix->MultiplyLeft(axesRotation);
    }

    TGeoVolume* top = fGeoManager->GetTopVolume();
    TGeoNode* readoutNode = top->AddNode(readoutGeom, 0, globalMatrix);
    readoutNode->SetUniqueID(kNoPhysicalID);

    fGeoManager->CloseGeometry();
    std::cout << "[+] Geometry built: " << nodeCounter << " pixels in a single plane." << std::endl;

    if (fReadoutParams.fCheckOverlaps) {
        const int overlaps = CheckOverlaps();
        if (overlaps > 0) {
            throw std::runtime_error("TRestMicromegasReadout::BuildGeometry - " + std::to_string(overlaps) +
                                     " geometry overlaps in '" + GetName() + "'");
        }
    }
}

int TRestMicromegasReadout::CheckOverlaps(double tolerance) const {
    TGeoVolume* plane = fGeoManager ? fGeoManager->GetVolume("readout_geom_rect") : nullptr;
    if (!plane) return 0;
    std::cout << "[*] Checking geometry overlaps (sampling)..." << std::endl;
    fGeoManager->ClearOverlaps();
    plane->CheckOverlaps(tolerance, "s1000");
    return PrintOverlaps(fGeoManager);
}

std::pair<int, int> TRestMicromegasReadout::GetAdjacentChannels(int daqID) const {
    auto adjacent = TRestDetectorReadout::GetAdjacentChannels(daqID);
    const int nChannels = fReadoutParams.fNChannels;
    const int physicalID = GetPhysicalID(daqID);
    if (physicalID == nChannels) adjacent.first = -1;       // first X strip
    if (physicalID == nChannels - 1) adjacent.second = -1;  // last Y strip
    return adjacent;
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
        RESTWarning << "DaqID " << daqID << " not found in Micromegas map" << RESTendl;
        return ROOT::Math::XYZVector(REST_nan, REST_nan, REST_nan);
    }

    double moduleSizeX =
        (fReadoutParams.fNChannels + 1) * fReadoutParams.fPitch - 0.5 * fReadoutParams.fPitch;
    double moduleSizeY =
        (fReadoutParams.fNChannels + 1) * fReadoutParams.fPitch - 0.75 * fReadoutParams.fPitch;
    double offsetX = -moduleSizeX / 2.0;
    double offsetY = -moduleSizeY / 2.0;

    // Coordinates along the X strips axis and the Y strips axis (in the frame of the plane)
    double posX = REST_nan;
    double posY = REST_nan;
    const int axisX = fReadoutParams.GetStripAxisX(), axisY = fReadoutParams.GetStripAxisY();

    bool isChannelX = (targetPhysicalID >= fReadoutParams.fNChannels);

    if (isChannelX) {
        int nCh = targetPhysicalID - fReadoutParams.fNChannels;
        double localY_fixed = nCh * fReadoutParams.fPitch - fReadoutParams.fPitch / 4.0;
        posX = localY_fixed + offsetX + fReadoutParams.fPositionRelative[axisX];
    } else {
        int nCh = targetPhysicalID;
        double localX_fixed = (1.0 + nCh) * fReadoutParams.fPitch;
        posY = localX_fixed + offsetY + fReadoutParams.fPositionRelative[axisY];
    }

    // Global axes: the strip coordinate on its axis, the others NaN
    double position[3] = {REST_nan, REST_nan, REST_nan};
    position[axisX] = posX;
    position[axisY] = posY;
    return ROOT::Math::XYZVector(position[0], position[1], position[2]);
}

std::pair<ROOT::Math::XYZVector, ROOT::Math::XYZVector> TRestMicromegasReadout::GetChannelsBoundingBox()
    const {
    if (fReadoutParams.fGlobalRotation == 0.0) return TRestDetectorReadout::GetChannelsBoundingBox();

    const double inf = std::numeric_limits<double>::infinity();
    const int axisX = fReadoutParams.GetStripAxisX(), axisY = fReadoutParams.GetStripAxisY();
    const int axisNormal = fReadoutParams.GetNormalAxis();
    double minimum[2] = {inf, inf}, maximum[2] = {-inf, -inf};
    for (const auto& [physicalID, daqID] : fPhysicalToDAQMap) {
        const auto position = GetPositionFromChannel(daqID);
        const double global[3] = {position.X(), position.Y(), position.Z()};
        const double coordinates[2] = {global[axisX], global[axisY]};
        for (int a = 0; a < 2; a++) {
            if (!std::isfinite(coordinates[a])) continue;
            minimum[a] = std::min(minimum[a], coordinates[a]);
            maximum[a] = std::max(maximum[a], coordinates[a]);
        }
    }
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (minimum[0] > maximum[0] || minimum[1] > maximum[1]) {
        return {ROOT::Math::XYZVector(nan, nan, nan), ROOT::Math::XYZVector(nan, nan, nan)};
    }
    const double halfPitch = fReadoutParams.fPitch / 2.0;
    double low[3], high[3];
    low[axisX] = minimum[0] - halfPitch;
    low[axisY] = minimum[1] - halfPitch;
    high[axisX] = maximum[0] + halfPitch;
    high[axisY] = maximum[1] + halfPitch;
    low[axisNormal] = high[axisNormal] = fReadoutParams.fPositionRelative[axisNormal];
    return {ROOT::Math::XYZVector(low[0], low[1], low[2]), ROOT::Math::XYZVector(high[0], high[1], high[2])};
}

double TRestMicromegasReadout::GetDistanceToReadout(const ROOT::Math::XYZVector& position) const {
    // Single readout plane at positionRelative along the normal axis; the global rotation is around it
    const int axis = fReadoutParams.GetNormalAxis();
    const double coordinate = axis == 0 ? position.X() : axis == 1 ? position.Y() : position.Z();
    return fReadoutParams.GetNormalSign() * (coordinate - fReadoutParams.fPositionRelative[axis]);
}

double TRestMicromegasReadout::GetMaxDistanceToReadout(const ROOT::Math::XYZVector& position) const {
    (void)position;
    if (fReadoutParams.fGasThickness <= 0) return std::numeric_limits<double>::quiet_NaN();
    // Same extent as the GAS volume built in BuildGeometry
    return fReadoutParams.fThickness / 2.0 + fReadoutParams.fGasThickness;
}
