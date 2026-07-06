#include "TRestDetectorReadoutManager.h"
#include "TDirectory.h"
#include "TFile.h"
#include "TObjString.h"
#include <iostream>

// Anonymous namespace to avoid global name pollution during compilation
namespace {
/// \brief Registers the dynamic readout manager type into the restv2 metadata registry.
const bool kRegistered = []() {
    MetadataRegistry::Instance().Register(
        "TRestDetectorReadoutManager",
        [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorReadoutManager>(instanceName, params);
        });
    return true;
}();
} // namespace

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

void TRestDetectorReadoutManager::PrintMetadata() {
    RESTInfo << "Detector readout manager (Automated Selective Loader): " << fName << RESTendl;
    RESTInfo << "Source ROOT Geometry File: " << fInputFileName << RESTendl;
    RESTInfo << "Readouts requested/loaded: " << fRequestedReadouts.size() << "/" << fReadoutMap.size() << RESTendl;
}

// =========================================================================
// FASE 1: LOAD CONFIG (Lectura pura de los parámetros del YAML)
// =========================================================================
void TRestDetectorReadoutManager::LoadConfig() {
    if (!fNode || !fNode["rest"] || !fNode["rest"]["experiment"]) return;
    auto expNode = fNode["rest"]["experiment"];

    fName = TRestTools::ReadYAMLParam<std::string>(expNode["name"]);
    fRequestedReadouts.clear();

    // 1. Extraer el archivo de almacenamiento maestro de los readouts
    if (expNode["file_name"]) {
        fInputFileName = TRestTools::ReadYAMLParam<std::string>(expNode["file_name"]);
    } else {
        RESTError << "TRestDetectorReadoutManager -> Missing 'file_name' parameter in YAML!" << RESTendl;
        return;
    }

    // 2. Extraer la colección detallada de importaciones solicitadas
    if (expNode["readouts"]) {
        for (const auto& rNode : expNode["readouts"]) {
            if (rNode["name"] && rNode["geometry_name"] && rNode["decoding_name"]) {
                TRestReadoutRequest req;
                req.fInstanceName = TRestTools::ReadYAMLParam<std::string>(rNode["name"]);
                req.fGeometryName = TRestTools::ReadYAMLParam<std::string>(rNode["geometry_name"]);
                req.fDecodingName = TRestTools::ReadYAMLParam<std::string>(rNode["decoding_name"]);
                fRequestedReadouts.push_back(req);
            } else {
                RESTError << "TRestDetectorReadoutManager -> Incomplete readout node definition in YAML!" << RESTendl;
            }
        }
    }
}

// =========================================================================
// FASE 2: INITIALIZE (Ejecución automática de la importación selectiva)
// =========================================================================
void TRestDetectorReadoutManager::Initialize() {
    if (fInputFileName.empty() || fRequestedReadouts.empty()) return;

    // Abrimos el archivo maestro especificado en el YAML
    TFile* fIn = TFile::Open(fInputFileName.c_str(), "READ");
    if (!fIn || fIn->IsZombie()) {
        RESTError << "TRestDetectorReadoutManager::Initialize -> Cannot open file: " << fInputFileName << RESTendl;
        if (fIn) delete fIn;
        return;
    }

    // Limpieza de seguridad de RAM anterior
    for (auto& [name, ptr] : fReadoutMap) delete ptr;
    fReadoutMap.clear();

    // Procesamos uno a uno los readouts solicitados de forma explícita
    for (const auto& req : fRequestedReadouts) {
        
        // Instanciamos el contenedor proxy genérico de la librería
        TRestGenericProxyReadout* activeReadout = new TRestGenericProxyReadout();
        activeReadout->SetName(req.fInstanceName);
        
        // El método Import se encarga de ir a buscar el TGeoManager (req.fGeometryName)
        // y la tabla TObjString (req.fDecodingName) dentro de los subdirectorios del archivo
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
