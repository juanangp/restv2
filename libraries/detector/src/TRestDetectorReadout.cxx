#include "TRestDetectorReadout.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm> 

#include "TDirectory.h"
#include "TFile.h"
#include <TGeoBBox.h>
#include <TGeoMatrix.h>
#include <TGeoVolume.h>
#include <TGeoNode.h>
#include <TGeoManager.h>
#include <TGeoMedium.h>
#include "TList.h"
#include "TObjString.h"
#include "TRestConstants.h"

using namespace TRestConstants;

namespace {
/// Layout in the ROOT file: Readouts/<geometry>/Geometry and Readouts/<geometry>/Decodings/<decoding>
const char* kReadoutsDir = "Readouts";
const char* kGeometryKey = "Geometry";
const char* kDecodingsDir = "Decodings";

/// \brief Returns parent/name, or nullptr if missing.
TDirectory* GetSubDirectory(TDirectory* parent, const char* name) {
    return parent ? parent->GetDirectory(name) : nullptr;
}

/// \brief Returns parent/name, creating it if missing.
TDirectory* GetOrMakeSubDirectory(TDirectory* parent, const char* name) {
    if (!parent) return nullptr;
    TDirectory* dir = parent->GetDirectory(name);
    return dir ? dir : parent->mkdir(name);
}
}  // namespace

static const bool TRestDetectorReadout_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestDetectorReadout>("decodingFile", &TRestDetectorReadout::fDecodingFile);
     reg.RegisterField<TRestDetectorReadout>("volumeToPhysicalIDMap", &TRestDetectorReadout::fPathToPhysicalIDMap);
    return true;
}();

/// \brief Constructs a generic readout metadata object with default name.
TRestDetectorReadout::TRestDetectorReadout() : TRestMetadata() { fName = "TRestDetectorReadout"; }

