#include "TRestGeant4EnergyDistribution.h"

#include <TString.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "TRestGeant4PrimaryGeneratorTypes.h"
#include "TRestTools.h"

using namespace std;
using namespace TRestGeant4PrimaryGeneratorTypes;
using TRestTools::ToLower;

static const bool TRestGeant4EnergyDistribution_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestGeant4EnergyDistribution>("type", &TRestGeant4EnergyDistribution::fType);

    using Mono = TRestGeant4EnergyDistributionMono;
    reg.RegisterField<Mono>("energy", &Mono::fEnergy);
    reg.RegisterField<TRestGeant4EnergyDistributionFlat>("range", &TRestGeant4EnergyDistributionFlat::fRange);
    reg.RegisterField<TRestGeant4EnergyDistributionLog>("range", &TRestGeant4EnergyDistributionLog::fRange);

    using Formula = TRestGeant4EnergyDistributionFormula;
    reg.RegisterField<Formula>("formula", &Formula::fFormula);
    reg.RegisterField<Formula>("range", &Formula::fRange);
    reg.RegisterField<Formula>("formulaNPoints", &Formula::fFormulaNPoints);

    using Histogram = TRestGeant4EnergyDistributionTH1D;
    reg.RegisterField<Histogram>("filename", &Histogram::fFilename);
    reg.RegisterField<Histogram>("nameInFile", &Histogram::fNameInFile);
    reg.RegisterField<Histogram>("range", &Histogram::fRange);

    using Joint = TRestGeant4EnergyDistributionJoint;
    reg.RegisterField<Joint>("formula", &Joint::fFormula);
    reg.RegisterField<Joint>("filename", &Joint::fFilename);
    reg.RegisterField<Joint>("nameInFile", &Joint::fNameInFile);
    reg.RegisterField<Joint>("range", &Joint::fRange);
    reg.RegisterField<Joint>("formulaNPoints", &Joint::fFormulaNPoints);
    return true;
}();

namespace {

/// A range with 0 <= min < max
void CheckRange(const pair<TRestWithUnits, TRestWithUnits>& range, const string& className) {
    if (range.first < 0 || range.first >= range.second) {
        throw runtime_error(className + ": 'range' must be [min, max] with 0 <= min < max");
    }
}

}  // namespace

// --- TRestGeant4EnergyDistribution ---

unique_ptr<TRestGeant4EnergyDistribution> TRestGeant4EnergyDistribution::Create(const string& name,
                                                                                const YAML::Node& node) {
    if (!node || !node["type"]) {
        throw runtime_error("TRestGeant4EnergyDistribution: the energy 'type' is missing");
    }
    const auto type = ToLower(node["type"].as<string>());
    if (type == "mono") return make_unique<TRestGeant4EnergyDistributionMono>(name, node);
    if (type == "flat") return make_unique<TRestGeant4EnergyDistributionFlat>(name, node);
    if (type == "log") return make_unique<TRestGeant4EnergyDistributionLog>(name, node);
    if (type == "formula") return make_unique<TRestGeant4EnergyDistributionFormula>(name, node);
    if (type == "th1d") return make_unique<TRestGeant4EnergyDistributionTH1D>(name, node);
    if (type == "formula2" || type == "th2d" || type == "cosmic") {
        return make_unique<TRestGeant4EnergyDistributionJoint>(name, node);
    }
    throw runtime_error("TRestGeant4EnergyDistribution: unknown energy type '" + type +
                        "', it must be mono, flat, log, formula, TH1D, formula2, TH2D or cosmic");
}

// --- TRestGeant4EnergyDistributionMono ---

TRestGeant4EnergyDistributionMono::TRestGeant4EnergyDistributionMono(const string& name,
                                                                     const YAML::Node& node)
    : TRestGeant4EnergyDistribution(name, node) {
    LoadConfig();
}

void TRestGeant4EnergyDistributionMono::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4EnergyDistributionMono>(fNode);
    if (fEnergy <= 0) throw runtime_error(GetClassName() + ": 'energy' must be positive");
    UpdateYAMLFromParams<TRestGeant4EnergyDistributionMono>(fNode);
}

// --- TRestGeant4EnergyDistributionFlat ---

TRestGeant4EnergyDistributionFlat::TRestGeant4EnergyDistributionFlat(const string& name,
                                                                     const YAML::Node& node)
    : TRestGeant4EnergyDistribution(name, node) {
    LoadConfig();
}

void TRestGeant4EnergyDistributionFlat::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4EnergyDistributionFlat>(fNode);
    CheckRange(fRange, GetClassName());
    UpdateYAMLFromParams<TRestGeant4EnergyDistributionFlat>(fNode);
}

double TRestGeant4EnergyDistributionFlat::Sample(TRandom& random) {
    return random.Uniform(fRange.first, fRange.second);
}

// --- TRestGeant4EnergyDistributionLog ---

TRestGeant4EnergyDistributionLog::TRestGeant4EnergyDistributionLog(const string& name, const YAML::Node& node)
    : TRestGeant4EnergyDistribution(name, node) {
    LoadConfig();
}

