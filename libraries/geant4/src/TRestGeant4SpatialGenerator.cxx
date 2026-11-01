#include "TRestGeant4SpatialGenerator.h"

#include <Math/AxisAngle.h>
#include <TFile.h>
#include <TTree.h>

#include <algorithm>
#include <mutex>
#include <stdexcept>

#include "TRestGeant4CRYShowers.h"
#include "TRestGeant4PrimaryGeneratorTypes.h"
#include "TRestTools.h"

using namespace std;
using namespace TRestGeant4PrimaryGeneratorTypes;
using ROOT::Math::XYZVector;
using TRestTools::ToLower;

static const bool TRestGeant4SpatialGenerator_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestGeant4SpatialGenerator>("type", &TRestGeant4SpatialGenerator::fType);

    using Point = TRestGeant4SpatialGeneratorPoint;
    reg.RegisterField<Point>("position", &Point::fPosition);

    using Shape = TRestGeant4SpatialGeneratorShape;
    reg.RegisterField<Shape>("from", &Shape::fFrom);
    reg.RegisterField<Shape>("shape", &Shape::fShape);
    reg.RegisterField<Shape>("size", &Shape::fSize);
    reg.RegisterField<Shape>("position", &Shape::fPosition);
    reg.RegisterField<Shape>("rotationAxis", &Shape::fRotationAxis);
    reg.RegisterField<Shape>("rotationValue", &Shape::fRotationValue);

    using Cosmic = TRestGeant4SpatialGeneratorCosmic;
    reg.RegisterField<Cosmic>("radius", &Cosmic::fRadius);

    using CRY = TRestGeant4SpatialGeneratorCRY;
    reg.RegisterField<CRY>("filename", &CRY::fFilename);
    reg.RegisterField<CRY>("position", &CRY::fPosition);
    reg.RegisterField<CRY>("verticalAxis", &CRY::fVerticalAxis);
    reg.RegisterField<CRY>("showersUsed", &CRY::fShowersUsed);
    reg.RegisterField<CRY>("exposureTime", &CRY::fExposureTime);
    return true;
}();

namespace {

XYZVector ToVector(const array<TRestWithUnits, 3>& values) { return {values[0], values[1], values[2]}; }

}  // namespace

// --- TRestGeant4SpatialGenerator ---

unique_ptr<TRestGeant4SpatialGenerator> TRestGeant4SpatialGenerator::Create(const string& name,
                                                                            const YAML::Node& node) {
    if (!node["type"]) throw runtime_error("TRestGeant4SpatialGenerator: the generator 'type' is missing");
    const auto type = ToLower(node["type"].as<string>());
    if (type == "point") return make_unique<TRestGeant4SpatialGeneratorPoint>(name, node);
    if (type == "volume") return make_unique<TRestGeant4SpatialGeneratorVolume>(name, node);
    if (type == "surface") return make_unique<TRestGeant4SpatialGeneratorSurface>(name, node);
    if (type == "cosmic") return make_unique<TRestGeant4SpatialGeneratorCosmic>(name, node);
    if (type == "cry") return make_unique<TRestGeant4SpatialGeneratorCRY>(name, node);
    throw runtime_error("TRestGeant4SpatialGenerator: unknown generator type '" + type +
                        "', it must be point, volume, surface, cosmic or cry");
}

// --- TRestGeant4SpatialGeneratorPoint ---

TRestGeant4SpatialGeneratorPoint::TRestGeant4SpatialGeneratorPoint(const string& name, const YAML::Node& node)
    : TRestGeant4SpatialGenerator(name, node) {
    LoadConfig();
}

void TRestGeant4SpatialGeneratorPoint::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4SpatialGeneratorPoint>(fNode);
    UpdateYAMLFromParams<TRestGeant4SpatialGeneratorPoint>(fNode);
}

XYZVector TRestGeant4SpatialGeneratorPoint::Sample(TRandom&) { return GetCenter(); }

XYZVector TRestGeant4SpatialGeneratorPoint::GetCenter() const { return ToVector(fPosition); }

// --- TRestGeant4SpatialGeneratorShape ---

XYZVector TRestGeant4SpatialGeneratorShape::GetCenter() const { return ToVector(fPosition); }

void TRestGeant4SpatialGeneratorShape::SetGdmlPlacement(const XYZVector& center, const XYZVector& axis,
                                                        double angle) {
    fPosition = {center.X(), center.Y(), center.Z()};
    fRotationAxis = {axis.X(), axis.Y(), axis.Z()};
    fRotationValue = angle;
}