TRestDetectorReadout::TRestDetectorReadout(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestDetectorReadout::TRestDetectorReadout(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

/// \brief Destructor.
TRestDetectorReadout::~TRestDetectorReadout() {
    // TGeoManager is globally owned by ROOT (gGeoManager), it unregisters automatically.
}

/// \brief Reads the readout parameters from YAML. The decoding file is only read by BuildGeometry: a
/// readout imported from a ROOT file takes the decoding stored there.
void TRestDetectorReadout::LoadConfig() {
    UpdateParamsFromYAML<TRestDetectorReadout>(fNode);


    // Sync resolved parameters to the node
    UpdateYAMLFromParams<TRestDetectorReadout>(fNode);
}

void TRestDetectorReadout::InitializeReadout() {
    if (fGeoManager) { delete fGeoManager; fGeoManager = nullptr; }
    gGeoManager = nullptr;   // el constructor no debe borrar la geometría de otro readout
    fGeoManager = new TGeoManager(fName.c_str(), fName.c_str());
    TGeoManager::SetVerboseLevel(0);

    TGeoVolume* top = fGeoManager->MakeVolumeAssembly("READOUT_TOP");
    fGeoManager->SetTopVolume(top);
}

/// \brief Parses decoding text with `daqChannel physicalID` rows (legacy order).
/// \param text Multiline decoding text.
/// \param decodingMap Output map receiving parsed values.
/// \return `true` when at least one valid row is parsed.
static bool ParseDecodingString(const std::string& text, std::map<int, int>& decodingMap) {
    std::stringstream stream(text);
    std::string line;
    int linesParsed = 0;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        size_t firstChar = line.find_first_not_of("     ");
        if (firstChar == std::string::npos || line[firstChar] == '#') continue;

        // As legacy decoding files: first column DAQ channel, second readout (physical) channel. A
        // negative physical ID marks an unconnected DAQ channel and is skipped
        std::stringstream ss(line);
        int daqID = -1;
        int physicalID = -1;
        if (ss >> daqID >> physicalID) {
            if (physicalID >= 0) decodingMap[physicalID] = daqID;
            linesParsed++;
        }
    }

    return (linesParsed > 0);
}

void TRestDetectorReadout::ViewReadoutGeometry(const std::string& option) const {
    std::vector<ViewItem> items;
    GetViewItems(items);
    if (items.empty()) { RESTError << "Nothing to draw for readout " << GetName() << RESTendl; return; }
    DrawViewItems(items, fViewGeo, option);
}

void TRestDetectorReadout::ViewActiveEvent(const std::vector<int>& activeChannels) const {
    std::vector<ViewItem> items;
    GetViewItems(items, activeChannels);
    if (items.empty()) { RESTError << "Nothing to draw for readout " << GetName() << RESTendl; return; }
    DrawViewItems(items, fViewGeo);
}

void TRestDetectorReadout::GetViewItems(std::vector<ViewItem>& items,
                                        const std::vector<int>& activeChannels) const {
    if (!fGeoManager) return;

    const std::set<int> activeDAQ(activeChannels.begin(), activeChannels.end());
    std::set<int> activeIDs;
    for (const auto& [physID, daqID] : fPhysicalToDAQMap)
        if (activeDAQ.count(daqID)) activeIDs.insert(physID);

    CollectViewItems(activeIDs, items);  
}

void TRestDetectorReadout::CollectViewItems(const std::set<int>& activeIDs,
                                            std::vector<ViewItem>& items) const {
    const bool usePathNavigation = !fPathToPhysicalIDMap.empty();

    auto add = [&](TGeoNode* n, const TGeoMatrix* m, int physID) {
        if (!n || !m || !n->GetVolume() || !n->GetVolume()->GetShape()) return;
        if (n->GetVolume()->IsAssembly()) return;

        TGeoVolume* vol = n->GetVolume();
        const bool active = activeIDs.count(physID) > 0;

        ViewItem item;
        item.shape = vol->GetShape();
        item.medium = vol->GetMedium();
        item.matrix = std::make_unique<TGeoHMatrix>(*m);
        item.color = active ? static_cast<Color_t>(kRed) : vol->GetLineColor();
        item.transparency = active ? 0 : vol->GetTransparency();
        item.name = n->GetName();
        items.push_back(std::move(item));
    };

    if (usePathNavigation) {
        for (const auto& [path, physID] : fPathToPhysicalIDMap) {
            if (!fGeoManager->cd(path.c_str())) continue;
            add(fGeoManager->GetCurrentNode(), fGeoManager->GetCurrentMatrix(), physID);
        }
    } else {
        TGeoIterator it(fGeoManager->GetTopVolume());
        while (TGeoNode* node = it()) {
            add(node, it.GetCurrentMatrix(), static_cast<int>(node->GetUniqueID()));
        }
    }
}

void TRestDetectorReadout::DrawViewItems(std::vector<ViewItem>& items, TGeoManager*& viewGeo,
                                         const std::string& option) {
    if (items.empty()) return;

    TGeoMedium* topMedium = nullptr;
    for (const auto& item : items)
        if (item.medium) { topMedium = item.medium; break; }
    if (!topMedium) return;

    double half = 0.0;
    for (const auto& item : items) {
        auto* bbox = dynamic_cast<TGeoBBox*>(item.shape);
        if (!bbox) continue;
        const double* org = bbox->GetOrigin();
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2)
                for (int sz = -1; sz <= 1; sz += 2) {
                    double local[3] = {org[0] + sx * bbox->GetDX(), org[1] + sy * bbox->GetDY(),
                                       org[2] + sz * bbox->GetDZ()};
                    double global[3];
                    item.matrix->LocalToMaster(local, global);
                    half = std::max<double>(
                        {half, std::abs(global[0]), std::abs(global[1]), std::abs(global[2])});
                }
    }
    half = (half > 0.0) ? 1.1 * half : 1.e5;

    TGeoManager* prevGeo = gGeoManager;
    if (viewGeo) {
        if (prevGeo == viewGeo) prevGeo = nullptr;
        delete viewGeo;
        viewGeo = nullptr;
    }

    gGeoManager = nullptr;   // el constructor borraría la geometría global existente
    viewGeo = new TGeoManager("activeEventView", "Active event view");

    TGeoVolume* top = viewGeo->MakeBox("TOP", topMedium, half, half, half);
    viewGeo->SetTopVolume(top);

    // Items are grouped in assemblies: a single volume with too many daughters (~1e5) overflows the
    // Int_t buffer size computed in TGeoVoxelFinder::SortAll, corrupting the heap at CloseGeometry
    const int kItemsPerGroup = 1000;
    TGeoVolumeAssembly* group = nullptr;
    int i = 0;
    for (auto& item : items) {
        if (i % kItemsPerGroup == 0) {
            group = new TGeoVolumeAssembly(Form("VIEW_GROUP_%d", i / kItemsPerGroup));
            top->AddNode(group, i / kItemsPerGroup);
        }
        TGeoMedium* med = item.medium ? item.medium : topMedium;
        auto* copy = new TGeoVolume(Form("%s_v%d", item.name.c_str(), i), item.shape, med);
        copy->SetLineColor(item.color);
        copy->SetVisibility(kTRUE);
        group->AddNode(copy, i++, item.matrix.release());
    }

    viewGeo->CloseGeometry();
    viewGeo->SetVisLevel(2);
    viewGeo->SetVisOption(0);
    top->SetVisibility(kFALSE);
    top->VisibleDaughters(kTRUE);
    top->Draw(option.c_str());

    gGeoManager = prevGeo;
}

