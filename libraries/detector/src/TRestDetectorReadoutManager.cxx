#include "TRestDetectorReadoutManager.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "TFile.h"
#include "TRestDetectorReadout.h"
#include "TRestRun.h"
#include "TRestTools.h"

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
    reg.RegisterField<TRestDetectorReadoutManager>("readout", &TRestDetectorReadoutManager::fDataReadoutName);
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

TRestReadoutInfo::TRestReadoutInfo() : TRestMetadata() { fName = "TRestReadoutInfo"; }

TRestReadoutInfo::TRestReadoutInfo(const std::string& name, const YAML::Node& node)
    : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestReadoutInfo::LoadConfig() {
    UpdateParamsFromYAML<TRestReadoutInfo>(fNode);
    UpdateYAMLFromParams<TRestReadoutInfo>(fNode);
}

/// \brief Constructs a generic readout metadata object with default name.
TRestDetectorReadoutManager::TRestDetectorReadoutManager() : TRestMetadata() {
    fName = "TRestDetectorReadoutManager";
}

TRestDetectorReadoutManager::TRestDetectorReadoutManager(const std::string& instanceName,
                                                         const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestDetectorReadoutManager::TRestDetectorReadoutManager(const std::string& fileName,
                                                         const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

TRestDetectorReadoutManager::~TRestDetectorReadoutManager() {
    // A view drawn by the manager uses the shapes and media of the geometries of its readouts and may still
    // be displayed: the readouts (and their geometries) are then left to ROOT, not deleted
    if (!fViewGeo) {
        for (auto& [name, ptr] : fReadoutMap) {
            delete ptr;
        }
    }
    fReadoutMap.clear();
}

// =========================================================================
/// \brief Phase 1: Load configuration (pure YAML parameter parsing).
// =========================================================================
void TRestDetectorReadoutManager::LoadConfig() {
    UpdateParamsFromYAML<TRestDetectorReadoutManager>(fNode);
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
        // kSelfFile: the readout is stored in the file the manager is read from, or the input file of the run
        std::unique_ptr<TFile> ownedFile;
        TFile* fIn = nullptr;
        if (readoutInfo.fInputFileName == kSelfFile) {
            fIn = fSelfFile ? fSelfFile : fRunInfo ? fRunInfo->GetInputFile() : nullptr;
            if (!fIn) {
                RESTError << "Readout '" << readoutInfo.fInstanceName << "' has inputFileName '" << kSelfFile
                          << "' but the manager is not read from a file nor has a run with an input file"
                          << RESTendl;
                continue;
            }
        } else {
            // A URL (e.g. a versioned readout of rest-common) is downloaded and cached
            const std::string inputFile = TRestTools::GetLocalFile(readoutInfo.fInputFileName);
            if (!TRestTools::isRootFile(inputFile)) {
                RESTError << "Input file " << readoutInfo.fInputFileName
                          << " not found, readout will not be loaded" << RESTendl;
                continue;
            }

            ownedFile.reset(TFile::Open(inputFile.c_str(), "READ"));
            if (!ownedFile || ownedFile->IsZombie()) {
                RESTError << "Could not open ROOT file: " << readoutInfo.fInputFileName << RESTendl;
                continue;
            }
            fIn = ownedFile.get();
        }

        auto yamlConfig = TRestMetadata::ReadMetadata(fIn, readoutInfo.fInstanceName);
        if (!yamlConfig) {
            RESTError << "Metadata instance " << readoutInfo.fInstanceName << " not found in file "
                      << readoutInfo.fInputFileName << RESTendl;
            continue;
        }

        std::string className = yamlConfig["class"].as<std::string>();
        // Written by REST (Export): not checked
        std::unique_ptr<TRestMetadata> metadata =
            MetadataClassRegistry::Instance().Create(className, readoutInfo.fInstanceName, yamlConfig, false);

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

        // The decoding name is the one given to Export when the readout file was written
        const std::string& decodingName = readoutInfo.fDecodingName;
        if (!readout->Import(fIn, readoutInfo.fInstanceName, decodingName)) {
            // Not kept: the readout would silently use the decoding file read from disk at construction
            std::string available;
            for (const auto& name : TRestDetectorReadout::GetDecodingNames(fIn, readoutInfo.fInstanceName)) {
                available += " '" + name + "'";
            }
            RESTError << "Decoding '" << decodingName << "' of readout '" << readoutInfo.fInstanceName
                      << "' (or its geometry) not found in " << readoutInfo.fInputFileName
                      << ". Available:" << available << RESTendl;
            continue;
        }

        fReadoutMap[readoutInfo.fInstanceName] = readout.release();
        RESTInfo << "Successfully imported readout instance '" << readoutInfo.fInstanceName
                 << "' with decoding '" << decodingName << "'" << RESTendl;

        fReadoutMap[readoutInfo.fInstanceName]->PrintMetadata();
    }
}

void TRestDetectorReadoutManager::SetRunInfo(TRestRun* run) {
    TRestMetadata::SetRunInfo(run);
    LoadReadout();
}

void TRestDetectorReadoutManager::LoadFromFile(TFile* file) {
    if (!fReadoutMap.empty()) return;
    fSelfFile = file;
    LoadReadout();
    fSelfFile = nullptr;
}

void TRestDetectorReadoutManager::WriteMetadata(TFile* file) {
    UpdateYAMLFromParams<TRestDetectorReadoutManager>(fNode);
    YAML::Node node = YAML::Clone(fNode);

    // With a data readout (`readout`), only that one is stored: the data of the file are those of a single
    // readout, the others were loaded for this processing (e.g. to view the whole detector)
    YAML::Node readoutsNode = node["readouts"];
    YAML::Node stored(YAML::NodeType::Sequence);
    for (size_t i = 0; i < fReadoutInfo.size() && i < readoutsNode.size(); ++i) {
        const auto& readoutInfo = fReadoutInfo[i];
        if (!fDataReadoutName.empty() && readoutInfo.fInstanceName != fDataReadoutName) continue;
        stored.push_back(readoutsNode[i]);
        auto* readout = GetReadout(readoutInfo.fInstanceName);
        // Readouts not imported keep pointing to their original file
        if (!readout) continue;

        if (!readout->Export(file, readoutInfo.fInstanceName, readoutInfo.fDecodingName)) {
            RESTError << "Readout '" << readoutInfo.fInstanceName << "' could not be exported to "
                      << file->GetName() << RESTendl;
            continue;
        }
        stored[stored.size() - 1]["inputFileName"] = kSelfFile;
    }
    node["readouts"] = stored;

    TRestMetadata::WriteMetadata(file, fName, node);
}

void TRestDetectorReadoutManager::ListReadouts() {
    for (auto& [name, ptr] : fReadoutMap) {
        std::cout << name << " " << ptr->GetClassName() << std::endl;
    }
}

TRestDetectorReadout* TRestDetectorReadoutManager::GetReadout(const std::string& name) const {
    auto it = fReadoutMap.find(name);
    return (it != fReadoutMap.end()) ? it->second : nullptr;
}

std::string TRestDetectorReadoutManager::GetDataReadoutName() const {
    if (!fDataReadoutName.empty()) return fDataReadoutName;
    return fReadoutMap.size() == 1 ? fReadoutMap.begin()->first : "";
}

TRestDetectorReadout* TRestDetectorReadoutManager::SelectReadout(std::string& name,
                                                                 const std::string& user) const {
    const std::string data = GetDataReadoutName();
    std::string available;
    for (const auto& [readoutName, readout] : fReadoutMap) available += " '" + readoutName + "'";
    if (available.empty()) available = " none";
    if (name.empty()) {
        if (data.empty()) {
            throw std::runtime_error(
                user + ": several readouts loaded (" + available.substr(1) +
                ") and no data readout: set 'readout' of the TRestDetectorReadoutManager");
        }
        name = data;
    } else if (!data.empty() && name != data) {
        RESTWarning << user << ": readout '" << name << "' instead of the data readout '" << data
                    << "' of the TRestDetectorReadoutManager" << RESTendl;
    }
    TRestDetectorReadout* readout = GetReadout(name);
    if (!readout) {
        throw std::runtime_error(user + ": readout '" + name + "' not loaded (readouts:" + available + ")");
    }
    return readout;
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