void TRestGeant4SpatialGeneratorShape::CheckShape(const map<string, size_t>& shapes) {
    // As legacy, a generator from a geometry volume has no shape
    if (!fFrom.empty()) {
        fShape = "";
        return;
    }
    fShape = ToLower(fShape);
    const auto shape = shapes.find(fShape);
    if (shape == shapes.end()) {
        string names;
        for (const auto& [name, nSizes] : shapes) names += (names.empty() ? "" : ", ") + name;
        throw runtime_error(GetClassName() + ": 'from' (a geometry volume) or 'shape' (" + names +
                            ") is needed, '" + fShape + "' is not valid");
    }
    for (size_t i = 0; i < shape->second; i++) {
        if (fSize[i] <= 0) {
            throw runtime_error(GetClassName() + ": the " + fShape + " needs " + to_string(shape->second) +
                                " positive 'size' values");
        }
    }
    if (fRotationValue != 0 && ToVector(fRotationAxis).R() == 0) {
        throw runtime_error(GetClassName() + ": 'rotationAxis' must not be zero");
    }
}

XYZVector TRestGeant4SpatialGeneratorShape::ToWorld(const XYZVector& local) const {
    if (fRotationValue == 0) return local + GetCenter();
    const ROOT::Math::AxisAngle rotation(ToVector(fRotationAxis).Unit(), fRotationValue);
    return rotation * local + GetCenter();
}

// --- TRestGeant4SpatialGeneratorVolume ---

TRestGeant4SpatialGeneratorVolume::TRestGeant4SpatialGeneratorVolume(const string& name,
                                                                     const YAML::Node& node)
    : TRestGeant4SpatialGeneratorShape(name, node) {
    LoadConfig();
}

void TRestGeant4SpatialGeneratorVolume::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4SpatialGeneratorVolume>(fNode);
    CheckShape({{"box", 3}, {"sphere", 1}, {"cylinder", 2}});
    UpdateYAMLFromParams<TRestGeant4SpatialGeneratorVolume>(fNode);
}

XYZVector TRestGeant4SpatialGeneratorVolume::Sample(TRandom& random) {
    if (IsGdml()) throw runtime_error(GetClassName() + ": a GDML volume needs the Geant4 geometry");
    const double phi = random.Uniform(0, 2 * M_PI);
    XYZVector local;
    if (fShape == "box") {
        local = {fSize[0] * (random.Uniform() - 0.5), fSize[1] * (random.Uniform() - 0.5),
                 fSize[2] * (random.Uniform() - 0.5)};
    } else if (fShape == "sphere") {
        local = fSize[0] * cbrt(random.Uniform()) * IsotropicDirection(random);
    } else {
        const double r = fSize[0] * sqrt(random.Uniform());
        local = {r * cos(phi), r * sin(phi), fSize[1] * (random.Uniform() - 0.5)};
    }
    return ToWorld(local);
}

// --- TRestGeant4SpatialGeneratorSurface ---

TRestGeant4SpatialGeneratorSurface::TRestGeant4SpatialGeneratorSurface(const string& name,
                                                                       const YAML::Node& node)
    : TRestGeant4SpatialGeneratorShape(name, node) {
    LoadConfig();
}

void TRestGeant4SpatialGeneratorSurface::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4SpatialGeneratorSurface>(fNode);
    CheckShape({{"sphere", 1}, {"cylinder", 2}, {"circle", 1}, {"wall", 2}});
    UpdateYAMLFromParams<TRestGeant4SpatialGeneratorSurface>(fNode);
}

XYZVector TRestGeant4SpatialGeneratorSurface::Sample(TRandom& random) {
    if (IsGdml()) throw runtime_error(GetClassName() + ": a GDML surface needs the Geant4 geometry");
    const double phi = random.Uniform(0, 2 * M_PI);
    XYZVector local;
    if (fShape == "sphere") {
        local = fSize[0] * IsotropicDirection(random);
    } else if (fShape == "cylinder") {
        local = {fSize[0] * cos(phi), fSize[0] * sin(phi), fSize[1] * (random.Uniform() - 0.5)};
    } else if (fShape == "circle") {
        const double r = fSize[0] * sqrt(random.Uniform());
        local = {r * cos(phi), r * sin(phi), 0};
    } else {
        local = {fSize[0] * (random.Uniform() - 0.5), fSize[1] * (random.Uniform() - 0.5), 0};
    }
    return ToWorld(local);
}