ROOT::Math::XYZVector TRestDetectorReadout::GetPositionFromChannel(int daqID) const {
    if (!fGeoManager) return ROOT::Math::XYZVector(REST_nan, REST_nan, REST_nan);

    int targetPhysicalID = -1;
    for (const auto& [physicalID, channelID] : fPhysicalToDAQMap) {
        if (channelID == daqID) { targetPhysicalID = physicalID; break; }
    }
    if (targetPhysicalID < 0) return ROOT::Math::XYZVector(REST_nan, REST_nan, REST_nan);

    bool usePathNavigation = !fPathToPhysicalIDMap.empty();

    if (usePathNavigation) {
        std::string targetPath = "";
        for (const auto& [path, id] : fPathToPhysicalIDMap) {
            if (id == targetPhysicalID) { targetPath = path; break; }
        }
        if (targetPath.empty()) return ROOT::Math::XYZVector(REST_nan, REST_nan, REST_nan);

        if (!fGeoManager->cd(targetPath.c_str())) {
            return ROOT::Math::XYZVector(REST_nan, REST_nan, REST_nan);
        }

        const TGeoMatrix* currentGlobalMatrix = fGeoManager->GetCurrentMatrix();
        if (!currentGlobalMatrix) return ROOT::Math::XYZVector(REST_nan, REST_nan, REST_nan);

        double localOrigin[] = {0.0, 0.0, 0.0};
        double globalMasterOrigin[] = {0.0, 0.0, 0.0};
        currentGlobalMatrix->LocalToMaster(localOrigin, globalMasterOrigin);

        return ROOT::Math::XYZVector(globalMasterOrigin[0], globalMasterOrigin[1], globalMasterOrigin[2]);
    }

    TGeoIterator it(fGeoManager->GetTopVolume());
    TGeoNode* node = nullptr;
    while ((node = it())) {
        if (static_cast<int>(node->GetUniqueID()) == targetPhysicalID) {
            const TGeoMatrix* currentGlobalMatrix = it.GetCurrentMatrix();
            if (!currentGlobalMatrix) continue;

            double localOrigin[] = {0.0, 0.0, 0.0};
            double globalMasterOrigin[] = {0.0, 0.0, 0.0};
            currentGlobalMatrix->LocalToMaster(localOrigin, globalMasterOrigin);

            return ROOT::Math::XYZVector(globalMasterOrigin[0], globalMasterOrigin[1],
                                         globalMasterOrigin[2]);
        }
    }
    return ROOT::Math::XYZVector(REST_nan, REST_nan, REST_nan);
}

int TRestDetectorReadout::GetChannelFromPosition(double x, double y, double z) const {
    if (!fGeoManager || fPhysicalToDAQMap.empty()) return -1;

    TGeoNode* node = fGeoManager->FindNode(x, y, z);
    if (!node || node == fGeoManager->GetTopNode()) return -1;

    int physicalID = -1;

    if (!fPathToPhysicalIDMap.empty()) {
        std::string nodePath = fGeoManager->GetPath();
        auto it = fPathToPhysicalIDMap.find(nodePath);
        if (it != fPathToPhysicalIDMap.end()) physicalID = it->second;
    } else {
        physicalID = static_cast<int>(node->GetUniqueID());
    }

    if (physicalID < 0) return -1;
    auto daqIt = fPhysicalToDAQMap.find(physicalID);
    return (daqIt != fPhysicalToDAQMap.end()) ? daqIt->second : -1;
}

/// \brief Loads decoding from a text file.
/// \param decFilename Decoding file path.
/// \return `true` on successful decoding load.
bool TRestDetectorReadout::LoadDecoding(const std::string& decFilename) {
    if (decFilename.empty()) {
        Error("TRestDetectorReadout::LoadDecoding", "Cannot open mapping file: %s", decFilename.c_str());
        return false;
    }

    std::ifstream file(decFilename);
    if (!file.is_open()) {
        Error("TRestDetectorReadout::LoadDecoding", "Cannot open mapping file: %s", decFilename.c_str());
        return false;
    }

    std::cout << "Loading decoding file " << decFilename << std::endl;

    std::stringstream buffer;
    buffer << file.rdbuf();

    std::map<int, int> decodingMap;
    if (!ParseDecodingString(buffer.str(), decodingMap)) {
        return false;
    }

    fPhysicalToDAQMap = std::move(decodingMap);
    return true;
}

/// \brief Converts the current decoding map into text format.
/// \return Multiline decoding text.
std::string TRestDetectorReadout::GetDecodingAsString() const {
    std::stringstream ss;
    ss << "# daqChannel\tphysicalID\n";
    for (const auto& [physicalID, daqID] : fPhysicalToDAQMap) {
        ss << daqID << "\t" << physicalID << "\n";
    }
    return ss.str();
}

