#include "TRestFibreReadout.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <stdexcept>

#include "TFile.h"
#include "TGeoManager.h"
#include "TGeoMatrix.h"
#include "TGeoNode.h"
#include "TGeoTube.h"
#include "TGeoVolume.h"

/// \class TRestFibreReadout
/// Readout of a scintillator crossed by a lattice of fibres, each one read by its own photosensor, e.g.
/// LiquidO: an opaque scintillator confines the light near where it is produced and wavelength shifting
/// fibres, read by SiPMs, collect it.
///
/// The scintillator is a box (`size`, centred at `position`, not rotated) and the fibres cross it in
/// families (`fibres`) of parallel fibres along an axis (`axis`: X, Y or Z), placed on a grid of side
/// `pitch` in the plane transverse to them, starting at `offset` from the lower corner of the box (the
/// transverse axes in cyclic order after the axis: X: Y and Z; Y: Z and X; Z: X and Y). The fibres of a
/// family span the whole box along their axis; the families must not intersect (e.g. alternated layers).
/// Each fibre is a channel: the physical IDs are consecutive, family by family, and inside a family
/// firstID + i * nV + j (i, j the indices along the first and second transverse axes). Without
/// `decodingFile`, the DAQ ID of each fibre is its physical ID.
///
/// A fibre measures its two transverse coordinates: GetPositionFromChannel gives them, with the coordinate
/// along the fibre NaN, so the hits reconstructed from the signals of the fibres along Y are XZ hits and
/// those along X are YZ hits (as the strips of a Micromegas). GetChannelFromPosition gives the closest
/// fibre to a position (in the plane transverse to each fibre).
///
/// The readout gives only the geometry: GetChannelsInRange gives the fibres closer than a distance to a
/// position (in the plane transverse to each fibre), used by the localCapture model of
/// TRestDetectorLightCollectionProcess with its capture probability versus that distance and the quantum
/// efficiency of the SiPMs; the light yield is in TRestDetectorScintillationProcess.
///
/// Geant4: the fibres are dead material for the simulation. WriteGDMLPlacements (macro
/// REST_Detector_GenerateFibreReadout) writes their placements (`physvol` named fibre_<physical ID>, of
/// the logical volume `volume` of the family, defined in the GDML with its solid, a tube along Z as long
/// as the box along the fibre, and material), to include as an external entity inside the volume of the
/// scintillator, so the simulation and the readout share the same fibres.
///
/// Parameters: decodingFile (optional) and, in `readoutParameters`, size, position and fibres (axis, pitch,
/// offset, volume).
///
/// Example (LiquidO prototype of 1.5 m, fibres along Y and along X in alternated layers 10 mm apart):
///
///    readout:
///      class: "TRestFibreReadout"
///      name: "LiquidO_Readout"
///      readoutParameters:
///        size: [1500mm, 1500mm, 1500mm]
///        position: [0, 0, 750mm]
///        fibres:
///          - axis: "Y"
///            pitch: 20mm
///            offset: [15mm, 15mm]
///            volume: "fibreYVolume"
///          - axis: "X"
///            pitch: 20mm
///            offset: [5mm, 5mm]
///            volume: "fibreXVolume"
///
/// The fibres along Y (axis Y: transverse Z and X) are at z = 15 + 20 k, x = -735 + 20 i; those along X
/// (transverse Y and Z) at y = -745 + 20 i, z = 5 + 20 k (mm).

using ROOT::Math::XYZVector;

namespace {
const double kNaN = std::numeric_limits<double>::quiet_NaN();

const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestFibreReadout", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestFibreReadout>(instanceName, params);
        });
    return true;
}();

double Component(const XYZVector& position, int axis) {
    return axis == 0 ? position.X() : axis == 1 ? position.Y() : position.Z();
}
}  // namespace

