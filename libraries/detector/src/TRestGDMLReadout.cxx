#include "TRestGDMLReadout.h"

#include <cmath>
#include <limits>
#include <regex>

#include "TGeoManager.h"
#include "TGeoNode.h"
#include "TGeoVolume.h"
#include "TRestGDMLParser.h"
#include "TRestTools.h"

static const bool TRestGDMLReadout_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterNestedField<TRestGDMLReadout>("readoutParameters", &TRestGDMLReadout::fReadoutParams);
    reg.RegisterField<TRestGDMLReadout>("physicalIDToReadoutPath",
                                        &TRestGDMLReadout::fPhysicalIDToReadoutPath);
    return true;
}();

static const bool TRestGDMLReadoutInfo_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestGDMLReadoutInfo>("volume", &TRestGDMLReadoutInfo::fVolume);
    reg.RegisterField<TRestGDMLReadoutInfo>("readoutVolume", &TRestGDMLReadoutInfo::fReadoutVolume);
    reg.RegisterField<TRestGDMLReadoutInfo>("offset", &TRestGDMLReadoutInfo::fOffset);
    reg.RegisterField<TRestGDMLReadoutInfo>("multiplier", &TRestGDMLReadoutInfo::fMultiplier);
    return true;
}();

TRestGDMLReadoutInfo::TRestGDMLReadoutInfo() : TRestMetadata() { fName = "TRestGDMLReadoutInfo"; }