// --- TRestGeant4SpatialGeneratorCosmic ---

TRestGeant4SpatialGeneratorCosmic::TRestGeant4SpatialGeneratorCosmic(const string& name,
                                                                     const YAML::Node& node)
    : TRestGeant4SpatialGenerator(name, node) {
    LoadConfig();
}

void TRestGeant4SpatialGeneratorCosmic::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4SpatialGeneratorCosmic>(fNode);
    if (fRadius < 0) throw runtime_error(GetClassName() + ": 'radius' must be positive");
    UpdateYAMLFromParams<TRestGeant4SpatialGeneratorCosmic>(fNode);
}

XYZVector TRestGeant4SpatialGeneratorCosmic::Sample(TRandom&) {
    throw runtime_error(GetClassName() + ": the cosmic origin depends on the direction");
}

XYZVector TRestGeant4SpatialGeneratorCosmic::SampleFromDirection(TRandom& random,
                                                                 const XYZVector& direction) const {
    if (fRadius <= 0) throw runtime_error(GetClassName() + ": the radius is not set");
    // Two unit vectors normal to the direction
    const XYZVector reference = abs(direction.Z()) < 0.9 ? XYZVector(0, 0, 1) : XYZVector(1, 0, 0);
    const XYZVector v = direction.Cross(reference).Unit();
    const XYZVector w = direction.Cross(v);

    const double r = fRadius * sqrt(random.Uniform());
    const double phi = random.Uniform(0, 2 * M_PI);
    const XYZVector onDisk = r * (cos(phi) * v + sin(phi) * w);
    return onDisk - sqrt(fRadius * fRadius - r * r) * direction;
}

void TRestGeant4SpatialGeneratorCosmic::SetDefaultRadius(double radius) {
    if (fRadius > 0) return;
    fRadius = radius;
    UpdateYAMLFromParams<TRestGeant4SpatialGeneratorCosmic>(fNode);
}

// --- TRestGeant4SpatialGeneratorCRY ---

/// The showers file, shared by the copies of the generator (threads): each shower is given once
struct TRestGeant4SpatialGeneratorCRY::Reader {
    std::mutex mutex;
    std::unique_ptr<TFile> file;
    TTree* tree = nullptr;
    int64_t entries = 0;
    int64_t next = 0;
    double timeSimulated = 0;  // us
    double subBoxLength = 0;   // mm

    // Branches, allocated by ROOT
    vector<string>* particle = nullptr;
    vector<int>* charge = nullptr;
    vector<double>* energy = nullptr;
    vector<double>* time = nullptr;
    vector<double>* positionX = nullptr;
    vector<double>* positionY = nullptr;
    vector<double>* positionZ = nullptr;
    vector<double>* directionX = nullptr;
    vector<double>* directionY = nullptr;
    vector<double>* directionZ = nullptr;

    ~Reader() {
        file.reset();  // and its tree, before the branch objects
        delete particle;
        delete charge;
        delete energy;
        delete time;
        delete positionX;
        delete positionY;
        delete positionZ;
        delete directionX;
        delete directionY;
        delete directionZ;
    }
};

TRestGeant4SpatialGeneratorCRY::TRestGeant4SpatialGeneratorCRY(const string& name, const YAML::Node& node)
    : TRestGeant4SpatialGenerator(name, node) {
    LoadConfig();
}

void TRestGeant4SpatialGeneratorCRY::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4SpatialGeneratorCRY>(fNode);
    if (fFilename.empty()) throw runtime_error(GetClassName() + ": 'filename' (restCRY showers) is needed");
    if (ToVector(fVerticalAxis).R() == 0) throw runtime_error(GetClassName() + ": 'verticalAxis' is zero");
    UpdateYAMLFromParams<TRestGeant4SpatialGeneratorCRY>(fNode);
}

XYZVector TRestGeant4SpatialGeneratorCRY::Sample(TRandom&) {
    throw runtime_error(GetClassName() + ": the positions are the ones of the showers");
}

XYZVector TRestGeant4SpatialGeneratorCRY::GetCenter() const { return ToVector(fPosition); }

