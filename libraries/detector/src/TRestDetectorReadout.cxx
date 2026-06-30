#include "TRestDetectorReadout.h"
#include "TGeoNode.h"
#include "TGeoMatrix.h"
#include "TError.h"
#include "TDirectory.h"
#include "TFile.h"
#include "TObjString.h"
#include <fstream>
#include <sstream>
#include <iostream>

/// \brief Constructs a generic readout metadata object with default name.
TRestDetectorReadout::TRestDetectorReadout() : TRestMetadata() {
    fName = "generic_detector_readout";
}

/// \brief Destructor.
TRestDetectorReadout::~TRestDetectorReadout() {
    // TGeoManager is globally owned by ROOT (gGeoManager), it unregisters automatically.
}

namespace {
/// \brief Parses decoding text with `physicalID readoutChannel` rows.
/// \param text Multiline decoding text.
/// \param decodingMap Output map receiving parsed values.
/// \return `true` when at least one valid row is parsed.
bool ParseDecodingText(const std::string& text, std::map<int, int>& decodingMap) {
    std::stringstream stream(text);
    std::string line;
    int linesParsed = 0;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        size_t firstChar = line.find_first_not_of("     ");
        if (firstChar == std::string::npos || line[firstChar] == '#') continue;

        std::stringstream ss(line);
        int physicalID = -1;
        int readoutChannel = -1;
        if (ss >> physicalID >> readoutChannel) {
            decodingMap[physicalID] = readoutChannel;
            linesParsed++;
        }
    }

    return (linesParsed > 0);
}
}  // namespace

namespace {
/// \brief Builds an identity decoding map from existing top-assembly nodes.
/// \param topAssembly Geometry top assembly.
/// \param decodingMap Output map receiving `physicalID -> physicalID` values.
void SetIdentityDecodingFromGeometry(TGeoVolumeAssembly* topAssembly, std::map<int, int>& decodingMap) {
    decodingMap.clear();
    if (!topAssembly) return;

    const int nNodes = topAssembly->GetNdaughters();
    for (int i = 0; i < nNodes; ++i) {
        TGeoNode* node = topAssembly->GetNode(i);
        if (!node) continue;
        const int physicalID = static_cast<int>(node->GetUniqueID());
        decodingMap[physicalID] = physicalID;
    }
}
}  // namespace

/// \brief Loads configuration by initializing from the internal YAML node.
void TRestDetectorReadout::LoadConfig() {
    InitFromYAML(fNode);
}

/// \brief Prints metadata summary for this detector readout.
void TRestDetectorReadout::PrintMetadata() {
    RESTInfo << "Detector readout: " << fName << RESTendl;
}

/// \brief Initializes geometry and decoding from YAML.
/// \param readoutNode YAML node with geometry and optional decoding settings.
void TRestDetectorReadout::InitFromYAML(const YAML::Node& readoutNode) {
    const auto nameNode = readoutNode["name"];
    if (nameNode) fName = TRestTools::ReadYAMLParam<std::string>(nameNode);

    const auto titleNode = readoutNode["title"];
    const std::string title = titleNode ? TRestTools::ReadYAMLParam<std::string>(titleNode) : fName;

    // Initialize an isolated, silent TGeoManager instance
    if (fGeoManager) delete fGeoManager;
    fGeoManager = new TGeoManager(fName.c_str(), title.c_str());
    TGeoManager::SetVerboseLevel(0);

    // Create a logical Assembly as Top Volume (no physical materials or vacuum setup required)
    fTopAssembly = fGeoManager->MakeVolumeAssembly("READOUT_TOP");
    fGeoManager->SetTopVolume(fTopAssembly);

    // POLYMORPHIC CALL: Delegate geometry population to the specific derived subclass
    BuildGeometry(readoutNode);

    // Close and optimize the geometry using ROOT internal voxelization trees
    fGeoManager->CloseGeometry();

    // Optional: Load automatic default connectivity mapping if defined inside YAML
    const auto decodingNode = readoutNode["default_decoding"];
    if (decodingNode && !TRestTools::ReadYAMLParam<std::string>(decodingNode).empty()) {
        LoadDecoding(TRestTools::ReadYAMLParam<std::string>(decodingNode));
    } else {
        SetIdentityDecodingFromGeometry(fTopAssembly, fPhysicalToDAQMap);
    }
}

