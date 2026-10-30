#include "TRestGeant4SpatialGenerator.h"

#include <Math/AxisAngle.h>

#include <stdexcept>

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
    throw runtime_error("TRestGeant4SpatialGenerator: unknown generator type '" + type +
                        "', it must be point, volume, surface or cosmic");
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