void TRestGeant4SpatialGeneratorCRY::Open() {
    if (fReader) return;
    // TFile::Open and the metadata reading change the current directory (e.g. the restG4 output file)
    TDirectory::TContext context;
    auto reader = make_shared<Reader>();
    const string fileName = TRestTools::GetFullPath(fFilename);
    reader->file.reset(TFile::Open(fileName.c_str(), "READ"));
    if (!reader->file || reader->file->IsZombie()) {
        throw runtime_error(GetClassName() + ": cannot open the showers file '" + fFilename + "'");
    }
    const auto showers = TRestGeant4CRYShowers::ReadFromFile(reader->file.get());
    reader->timeSimulated = showers.fTimeSimulated;
    reader->subBoxLength = showers.fSubBoxLength;
    reader->file->GetObject(TRestGeant4CRYShowers::kTreeName, reader->tree);
    if (!reader->tree) {
        throw runtime_error(GetClassName() + ": no tree '" + TRestGeant4CRYShowers::kTreeName + "' in " +
                            fFilename);
    }
    reader->entries = reader->tree->GetEntries();
    if (reader->entries == 0) throw runtime_error(GetClassName() + ": no showers in " + fFilename);

    const auto setAddress = [&](const char* branch, auto& address) {
        if (!reader->tree->GetBranch(branch)) {
            throw runtime_error(GetClassName() + ": no branch '" + branch + "' in " + fFilename);
        }
        reader->tree->SetBranchAddress(branch, &address);
    };
    setAddress("particle", reader->particle);
    setAddress("charge", reader->charge);
    setAddress("energy", reader->energy);
    setAddress("time", reader->time);
    setAddress("positionX", reader->positionX);
    setAddress("positionY", reader->positionY);
    setAddress("positionZ", reader->positionZ);
    setAddress("directionX", reader->directionX);
    setAddress("directionY", reader->directionY);
    setAddress("directionZ", reader->directionZ);
    fReader = reader;
}

bool TRestGeant4SpatialGeneratorCRY::NextShower(vector<TRestGeant4ParticleState>& particles) {
    if (!fReader) throw runtime_error(GetClassName() + ": the showers file is not open");
    particles.clear();

    const XYZVector center = GetCenter();

    lock_guard<mutex> lock(fReader->mutex);
    if (fReader->next >= fReader->entries) return false;
    fReader->tree->GetEntry(fReader->next++);
    const Reader& r = *fReader;
    for (size_t i = 0; i < r.particle->size(); i++) {
        TRestGeant4ParticleState particle;
        particle.SetParticleName((*r.particle)[i]);
        particle.SetParticleCharge((*r.charge)[i]);
        particle.SetEnergy((*r.energy)[i]);
        particle.SetTime((*r.time)[i]);
        particle.SetOrigin(RotateToWorld({(*r.positionX)[i], (*r.positionY)[i], (*r.positionZ)[i]}) + center);
        particle.SetDirection(RotateToWorld({(*r.directionX)[i], (*r.directionY)[i], (*r.directionZ)[i]}));
        particles.push_back(particle);
    }
    return true;
}

XYZVector TRestGeant4SpatialGeneratorCRY::RotateToWorld(const XYZVector& v) const {
    const XYZVector up = ToVector(fVerticalAxis).Unit();
    const XYZVector z(0, 0, 1);
    const XYZVector axis = z.Cross(up);
    const double angle = acos(clamp(z.Dot(up), -1.0, 1.0));
    const ROOT::Math::AxisAngle rotation(axis.R() > 0 ? axis.Unit() : XYZVector(1, 0, 0), angle);
    return rotation * v;
}

int64_t TRestGeant4SpatialGeneratorCRY::GetShowersInFile() const {
    if (!fReader) throw runtime_error(GetClassName() + ": the showers file is not open");
    return fReader->entries;
}

vector<XYZVector> TRestGeant4SpatialGeneratorCRY::GetPlaneCorners() const {
    if (!fReader) throw runtime_error(GetClassName() + ": the showers file is not open");
    const double half = fReader->subBoxLength / 2;
    vector<XYZVector> corners;
    for (const double x : {-half, half}) {
        for (const double y : {-half, half}) corners.push_back(RotateToWorld({x, y, 0}) + GetCenter());
    }
    return corners;
}

void TRestGeant4SpatialGeneratorCRY::UpdateExposure() {
    if (!fReader) return;
    int64_t used = 0;
    {
        lock_guard<mutex> lock(fReader->mutex);
        used = fReader->next;
    }
    SetExposure(used, fReader->timeSimulated * used / fReader->entries);
}

void TRestGeant4SpatialGeneratorCRY::SetExposure(int64_t showers, double exposureTime) {
    fShowersUsed = showers;
    fExposureTime = exposureTime;
    UpdateYAMLFromParams<TRestGeant4SpatialGeneratorCRY>(fNode);
}
