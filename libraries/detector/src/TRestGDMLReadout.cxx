#include "TRestGDMLReadout.h"

#include <cctype>
#include <iostream>
#include <regex> 

#include "TGeoNode.h"
#include "TGeoVolume.h"
#include "TRestTools.h"

static const bool TRestGDMLReadout_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterNestedField<TRestGDMLReadout>("readoutParameters", &TRestGDMLReadout::fReadoutParams);
    return true;
}();

static const bool TRestGDMLReadoutInfo_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestGDMLReadoutInfo>("channelPrefix", &TRestGDMLReadoutInfo::fChannelPrefix);
    reg.RegisterField<TRestGDMLReadoutInfo>("offset", &TRestGDMLReadoutInfo::fOffset);
    reg.RegisterField<TRestGDMLReadoutInfo>("multiplier", &TRestGDMLReadoutInfo::fMultiplier);
    return true;
}();

TRestGDMLReadoutInfo::TRestGDMLReadoutInfo() : TRestMetadata() {
    fName = "TRestGDMLReadoutInfo";
}

TRestGDMLReadoutInfo::TRestGDMLReadoutInfo(
    const std::string& name, const YAML::Node& node) : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestGDMLReadoutInfo::LoadConfig() {
    UpdateParamsFromYAML<TRestGDMLReadoutInfo>(fNode);
    UpdateYAMLFromParams<TRestGDMLReadoutInfo>(fNode);
}

static const bool TRestGDMLReadoutParameters_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestGDMLReadoutParameters>("gdmlFileName", &TRestGDMLReadoutParameters::fGDMLFileName);
    reg.RegisterField<TRestGDMLReadoutParameters>("readoutInfo", &TRestGDMLReadoutParameters::fReadoutInfo);
    return true;
}();

TRestGDMLReadoutParameters::TRestGDMLReadoutParameters() : TRestMetadata() {
    fName = "TRestGDMLReadoutParameters";
}

TRestGDMLReadoutParameters::TRestGDMLReadoutParameters(
    const std::string& name, const YAML::Node& node) : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestGDMLReadoutParameters::LoadConfig() {
    UpdateParamsFromYAML<TRestGDMLReadoutParameters>(fNode);
    UpdateYAMLFromParams<TRestGDMLReadoutParameters>(fNode);
}

namespace {
/// \brief Automatic self-registration into the restv2 metadata factory.
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestGDMLReadout", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestGDMLReadout>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestGDMLReadout::TRestGDMLReadout() : TRestDetectorReadout() { fName = "TRestGDMLReadout"; }

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

    UpdateParamsFromYAML<TRestGDMLReadout>(fNode);
    UpdateYAMLFromParams<TRestGDMLReadout>(fNode);
}

void TRestGDMLReadout::BuildGeometry() {
    if (!fNode) {
        throw std::runtime_error("TRestGDMLReadout: fNode is not initialized!");
    }

    if (fGeoManager) {
        delete fGeoManager;
        fGeoManager = nullptr;
    }

    fPathToPhysicalIDMap.clear();

    RESTInfo << "Importing external GDML layout file: " << fReadoutParams.fGDMLFileName << RESTendl;

    gGeoManager = nullptr;
    fGeoManager = TGeoManager::Import(fReadoutParams.fGDMLFileName.c_str());

    if (!fGeoManager) {
        throw std::runtime_error("TRestGDMLReadout::BuildGeometry - ROOT failed to parse GDML input: " + fReadoutParams.fGDMLFileName);
    }
    TGeoManager::SetVerboseLevel(0);

    int parsedChannelsCount = 0;
    std::smatch match;

    std::vector<int> currentOffsets;
    for (const auto& info : fReadoutParams.fReadoutInfo) {
        currentOffsets.push_back(info.fOffset);
    }

    TGeoIterator it(fGeoManager->GetTopVolume());
    TGeoNode* node = nullptr;

    while ((node = it())) {
        if (!node->GetVolume() || node->GetVolume()->IsAssembly()) continue;

        TString tPath;
        it.GetPath(tPath);
        std::string nodePath = tPath.Data();

        for (size_t i = 0; i < fReadoutParams.fReadoutInfo.size(); ++i) {
            const auto& info = fReadoutParams.fReadoutInfo[i];
            try {
                std::regex channelRegex(info.fChannelPrefix);

                if (std::regex_search(nodePath, match, channelRegex)) {
                    int physicalID = 0;

                    if (match.size() > 2) {
                        try {
                            int firstCapturedNum  = std::stoi(match.str(1));
                            int secondCapturedNum = std::stoi(match.str(2));
                            physicalID = currentOffsets[i] + (firstCapturedNum * info.fMultiplier) + secondCapturedNum;
                        } catch (const std::exception& e) {
                            continue;
                        }
                    }
                    else if (match.size() > 1) {
                        try {
                            physicalID = currentOffsets[i] + std::stoi(match.str(1));
                        } catch (const std::exception& e) {
                            continue;
                        }
                    }
                    else {
                        physicalID = currentOffsets[i];
                    }

                    if (!fGeoManager->cd(nodePath.c_str())) continue;
                    std::string canonicalPath = fGeoManager->GetPath();

                    if (fPathToPhysicalIDMap.count(canonicalPath)) break;

                    fPathToPhysicalIDMap[canonicalPath] = physicalID;
                    parsedChannelsCount++;

                    if (match.size() <= 1) currentOffsets[i]++;
                    break;
                }
            } catch (const std::regex_error& e) {
                RESTError << "TRestGDMLReadout: Invalid regex format in prefix: " << e.what() << RESTendl;
            }
        }
    }

    TGeoIterator maskIt(fGeoManager->GetTopVolume());
    while ((node = maskIt())) {
        TString tPath;
        maskIt.GetPath(tPath);
        std::string nodePath = tPath.Data();
        TGeoVolume* vol = node->GetVolume();
        if (!vol) continue;

        bool isReadoutNode = false;
        for (const auto& [canonicalPath, id] : fPathToPhysicalIDMap) {
            if (canonicalPath.find(nodePath) != std::string::npos ||
                nodePath.find(canonicalPath) != std::string::npos) {
                isReadoutNode = true;
                break;
            }
        }

        if (!isReadoutNode) {
            if (!vol->IsAssembly() && vol->GetNdaughters() == 0) {
                vol->SetVisibility(kFALSE);
                node->SetVisibility(kFALSE);
            } else {
                vol->SetVisibility(kFALSE);
                node->SetVisibility(kTRUE);
            }
        } else {
            vol->SetVisibility(kTRUE);
            node->SetVisibility(kTRUE);
            vol->SetLineColor(kGray);
        }
    }

    fGeoManager->CloseGeometry();
    UpdateYAMLFromParams<TRestGDMLReadout>(fNode);

    RESTInfo << "Successfully indexed " << parsedChannelsCount << " channels conserving native geometry structures." << RESTendl;
}
