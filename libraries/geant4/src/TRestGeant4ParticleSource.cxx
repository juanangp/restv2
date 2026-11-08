#include "TRestGeant4ParticleSource.h"

#include <TMath.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <stdexcept>
#include <vector>

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

/// Reads a Decay0 file, in the format of bxdecay0 ("@nevents=", "@event_start" blocks) or in the old one of
/// Decay0 (header "First event and full number of events:", then for each event its number, time and number
/// of particles, and for each particle its Geant3 ID, momentum (MeV/c) and time), as legacy
vector<vector<TRestGeant4ParticleState>> ReadDecay0(const string& fileName) {
    const string error = "TRestGeant4ParticleSource: Decay0 file '" + fileName + "': ";
    ifstream input(TRestTools::GetFullPath(fileName));
    if (!input.is_open()) throw runtime_error(error + "cannot be opened");

    // Geant3 particle IDs: mass (MeV), name and charge
    struct Particle {
        double mass;
        string name;
        int charge;
    };
    const map<int, Particle> particles = {{1, {0, "gamma", 0}},        {2, {0.511, "e+", 1}},
                                          {3, {0.511, "e-", -1}},      {5, {105.658, "mu+", 1}},
                                          {6, {105.658, "mu-", -1}},   {14, {938.272, "proton", 1}},
                                          {47, {3727.379, "alpha", 2}}};
    auto readParticle = [&](int id, double px, double py, double pz) {
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
        return state;
    };

    // The header: the number of events of either format
    int nEvents = 0;
    bool oldFormat = false;
    string line;
    for (int i = 0; i < 30 && getline(input, line); ++i) {
        const auto position = line.find("@nevents=");
        if (position != string::npos) {
            nEvents = stoi(line.substr(position + 9));
            break;
        }
        if (line.find("First event and full number of events:") != string::npos) {
            int firstEvent = 0;
            input >> firstEvent >> nEvents;
            oldFormat = true;
            break;
        }
    }
    if (nEvents <= 0) {
        throw runtime_error(error +
                            "no '@nevents=' (bxdecay0) nor 'First event and full number of events:' "
                            "(old Decay0) in its header");
    }

    vector<vector<TRestGeant4ParticleState>> events;
    for (int event = 0; event < nEvents; ++event) {
        int nParticles = 0;
        if (oldFormat) {
            int eventNumber = 0;
            double time = 0;
            input >> eventNumber >> time >> nParticles;
        } else {
            while (getline(input, line) && line.find("@event_start") == string::npos) {
            }
            getline(input, line);
            input >> nParticles;
        }
        if (!input) throw runtime_error(error + "less events than given in its header");
        vector<TRestGeant4ParticleState> eventParticles;
        for (int i = 0; i < nParticles; ++i) {
            int id = 0;
            double time = 0, px = 0, py = 0, pz = 0;
            if (oldFormat) {
                input >> id >> px >> py >> pz >> time;
            } else {
                string name;
                input >> id >> time >> px >> py >> pz >> name;
            }
            eventParticles.push_back(readParticle(id, px, py, pz));
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
    copy.fJointCumulative = fJointCumulative;  // Read only: shared
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

void TRestGeant4ParticleSource::BuildJointCumulative() {
    const int nx = fJointFunction->GetNpx(), ny = fJointFunction->GetNpy();
    const double xMin = fJointFunction->GetXmin(), yMin = fJointFunction->GetYmin();
    const double dx = (fJointFunction->GetXmax() - xMin) / nx, dy = (fJointFunction->GetYmax() - yMin) / ny;
    // Integral of each cell with 2 x 2 Gauss-Legendre points (exact for cubic polynomials)
    const double node = 0.5 / std::sqrt(3.0);
    auto cumulative = make_shared<vector<double>>(static_cast<size_t>(nx) * ny + 1, 0.0);
    size_t cell = 0;
    for (int j = 0; j < ny; j++) {
        const double yCenter = yMin + (j + 0.5) * dy;
        for (int i = 0; i < nx; i++) {
            const double xCenter = xMin + (i + 0.5) * dx;
            double integral = 0;
            for (const double u : {-node, node})
                for (const double v : {-node, node})
                    integral += fJointFunction->Eval(xCenter + u * dx, yCenter + v * dy);
            // Negative values taken as positive, as TF2::GetRandom2
            (*cumulative)[cell + 1] = (*cumulative)[cell] + std::abs(integral) * 0.25 * dx * dy;
            cell++;
        }
    }
    const double total = cumulative->back();
    if (!(total > 0)) throw runtime_error(GetClassName() + ": the integral of the formula is zero");
    for (auto& value : *cumulative) value /= total;
    fJointCumulative = cumulative;
}

void TRestGeant4ParticleSource::SampleJointFunction(TRandom& random, double& x, double& y) const {
    const auto& cumulative = *fJointCumulative;
    const int nx = fJointFunction->GetNpx();
    const double xMin = fJointFunction->GetXmin(), yMin = fJointFunction->GetYmin();
    const double dx = (fJointFunction->GetXmax() - xMin) / nx;
    const double dy = (fJointFunction->GetYmax() - yMin) / fJointFunction->GetNpy();
    const double r = random.Rndm();
    // The cell of r, as TF2::GetRandom2: x linear in the cell with r, y uniform
    size_t cell = std::upper_bound(cumulative.begin(), cumulative.end(), r) - cumulative.begin();
    cell = std::min(std::max<size_t>(cell, 1), cumulative.size() - 1) - 1;
    const double width = cumulative[cell + 1] - cumulative[cell];
    const double xInCell = width > 0 ? (r - cumulative[cell]) / width : 0.5;
    x = xMin + dx * (cell % nx + xInCell);
    y = yMin + dy * (cell / nx + random.Rndm());
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
        if (!fJointCumulative) BuildJointCumulative();
        fJointEnergyUnit = 1;
        fJointAngleUnit = 1;
        return;
    }

    // As legacy, the histograms have the energy in MeV and the angle in deg
    fJointHistogram = ReadHistogram2D(fJointFile, fJointName);  // Any 2D histogram, as a TH2D
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
        auto particles = (*fDecay0Events)[random.Integer(static_cast<unsigned int>(fDecay0Events->size()))];
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
            SampleJointFunction(random, x, angle);
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