void TRestGeant4EnergyDistributionLog::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4EnergyDistributionLog>(fNode);
    CheckRange(fRange, GetClassName());
    if (fRange.first <= 0) throw runtime_error(GetClassName() + ": the 'range' minimum must be positive");
    UpdateYAMLFromParams<TRestGeant4EnergyDistributionLog>(fNode);
}

double TRestGeant4EnergyDistributionLog::Sample(TRandom& random) {
    return exp(random.Uniform(log(fRange.first), log(fRange.second)));
}

// --- TRestGeant4EnergyDistributionFormula ---

TRestGeant4EnergyDistributionFormula::TRestGeant4EnergyDistributionFormula(const string& name,
                                                                           const YAML::Node& node)
    : TRestGeant4EnergyDistribution(name, node) {
    LoadConfig();
}

void TRestGeant4EnergyDistributionFormula::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4EnergyDistributionFormula>(fNode);
    CheckRange(fRange, GetClassName());
    // An unknown formula name throws
    StringToEnergyDistributionFormulas(fFormula);
    UpdateYAMLFromParams<TRestGeant4EnergyDistributionFormula>(fNode);
}

unique_ptr<TRestGeant4EnergyDistribution> TRestGeant4EnergyDistributionFormula::Clone() const {
    auto copy = make_unique<TRestGeant4EnergyDistributionFormula>(*this);
    copy->fFunction.reset();
    return copy;
}

void TRestGeant4EnergyDistributionFormula::LoadDistribution() {
    fFunction = make_shared<TF1>(
        EnergyDistributionFormulasToRootFormula(StringToEnergyDistributionFormulas(fFormula)));
    // The range cannot be larger than the formula one
    const double min = std::max<double>(fRange.first, fFunction->GetXmin());
    const double max = std::min<double>(fRange.second, fFunction->GetXmax());
    if (min >= max) throw runtime_error(GetClassName() + ": 'range' is outside the formula one");
    fFunction->SetRange(min, max);
    fFunction->SetNpx(fFormulaNPoints);
}

double TRestGeant4EnergyDistributionFormula::Sample(TRandom& random) {
    if (!fFunction) throw runtime_error(GetClassName() + ": the distribution is not loaded");
    return fFunction->GetRandom(&random);
}

// --- TRestGeant4EnergyDistributionTH1D ---

TRestGeant4EnergyDistributionTH1D::TRestGeant4EnergyDistributionTH1D(const string& name,
                                                                     const YAML::Node& node)
    : TRestGeant4EnergyDistribution(name, node) {
    LoadConfig();
}

void TRestGeant4EnergyDistributionTH1D::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4EnergyDistributionTH1D>(fNode);
    if (fFilename.empty() || fNameInFile.empty()) {
        throw runtime_error(GetClassName() + ": 'filename' and 'nameInFile' are needed");
    }
    CheckRange(fRange, GetClassName());
    UpdateYAMLFromParams<TRestGeant4EnergyDistributionTH1D>(fNode);
}

unique_ptr<TRestGeant4EnergyDistribution> TRestGeant4EnergyDistributionTH1D::Clone() const {
    auto copy = make_unique<TRestGeant4EnergyDistributionTH1D>(*this);
    copy->fHistogram.reset();
    return copy;
}

void TRestGeant4EnergyDistributionTH1D::LoadDistribution() {
    auto histogram = ReadHistogram(fFilename, fNameInFile);
    fHistogram.reset(dynamic_cast<TH1D*>(histogram.release()));
    if (!fHistogram) throw runtime_error(GetClassName() + ": '" + fNameInFile + "' is not a TH1D");

    // As legacy, the unit from the axis title
    const TString title = fHistogram->GetXaxis()->GetTitle();
    fUnit = title.Contains("GeV") ? 1e6 : title.Contains("MeV") ? 1e3 : 1;

    for (int i = 1; i <= fHistogram->GetNbinsX(); i++) {
        const double energy = fHistogram->GetBinCenter(i) * fUnit;
        if (energy < fRange.first || energy > fRange.second) fHistogram->SetBinContent(i, 0);
    }
    if (fHistogram->Integral() <= 0) {
        throw runtime_error(GetClassName() + ": the histogram is empty in 'range'");
    }
    fHistogram->ComputeIntegral();
}

double TRestGeant4EnergyDistributionTH1D::Sample(TRandom& random) {
    if (!fHistogram) throw runtime_error(GetClassName() + ": the distribution is not loaded");
    return fHistogram->GetRandom(&random) * fUnit;
}

// --- TRestGeant4EnergyDistributionJoint ---

TRestGeant4EnergyDistributionJoint::TRestGeant4EnergyDistributionJoint(const string& name,
                                                                       const YAML::Node& node)
    : TRestGeant4EnergyDistribution(name, node) {
    LoadConfig();
}

void TRestGeant4EnergyDistributionJoint::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4EnergyDistributionJoint>(fNode);
    fType = ToLower(fType);
    CheckRange(fRange, GetClassName());
    UpdateYAMLFromParams<TRestGeant4EnergyDistributionJoint>(fNode);
}

double TRestGeant4EnergyDistributionJoint::Sample(TRandom&) {
    throw runtime_error(GetClassName() + ": the energy is sampled with the angle (ParticleSource)");
}
