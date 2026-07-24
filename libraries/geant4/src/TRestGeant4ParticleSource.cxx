#include "TRestGeant4ParticleSource.h"
#include "TRestTools.h"
#include "TRestGeant4Metadata.h"
#include <TClass.h>
#include <iostream>
#include <algorithm>
#include <cmath>

using namespace std;
using namespace TRestGeant4PrimaryGeneratorTypes;

// Modern REST v3 field registry reflection hook for safe YAML pipeline loading
static const bool TRestGeant4ParticleSource_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    
    // Core distribution mappings
    reg.RegisterField<TRestGeant4ParticleSource>("angularDistributionType", &TRestGeant4ParticleSource::fAngularDistributionType);
    reg.RegisterField<TRestGeant4ParticleSource>("angularDistributionFilename", &TRestGeant4ParticleSource::fAngularDistributionFilename);
    reg.RegisterField<TRestGeant4ParticleSource>("angularDistributionNameInFile", &TRestGeant4ParticleSource::fAngularDistributionNameInFile);
    reg.RegisterField<TRestGeant4ParticleSource>("angularDistributionFormulaNPoints", &TRestGeant4ParticleSource::fAngularDistributionFormulaNPoints);
    reg.RegisterField<TRestGeant4ParticleSource>("angularDistributionRange", &TRestGeant4ParticleSource::fAngularDistributionRange);
    reg.RegisterField<TRestGeant4ParticleSource>("angularDistributionIsotropicConeHalfAngle", &TRestGeant4ParticleSource::fAngularDistributionIsotropicConeHalfAngle);
    
    reg.RegisterField<TRestGeant4ParticleSource>("energyDistributionType", &TRestGeant4ParticleSource::fEnergyDistributionType);
    reg.RegisterField<TRestGeant4ParticleSource>("energyDistributionFilename", &TRestGeant4ParticleSource::fEnergyDistributionFilename);
    reg.RegisterField<TRestGeant4ParticleSource>("energyDistributionNameInFile", &TRestGeant4ParticleSource::fEnergyDistributionNameInFile);
    reg.RegisterField<TRestGeant4ParticleSource>("energyDistributionFormulaNPoints", &TRestGeant4ParticleSource::fEnergyDistributionFormulaNPoints);
    reg.RegisterField<TRestGeant4ParticleSource>("energyDistributionRange", &TRestGeant4ParticleSource::fEnergyDistributionRange);
    
    reg.RegisterField<TRestGeant4ParticleSource>("genFilename", &TRestGeant4ParticleSource::fGenFilename);
    
    return true;
}();

/// \brief Default constructor.
TRestGeant4ParticleSource::TRestGeant4ParticleSource() : TRestMetadata() {
    fName = "TRestGeant4Metadata";
}

/// \brief Modern YAML node constructor.
TRestGeant4ParticleSource::TRestGeant4ParticleSource(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

/// \brief Destructor implementing clean heap deallocations.
TRestGeant4ParticleSource::~TRestGeant4ParticleSource() {
    if (fAngularDistributionFunction) delete fAngularDistributionFunction;
    if (fEnergyDistributionFunction) delete fEnergyDistributionFunction;
    if (fEnergyAndAngularDistributionFunction) delete fEnergyAndAngularDistributionFunction;
}

/// \brief Modern framework configuration pipeline loader.
void TRestGeant4ParticleSource::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4ParticleSource>(fNode);

    // Resolve decay data filenames automatically if declared in the configuration node
    if (fNode["use"]) {
        std::string modelUse = fNode["use"].as<std::string>();
        if (modelUse.find(".dat") != std::string::npos) {
            std::string fullPathName = TRestTools::GetFullPath(modelUse);
            if (fullPathName.empty()) {
                RESTError << "File not found: " << modelUse << RESTendl;
                exit(1);
            }
            modelUse = fullPathName;
        }
        SetGenFilename(modelUse);
        if (TRestTools::fileExists(fGenFilename)) {
            ReadEventDataFile(fGenFilename);
        }
    }

    ReadYAMLVerbose(fNode);
    UpdateYAMLFromParams<TRestGeant4ParticleSource>(fNode);
}