TRestGDMLReadoutInfo::TRestGDMLReadoutInfo(const std::string& name, const YAML::Node& node)
    : TRestMetadata(name, node) {
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

TRestGDMLReadoutParameters::TRestGDMLReadoutParameters(const std::string& name, const YAML::Node& node)
    : TRestMetadata(name, node) {
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

    ReleaseGeometry();

    fPathToPhysicalIDMap.clear();
    fPhysicalIDToReadoutPath.clear();
    fReadoutPositions.clear();
    if (!LoadDecoding(fDecodingFile)) {
        throw std::runtime_error("TRestGDMLReadout::BuildGeometry - cannot load the decoding file '" +
                                 fDecodingFile + "' (not found or without channels)");
    }

    RESTInfo << "Importing external GDML layout file: " << fReadoutParams.fGDMLFileName << RESTendl;

    gGeoManager = nullptr;
    // Entities and parameters of the configuration resolved, as restG4 reads it
    fGeoManager = TRestGDMLParser::Import(fReadoutParams.fGDMLFileName);

    if (!fGeoManager) {
        throw std::runtime_error("TRestGDMLReadout::BuildGeometry - ROOT failed to parse GDML input: " +
                                 fReadoutParams.fGDMLFileName);
    }
    TGeoManager::SetVerboseLevel(0);

    int parsedChannelsCount = 0;
    std::smatch match;
    // Channels of each readoutInfo entry (path, physical ID), to find their readout volumes
    std::vector<std::vector<std::pair<std::string, int>>> channelsOfInfo(fReadoutParams.fReadoutInfo.size());

    std::vector<int> currentOffsets;
    // Compiled once (not for each node); an invalid expression is an error of the configuration
    std::vector<std::regex> channelRegexes;
    for (const auto& info : fReadoutParams.fReadoutInfo) {
        currentOffsets.push_back(info.fOffset);
        try {
            channelRegexes.emplace_back(info.fVolume);
        } catch (const std::regex_error& e) {
            throw std::runtime_error("TRestGDMLReadout: invalid regular expression volume '" + info.fVolume +
                                     "': " + e.what());
        }
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
            if (std::regex_search(nodePath, match, channelRegexes[i])) {
                int physicalID = 0;

                if (match.size() > 2) {
                    try {
                        int firstCapturedNum = std::stoi(match.str(1));
                        int secondCapturedNum = std::stoi(match.str(2));
                        physicalID =
                            currentOffsets[i] + (firstCapturedNum * info.fMultiplier) + secondCapturedNum;
                    } catch (const std::exception& e) {
                        continue;
                    }
                } else if (match.size() > 1) {
                    try {
                        physicalID = currentOffsets[i] + std::stoi(match.str(1));
                    } catch (const std::exception& e) {
                        continue;
                    }
                } else {
                    physicalID = currentOffsets[i];
                }

                if (!fGeoManager->cd(nodePath.c_str())) continue;
                std::string canonicalPath = fGeoManager->GetPath();

                if (fPathToPhysicalIDMap.count(canonicalPath)) break;

                fPathToPhysicalIDMap[canonicalPath] = physicalID;
                channelsOfInfo[i].emplace_back(canonicalPath, physicalID);
                parsedChannelsCount++;

                if (match.size() <= 1) currentOffsets[i]++;
                break;
            }
        }
    }

    // Readout volume of each channel: the one matching readoutVolume in the same parent volume (the copy
    // of the assembly holding the sensitive volume), so its position follows the rotations of each copy
    auto parentOf = [](const std::string& path) { return path.substr(0, path.rfind('/')); };
    for (size_t i = 0; i < fReadoutParams.fReadoutInfo.size(); ++i) {
        const auto& info = fReadoutParams.fReadoutInfo[i];
        if (info.fReadoutVolume.empty()) continue;
        std::regex readoutRegex;
        try {
            readoutRegex = std::regex(info.fReadoutVolume);
        } catch (const std::regex_error& e) {
            throw std::runtime_error("TRestGDMLReadout: invalid regular expression readoutVolume '" +
                                     info.fReadoutVolume + "': " + e.what());
        }
        std::map<std::string, std::vector<std::string>> readoutsOfParent;
        TGeoIterator readoutIt(fGeoManager->GetTopVolume());
        while ((node = readoutIt())) {
            if (!node->GetVolume() || node->GetVolume()->IsAssembly()) continue;
            TString tPath;
            readoutIt.GetPath(tPath);
            const std::string nodePath = tPath.Data();
            if (!std::regex_search(nodePath, readoutRegex) || !fGeoManager->cd(nodePath.c_str())) continue;
            const std::string canonicalPath = fGeoManager->GetPath();
            readoutsOfParent[parentOf(canonicalPath)].push_back(canonicalPath);
        }
        for (const auto& [path, physicalID] : channelsOfInfo[i]) {
            const auto it = readoutsOfParent.find(parentOf(path));
            const size_t found = it == readoutsOfParent.end() ? 0 : it->second.size();
            if (found != 1) {
                throw std::runtime_error("TRestGDMLReadout: the channel " + path + " has " +
                                         std::to_string(found) + " volumes matching readoutVolume '" +
                                         info.fReadoutVolume + "' in its parent volume (exactly one needed)");
            }
            fPhysicalIDToReadoutPath[physicalID] = it->second.front();
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
        // The readout volumes (e.g. light guides) are shown too, in another colour
        bool isSignalReadout = false;
        for (const auto& [id, readoutPath] : fPhysicalIDToReadoutPath) {
            if (readoutPath.find(nodePath) != std::string::npos ||
                nodePath.find(readoutPath) != std::string::npos) {
                isReadoutNode = true;
                isSignalReadout = nodePath.find(readoutPath) != std::string::npos;
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
            vol->SetLineColor(isSignalReadout ? kAzure + 1 : kGray);
        }
    }

    fGeoManager->CloseGeometry();
    UpdateYAMLFromParams<TRestGDMLReadout>(fNode);

    RESTInfo << "Successfully indexed " << parsedChannelsCount << " channels ("
             << fPhysicalIDToReadoutPath.size()
             << " with readout volume) conserving native geometry structures." << RESTendl;
}

ROOT::Math::XYZVector TRestGDMLReadout::GetReadoutPosition(int daqID) const {
    if (fReadoutPositions.empty() && fGeoManager) {
        // Origin of each readout volume in the global frame
        for (const auto& [physicalID, path] : fPhysicalIDToReadoutPath) {
            if (!fGeoManager->cd(path.c_str())) continue;
            const TGeoMatrix* matrix = fGeoManager->GetCurrentMatrix();
            if (!matrix) continue;
            const double local[3] = {0, 0, 0};
            double global[3] = {0, 0, 0};
            matrix->LocalToMaster(local, global);
            fReadoutPositions[physicalID] = {global[0], global[1], global[2]};
        }
    }
    const auto it = fReadoutPositions.find(GetPhysicalID(daqID));
    if (it != fReadoutPositions.end()) return it->second;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    return {nan, nan, nan};
}

double TRestGDMLReadout::GetDistanceToReadout(int daqID, const ROOT::Math::XYZVector& position) const {
    const auto readoutPosition = GetReadoutPosition(daqID);
    if (std::isnan(readoutPosition.X())) return std::numeric_limits<double>::quiet_NaN();
    return (position - readoutPosition).R();
}
