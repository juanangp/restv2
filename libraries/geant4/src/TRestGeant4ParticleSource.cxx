#include "TRestGeant4ParticleSource.h"

#include <TMath.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <stdexcept>

#include "TRestGeant4PrimaryGeneratorTypes.h"
#include "TRestTools.h"

using namespace std;
using namespace TRestGeant4PrimaryGeneratorTypes;
using ROOT::Math::XYZVector;

static const bool TRestGeant4ParticleSource_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using Source = TRestGeant4ParticleSource;
    reg.RegisterField<Source>("particle", &Source::fParticleName);
    reg.RegisterField<Source>("excitedLevel", &Source::fExcitedLevel);
    reg.RegisterField<Source>("charge", &Source::fCharge);
    reg.RegisterField<Source>("use", &Source::fUse);
    reg.RegisterFactoryField<Source>("energy", &Source::fEnergy, &TRestGeant4EnergyDistribution::Create);
    reg.RegisterFactoryField<Source>("angular", &Source::fAngular, &TRestGeant4AngularDistribution::Create);
    return true;
}();

namespace {

/// As legacy, the particle of the cosmic histograms <key>_energy_zenith
const map<string, string> kCosmicParticles = {
    {"neutron", "neutron"},
    {"proton", "proton"},
    {"gamma", "gamma"},
    {"electron_minus", "e-"},
    {"electron_plus", "e+"},
    {"muon_minus", "mu-"},
    {"muon_plus", "mu+"},
    {"neutron_below_1MeV", "neutron"},
    {"neutron_above_10GeV", "neutron"},
    {"neutron_between_1MeV_and_10GeV", "neutron"},
};

/// The value given in the energy or in the angular section (an error if both differ)
string Pick(const string& energyValue, const string& angularValue, const string& key) {
    if (!energyValue.empty() && !angularValue.empty() && energyValue != angularValue) {
        throw runtime_error("TRestGeant4ParticleSource: '" + key + "' differs in energy and angular");
    }
    return energyValue.empty() ? angularValue : energyValue;
}

vector<vector<TRestGeant4ParticleState>> ReadDecay0(const string& fileName) {
    const string error = "TRestGeant4ParticleSource: Decay0 file '" + fileName + "': ";
    ifstream input(TRestTools::GetFullPath(fileName));
    if (!input.is_open()) throw runtime_error(error + "cannot be opened");

    int nEvents = 0;
    string line;
    for (int i = 0; i < 24 && getline(input, line); ++i) {
        const auto position = line.find("@nevents=");
        if (position != string::npos) nEvents = stoi(line.substr(position + 9));
    }
    if (nEvents <= 0) throw runtime_error(error + "no '@nevents='");

    // Geant3 particle IDs: mass (MeV), name and charge
    struct Particle {
        double mass;
        string name;
        int charge;
    };
    const map<int, Particle> particles = {
        {1, {0, "gamma", 0}}, {2, {0.511, "e+", 1}}, {3, {0.511, "e-", -1}}, {47, {3727.379, "alpha", 2}}};

    vector<vector<TRestGeant4ParticleState>> events;
    for (int event = 0; event < nEvents; ++event) {
        while (getline(input, line) && line.find("@event_start") == string::npos) {
        }
        getline(input, line);
        int nParticles = 0;
        input >> nParticles;
        if (!input) throw runtime_error(error + "less events than '@nevents='");
        vector<TRestGeant4ParticleState> eventParticles;
        for (int i = 0; i < nParticles; ++i) {
            int id = 0;
            double time = 0, px = 0, py = 0, pz = 0;
            string name;
            input >> id >> time >> px >> py >> pz >> name;
            const auto particle = particles.find(id);
            if (particle == particles.end()) {
                throw runtime_error(error + "particle ID " + to_string(id) + " not supported");
            }
            const double mass = particle->second.mass;
            TRestGeant4ParticleState state;
            state.SetParticleName(particle->second.name);
            state.SetParticleCharge(particle->second.charge);
            state.SetEnergy(1000.0 * (sqrt(px * px + py * py + pz * pz + mass * mass) - mass));
            state.SetDirection(XYZVector(px, py, pz));
            eventParticles.push_back(state);
        }
        events.push_back(std::move(eventParticles));
    }
    return events;
}

}  // namespace

