#include "TRestGDMLReadout.h"
#include "TRestTools.h"
#include "TGeoNode.h"
#include "TGeoVolume.h"
#include <iostream>
#include <cctype>

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

void TRestGDMLReadout::BuildGeometry( ) {

    if (!fNode || !fNode["gdml_parameters"]) {
        throw std::runtime_error("TRestGDLMReadout: 'gdml_parameters' section is missing or fNode is not initialized!");
    }
    
    auto params = fNode["gdml_parameters"];

    if (!params["gdml_file"]) {
        throw std::runtime_error("TRestMicromegasReadout: Required parameter 'gdml_file' is missing in gdml_parameters!");
    }

    if (!params["channel_prefix"]) {
        throw std::runtime_error("TRestMicromegasReadout: Required parameter 'channel_prefix' is missing in gdml_parameters!");
    }

    // 3. Lectura segura utilizando las herramientas nativas de TRestTools
    fGDMLFilePath = TRestTools::ReadYAMLParam<std::string>(params["gdml_file"]);
    fChannelPrefix = TRestTools::ReadYAMLParam<std::string>(params["channel_prefix"]);

    // 1. Terminate the default TGeoManager instance created by the base class
    // because TGeoManager::Import will spawn its own global manager context.
    if (fGeoManager) {
        delete fGeoManager;
        fGeoManager = nullptr;
    }

    // 2. Execute native ROOT high-speed binary GDML streaming
    RESTInfo << "Importing external GDML layout file: " << fGDMLFilePath << RESTendl;
    fGeoManager = TGeoManager::Import(fGDMLFilePath.c_str());
    
    if (!fGeoManager) {
        throw std::runtime_error("TRestGDMLReadout::BuildGeometry - ROOT failed to parse GDML input: " + fGDMLFilePath);
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

