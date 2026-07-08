#include "TRestGDMLReadout.h"
#include "TRestTools.h"
#include "TGeoNode.h"
#include "TGeoVolume.h"
#include <iostream>
#include <cctype>

static const bool TRestGDMLReadout_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();

     reg.RegisterField<TRestGDMLReadout>("gdmlFileName", &TRestGDMLReadout::fDecodingFile);
     reg.RegisterField<TRestGDMLReadout>("channelPrefix", &TRestGDMLReadout::fChannelPrefix);

    return true;
}();

namespace {
/// \brief Automatic self-registration into the restv2 metadata factory.
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestGDMLReadout",
        [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestGDMLReadout>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestGDMLReadout::TRestGDMLReadout() : TRestDetectorReadout() {
    fName = "TRestGDMLReadout";
}

TRestGDMLReadout::TRestGDMLReadout(const std::string& instanceName, const YAML::Node& node)
    : TRestDetectorReadout(instanceName, node) {
    LoadConfig();
}

TRestGDMLReadout::TRestGDMLReadout(const std::string& fileName, const std::string& sectionName)
    : TRestDetectorReadout(fileName, sectionName) {
    LoadConfig();
}

void TRestGDMLReadout::LoadConfig() {
    TRestDetectorReadout::LoadConfig();

    if (!fNode || !fNode["readoutParameters"]) {
        RESTError << "TRestGDMLReadout::LoadConfig - 'gdml_parameters' section is missing" << RESTendl;
        return;
    }

    fReadoutNode = fNode["readoutParameters"];

    UpdateParamsFromYAML<TRestGDMLReadout>(fReadoutNode);
    UpdateYAMLFromParams<TRestGDMLReadout>(fReadoutNode);
}

void TRestGDMLReadout::BuildGeometry( ) {

    if (!fNode || !fReadoutNode) {
        throw std::runtime_error("TRestGDLMReadout: 'readoutParameters' section is missing or fNode is not initialized!");
    }

    if (fGeoManager) {
        delete fGeoManager;
        fGeoManager = nullptr;
    }

    // 2. Execute native ROOT high-speed binary GDML streaming
    RESTInfo << "Importing external GDML layout file: " << fGDMLFileName << RESTendl;
    fGeoManager = TGeoManager::Import(fGDMLFileName.c_str());
    
    if (!fGeoManager) {
        throw std::runtime_error("TRestGDMLReadout::BuildGeometry - ROOT failed to parse GDML input: " + fGDMLFileName);
    }

    // Restore silence and grab the top assembly reference from the imported GDML world
    TGeoManager::SetVerboseLevel(0);
    fTopAssembly = dynamic_cast<TGeoVolumeAssembly*>(fGeoManager->GetTopVolume());
    if (!fTopAssembly) {
        // If the top volume is a primitive solid instead of an assembly, fetch it as a standard volume
        fTopAssembly = (TGeoVolumeAssembly*)fGeoManager->GetTopVolume();
    }

    // 3. LOGICAL HARDWARE MATCHING PIPELINE
    // Scan every geometry element within the GDML hierarchy to automatically extract 
    // the fixed physicalID out of their textual string names.
    TGeoIterator nextNode(fGeoManager->GetTopVolume());
    TGeoNode* node = nullptr;
    int parsedChannelsCount = 0;

    while ((node = nextNode())) {
        std::string nodeName = node->GetName(); // E.g., "strip_4323_1", "veto_39"

        // Find the user-defined prefix position (e.g., "veto_")
        size_t prefixPos = nodeName.find(fChannelPrefix);
        if (prefixPos != std::string::npos) {
            std::string idBuffer = "";
            // Extract trailing numerical digits following the prefix token
            for (size_t i = prefixPos + fChannelPrefix.length(); i < nodeName.length(); ++i) {
                if (std::isdigit(nodeName[i])) {
                    idBuffer += nodeName[i];
                } else {
                    break; // Stop parsing when reaching trailing suffix additions (like _pv or _1)
                }
            }

            if (!idBuffer.empty()) {
                int physicalID = std::stoi(idBuffer);
                
                // Embed the invariant physical structural layout index straight into the ROOT Node
                node->SetUniqueID(physicalID);
                parsedChannelsCount++;
            }
        }
    }

    fGeoManager->CloseGeometry();

    RESTInfo << "Successfully indexed " << parsedChannelsCount 
             << " structural channels out of GDML volume text labels." << RESTendl;
}