TRestGeant4ParticleSource::TRestGeant4ParticleSource(const string& name, const YAML::Node& node)
    : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestGeant4ParticleSource::LoadConfig() {
    // The distributions keep a handle to their section, so what they write is stored with the source
    fEnergy.reset();
    fAngular.reset();
    UpdateParamsFromYAML<TRestGeant4ParticleSource>(fNode);

    if (IsDecay0()) {
        if (!fParticleName.empty() || fEnergy || fAngular) {
            throw runtime_error(GetClassName() +
                                ": a Decay0 source ('use') gives the particles, it cannot have 'particle', "
                                "'energy' or 'angular'");
        }
    } else {
        if (!fEnergy || !fAngular) {
            throw runtime_error(GetClassName() + ": 'energy' and 'angular' are needed");
        }
        if (fEnergy->IsJoint() || fAngular->IsJoint()) CheckJoint();
        if (fParticleName.empty()) throw runtime_error(GetClassName() + ": 'particle' is needed");
    }

    UpdateYAMLFromParams<TRestGeant4ParticleSource>(fNode);
}

void TRestGeant4ParticleSource::CheckJoint() {
    const auto* energy = dynamic_cast<const TRestGeant4EnergyDistributionJoint*>(fEnergy.get());
    const auto* angular = dynamic_cast<const TRestGeant4AngularDistributionJoint*>(fAngular.get());
    if (!energy || !angular || energy->fType != angular->fType) {
        throw runtime_error(GetClassName() +
                            ": formula2, TH2D and cosmic must be the type of both energy and angular");
    }
    fJointType = energy->fType;

    if (fJointType == "formula2") {
        fJointFormula = Pick(energy->fFormula, angular->fFormula, "formula");
        // An unknown formula name throws
        StringToEnergyAndAngularDistributionFormulas(fJointFormula);
        return;
    }

    fJointFile = Pick(energy->fFilename, angular->fFilename, "filename");
    fJointName = Pick(energy->fNameInFile, angular->fNameInFile, "nameInFile");
    if (fJointFile.empty() || fJointName.empty()) {
        throw runtime_error(GetClassName() + ": 'filename' and 'nameInFile' are needed for " + fJointType);
    }
    if (fJointType == "cosmic") {
        // As legacy, nameInFile is the particle key, the histogram <key>_energy_zenith
        const auto particle = kCosmicParticles.find(fJointName);
        if (particle == kCosmicParticles.end()) {
            throw runtime_error(GetClassName() + ": unknown cosmic particle key '" + fJointName + "'");
        }
        if (fParticleName.empty()) fParticleName = particle->second;
        if (fParticleName != particle->second) {
            throw runtime_error(GetClassName() + ": the particle of the cosmic key '" + fJointName + "' is " +
                                particle->second + ", not " + fParticleName);
        }
        fJointName += "_energy_zenith";
    }
}

TRestGeant4ParticleSource TRestGeant4ParticleSource::Clone() const {
    TRestGeant4ParticleSource copy(GetName(), YAML::Clone(fNode));
    copy.fDecay0Events = fDecay0Events;
    return copy;
}

void TRestGeant4ParticleSource::LoadDistributions() {
    if (IsDecay0()) {
        if (!fDecay0Events) {
            fDecay0Events = make_shared<const vector<vector<TRestGeant4ParticleState>>>(ReadDecay0(fUse));
        }
        return;
    }
    if (!fJointType.empty()) {
        LoadJointDistribution();
        return;
    }
    fEnergy->LoadDistribution();
    fAngular->LoadDistribution();
}

