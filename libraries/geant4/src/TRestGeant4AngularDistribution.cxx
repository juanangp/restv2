#include "TRestGeant4AngularDistribution.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "TRestGeant4PrimaryGeneratorTypes.h"
#include "TRestTools.h"

using namespace std;
using namespace TRestGeant4PrimaryGeneratorTypes;
using ROOT::Math::XYZVector;
using TRestTools::ToLower;

static const bool TRestGeant4AngularDistribution_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestGeant4AngularDistribution>("type", &TRestGeant4AngularDistribution::fType);
    reg.RegisterField<TRestGeant4AngularDistribution>("direction",
                                                      &TRestGeant4AngularDistribution::fDirection);

    reg.RegisterField<TRestGeant4AngularDistributionIsotropic>(
        "isotropicConeHalfAngle", &TRestGeant4AngularDistributionIsotropic::fIsotropicConeHalfAngle);

    using Formula = TRestGeant4AngularDistributionFormula;
    reg.RegisterField<Formula>("formula", &Formula::fFormula);
    reg.RegisterField<Formula>("range", &Formula::fRange);
    reg.RegisterField<Formula>("formulaNPoints", &Formula::fFormulaNPoints);

    using Histogram = TRestGeant4AngularDistributionTH1D;
    reg.RegisterField<Histogram>("filename", &Histogram::fFilename);
    reg.RegisterField<Histogram>("nameInFile", &Histogram::fNameInFile);
    reg.RegisterField<Histogram>("range", &Histogram::fRange);

    using Joint = TRestGeant4AngularDistributionJoint;
    reg.RegisterField<Joint>("formula", &Joint::fFormula);
    reg.RegisterField<Joint>("filename", &Joint::fFilename);
    reg.RegisterField<Joint>("nameInFile", &Joint::fNameInFile);
    reg.RegisterField<Joint>("range", &Joint::fRange);
    reg.RegisterField<Joint>("formulaNPoints", &Joint::fFormulaNPoints);
    return true;
}();

namespace {

/// An angle range within [0, pi] with min < max
void CheckAngleRange(const pair<TRestWithUnits, TRestWithUnits>& range, const string& className) {
    if (range.first < 0 || range.first >= range.second || range.second > TMath::Pi() + 1e-9) {
        throw runtime_error(className + ": 'range' must be [min, max] within [0, 180 deg] with min < max");
    }
}

}  // namespace

// --- TRestGeant4AngularDistribution ---

unique_ptr<TRestGeant4AngularDistribution> TRestGeant4AngularDistribution::Create(const string& name,
                                                                                  const YAML::Node& node) {
    if (!node || !node["type"]) {
        throw runtime_error("TRestGeant4AngularDistribution: the angular 'type' is missing");
    }
    const auto type = ToLower(node["type"].as<string>());
    if (type == "flux") return make_unique<TRestGeant4AngularDistributionFlux>(name, node);
    if (type == "isotropic") return make_unique<TRestGeant4AngularDistributionIsotropic>(name, node);
    if (type == "formula") return make_unique<TRestGeant4AngularDistributionFormula>(name, node);
    if (type == "th1d") return make_unique<TRestGeant4AngularDistributionTH1D>(name, node);
    if (type == "formula2" || type == "th2d" || type == "cosmic") {
        return make_unique<TRestGeant4AngularDistributionJoint>(name, node);
    }
    throw runtime_error("TRestGeant4AngularDistribution: unknown angular type '" + type +
                        "', it must be flux, isotropic, formula, TH1D, formula2, TH2D or cosmic");
}

XYZVector TRestGeant4AngularDistribution::GetDirection() const {
    return XYZVector(fDirection[0], fDirection[1], fDirection[2]).Unit();
}

void TRestGeant4AngularDistribution::CheckDirection() {
    if (XYZVector(fDirection[0], fDirection[1], fDirection[2]).R() == 0) {
        throw runtime_error(GetClassName() + ": 'direction' must not be zero");
    }
}

// --- TRestGeant4AngularDistributionFlux ---

TRestGeant4AngularDistributionFlux::TRestGeant4AngularDistributionFlux(const string& name,
                                                                       const YAML::Node& node)
    : TRestGeant4AngularDistribution(name, node) {
    LoadConfig();
}

void TRestGeant4AngularDistributionFlux::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4AngularDistributionFlux>(fNode);
    CheckDirection();
    UpdateYAMLFromParams<TRestGeant4AngularDistributionFlux>(fNode);
}

// --- TRestGeant4AngularDistributionIsotropic ---

TRestGeant4AngularDistributionIsotropic::TRestGeant4AngularDistributionIsotropic(const string& name,
                                                                                 const YAML::Node& node)
    : TRestGeant4AngularDistribution(name, node) {
    LoadConfig();
}

void TRestGeant4AngularDistributionIsotropic::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4AngularDistributionIsotropic>(fNode);
    if (fIsotropicConeHalfAngle < 0 || fIsotropicConeHalfAngle > TMath::Pi() + 1e-9) {
        throw runtime_error(GetClassName() + ": 'isotropicConeHalfAngle' must be within [0, 180 deg]");
    }
    if (fIsotropicConeHalfAngle > 0) CheckDirection();
    UpdateYAMLFromParams<TRestGeant4AngularDistributionIsotropic>(fNode);
}