static const bool TRestFibreReadout_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterNestedField<TRestFibreReadout>("readoutParameters", &TRestFibreReadout::fReadoutParams);

    using P = TRestFibreReadoutParameters;
    reg.RegisterField<P>("size", &P::fSize);
    reg.RegisterField<P>("position", &P::fPosition);
    reg.RegisterField<P>("fibres", &P::fFibres);

    using F = TRestFibreFamily;
    reg.RegisterField<F>("axis", &F::fAxis);
    reg.RegisterField<F>("pitch", &F::fPitch);
    reg.RegisterField<F>("offset", &F::fOffset);
    reg.RegisterField<F>("volume", &F::fVolume);
    return true;
}();

// ============================================================
// TRestFibreFamily
// ============================================================

TRestFibreFamily::TRestFibreFamily() : TRestMetadata() { fName = "TRestFibreFamily"; }

TRestFibreFamily::TRestFibreFamily(const std::string& name, const YAML::Node& node)
    : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestFibreFamily::LoadConfig() {
    UpdateParamsFromYAML<TRestFibreFamily>(fNode);
    const std::string prefix = "TRestFibreFamily: ";
    if (fAxis != "X" && fAxis != "Y" && fAxis != "Z") {
        throw std::runtime_error(prefix + "axis must be X, Y or Z, not '" + fAxis + "'");
    }
    if (fPitch <= 0) throw std::runtime_error(prefix + "pitch must be positive");
    UpdateYAMLFromParams<TRestFibreFamily>(fNode);
}

// ============================================================
// TRestFibreReadoutParameters
// ============================================================

TRestFibreReadoutParameters::TRestFibreReadoutParameters() : TRestMetadata() {
    fName = "TRestFibreReadoutParameters";
}

TRestFibreReadoutParameters::TRestFibreReadoutParameters(const std::string& name, const YAML::Node& node)
    : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestFibreReadoutParameters::LoadConfig() {
    UpdateParamsFromYAML<TRestFibreReadoutParameters>(fNode);
    const std::string prefix = "TRestFibreReadoutParameters: ";
    for (const auto& size : fSize) {
        if (size <= 0) throw std::runtime_error(prefix + "the size must be positive along X, Y and Z");
    }
    if (fFibres.empty()) throw std::runtime_error(prefix + "no fibres");
    UpdateYAMLFromParams<TRestFibreReadoutParameters>(fNode);
}

// ============================================================
// TRestFibreReadout
// ============================================================

TRestFibreReadout::TRestFibreReadout() : TRestDetectorReadout() { fName = "TRestFibreReadout"; }

TRestFibreReadout::TRestFibreReadout(const std::string& instanceName, const YAML::Node& node)
    : TRestDetectorReadout(instanceName, node) {
    LoadConfig();
}

TRestFibreReadout::TRestFibreReadout(const std::string& fileName, const std::string& sectionName)
    : TRestDetectorReadout(fileName, sectionName) {
    LoadConfig();
}

void TRestFibreReadout::LoadConfig() {
    TRestDetectorReadout::LoadConfig();
    UpdateParamsFromYAML<TRestFibreReadout>(fNode);
    UpdateYAMLFromParams<TRestFibreReadout>(fNode);
    BuildGrids();
}

void TRestFibreReadout::BuildGrids() {
    fGrids.clear();
    int firstID = 0;
    for (const auto& family : fReadoutParams.fFibres) {
        Grid grid;
        grid.axis = family.GetAxis();
        grid.u = (grid.axis + 1) % 3;
        grid.v = (grid.axis + 2) % 3;
        const int transverse[2] = {grid.u, grid.v};
        int count[2] = {0, 0};
        for (int t = 0; t < 2; t++) {
            const double offset = family.fOffset[t];
            const double size = fReadoutParams.fSize[transverse[t]];
            if (offset >= 0 && offset <= size) {
                // Fibres at offset + n pitch inside the box (the small tolerance keeps one at the far edge)
                count[t] = static_cast<int>(std::floor((size - offset) / family.fPitch + 1e-9)) + 1;
            }
        }
        grid.nU = count[0];
        grid.nV = count[1];
        grid.firstID = firstID;
        firstID += grid.nU * grid.nV;
        fGrids.push_back(grid);
    }
}

