#include "TRestGeant4PrimaryGeneratorInfo.h"

#include <stdexcept>

using namespace std;

static const bool TRestGeant4PrimaryGeneratorInfo_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestGeant4PrimaryGeneratorInfo>(
        "spatialDensityFunction", &TRestGeant4PrimaryGeneratorInfo::fSpatialDensityFunction);
    return true;
}();

TRestGeant4PrimaryGeneratorInfo::TRestGeant4PrimaryGeneratorInfo(const string& name, const YAML::Node& node)
    : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestGeant4PrimaryGeneratorInfo::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4PrimaryGeneratorInfo>(fNode);

    // The spatial generator reads its fields (type, from, shape...) from the generator section itself
    fSpatialGenerator = TRestGeant4SpatialGenerator::Create("generator", fNode);

    // The sources are created here, not as a registered field, so their errors are not ignored; they
    // keep a handle to their section, so what they write is stored with the generator
    fSources.clear();
    const YAML::Node& node = fNode;
    if (!node["source"]) throw runtime_error(GetClassName() + ": no 'source' in the generator");
    YAML::Node sources = fNode["source"];
    if (sources.IsSequence()) {
        for (auto source : sources) fSources.emplace_back("source", source);
    } else {
        fSources.emplace_back("source", sources);
    }
    if (fSources.empty()) throw runtime_error(GetClassName() + ": no 'source' in the generator");

    if (!fSpatialDensityFunction.empty() &&
        !dynamic_cast<const TRestGeant4SpatialGeneratorShape*>(fSpatialGenerator.get())) {
        throw runtime_error(GetClassName() +
                            ": 'spatialDensityFunction' needs a volume or surface generator");
    }
    if (IsCosmic() && (fSources.size() != 1 || fSources[0].IsDecay0())) {
        throw runtime_error(GetClassName() +
                            ": the cosmic generator needs a single source, not a Decay0 one");
    }

    UpdateYAMLFromParams<TRestGeant4PrimaryGeneratorInfo>(fNode);
}

void TRestGeant4PrimaryGeneratorInfo::LoadDistributions() {
    for (auto& source : fSources) source.LoadDistributions();
}

bool TRestGeant4PrimaryGeneratorInfo::HasDecay0Sources() const {
    for (const auto& source : fSources) {
        if (source.IsDecay0()) return true;
    }
    return false;
}

bool TRestGeant4PrimaryGeneratorInfo::IsCosmic() const {
    return dynamic_cast<const TRestGeant4SpatialGeneratorCosmic*>(fSpatialGenerator.get()) != nullptr;
}

bool TRestGeant4PrimaryGeneratorInfo::IsGdml() const {
    return fSpatialGenerator && fSpatialGenerator->IsGdml();
}

bool TRestGeant4PrimaryGeneratorInfo::IsGdmlSurface() const {
    return IsGdml() && dynamic_cast<const TRestGeant4SpatialGeneratorSurface*>(fSpatialGenerator.get());
}

string TRestGeant4PrimaryGeneratorInfo::GetGdmlVolume() const {
    const auto* shape = dynamic_cast<const TRestGeant4SpatialGeneratorShape*>(fSpatialGenerator.get());
    return shape ? shape->fFrom : "";
}

void TRestGeant4PrimaryGeneratorInfo::SetGdmlPlacement(const ROOT::Math::XYZVector& center,
                                                      const ROOT::Math::XYZVector& axis, double angle) {
    if (auto* shape = dynamic_cast<TRestGeant4SpatialGeneratorShape*>(fSpatialGenerator.get())) {
        shape->SetGdmlPlacement(center, axis, angle);
    }
}

void TRestGeant4PrimaryGeneratorInfo::SetCosmicRadius(double radius) {
    if (auto* cosmic = dynamic_cast<TRestGeant4SpatialGeneratorCosmic*>(fSpatialGenerator.get())) {
        cosmic->SetDefaultRadius(radius);
    }
}

double TRestGeant4PrimaryGeneratorInfo::GetCosmicSurfaceCm2() const {
    const auto* cosmic = dynamic_cast<const TRestGeant4SpatialGeneratorCosmic*>(fSpatialGenerator.get());
    return cosmic ? cosmic->GetSurfaceCm2() : 0;
}

void TRestGeant4PrimaryGeneratorInfo::Print() const {
    if (!fSpatialGenerator) return;
    RESTMetadata << "Generator: " << fSpatialGenerator->GetClassName() << RESTendl;
    if (IsGdml()) RESTMetadata << "Generator volume: " << GetGdmlVolume() << RESTendl;
    if (IsCosmic()) {
        RESTMetadata << "Cosmic generation surface: " << GetCosmicSurfaceCm2() << " cm2" << RESTendl;
    }
    if (!fSpatialDensityFunction.empty()) {
        RESTMetadata << "Density: " << fSpatialDensityFunction << RESTendl;
    }
    for (const auto& source : fSources) {
        if (source.IsDecay0()) {
            RESTMetadata << "Source: Decay0 file " << source.fUse << RESTendl;
        } else {
            RESTMetadata << "Source: " << source.fParticleName << ", "
                         << source.GetEnergyDistribution()->GetClassName() << ", "
                         << source.GetAngularDistribution()->GetClassName() << RESTendl;
        }
    }
}