void TRestGeant4ParticleSource::LoadJointDistribution() {
    const auto* energy = static_cast<const TRestGeant4EnergyDistributionJoint*>(fEnergy.get());
    const auto& energyRange = energy->fRange;
    const auto* angular = static_cast<const TRestGeant4AngularDistributionJoint*>(fAngular.get());
    const auto& angleRange = angular->fRange;

    if (fJointType == "formula2") {
        // Energy in keV and angle in rad
        fJointFunction = make_unique<TF2>(EnergyAndAngularDistributionFormulasToRootFormula(
            StringToEnergyAndAngularDistributionFormulas(fJointFormula)));
        const double energyMin = std::max<double>(energyRange.first, fJointFunction->GetXmin());
        const double energyMax = std::min<double>(energyRange.second, fJointFunction->GetXmax());
        const double angleMin = std::max<double>(angleRange.first, fJointFunction->GetYmin());
        const double angleMax = std::min<double>(angleRange.second, fJointFunction->GetYmax());
        if (energyMin >= energyMax || angleMin >= angleMax) {
            throw runtime_error(GetClassName() + ": 'range' is outside the formula one");
        }
        fJointFunction->SetRange(energyMin, angleMin, energyMax, angleMax);
        fJointFunction->SetNpx(energy->fFormulaNPoints);
        fJointFunction->SetNpy(angular->fFormulaNPoints);
        fJointEnergyUnit = 1;
        fJointAngleUnit = 1;
        return;
    }

    // As legacy, the histograms have the energy in MeV and the angle in deg
    auto histogram = ReadHistogram(fJointFile, fJointName);
    fJointHistogram.reset(dynamic_cast<TH2D*>(histogram.release()));
    if (!fJointHistogram) throw runtime_error(GetClassName() + ": '" + fJointName + "' is not a TH2D");
    fJointEnergyUnit = 1000;
    fJointAngleUnit = TMath::DegToRad();
    for (int i = 1; i <= fJointHistogram->GetNbinsX(); i++) {
        const double energy = fJointHistogram->GetXaxis()->GetBinCenter(i) * fJointEnergyUnit;
        for (int j = 1; j <= fJointHistogram->GetNbinsY(); j++) {
            const double angle = fJointHistogram->GetYaxis()->GetBinCenter(j) * fJointAngleUnit;
            double content = fJointHistogram->GetBinContent(i, j);
            if (energy < energyRange.first || energy > energyRange.second || angle < angleRange.first ||
                angle > angleRange.second) {
                content = 0;
            } else if (fJointType == "cosmic" && std::abs(std::cos(angle)) > 1e-6) {
                // As legacy, the flux through a horizontal surface to the one through the disk normal to the
                // direction of the cosmic generator
                content /= std::abs(std::cos(angle));
            }
            fJointHistogram->SetBinContent(i, j, content);
        }
    }
    if (fJointHistogram->Integral() <= 0) {
        throw runtime_error(GetClassName() + ": the histogram is empty in 'range'");
    }
    fJointHistogram->ComputeIntegral();
}

vector<TRestGeant4ParticleState> TRestGeant4ParticleSource::Generate(TRandom& random, const XYZVector& origin,
                                                                     const XYZVector& center, bool inward) {
    if (IsDecay0()) {
        if (!fDecay0Events) throw runtime_error(GetClassName() + ": the Decay0 file is not loaded");
        auto particles = (*fDecay0Events)[random.Integer(static_cast<UInt_t>(fDecay0Events->size()))];
        for (auto& particle : particles) particle.SetOrigin(origin);
        return particles;
    }

    XYZVector axis = fAngular->GetDirection();
    if (inward && (center - origin).R() > 0) axis = (center - origin).Unit();

    double energy = 0;
    XYZVector direction;
    if (!fJointType.empty()) {
        double x = 0, angle = 0;
        if (fJointFunction) {
            fJointFunction->GetRandom2(x, angle, &random);
        } else if (fJointHistogram) {
            fJointHistogram->GetRandom2(x, angle, &random);
        } else {
            throw runtime_error(GetClassName() + ": the distribution is not loaded");
        }
        energy = x * fJointEnergyUnit;
        direction = DirectionFromAxis(random, axis, angle * fJointAngleUnit);
    } else {
        energy = fEnergy->Sample(random);
        direction = fAngular->Sample(random, axis);
    }

    TRestGeant4ParticleState particle;
    particle.SetParticleName(fParticleName);
    particle.SetExcitationLevel(fExcitedLevel);
    particle.SetParticleCharge(fCharge);
    particle.SetEnergy(energy);
    particle.SetDirection(direction);
    particle.SetOrigin(origin);
    return {particle};
}