int TRestFibreReadout::GetNumberOfFibres() const {
    int total = 0;
    for (const auto& grid : fGrids) total += grid.nU * grid.nV;
    return total;
}

double TRestFibreReadout::GetFibreCoordinate(size_t family, int t, int index) const {
    const auto& grid = fGrids[family];
    const int axis = t == 0 ? grid.u : grid.v;
    return GetMinimum(axis) + fReadoutParams.fFibres[family].fOffset[t] +
           index * static_cast<double>(fReadoutParams.fFibres[family].fPitch);
}

bool TRestFibreReadout::GetFibre(int physicalID, size_t& family, int& i, int& j) const {
    for (size_t f = 0; f < fGrids.size(); f++) {
        const auto& grid = fGrids[f];
        const int local = physicalID - grid.firstID;
        if (local < 0 || local >= grid.nU * grid.nV) continue;
        family = f;
        i = local / grid.nV;
        j = local % grid.nV;
        return true;
    }
    return false;
}

int TRestFibreReadout::GetDAQID(int physicalID) const {
    const auto it = fPhysicalToDAQMap.find(physicalID);
    return it == fPhysicalToDAQMap.end() ? -1 : it->second;
}

void TRestFibreReadout::BuildGeometry() {
    if (!fNode || fNode.IsNull()) {
        throw std::runtime_error("TRestFibreReadout: the YAML node is not initialized");
    }
    InitializeReadout();

    // Decoding: the file if given (not empty nor the default of TRestDetectorReadout), otherwise DAQ ID =
    // physical ID
    if (!fDecodingFile.empty() && fDecodingFile != "default_decoding.dec") {
        if (!LoadDecoding(fDecodingFile)) {
            throw std::runtime_error("TRestFibreReadout::BuildGeometry - cannot load the decoding file '" +
                                     fDecodingFile + "' (not found or without channels)");
        }
    } else {
        fPhysicalToDAQMap.clear();
        for (int id = 0; id < GetNumberOfFibres(); id++) fPhysicalToDAQMap[id] = id;
    }

    TGeoMedium* fibreMedium = fGeoManager->GetMedium("fibre_medium");
    if (!fibreMedium) {
        auto* polystyrene = new TGeoMaterial("Polystyrene", 104.15 / 8, 3.5, 1.05);
        fibreMedium = new TGeoMedium("fibre_medium", 1, polystyrene);
    }

    TGeoVolume* top = fGeoManager->GetTopVolume();
    const unsigned int kNoPhysicalID = static_cast<unsigned int>(-1);
    int nodeCounter = 0;
    for (size_t f = 0; f < fGrids.size(); f++) {
        const auto& grid = fGrids[f];
        const auto& family = fReadoutParams.fFibres[f];
        // A thin tube to draw the fibre (the real one is in the GDML), along local Z rotated to the axis
        const double radius = family.fPitch / 10.0;
        const double halfLength = fReadoutParams.fSize[grid.axis] / 2.0;
        auto* tube = new TGeoTube(("fibre_shape_" + std::to_string(f)).c_str(), 0, radius, halfLength);
        auto* volume = new TGeoVolume(("FIBRE_" + std::to_string(f)).c_str(), tube, fibreMedium);
        volume->SetLineColor(f % 2 == 0 ? kBlue : kGreen + 2);
        auto* rotation = new TGeoRotation(("fibre_rotation_" + std::to_string(f)).c_str());
        if (grid.axis == 0) rotation->RotateY(90);
        if (grid.axis == 1) rotation->RotateX(90);

        // One assembly per row of fibres: a single volume with all of them overflows the voxel finder
        for (int i = 0; i < grid.nU; i++) {
            auto* row =
                new TGeoVolumeAssembly(("FIBRE_ROW_" + std::to_string(f) + "_" + std::to_string(i)).c_str());
            for (int j = 0; j < grid.nV; j++) {
                double centre[3];
                centre[grid.axis] = fReadoutParams.fPosition[grid.axis];
                centre[grid.u] = GetFibreCoordinate(f, 0, i);
                centre[grid.v] = GetFibreCoordinate(f, 1, j);
                const int physicalID = grid.firstID + i * grid.nV + j;
                TGeoNode* node = row->AddNode(volume, physicalID,
                                              new TGeoCombiTrans(centre[0], centre[1], centre[2], rotation));
                node->SetUniqueID(physicalID);
                nodeCounter++;
            }
            TGeoNode* rowNode = top->AddNode(row, static_cast<int>(f * 100000 + i));
            rowNode->SetUniqueID(kNoPhysicalID);
        }
    }
    fGeoManager->CloseGeometry();
    std::cout << "[+] Geometry built: " << nodeCounter << " fibres in " << fGrids.size() << " families."
              << std::endl;
}

