#include "TRestDetectorReadoutManager.h"
#include "TRestTools.h"

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

static const bool TRestDetectorReadoutManager_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestDetectorReadoutManager>("readouts", &TRestDetectorReadoutManager::fReadoutInfo);
    return true;
}();

static const bool TRestReadoutInfo_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestReadoutInfo>("inputFileName", &TRestReadoutInfo::fInputFileName);
    reg.RegisterField<TRestReadoutInfo>("decodingName", &TRestReadoutInfo::fDecodingName);
    reg.RegisterField<TRestReadoutInfo>("instanceName", &TRestReadoutInfo::fInstanceName);
    return true;
}();

TRestReadoutInfo::TRestReadoutInfo() : TRestMetadata() {
    fName = "TRestReadoutInfo";
}

TRestReadoutInfo::TRestReadoutInfo(
    const std::string& name, const YAML::Node& node) : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestReadoutInfo::LoadConfig() {
    UpdateParamsFromYAML<TRestReadoutInfo>(fNode);
    UpdateYAMLFromParams<TRestReadoutInfo>(fNode);
}

/// \brief Constructs a generic readout metadata object with default name.
TRestDetectorReadoutManager::TRestDetectorReadoutManager() : TRestMetadata() { fName = "TRestDetectorReadoutManager"; }

TRestDetectorReadoutManager::TRestDetectorReadoutManager(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestDetectorReadoutManager::TRestDetectorReadoutManager(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

TRestDetectorReadoutManager::~TRestDetectorReadoutManager() {
    for (auto& [name, ptr] : fReadoutMap) {
        delete ptr;
    }
    fReadoutMap.clear();
}

// =========================================================================
/// \brief Phase 1: Load configuration (pure YAML parameter parsing).
// =========================================================================
void TRestDetectorReadoutManager::LoadConfig() {
    UpdateParamsFromYAML<TRestDetectorReadoutManager>(fNode);
    LoadReadout();
    UpdateYAMLFromParams<TRestDetectorReadoutManager>(fNode);
}

// =========================================================================
/// \brief Phase 2: Initialize (automatic selective import execution).
// =========================================================================
void TRestDetectorReadoutManager::LoadReadout() {

    for (auto& [name, ptr] : fReadoutMap) {
        delete ptr;
    }

    fReadoutMap.clear();

    for (const auto& readoutInfo : fReadoutInfo) {
        if (!TRestTools::isRootFile(readoutInfo.fInputFileName)) {
            RESTError << "Input file " << readoutInfo.fInputFileName << " not found, readout will not be loaded" << RESTendl;
            continue;
        }

        std::unique_ptr<TFile> fIn(TFile::Open(readoutInfo.fInputFileName.c_str(), "READ"));
        if (!fIn || fIn->IsZombie()) {
            RESTError << "Could not open ROOT file: " << readoutInfo.fInputFileName << RESTendl;
            continue;
        }

        auto yamlConfig = TRestMetadata::ReadMetadata(fIn.get(), readoutInfo.fInstanceName);
        if (!yamlConfig) {
            RESTError << "Metadata instance " << readoutInfo.fInstanceName << " not found in file " << readoutInfo.fInputFileName << RESTendl;
            continue;
        }

        std::string className = yamlConfig["class"].as<std::string>();
        std::unique_ptr<TRestMetadata> metadata = MetadataClassRegistry::Instance().Create(className, readoutInfo.fInstanceName, yamlConfig);

        if (!metadata) {
            RESTError << className << " not found in MetadataRegistry." << RESTendl;
            continue;
        }

        std::unique_ptr<TRestDetectorReadout> readout(dynamic_cast<TRestDetectorReadout*>(metadata.get()));
        if (!readout) {
            RESTError << "Cannot cast metadata to TRestDetectorReadout." << RESTendl;
            continue;
        }
        
        metadata.release(); 

        readout->Import(fIn.get(), readoutInfo.fInstanceName, readoutInfo.fDecodingName);
        
        fReadoutMap[readoutInfo.fInstanceName] = readout.release();
        RESTInfo << "Successfully imported readout instance '" << readoutInfo.fInstanceName 
                 << "' with decoding '" << readoutInfo.fDecodingName << "'" << RESTendl;
                 
        fReadoutMap[readoutInfo.fInstanceName]->PrintMetadata();
    }
}


void TRestDetectorReadoutManager::ListReadouts(){

  for (auto& [name, ptr] : fReadoutMap) {
        std::cout<<name<<" "<<ptr->GetClassName()<<std::endl;
    }

}

TRestDetectorReadout* TRestDetectorReadoutManager::GetReadout(const std::string& name) const {
    auto it = fReadoutMap.find(name);
    return (it != fReadoutMap.end()) ? it->second : nullptr;
}

void TRestDetectorReadoutManager::ViewReadoutGeometry(const std::vector<std::string>& names,
                                                      const std::string& option) const {
    ViewImpl({}, names, option);
}

void TRestDetectorReadoutManager::ViewActiveEvent(const std::vector<int>& activeChannels,
                                                  const std::vector<std::string>& names) const {
    ViewImpl(activeChannels, names, "ogl");
}

void TRestDetectorReadoutManager::ViewImpl(const std::vector<int>& activeChannels,
                                           const std::vector<std::string>& names,
                                           const std::string& option) const {
    std::vector<TRestDetectorReadout::ViewItem> items;
    for (const auto& [name, readout] : fReadoutMap) {
        if (!names.empty() && std::find(names.begin(), names.end(), name) == names.end()) continue;
        readout->GetViewItems(items, activeChannels);
    }
    TRestDetectorReadout::DrawViewItems(items, fViewGeo, option);
}