/// \brief Prints advanced kinetics metadata information to framework logs.
void TRestGeant4ParticleSource::PrintMetadata() {
    RESTMetadata << " " << RESTendl;
    if (!GetParticleName().empty() && GetParticleName() != "NO_SUCH_PARA")
        RESTMetadata << "Particle Source Name: " << GetParticleName() << RESTendl;
        
    if (!fParticlesTemplate.empty() && fGenFilename != "NO_SUCH_PARA") {
        RESTMetadata << "Generator file: " << GetGenFilename() << RESTendl;
        RESTMetadata << "Stored templates: " << fParticlesTemplate.size() << RESTendl;
        RESTMetadata << "Particles: ";
        for (const auto& particle : fParticles) RESTMetadata << particle.GetParticleName() << ", ";
        RESTMetadata << RESTendl;
    } else {
        if (GetParticleCharge() != 0) {
            RESTMetadata << "Particle charge: " << GetParticleCharge() << RESTendl;
        }
        RESTMetadata << "Angular distribution type: " << GetAngularDistributionType() << RESTendl;
        auto angType = 
        TRestGeant4PrimaryGeneratorTypes::StringToAngularDistributionTypes(GetAngularDistributionType());
        if (angType == AngularDistributionTypes::TH1D || angType == AngularDistributionTypes::TH2D) {
            RESTMetadata << "Angular distribution filename: " << TRestTools::GetFullPath(GetAngularDistributionFilename()) << RESTendl;
            RESTMetadata << "Angular distribution histogram name: " << GetAngularDistributionNameInFile() << RESTendl;
        }
        RESTMetadata << "Angular distribution direction: (" << fParticle.GetDirection().X() << "," << fParticle.GetDirection().Y() << "," << fParticle.GetDirection().Z() << ")" << RESTendl;
        
        if (angType == AngularDistributionTypes::ISOTROPIC && GetAngularDistributionIsotropicConeHalfAngle() != 0) {
            const double solidAngle = 2.0 * TMath::Pi() * (1.0 - std::cos(GetAngularDistributionIsotropicConeHalfAngle()));
            RESTMetadata << "Angular distribution isotropic cone half angle (deg): " << GetAngularDistributionIsotropicConeHalfAngle() * TMath::RadToDeg()
                         << " - solid angle: " << solidAngle << " (" << solidAngle / (4.0 * TMath::Pi()) * 100.0 << "%)" << RESTendl;
        }
        if (angType == AngularDistributionTypes::TH1D || angType == AngularDistributionTypes::TH2D) {
            RESTMetadata << "Angular distribution range (deg): (" << GetAngularDistributionRangeMin() * TMath::RadToDeg() << ", " << GetAngularDistributionRangeMax() * TMath::RadToDeg() << ")" << RESTendl;
            RESTMetadata << "Angular distribution random sampling grid size: " << GetAngularDistributionFormulaNPoints() << RESTendl;
        }
        
        RESTMetadata << "Energy distribution type: " << GetEnergyDistributionType() << RESTendl;
        auto engType = StringToEnergyDistributionTypes(GetEnergyDistributionType());
        if (engType == EnergyDistributionTypes::TH1D || engType == EnergyDistributionTypes::TH2D) {
            RESTMetadata << "Energy distribution filename: " << TRestTools::GetFullPath(GetEnergyDistributionFilename()) << RESTendl;
            RESTMetadata << "Energy distribution histogram name: " << GetEnergyDistributionNameInFile() << RESTendl;
        }
        if (GetEnergyDistributionRangeMin() == GetEnergyDistributionRangeMax()) {
            RESTMetadata << "Energy distribution energy: " << GetEnergy() << " keV" << RESTendl;
        } else {
            RESTMetadata << "Energy distribution range (keV): (" << GetEnergyDistributionRangeMin() << ", " << GetEnergyDistributionRangeMax() << ")" << RESTendl;
        }
        if (engType == EnergyDistributionTypes::FORMULA || engType == EnergyDistributionTypes::FORMULA2) {
            RESTMetadata << "Energy distribution random sampling grid size: " << GetEnergyDistributionFormulaNPoints() << RESTendl;
        }
    }
}

TRestGeant4ParticleSource* TRestGeant4ParticleSource::Clone() const {
    // Bypasses the abstract class restriction by using your factory method
    TRestGeant4ParticleSource* cloned = TRestGeant4ParticleSource::instantiate();
    if (cloned) {
        *cloned = *this; // Copies all metadata variables
    }
    return cloned;
}