int TRestFibreReadout::GetChannelFromPosition(double x, double y, double z) const {
    const double position[3] = {x, y, z};
    for (int axis = 0; axis < 3; axis++) {
        const double minimum = GetMinimum(axis);
        if (!(position[axis] >= minimum && position[axis] <= minimum + fReadoutParams.fSize[axis])) return -1;
    }

    int closestID = -1;
    double closest = std::numeric_limits<double>::infinity();
    for (size_t f = 0; f < fGrids.size(); f++) {
        const auto& grid = fGrids[f];
        if (grid.nU == 0 || grid.nV == 0) continue;
        const double pitch = fReadoutParams.fFibres[f].fPitch;
        const int counts[2] = {grid.nU, grid.nV};
        const int axes[2] = {grid.u, grid.v};
        int index[2];
        double distance2 = 0;
        for (int t = 0; t < 2; t++) {
            const double first = GetFibreCoordinate(f, t, 0);
            index[t] = static_cast<int>(std::lround((position[axes[t]] - first) / pitch));
            index[t] = std::clamp(index[t], 0, counts[t] - 1);
            const double delta = position[axes[t]] - GetFibreCoordinate(f, t, index[t]);
            distance2 += delta * delta;
        }
        if (distance2 < closest) {
            closest = distance2;
            closestID = grid.firstID + index[0] * grid.nV + index[1];
        }
    }
    return closestID < 0 ? -1 : GetDAQID(closestID);
}

XYZVector TRestFibreReadout::GetPositionFromChannel(int daqID) const {
    const int physicalID = GetPhysicalID(daqID);
    size_t family;
    int i, j;
    if (physicalID < 0 || !GetFibre(physicalID, family, i, j)) return XYZVector(kNaN, kNaN, kNaN);
    double position[3] = {kNaN, kNaN, kNaN};
    position[fGrids[family].u] = GetFibreCoordinate(family, 0, i);
    position[fGrids[family].v] = GetFibreCoordinate(family, 1, j);
    return XYZVector(position[0], position[1], position[2]);
}

std::pair<XYZVector, XYZVector> TRestFibreReadout::GetChannelsBoundingBox() const {
    double low[3], high[3];
    for (int axis = 0; axis < 3; axis++) {
        low[axis] = GetMinimum(axis);
        high[axis] = low[axis] + fReadoutParams.fSize[axis];
    }
    return {XYZVector(low[0], low[1], low[2]), XYZVector(high[0], high[1], high[2])};
}

std::pair<int, int> TRestFibreReadout::GetAdjacentChannels(int daqID) const {
    const int physicalID = GetPhysicalID(daqID);
    size_t family;
    int i, j;
    if (physicalID < 0 || !GetFibre(physicalID, family, i, j)) return {-1, -1};
    const int nV = fGrids[family].nV;
    return {j > 0 ? GetDAQID(physicalID - 1) : -1, j < nV - 1 ? GetDAQID(physicalID + 1) : -1};
}

