#include "TRestConstants.h"
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
#include <limits>

using namespace TRestConstants;

static const bool TRestDetectorReadout_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestDetectorReadout>("decodingFile", &TRestDetectorReadout::fDecodingFile);
    return true;
}();

/// \brief Constructs a generic readout metadata object with default name.
TRestDetectorReadout::TRestDetectorReadout() : TRestMetadata() {
    fName = "TRestDetectorReadout";
}

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

/// \brief Initializes geometry and decoding from YAML.
void TRestDetectorReadout::LoadConfig() {

    UpdateParamsFromYAML<TRestDetectorReadout>(fNode);

    if (!LoadDecoding(fDecodingFile) ){
        RESTError <<"Decoding file not found "<< RESTendl;
    }

    //Sync resolved parameters to the node
    UpdateYAMLFromParams<TRestDetectorReadout>(fNode);
}

void TRestDetectorReadout::InitializeReadout(){

// Initialize an isolated, silent TGeoManager instance
    if (fGeoManager) delete fGeoManager;
    fGeoManager = new TGeoManager(fName.c_str(), fName.c_str());
    TGeoManager::SetVerboseLevel(0);

    // Create a logical Assembly as Top Volume (no physical materials or vacuum setup required)
    fTopAssembly = fGeoManager->MakeVolumeAssembly("READOUT_TOP");
    fGeoManager->SetTopVolume(fTopAssembly);

}

/// \brief Parses decoding text with `physicalID readoutChannel` rows.
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

/// \brief Prints metadata summary for this detector readout.
void TRestDetectorReadout::PrintMetadata() {
    RESTMetadata << "=== TRestDetectorReadout ===" << RESTendl;
    if(fNode && !fNode.IsNull())RESTMetadata << YAML::Dump(fNode) << RESTendl;
}

/// \brief Opens a graphical window to visualize the readout geometry.
/// \param option Drawing option passed to ROOT (e.g., "ogl" for OpenGL).
void TRestDetectorReadout::ViewReadoutGeometry(const std::string& option) const {
    if (!fGeoManager || !fTopAssembly) {
        RESTError << "Cannot visualize readout: Geometry is not initialized!" << RESTendl;
        return;
    }

    RESTInfo << "Opening visualizer for readout geometry: " << GetName() << RESTendl;

    // Set visibility settings for the logical assembly container so it doesn't 
    // obstruct the view of the internal sensitive channels/pixels
    fTopAssembly->SetVisibility(kFALSE);
    fTopAssembly->VisibleDaughters(kTRUE);

    fGeoManager->SetVisLevel(10);
    fGeoManager->SetVisOption(0);

    // Tell ROOT to draw the top assembly volume containing all the physical nodes
    // If option is "ogl", it spawns the standalone high-performance OpenGL viewer
    fTopAssembly->Draw(option.c_str());
}

/// \brief Visualizes the geometry highlighting a specific set of active DAQ channels.
/// \param activeChannels Vector containing the DAQ channel IDs that fired in the event.
void TRestDetectorReadout::ViewActiveEvent(const std::vector<int>& activeChannels) const {
    if (!fTopAssembly) return;

    // 1. Accedemos al volumen de nuestro ensamblaje intermedio (primer hijo de fTopAssembly)
    if (fTopAssembly->GetNdaughters() == 0) return;
    TGeoNode* subAssemblyNode = fTopAssembly->GetNode(0);
    TGeoVolume* subAssemblyVol = subAssemblyNode->GetVolume();

    // Ahora el número de nodos corresponde a la cantidad de píxeles reales
    int nNodes = subAssemblyVol->GetNdaughters();

    // 2. Analizar qué canales electrónicos del evento se han disparado
    for (int daqID : activeChannels) {
        int targetPhysicalID = -1;
        
        for (const auto& [physicalID, readoutChannel] : fPhysicalToDAQMap) {
            if (readoutChannel == daqID) {
                targetPhysicalID = physicalID;
                break;
            }
        }

        if (targetPhysicalID == -1) continue;

        // 3. Buscamos el píxel dentro de los hijos del ensamblaje intermedio
        for (int i = 0; i < nNodes; ++i) {
            TGeoNode* node = subAssemblyVol->GetNode(i);
            int combinedID = node->GetUniqueID();

            if(combinedID == targetPhysicalID) {
                // Iluminamos en rojo el volumen del píxel del canal activo
                node->GetVolume()->SetLineColor(kRed);
            }
        }
    }

    fGeoManager->SetVisLevel(10);
    fGeoManager->SetVisOption(0); 
    
    // 4. Redibujar el canvas OpenGL interactivo con toda la estructura rotada
    fTopAssembly->Draw("ogl");
}