/// \brief Loads decoding from a text file.
/// \param decFilename Decoding file path.
/// \return `true` on successful decoding load.
bool TRestDetectorReadout::LoadDecoding(const std::string& decFilename) {
    if (decFilename.empty()) {
        SetIdentityDecodingFromGeometry(fTopAssembly, fPhysicalToDAQMap);
        return !fPhysicalToDAQMap.empty() || (fTopAssembly && fTopAssembly->GetNdaughters() == 0);
    }

    std::ifstream file(decFilename);
    if (!file.is_open()) {
        Error("TRestDetectorReadout::LoadDecoding", "Cannot open mapping file: %s", decFilename.c_str());
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    std::map<int, int> decodingMap;
    if (!ParseDecodingText(buffer.str(), decodingMap)) {
        return false;
    }

    fPhysicalToDAQMap = std::move(decodingMap);
    return true;
}

/// \brief Converts the current decoding map into text format.
/// \return Multiline decoding text.
std::string TRestDetectorReadout::GetDecodingAsString() const {
    std::stringstream ss;
    ss << "# physicalID\treadoutChannel\n";
    for (const auto& [physicalID, readoutChannel] : fPhysicalToDAQMap) {
        ss << physicalID << "\t" << readoutChannel << "\n";
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

    TDirectory* geoDir = fIn->GetDirectory("Geometries");
    if (!geoDir) return false;

    TGeoManager* geo = nullptr;
    geoDir->GetObject(resolvedGeometryName.c_str(), geo);
    if (!geo) return false;

    SetGeoManager(geo);
    return true;
}

/// \brief Imports geometry and decoding from an input ROOT file.
/// \param fIn Input ROOT file.
/// \param geometryName Geometry object name.
/// \param decodingName Decoding object name.
/// \return `true` if import succeeded.
bool TRestDetectorReadout::Import(TFile* fIn, const std::string& geometryName, const std::string& decodingName) {
    if (!ImportGeometry(fIn, geometryName)) return false;

    const std::string resolvedGeometryName = geometryName.empty() ? GetName() : geometryName;
    TDirectory* decDir = fIn->GetDirectory("Decodings");
    if (!decDir) return false;

    TDirectory* geometryDecDir = decDir->GetDirectory(resolvedGeometryName.c_str());
    if (!geometryDecDir) return false;

    TObjString* decObj = dynamic_cast<TObjString*>(geometryDecDir->Get(decodingName.c_str()));
    if (!decObj) return false;

    std::map<int, int> decodingMap;
    if (!ParseDecodingText(decObj->GetString().Data(), decodingMap)) return false;

    fPhysicalToDAQMap = std::move(decodingMap);
    return true;
}

/// \brief Exports geometry and decoding to an output ROOT file.
/// \param fOut Output ROOT file.
/// \param geometryName Geometry object name.
/// \param decodingName Decoding object name.
/// \return `true` if export succeeded.
bool TRestDetectorReadout::Export(TFile* fOut, const std::string& geometryName, const std::string& decodingName) const {
    if (!fOut || fOut->IsZombie()) return false;

    const std::string resolvedGeometryName = geometryName.empty() ? GetName() : geometryName;

    TDirectory* geoDir = fOut->GetDirectory("Geometries");
    if (!geoDir) geoDir = fOut->mkdir("Geometries");
    if (!geoDir) return false;
    geoDir->cd();

    if (GetGeoManager()) {
        GetGeoManager()->Write(resolvedGeometryName.c_str(), TObject::kOverwrite);
    }

    TDirectory* decDir = fOut->GetDirectory("Decodings");
    if (!decDir) decDir = fOut->mkdir("Decodings");
    if (!decDir) return false;

    TDirectory* geometryDecDir = decDir->GetDirectory(resolvedGeometryName.c_str());
    if (!geometryDecDir) geometryDecDir = decDir->mkdir(resolvedGeometryName.c_str());
    if (!geometryDecDir) return false;
    geometryDecDir->cd();

    std::string decText = GetDecodingAsString();
    TObjString rootDecString(decText.c_str());
    rootDecString.Write(decodingName.c_str(), TObject::kOverwrite);

    fOut->cd();
    return true;
}

/// \brief Returns the DAQ channel associated with a spatial point.
/// \param x X coordinate.
/// \param y Y coordinate.
/// \param z Z coordinate.
/// \return DAQ channel ID, or `-1` when no channel is mapped.
int TRestDetectorReadout::GetChannelFromPosition(double x, double y, double z) const {
    if (!fGeoManager) return -1;

    // ROOT high-speed navigation tree lookup over RAM voxel cells
    TGeoNode* node = fGeoManager->FindNode(x, y, z);
    if (!node || node == fGeoManager->GetTopNode()) return -1;

    const int physicalID = static_cast<int>(node->GetUniqueID());

    // Translate physical node layout ID into routing electronic ID
    auto it = fPhysicalToDAQMap.find(physicalID);
    return (it != fPhysicalToDAQMap.end()) ? it->second : -1;
}

/// \brief Returns spatial position associated with a DAQ channel.
/// \param daqID DAQ channel identifier.
/// \return Channel position, or `(0,0,0)` when not found.
TVector3 TRestDetectorReadout::GetPositionFromChannel(int daqID) const {
    if (!fTopAssembly) return TVector3(0,0,0);

    int targetPhysicalID = -1;
    for (const auto& [physicalID, channelID] : fPhysicalToDAQMap) {
        if (channelID == daqID) {
            targetPhysicalID = physicalID;
            break;
        }
    }
    if (targetPhysicalID < 0) return TVector3(0,0,0);

    // Scan top layout nodes to extract coordinates matching the mapped physical ID
    int nNodes = fTopAssembly->GetNdaughters();
    for (int i = 0; i < nNodes; ++i) {
        TGeoNode* node = fTopAssembly->GetNode(i);
        if (static_cast<int>(node->GetUniqueID()) == targetPhysicalID) {
            const TGeoMatrix* matrix = node->GetMatrix();
            if (!matrix) return TVector3(0, 0, 0);
            const double* trans = matrix->GetTranslation();
            return TVector3(trans[0], trans[1], trans[2]);
        }
    }
    return TVector3(0,0,0);
}

/// \brief Sets the geometry manager and updates top assembly pointer.
/// \param geo Geometry manager pointer.
void TRestDetectorReadout::SetGeoManager(TGeoManager* geo) {
    fGeoManager = geo;
    if (fGeoManager) fTopAssembly = (TGeoVolumeAssembly*)fGeoManager->GetTopVolume();
}
