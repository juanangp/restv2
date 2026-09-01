#include "TRestDetectorReadoutManager.h"

#include <iostream>

#include "TDirectory.h"
#include "TFile.h"
#include "TObjString.h"

// Anonymous namespace to avoid global name pollution during compilation
namespace {
/// \brief Registers the dynamic readout manager type into the restv2 metadata registry.
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorReadoutManager", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorReadoutManager>(instanceName, params);
        });
    return true;
}();
}  // namespace

// Minimal proxy class to hold imported TGeo geometries agnostically
class TRestGenericProxyReadout : public TRestDetectorReadout {
   public:
    TRestGenericProxyReadout() : TRestDetectorReadout() {}
    virtual void BuildGeometry() override {}
};

TRestDetectorReadoutManager::~TRestDetectorReadoutManager() {
    for (auto& [name, readoutPtr] : fReadoutMap) {
        if (readoutPtr) delete readoutPtr;
    }
    fReadoutMap.clear();
}

void TRestDetectorReadoutManager::PrintMetadata() const {
    RESTInfo << "Detector readout manager (Automated Selective Loader): " << fName << RESTendl;
    RESTInfo << "Source ROOT Geometry File: " << fInputFileName << RESTendl;
    RESTInfo << "Readouts requested/loaded: " << fRequestedReadouts.size() << "/" << fReadoutMap.size()
             << RESTendl;
}

// =========================================================================
/// \brief Phase 1: Load configuration (pure YAML parameter parsing).
// =========================================================================
void TRestDetectorReadoutManager::LoadConfig() {
    if (!fNode || !fNode["rest"] || !fNode["rest"]["experiment"]) return;
    auto expNode = fNode["rest"]["experiment"];

    fName = TRestTools::ReadYAMLParam<std::string>(expNode["name"]);
    fRequestedReadouts.clear();

    // 1. Read the master storage file for readouts.
    if (expNode["file_name"]) {
        fInputFileName = TRestTools::ReadYAMLParam<std::string>(expNode["file_name"]);
    } else {
        RESTError << "TRestDetectorReadoutManager -> Missing 'file_name' parameter in YAML!" << RESTendl;
        return;
    }

    // 2. Read the detailed list of requested imports.
    if (expNode["readouts"]) {
        for (const auto& rNode : expNode["readouts"]) {
            if (rNode["name"] && rNode["geometry_name"] && rNode["decoding_name"]) {
                TRestReadoutRequest req;
                req.fInstanceName = TRestTools::ReadYAMLParam<std::string>(rNode["name"]);
                req.fGeometryName = TRestTools::ReadYAMLParam<std::string>(rNode["geometry_name"]);
                req.fDecodingName = TRestTools::ReadYAMLParam<std::string>(rNode["decoding_name"]);
                fRequestedReadouts.push_back(req);
            } else {
                RESTError << "TRestDetectorReadoutManager -> Incomplete readout node definition in YAML!"
                          << RESTendl;
            }
        }
    }
}

// =========================================================================
/// \brief Phase 2: Initialize (automatic selective import execution).
// =========================================================================
void TRestDetectorReadoutManager::Initialize() {
    if (fInputFileName.empty() || fRequestedReadouts.empty()) return;

    // Open the master file specified in the YAML.
    TFile* fIn = TFile::Open(fInputFileName.c_str(), "READ");
    if (!fIn || fIn->IsZombie()) {
        RESTError << "TRestDetectorReadoutManager::Initialize -> Cannot open file: " << fInputFileName
                  << RESTendl;
        if (fIn) delete fIn;
        return;
    }

    // Limpieza de seguridad de RAM anterior
    for (auto& [name, ptr] : fReadoutMap) delete ptr;
    fReadoutMap.clear();

    // Process requested readouts one by one.
    for (const auto& req : fRequestedReadouts) {
        // Instantiate the library generic proxy container.
        TRestGenericProxyReadout* activeReadout = new TRestGenericProxyReadout();
        activeReadout->SetName(req.fInstanceName);

        // The `Import` method retrieves the `TGeoManager` (req.fGeometryName)
        // and the `TObjString` table (req.fDecodingName) from file subdirectories.
        if (activeReadout->Import(fIn, req.fGeometryName, req.fDecodingName)) {
            fReadoutMap[req.fInstanceName] = activeReadout;
            RESTInfo << "Successfully imported readout instance '" << req.fInstanceName
                     << "' [Geo: " << req.fGeometryName << ", Dec: " << req.fDecodingName << "]" << RESTendl;
        } else {
            RESTError << "Failed to import requested readout layout: " << req.fInstanceName << RESTendl;
            delete activeReadout;
        }
    }

    fIn->Close();
    delete fIn;
}

TRestDetectorReadout* TRestDetectorReadoutManager::GetReadout(const std::string& name) const {
    auto it = fReadoutMap.find(name);
    return (it != fReadoutMap.end()) ? it->second : nullptr;
}