std::vector<TRestDetectorReadout::ChannelInRange> TRestFibreReadout::GetChannelsInRange(
    const XYZVector& position, double maxDistance) const {
    std::vector<ChannelInRange> channels;
    if (!(maxDistance >= 0)) return channels;

    for (size_t f = 0; f < fGrids.size(); f++) {
        const auto& grid = fGrids[f];
        // The fibres span the box along their axis
        const double along = Component(position, grid.axis);
        const double minimum = GetMinimum(grid.axis);
        if (!(along >= minimum && along <= minimum + fReadoutParams.fSize[grid.axis])) continue;

        const double pitch = fReadoutParams.fFibres[f].fPitch;
        const double pu = Component(position, grid.u), pv = Component(position, grid.v);
        const double firstU = GetFibreCoordinate(f, 0, 0), firstV = GetFibreCoordinate(f, 1, 0);
        const int iMin = std::max(0, static_cast<int>(std::ceil((pu - maxDistance - firstU) / pitch)));
        const int iMax =
            std::min(grid.nU - 1, static_cast<int>(std::floor((pu + maxDistance - firstU) / pitch)));
        const int jMin = std::max(0, static_cast<int>(std::ceil((pv - maxDistance - firstV) / pitch)));
        const int jMax =
            std::min(grid.nV - 1, static_cast<int>(std::floor((pv + maxDistance - firstV) / pitch)));
        for (int i = iMin; i <= iMax; i++) {
            const double u = firstU + i * pitch;
            for (int j = jMin; j <= jMax; j++) {
                const double v = firstV + j * pitch;
                const double distance = std::hypot(pu - u, pv - v);
                if (distance > maxDistance) continue;
                const int daqID = GetDAQID(grid.firstID + i * grid.nV + j);
                if (daqID < 0) continue;
                // The projection of the position on the fibre
                double projected[3];
                projected[grid.axis] = along;
                projected[grid.u] = u;
                projected[grid.v] = v;
                channels.push_back({daqID, distance, XYZVector(projected[0], projected[1], projected[2])});
            }
        }
    }
    return channels;
}

bool TRestFibreReadout::WriteGDMLPlacements(const std::string& fileName) const {
    for (const auto& family : fReadoutParams.fFibres) {
        if (family.fVolume.empty()) {
            RESTError << "TRestFibreReadout::WriteGDMLPlacements: a fibre family has no volume" << RESTendl;
            return false;
        }
    }
    std::ofstream file(fileName);
    if (!file) {
        RESTError << "TRestFibreReadout::WriteGDMLPlacements: cannot write " << fileName << RESTendl;
        return false;
    }
    file << "<!-- Fibres of the readout " << fName << " (" << GetNumberOfFibres()
         << "), written by TRestFibreReadout::WriteGDMLPlacements: positions relative to the centre of the "
            "scintillator -->\n";
    file << std::setprecision(10);
    for (size_t f = 0; f < fGrids.size(); f++) {
        const auto& grid = fGrids[f];
        // The tubes of the GDML are along Z
        const char* rotation = grid.axis == 0   ? R"( x="0" y="90" z="0")"
                               : grid.axis == 1 ? R"( x="90" y="0" z="0")"
                                                : "";
        for (int i = 0; i < grid.nU; i++) {
            for (int j = 0; j < grid.nV; j++) {
                double local[3] = {0, 0, 0};
                local[grid.u] = GetFibreCoordinate(f, 0, i) - fReadoutParams.fPosition[grid.u];
                local[grid.v] = GetFibreCoordinate(f, 1, j) - fReadoutParams.fPosition[grid.v];
                const std::string name = "fibre_" + std::to_string(grid.firstID + i * grid.nV + j);
                file << "<physvol name=\"" << name << "\" copynumber=\"" << grid.firstID + i * grid.nV + j
                     << "\">\n";
                file << "    <volumeref ref=\"" << fReadoutParams.fFibres[f].fVolume << "\"/>\n";
                file << "    <position name=\"" << name << "_position\" unit=\"mm\" x=\"" << local[0]
                     << "\" y=\"" << local[1] << "\" z=\"" << local[2] << "\"/>\n";
                if (*rotation) {
                    file << "    <rotation name=\"" << name << "_rotation\" unit=\"deg\"" << rotation
                         << "/>\n";
                }
                file << "</physvol>\n";
            }
        }
    }
    return static_cast<bool>(file);
}
