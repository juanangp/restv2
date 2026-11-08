#include "TRestGeant4BiasingVolume.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "TRestTools.h"

// Modern REST v3 field registry reflection hook
static const bool TRestGeant4BiasingVolume_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestGeant4BiasingVolume>("position", &TRestGeant4BiasingVolume::fVolumePosition);
    reg.RegisterField<TRestGeant4BiasingVolume>("size", &TRestGeant4BiasingVolume::fVolumeSize);
    reg.RegisterField<TRestGeant4BiasingVolume>("biasingFactor", &TRestGeant4BiasingVolume::fBiasingFactor);
    reg.RegisterField<TRestGeant4BiasingVolume>("energyRange", &TRestGeant4BiasingVolume::fEnergyRange);
    reg.RegisterField<TRestGeant4BiasingVolume>("type", &TRestGeant4BiasingVolume::fVolumeType);
    return true;
}();

TRestGeant4BiasingVolume::TRestGeant4BiasingVolume() : TRestMetadata() { fName = "TRestGeant4BiasingVolume"; }

TRestGeant4BiasingVolume::TRestGeant4BiasingVolume(const std::string& configFilename, const std::string& name)
    : TRestMetadata(configFilename, name) {
    LoadConfig();
}

TRestGeant4BiasingVolume::TRestGeant4BiasingVolume(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestGeant4BiasingVolume::~TRestGeant4BiasingVolume() = default;

void TRestGeant4BiasingVolume::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4BiasingVolume>(fNode);
    fVolumeType = TRestTools::NormalizeOption("type", fVolumeType, {"virtualBox", "virtualSphere"});
    if (!(fVolumeSize > 0)) {
        throw std::runtime_error("TRestGeant4BiasingVolume: 'size' must be > 0");
    }
    // Each particle stored is simulated biasingFactor times in the next stage of restG4
    if (!(fBiasingFactor >= 1) || fBiasingFactor != std::floor(fBiasingFactor) ||
        fBiasingFactor > std::numeric_limits<int>::max()) {
        throw std::runtime_error("TRestGeant4BiasingVolume: 'biasingFactor' must be an integer >= 1");
    }
    if (GetMinEnergy() > GetMaxEnergy()) {
        throw std::runtime_error("TRestGeant4BiasingVolume: 'energyRange' minimum is above its maximum");
    }
    UpdateYAMLFromParams<TRestGeant4BiasingVolume>(fNode);
}

double TRestGeant4BiasingVolume::GetEntryFraction(const ROOT::Math::XYZVector& from,
                                                  const ROOT::Math::XYZVector& to) const {
    if (isInside(from.X(), from.Y(), from.Z())) return -1;

    const ROOT::Math::XYZVector p = from - GetBiasingVolumePosition();
    const ROOT::Math::XYZVector d = to - from;

    if (fVolumeType == "virtualBox") {
        // Slab method: the segment enters the box at the largest entry of the three slabs
        const double half = fVolumeSize / 2.0;
        const double origin[3] = {p.X(), p.Y(), p.Z()};
        const double direction[3] = {d.X(), d.Y(), d.Z()};
        double tEnter = -std::numeric_limits<double>::infinity();
        double tExit = std::numeric_limits<double>::infinity();
        for (int i = 0; i < 3; i++) {
            if (direction[i] == 0) {
                if (origin[i] <= -half || origin[i] >= half) return -1;  // parallel, outside the slab
                continue;
            }
            double t1 = (-half - origin[i]) / direction[i];
            double t2 = (half - origin[i]) / direction[i];
            if (t1 > t2) std::swap(t1, t2);
            tEnter = std::max(tEnter, t1);
            tExit = std::min(tExit, t2);
        }
        if (tEnter >= tExit || tExit <= 0 || tEnter > 1) return -1;
        return std::max(tEnter, 0.0);
    } else if (fVolumeType == "virtualSphere") {
        // |p + t d|^2 = r^2, the first root is the entry (p is outside)
        const double a = d.Mag2();
        if (a == 0) return -1;
        const double b = 2 * p.Dot(d);
        const double c = p.Mag2() - fVolumeSize * fVolumeSize;
        const double discriminant = b * b - 4 * a * c;
        if (discriminant <= 0) return -1;
        const double t = (-b - std::sqrt(discriminant)) / (2 * a);
        if (t < 0 || t > 1) return -1;
        return t;
    }
    return -1;
}

bool TRestGeant4BiasingVolume::Contains(const TRestGeant4BiasingVolume& inner) const {
    const ROOT::Math::XYZVector d = inner.GetBiasingVolumePosition() - GetBiasingVolumePosition();
    const double offset[3] = {std::abs(d.X()), std::abs(d.Y()), std::abs(d.Z())};
    // Extent of the inner volume along each axis from its center (half side or radius)
    const bool innerBox = inner.fVolumeType == "virtualBox";
    const double innerExtent = innerBox ? inner.fVolumeSize / 2.0 : double(inner.fVolumeSize);

    if (fVolumeType == "virtualBox") {
        const double half = fVolumeSize / 2.0;
        for (int i = 0; i < 3; i++) {
            if (offset[i] + innerExtent >= half) return false;
        }
        return true;
    } else if (fVolumeType == "virtualSphere") {
        if (innerBox) {
            // The farthest corner of the box
            double r2 = 0;
            for (int i = 0; i < 3; i++) r2 += (offset[i] + innerExtent) * (offset[i] + innerExtent);
            return std::sqrt(r2) < fVolumeSize;
        }
        return d.R() + innerExtent < fVolumeSize;
    }
    return false;
}