/// \brief Imports only geometry from an input ROOT file.
/// \param fIn Input ROOT file.
/// \param geometryName Geometry object name.
/// \return `true` if geometry import succeeded.
bool TRestDetectorReadout::ImportGeometry(TFile* fIn, const std::string& geometryName) {
    if (!fIn || fIn->IsZombie()) return false;

    const std::string resolvedGeometryName = geometryName.empty() ? GetName() : geometryName;

    TDirectory* readoutDir = GetSubDirectory(GetSubDirectory(fIn, kReadoutsDir), resolvedGeometryName.c_str());
    if (!readoutDir) return false;

    TGeoManager* prevGeo = gGeoManager;
    gGeoManager = nullptr;

    TGeoManager* geo = nullptr;
    readoutDir->GetObject(kGeometryKey, geo);

    if (!geo) {
        gGeoManager = prevGeo;
        return false;
    }

    SetGeoManager(geo);

    fNode = ReadMetadata(fIn, geometryName);
    return true;
}

std::vector<std::string> TRestDetectorReadout::GetDecodingNames(TFile* fIn, const std::string& geometryName) {
    std::vector<std::string> names;
    TDirectory* readoutDir = GetSubDirectory(GetSubDirectory(fIn, kReadoutsDir), geometryName.c_str());
    TDirectory* geometryDecDir = GetSubDirectory(readoutDir, kDecodingsDir);
    if (!geometryDecDir) return names;
    for (TObject* key : *geometryDecDir->GetListOfKeys()) names.emplace_back(key->GetName());
    return names;
}

/// \brief Imports geometry and decoding from an input ROOT file.
/// \param fIn Input ROOT file.
/// \param geometryName Geometry object name.
/// \param decodingName Decoding object name.
/// \return `true` if import succeeded.
bool TRestDetectorReadout::Import(TFile* fIn, const std::string& geometryName,
                                  const std::string& decodingName) {
    if (!ImportGeometry(fIn, geometryName)) return false;

    const std::string resolvedGeometryName = geometryName.empty() ? GetName() : geometryName;
    TDirectory* readoutDir = GetSubDirectory(GetSubDirectory(fIn, kReadoutsDir), resolvedGeometryName.c_str());
    TDirectory* geometryDecDir = GetSubDirectory(readoutDir, kDecodingsDir);
    if (!geometryDecDir) return false;

    TObjString* decObj = dynamic_cast<TObjString*>(geometryDecDir->Get(decodingName.c_str()));
    if (!decObj) return false;

    std::map<int, int> decodingMap;
    if (!ParseDecodingString(decObj->GetString().Data(), decodingMap)) return false;

    fPhysicalToDAQMap = std::move(decodingMap);
    return true;
}

/// \brief Exports geometry and decoding to an output ROOT file.
/// \param fOut Output ROOT file.
/// \param geometryName Geometry object name.
/// \param decodingName Decoding object name.
/// \return `true` if export succeeded.
bool TRestDetectorReadout::Export(TFile* fOut, const std::string& geometryName,
                                  const std::string& decodingName) const {
    if (!fOut || fOut->IsZombie()) return false;

    const std::string resolvedGeometryName = geometryName.empty() ? GetName() : geometryName;

    TDirectory* readoutDir =
        GetOrMakeSubDirectory(GetOrMakeSubDirectory(fOut, kReadoutsDir), resolvedGeometryName.c_str());
    if (!readoutDir) return false;
    readoutDir->cd();

    if (GetGeoManager()) {
        GetGeoManager()->Write(kGeometryKey, TObject::kOverwrite);
    }

    // Export the decoding payload (original logic preserved).
    TDirectory* geometryDecDir = GetOrMakeSubDirectory(readoutDir, kDecodingsDir);
    if (!geometryDecDir) return false;
    geometryDecDir->cd();

    std::string decText = GetDecodingAsString();
    TObjString rootDecString(decText.c_str());
    rootDecString.Write(decodingName.c_str(), TObject::kOverwrite);

    fOut->cd();

    try {
        WriteMetadata(fOut, resolvedGeometryName, fNode);
    } catch (const std::exception& e) {
        RESTError << "Error exportando metadatos en TRestDetectorReadout: " << e.what() << RESTendl;
        return false;
    }
    // =========================================================================

    fOut->cd();

    return true;
}

/// \brief Sets the geometry manager and updates top assembly pointer.
/// \param geo Geometry manager pointer.
void TRestDetectorReadout::SetGeoManager(TGeoManager* geo) {
    fGeoManager = geo; 
}
