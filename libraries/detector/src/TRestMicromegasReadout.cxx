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
    MetadataRegistry::Instance().Register(
        "TRestMicromegasReadout",
        [](const std::string& instanceName, const std::string& sectionName, const YAML::Node& params) {
            auto readout = std::make_unique<TRestMicromegasReadout>();
            readout->SetName(instanceName);
            readout->SetSectionName(sectionName);
            readout->SetYAMLNode(params);
            readout->LoadConfig();
            return readout;
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
void TRestMicromegasReadout::BuildGeometry(const YAML::Node& readoutNode) {
    if (!readoutNode["geometry_parameters"]) return;
    auto params = readoutNode["geometry_parameters"];

    int rows = params["rows"].as<int>();
    int cols = params["cols"].as<int>();
    double pitchX = params["pitch_x"].as<double>();
    double pitchY = params["pitch_y"].as<double>();

    std::vector<double> positionRelative = {0.0, 0.0, 0.0};
    if (params["position_relative"]) {
        positionRelative = TRestTools::ReadYAMLParamVector<double>(params["position_relative"]);
        if (positionRelative.size() != 3) {
            throw std::runtime_error("TRestMicromegasReadout::BuildGeometry - position_relative must have 3 values");
        }
    }

    double positionRotation = 0.0;
    if (params["position_rotation"]) {
        positionRotation = TRestTools::ReadYAMLParam<double>(params["position_rotation"]);
    }

    // Instantiating a single box template primitive to preserve RAM
    TGeoBBox* pixelShape = new TGeoBBox("pixel_shape", pitchX / 2.0, pitchY / 2.0, 0.1);
    TGeoVolume* pixelVol = new TGeoVolume("PIXEL", pixelShape);

    TGeoRotation* globalRotation = new TGeoRotation();
    globalRotation->RotateZ(positionRotation);

    int physicalID = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            double posX = (c * pitchX) - (cols * pitchX / 2.0) + positionRelative[0];
            double posY = (r * pitchY) - (rows * pitchY / 2.0) + positionRelative[1];
            double posZ = positionRelative[2];

            // Populate coordinates on the inherited abstract fTopAssembly object
            TGeoCombiTrans* trans = new TGeoCombiTrans(posX, posY, posZ, globalRotation);
            TGeoNode* node = fTopAssembly->AddNode(pixelVol, physicalID, trans);

            // Tie the native fixed layout node mapping to its internal physical ID
            node->SetUniqueID(physicalID);
            physicalID++;
        }
    }
}