/// \brief Factory instantiation logic following class string name patterns.
TRestGeant4ParticleSource* TRestGeant4ParticleSource::instantiate(const std::string& model) {
    if (model.empty() || model == "geant4" || model.find(".dat") != string::npos) {
        return new TRestGeant4ParticleSource();
    } else {
        TClass* c = TClass::GetClass(("TRestGeant4ParticleSource" + model).c_str());
        if (c) {
            return static_cast<TRestGeant4ParticleSource*>(c->New());
        } else {
            std::cout << "REST ERROR! generator wrapper \"TRestGeant4ParticleSource" << model << "\" not found!" << std::endl;
        }
    }
    return nullptr;
}

/// \brief Randomizes kinetics states utilizing historical sequence template queues.
void TRestGeant4ParticleSource::Update() {
    if (!fParticlesTemplate.empty() && fRandomMethod) {
        size_t rndCollection = static_cast<size_t>(fRandomMethod() * fParticlesTemplate.size());
        size_t pCollectionID = rndCollection % fParticlesTemplate.size();
        fParticles = fParticlesTemplate[pCollectionID];
    } else {
        TRestGeant4Particle p(fParticle);
        fParticles = {p};
    }
}

/// \brief Verifies vector normalization constraints using MathCore.
ROOT::Math::XYZVector TRestGeant4ParticleSource::GetDirection() const {
    const double magnitude = std::sqrt(fParticle.GetDirection().Mag2());
    constexpr double tolerance = 0.001;
    if (std::abs(magnitude - 1.0) > tolerance) {
        cerr << "TRestGeant4ParticleSource::GetDirection: Direction must be a unit vector!" << endl;
        exit(1);
    }
    return fParticle.GetDirection();
}

/// \brief Master orchestrator routing to the active Decay0 file parser.
void TRestGeant4ParticleSource::ReadEventDataFile(const std::string& filename) {
    ReadDecay0File(filename);
}

/// \brief Parser handling current semantic Decay0 output tables.
bool TRestGeant4ParticleSource::ReadDecay0File(const std::string& filename) {
    ifstream infile(filename);
    if (!infile.is_open()) {
        printf("Error when opening file %s\n", filename.c_str());
        return false;
    }

    int generatorEvents = 0;
    string s;
    for (int i = 0; i < 24 && std::getline(infile, s); i++) {
        size_t pos = s.find("@nevents=");
        if (pos != string::npos) {
            generatorEvents = std::stoi(s.substr(pos + 9));
        }
    }

    if (generatorEvents == 0) {
        RESTError << "TRestG4Metadata::ReadDecay0File. Problem reading generator file" << RESTendl;
        exit(1);
    }

    TRestGeant4Particle particle;
    RESTDebug << "Reading generator file: " << filename << RESTendl;
    RESTDebug << "Total number of events: " << generatorEvents << RESTendl;

    for (int n = 0; n < generatorEvents && !infile.eof(); n++) {
        size_t pos = string::npos;
        while (!infile.eof() && pos == string::npos) {
            std::getline(infile, s);
            pos = s.find("@event_start");
        }

        std::getline(infile, s); // Skip time/nuclide header row

        int nParticles = 0;
        infile >> nParticles;
        RESTDebug << "Number of particles: " << nParticles << RESTendl;

        for (int i = 0; i < nParticles && !infile.eof(); i++) {
            int pID;
            double momx, momy, momz, mass;
            double energy = -1.0, momentum2;
            string pName;
            double pTime;

            infile >> pID >> pTime >> momx >> momy >> momz >> pName;

            if (pID == 3) {
                momentum2 = (momx * momx) + (momy * momy) + (momz * momz);
                mass = 0.511;
                energy = std::sqrt(momentum2 + mass * mass) - mass;
                particle.SetParticleName("e-");
                particle.SetParticleCharge(-1);
                particle.SetExcitationLevel(0);
            } else if (pID == 1) {
                momentum2 = (momx * momx) + (momy * momy) + (momz * momz);
                energy = std::sqrt(momentum2);
                particle.SetParticleName("gamma");
                particle.SetParticleCharge(0);
                particle.SetExcitationLevel(0);
            } else {
                cout << "Particle id " << pID << " not recognized" << std::endl;
                continue;
            }

            ROOT::Math::XYZVector momDirection(momx, momy, momz);
            momDirection = momDirection.Unit();

            particle.SetEnergy(1000. * energy);
            particle.SetDirection(momDirection);
            this->AddParticle(particle);
        }
        this->FlushParticlesTemplate();
    }
    return true;
}