/// \brief Loads decoding from a text file.
/// \param decFilename Decoding file path.
/// \return `true` on successful decoding load.
bool TRestDetectorReadout::LoadDecoding(const std::string& decFilename) {
    if (decFilename.empty()){
        Error("TRestDetectorReadout::LoadDecoding", "Cannot open mapping file: %s", decFilename.c_str());
        return false;
    }

    std::ifstream file(decFilename);
    if (!file.is_open()) {
        Error("TRestDetectorReadout::LoadDecoding", "Cannot open mapping file: %s", decFilename.c_str());
        return false;
    }

    std::cout<<"Loading decoding file "<<decFilename<<std::endl;

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

    fNode = ReadMetadata(fIn, geometryName);

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
    if (!ParseDecodingString(decObj->GetString().Data(), decodingMap)) return false;

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

    // Exportación del Decoding (Tu lógica original intacta)
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

/// \brief Returns the DAQ channel associated with a spatial point.
/// \param x X coordinate.
/// \param y Y coordinate.
/// \param z Z coordinate.
/// \return DAQ channel ID, or `-1` when no channel is mapped.
int TRestDetectorReadout::GetChannelFromPosition(double x, double y, double z) const {
    if (!fGeoManager) {
      RESTError << "Geometry not initialized " << RESTendl;
      return -1;
    }

    if (fPhysicalToDAQMap.empty()){
      RESTError << "Decoding not initialazed " << RESTendl;
      return -1;
    }

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
/// \return Channel position, or `(nan,nan,nan)` when not found.
TVector3 TRestDetectorReadout::GetPositionFromChannel(int daqID) const {
    if (!fTopAssembly) {
      RESTError << "Geometry not initialized " << RESTendl;
      return TVector3(REST_nan, REST_nan, REST_nan);
    }

    int targetPhysicalID = -1;
    for (const auto& [physicalID, channelID] : fPhysicalToDAQMap) {
        if (channelID == daqID) {
            targetPhysicalID = physicalID;
            break;
        }
    }
    if (targetPhysicalID < 0){
      RESTError << "DaqID " << daqID << " not found" << RESTendl;
      return TVector3(REST_nan, REST_nan, REST_nan);
    }

    if (fTopAssembly->GetNdaughters() == 0) return TVector3(REST_nan, REST_nan, REST_nan);
    TGeoNode* subAssemblyNode = fTopAssembly->GetNode(0);
    TGeoVolume* subAssemblyVol = subAssemblyNode->GetVolume();

    int nNodes = subAssemblyVol->GetNdaughters();
    for (int i = 0; i < nNodes; ++i) {
        TGeoNode* node = subAssemblyVol->GetNode(i);
        
        if (static_cast<int>(node->GetUniqueID()) == targetPhysicalID) {
            const TGeoMatrix* matrix = node->GetMatrix();
            if (!matrix) return TVector3(REST_nan, REST_nan, REST_nan);
            
            const double* localTrans = matrix->GetTranslation();
            
            const TGeoMatrix* globalMatrix = subAssemblyNode->GetMatrix();
            if (!globalMatrix) return TVector3(localTrans[0], localTrans[1], localTrans[2]);

            double masterTrans[3];
            globalMatrix->LocalToMaster(localTrans, masterTrans);

            return TVector3(masterTrans[0], masterTrans[1], masterTrans[2]);
        }
    }
    
    return TVector3(REST_nan, REST_nan, REST_nan);
}

/// \brief Sets the geometry manager and updates top assembly pointer.
/// \param geo Geometry manager pointer.
void TRestDetectorReadout::SetGeoManager(TGeoManager* geo) {
    fGeoManager = geo;
    if (fGeoManager) fTopAssembly = (TGeoVolumeAssembly*)fGeoManager->GetTopVolume();
}