XYZVector TRestGeant4AngularDistributionIsotropic::Sample(TRandom& random, const XYZVector& axis) {
    if (fIsotropicConeHalfAngle <= 0) return IsotropicDirection(random);
    // Uniform in the solid angle of the cone
    const double cosTheta = random.Uniform(cos(static_cast<double>(fIsotropicConeHalfAngle)), 1);
    return DirectionFromAxis(random, axis, acos(cosTheta));
}

// --- TRestGeant4AngularDistributionFormula ---

TRestGeant4AngularDistributionFormula::TRestGeant4AngularDistributionFormula(const string& name,
                                                                             const YAML::Node& node)
    : TRestGeant4AngularDistribution(name, node) {
    LoadConfig();
}

void TRestGeant4AngularDistributionFormula::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4AngularDistributionFormula>(fNode);
    CheckDirection();
    CheckAngleRange(fRange, GetClassName());
    // An unknown formula name throws
    StringToAngularDistributionFormulas(fFormula);
    UpdateYAMLFromParams<TRestGeant4AngularDistributionFormula>(fNode);
}

unique_ptr<TRestGeant4AngularDistribution> TRestGeant4AngularDistributionFormula::Clone() const {
    auto copy = make_unique<TRestGeant4AngularDistributionFormula>(*this);
    copy->fFunction.reset();
    return copy;
}

void TRestGeant4AngularDistributionFormula::LoadDistribution() {
    fFunction = make_shared<TF1>(
        AngularDistributionFormulasToRootFormula(StringToAngularDistributionFormulas(fFormula)));
    // The range cannot be larger than the formula one
    const double min = std::max<double>(fRange.first, fFunction->GetXmin());
    const double max = std::min<double>(fRange.second, fFunction->GetXmax());
    if (min >= max) throw runtime_error(GetClassName() + ": 'range' is outside the formula one");
    fFunction->SetRange(min, max);
    fFunction->SetNpx(fFormulaNPoints);
}

XYZVector TRestGeant4AngularDistributionFormula::Sample(TRandom& random, const XYZVector& axis) {
    if (!fFunction) throw runtime_error(GetClassName() + ": the distribution is not loaded");
    return DirectionFromAxis(random, axis, fFunction->GetRandom(&random));
}

// --- TRestGeant4AngularDistributionTH1D ---

TRestGeant4AngularDistributionTH1D::TRestGeant4AngularDistributionTH1D(const string& name,
                                                                       const YAML::Node& node)
    : TRestGeant4AngularDistribution(name, node) {
    LoadConfig();
}

void TRestGeant4AngularDistributionTH1D::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4AngularDistributionTH1D>(fNode);
    if (fFilename.empty() || fNameInFile.empty()) {
        throw runtime_error(GetClassName() + ": 'filename' and 'nameInFile' are needed");
    }
    CheckDirection();
    CheckAngleRange(fRange, GetClassName());
    UpdateYAMLFromParams<TRestGeant4AngularDistributionTH1D>(fNode);
}

unique_ptr<TRestGeant4AngularDistribution> TRestGeant4AngularDistributionTH1D::Clone() const {
    auto copy = make_unique<TRestGeant4AngularDistributionTH1D>(*this);
    copy->fHistogram.reset();
    return copy;
}

void TRestGeant4AngularDistributionTH1D::LoadDistribution() {
    auto histogram = ReadHistogram(fFilename, fNameInFile);
    fHistogram.reset(dynamic_cast<TH1D*>(histogram.release()));
    if (!fHistogram) throw runtime_error(GetClassName() + ": '" + fNameInFile + "' is not a TH1D");
    for (int i = 1; i <= fHistogram->GetNbinsX(); i++) {
        const double angle = fHistogram->GetBinCenter(i);
        if (angle < fRange.first || angle > fRange.second) fHistogram->SetBinContent(i, 0);
    }
    if (fHistogram->Integral() <= 0) {
        throw runtime_error(GetClassName() + ": the histogram is empty in 'range'");
    }
    fHistogram->ComputeIntegral();
}

XYZVector TRestGeant4AngularDistributionTH1D::Sample(TRandom& random, const XYZVector& axis) {
    if (!fHistogram) throw runtime_error(GetClassName() + ": the distribution is not loaded");
    return DirectionFromAxis(random, axis, fHistogram->GetRandom(&random));
}

// --- TRestGeant4AngularDistributionJoint ---

TRestGeant4AngularDistributionJoint::TRestGeant4AngularDistributionJoint(const string& name,
                                                                         const YAML::Node& node)
    : TRestGeant4AngularDistribution(name, node) {
    LoadConfig();
}

void TRestGeant4AngularDistributionJoint::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4AngularDistributionJoint>(fNode);
    fType = ToLower(fType);
    CheckDirection();
    CheckAngleRange(fRange, GetClassName());
    UpdateYAMLFromParams<TRestGeant4AngularDistributionJoint>(fNode);
}

XYZVector TRestGeant4AngularDistributionJoint::Sample(TRandom&, const XYZVector&) {
    throw runtime_error(GetClassName() + ": the angle is sampled with the energy (ParticleSource)");
}
