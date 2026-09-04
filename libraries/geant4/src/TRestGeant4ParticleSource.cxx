#include "TRestGeant4ParticleSource.h"

#include <TClass.h>

#include <algorithm>
#include <cmath>
#include <iostream>

#include "TRestGeant4Metadata.h"
#include "TRestTools.h"

using namespace std;
using namespace TRestGeant4PrimaryGeneratorTypes;

namespace {
const bool kParticleSourceRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestGeant4ParticleSource", [](const std::string& name, const YAML::Node& node) {
            return std::make_unique<TRestGeant4ParticleSource>(name, node);
        });
    return true;
}();

const bool kAngularDistributionRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestGeant4ParticleAngularDistribution",
        [](const std::string& name, const YAML::Node& node) {
            return std::make_unique<TRestGeant4ParticleAngularDistribution>(name, node);
        });
    return true;
}();

const bool kEnergyDistributionRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestGeant4ParticleEnergyDistribution",
        [](const std::string& name, const YAML::Node& node) {
            return std::make_unique<TRestGeant4ParticleEnergyDistribution>(name, node);
        });
    return true;
}();
}  // namespace

static const bool TRestGeant4ParticleAngularDistribution_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestGeant4ParticleAngularDistribution>("type", &TRestGeant4ParticleAngularDistribution::fType);
    reg.RegisterField<TRestGeant4ParticleAngularDistribution>("formula", &TRestGeant4ParticleAngularDistribution::fFormula);
    reg.RegisterField<TRestGeant4ParticleAngularDistribution>("filename", &TRestGeant4ParticleAngularDistribution::fFilename);
    reg.RegisterField<TRestGeant4ParticleAngularDistribution>("nameInFile", &TRestGeant4ParticleAngularDistribution::fNameInFile);
    reg.RegisterField<TRestGeant4ParticleAngularDistribution>("formulaNPoints", &TRestGeant4ParticleAngularDistribution::fFormulaNPoints);
    reg.RegisterField<TRestGeant4ParticleAngularDistribution>("range", &TRestGeant4ParticleAngularDistribution::fRange);
    reg.RegisterField<TRestGeant4ParticleAngularDistribution>("isotropicConeHalfAngle", &TRestGeant4ParticleAngularDistribution::fIsotropicConeHalfAngle);
    reg.RegisterField<TRestGeant4ParticleAngularDistribution>("direction", &TRestGeant4ParticleAngularDistribution::fDirection);
    return true;
}();

static const bool TRestGeant4ParticleEnergyDistribution_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestGeant4ParticleEnergyDistribution>("type", &TRestGeant4ParticleEnergyDistribution::fType);
    reg.RegisterField<TRestGeant4ParticleEnergyDistribution>("formula", &TRestGeant4ParticleEnergyDistribution::fFormula);
    reg.RegisterField<TRestGeant4ParticleEnergyDistribution>("filename", &TRestGeant4ParticleEnergyDistribution::fFilename);
    reg.RegisterField<TRestGeant4ParticleEnergyDistribution>("nameInFile", &TRestGeant4ParticleEnergyDistribution::fNameInFile);
    reg.RegisterField<TRestGeant4ParticleEnergyDistribution>("formulaNPoints", &TRestGeant4ParticleEnergyDistribution::fFormulaNPoints);
    reg.RegisterField<TRestGeant4ParticleEnergyDistribution>("range", &TRestGeant4ParticleEnergyDistribution::fRange);
    reg.RegisterField<TRestGeant4ParticleEnergyDistribution>("energy", &TRestGeant4ParticleEnergyDistribution::fEnergy);
    return true;
}();

static const bool TRestGeant4ParticleSource_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();

    reg.RegisterNestedField("angular", &TRestGeant4ParticleSource::fAngularDistribution);
    reg.RegisterNestedField("energy", &TRestGeant4ParticleSource::fEnergyDistribution);
    reg.RegisterField<TRestGeant4ParticleSource>("use", &TRestGeant4ParticleSource::fGenFilename);
    reg.RegisterField<TRestGeant4ParticleSource>("particle", &TRestGeant4ParticleSource::fParticleName);

    return true;
}();

TRestGeant4ParticleAngularDistribution::TRestGeant4ParticleAngularDistribution() : TRestMetadata() {
    fName = "TRestGeant4ParticleAngularDistribution";
}

TRestGeant4ParticleAngularDistribution::TRestGeant4ParticleAngularDistribution(
    const std::string& name, const YAML::Node& node) : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestGeant4ParticleAngularDistribution::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4ParticleAngularDistribution>(fNode);
    UpdateYAMLFromParams<TRestGeant4ParticleAngularDistribution>(fNode);
}

TRestGeant4ParticleEnergyDistribution::TRestGeant4ParticleEnergyDistribution() : TRestMetadata() {
    fName = "TRestGeant4ParticleEnergyDistribution";
}

TRestGeant4ParticleEnergyDistribution::TRestGeant4ParticleEnergyDistribution(
    const std::string& name, const YAML::Node& node) : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestGeant4ParticleEnergyDistribution::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4ParticleEnergyDistribution>(fNode);
    UpdateYAMLFromParams<TRestGeant4ParticleEnergyDistribution>(fNode);
}

/// \brief Default constructor.
TRestGeant4ParticleSource::TRestGeant4ParticleSource() : TRestMetadata() {
    fName = "TRestGeant4ParticleSource";
}

/// \brief Modern YAML node constructor.
TRestGeant4ParticleSource::TRestGeant4ParticleSource(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestGeant4ParticleSource::TRestGeant4ParticleSource(const TRestGeant4ParticleSource& other)
    : TRestMetadata(other),
      fDecayRandomMethod(other.fDecayRandomMethod),
      fAngularDistribution(other.fAngularDistribution),
      fEnergyDistribution(other.fEnergyDistribution),
      fAngularDistributionFunction(other.fAngularDistributionFunction ? new TF1(*other.fAngularDistributionFunction)
                                                                       : nullptr),
      fEnergyDistributionFunction(other.fEnergyDistributionFunction ? new TF1(*other.fEnergyDistributionFunction)
                                                                     : nullptr),
      fEnergyAndAngularDistributionFunction(other.fEnergyAndAngularDistributionFunction
                                                ? new TF2(*other.fEnergyAndAngularDistributionFunction)
                                                : nullptr),
       fCosmicHistogramsTransformed(other.fCosmicHistogramsTransformed
                                          ? new TH2D(*other.fCosmicHistogramsTransformed) 
                                          : nullptr),
      fGenFilename(other.fGenFilename),
      fParticleName(other.fParticleName),
      fParticleExcitationLevel(other.fParticleExcitationLevel),
      fParticleCharge(other.fParticleCharge),
      fParticleOrigin(other.fParticleOrigin) {}

TRestGeant4ParticleSource& TRestGeant4ParticleSource::operator=(const TRestGeant4ParticleSource& other) {
    if (this == &other) return *this;
    TRestGeant4ParticleSource copy(other);
    *this = std::move(copy);
    return *this;
}

TRestGeant4ParticleSource::TRestGeant4ParticleSource(TRestGeant4ParticleSource&& other) noexcept
    : TRestMetadata(std::move(other)),
      fDecayRandomMethod(other.fDecayRandomMethod),
      fAngularDistribution(std::move(other.fAngularDistribution)),
      fEnergyDistribution(std::move(other.fEnergyDistribution)),
      fAngularDistributionFunction(other.fAngularDistributionFunction),
      fEnergyDistributionFunction(other.fEnergyDistributionFunction),
      fEnergyAndAngularDistributionFunction(other.fEnergyAndAngularDistributionFunction),
      fCosmicHistogramsTransformed(other.fCosmicHistogramsTransformed),
      fGenFilename(std::move(other.fGenFilename)),
      fParticleName(std::move(other.fParticleName)),
      fParticleExcitationLevel(other.fParticleExcitationLevel),
      fParticleCharge(other.fParticleCharge),
      fParticleOrigin(other.fParticleOrigin) {
      other.fAngularDistributionFunction = nullptr;
      other.fEnergyDistributionFunction = nullptr;
      other.fEnergyAndAngularDistributionFunction = nullptr;
      other.fDecayRandomMethod = nullptr;
      other.fCosmicHistogramsTransformed = nullptr;
      other.InitializeDistributions();
}

/// \brief Operador de asignación por movimiento (Move Assignment Operator).
TRestGeant4ParticleSource& TRestGeant4ParticleSource::operator=(TRestGeant4ParticleSource&& other) noexcept {
    if (this == &other) return *this;

    delete fAngularDistributionFunction;
    delete fEnergyDistributionFunction;
    delete fEnergyAndAngularDistributionFunction;
    delete fCosmicHistogramsTransformed;

    TRestMetadata::operator=(std::move(other));
    fAngularDistribution = std::move(other.fAngularDistribution);
    fEnergyDistribution = std::move(other.fEnergyDistribution);
    fAngularDistributionFunction = other.fAngularDistributionFunction;
    fEnergyDistributionFunction = other.fEnergyDistributionFunction;
    fEnergyAndAngularDistributionFunction = other.fEnergyAndAngularDistributionFunction;
    fCosmicHistogramsTransformed = other.fCosmicHistogramsTransformed;
    fGenFilename = std::move(other.fGenFilename);
    fParticleName = std::move(other.fParticleName);
    fParticleExcitationLevel = other.fParticleExcitationLevel;
    fParticleCharge = other.fParticleCharge;
    fParticleOrigin = other.fParticleOrigin;
    fDecayRandomMethod = other.fDecayRandomMethod;
    other.fAngularDistributionFunction = nullptr;
    other.fEnergyDistributionFunction = nullptr;
    other.fEnergyAndAngularDistributionFunction = nullptr;
    other.fCosmicHistogramsTransformed=nullptr;
    other.fDecayRandomMethod = nullptr;
    other.InitializeDistributions();

    return *this;
}

TRestGeant4ParticleSource::~TRestGeant4ParticleSource() {
    delete fAngularDistributionFunction;
    delete fEnergyDistributionFunction;
    delete fEnergyAndAngularDistributionFunction;
    delete fCosmicHistogramsTransformed;
}

/// \brief Modern framework configuration pipeline loader.
void TRestGeant4ParticleSource::LoadConfig() {
    delete fAngularDistributionFunction;
    delete fEnergyDistributionFunction;
    delete fEnergyAndAngularDistributionFunction;
    delete fCosmicHistogramsTransformed;
    fAngularDistributionFunction = nullptr;
    fEnergyDistributionFunction = nullptr;
    fEnergyAndAngularDistributionFunction = nullptr;
    fCosmicHistogramsTransformed = nullptr;

    UpdateParamsFromYAML<TRestGeant4ParticleSource>(fNode);
    ReadYAMLVerbose(fNode);
    UpdateYAMLFromParams<TRestGeant4ParticleSource>(fNode);
    InitializeDistributions();
}

void TRestGeant4ParticleSource::InitializeDistributions(){

  if (StringToAngularDistributionTypes(fAngularDistribution.fType )  ==
      TRestGeant4PrimaryGeneratorTypes::AngularDistributionTypes::FORMULA) {
        SetAngularDistributionFormula(fAngularDistribution.fFormula);
        // We cannot use an angular range bigger than the range of the formula
        const auto function = GetAngularDistributionFunction();
        if (GetAngularDistributionRangeMin() < function->GetXaxis()->GetXmin()) {
            SetAngularDistributionRange({function->GetXaxis()->GetXmin(), GetAngularDistributionRangeMax()});
        }
        if (GetAngularDistributionRangeMax() > function->GetXaxis()->GetXmax()) {
            SetAngularDistributionRange({GetAngularDistributionRangeMin(), function->GetXaxis()->GetXmax()});
        }
  }

  if (StringToEnergyDistributionTypes(fEnergyDistribution.fType )  ==
      TRestGeant4PrimaryGeneratorTypes::EnergyDistributionTypes::FORMULA) {
        SetEnergyDistributionFormula(fEnergyDistribution.fFormula);
        // We cannot use an energy range bigger than the range of the formula
        const auto function = GetEnergyDistributionFunction();
        if (GetEnergyDistributionRangeMin() < function->GetXaxis()->GetXmin()) {
            SetEnergyDistributionRange({function->GetXaxis()->GetXmin(), GetEnergyDistributionRangeMax()});
        }
        if (GetEnergyDistributionRangeMax() > function->GetXaxis()->GetXmax()) {
            SetEnergyDistributionRange({GetEnergyDistributionRangeMin(), function->GetXaxis()->GetXmax()});
        }
  }

  if (StringToAngularDistributionTypes(fAngularDistribution.fType )  ==
      AngularDistributionTypes::FORMULA2 && 
      StringToEnergyDistributionTypes(fEnergyDistribution.fType )  ==
      EnergyDistributionTypes::FORMULA2) {
        if(fAngularDistribution.fFormula.empty() && fEnergyDistribution.fFormula.empty()){
          RESTError << "No name specified for energy and angular distribution" << RESTendl;
          exit(1);
        }
        std::string formula;
        if(!fAngularDistribution.fFormula.empty() && fEnergyDistribution.fFormula.empty()){
          formula = fAngularDistribution.fFormula;
        } else if (fAngularDistribution.fFormula.empty() &&!fEnergyDistribution.fFormula.empty()){
          formula = fEnergyDistribution.fFormula;
        } else if (fAngularDistribution.fFormula.empty() != fEnergyDistribution.fFormula){
          RESTError << "When using 'formula2' the name of energy and angular dist must match" << RESTendl;
          exit(1);
        } else {
          formula = fAngularDistribution.fFormula;
        }
        
        SetEnergyAndAngularDistributionFormula(formula);

        const auto function = GetEnergyAndAngularDistributionFunction();
        // We cannot use an energy range bigger than the range of the formula
        if (GetEnergyDistributionRangeMin() < function->GetXaxis()->GetXmin()) {
            SetEnergyDistributionRange({function->GetXaxis()->GetXmin(), GetEnergyDistributionRangeMax()});
        }
        if (GetEnergyDistributionRangeMax() > function->GetXaxis()->GetXmax()) {
            SetEnergyDistributionRange({GetEnergyDistributionRangeMin(), function->GetXaxis()->GetXmax()});
        }
        // We cannot use an angular range bigger than the range of the formula
        if (GetAngularDistributionRangeMin() < function->GetYaxis()->GetXmin()) {
            SetAngularDistributionRange({function->GetYaxis()->GetXmin(), GetAngularDistributionRangeMax()});
        }
        if (GetAngularDistributionRangeMax() > function->GetYaxis()->GetXmax()) {
            SetAngularDistributionRange({GetAngularDistributionRangeMin(), function->GetYaxis()->GetXmax()});
        }
  }

}


void TRestGeant4ParticleSource::InitializeCosmics() {

    std::string rootFilename = fAngularDistribution.fFilename;
    if (rootFilename.empty()) {
        rootFilename = fEnergyDistribution.fFilename;
    }

    if (rootFilename.empty()) {
        throw std::runtime_error("TRestGeant4ParticleSource::InitializeCosmics - No cosmic root filename found in YAML config.");
    }

    std::string particleKey = fEnergyDistribution.fNameInFile;
    if (particleKey.empty()) particleKey = fAngularDistribution.fNameInFile;
    
    auto keyIt = cosmicParticleNames.find(particleKey);
    if (keyIt == cosmicParticleNames.end()) {
        throw std::runtime_error("TRestGeant4ParticleSource::InitializeCosmics - Particle key '" + particleKey + "' not found in cosmicParticleNames translation map.");
    }

    fParticleName = keyIt->second;

    TFile* file = TFile::Open(rootFilename.c_str(), "READ");
    if (!file || file->IsZombie()) {
        if (file) file->Close();
        throw std::runtime_error("TRestGeant4ParticleSource::InitializeCosmics - File '" + rootFilename + "' not found or corrupted.");
    }

    std::string histogramName = particleKey + "_energy_zenith";
    auto histogramFromFile = file->Get<TH2D>(histogramName.c_str());
    if (!histogramFromFile) {
        file->Close();
        delete file;
        throw std::runtime_error("TRestGeant4ParticleSource::InitializeCosmics - Histogram '" + histogramName + "' not found in file '" + rootFilename + "'.");
    }

    if (fCosmicHistogramsTransformed != nullptr) {
        delete fCosmicHistogramsTransformed;
    }
    
    fCosmicHistogramsTransformed = new TH2D(*histogramFromFile);
    fCosmicHistogramsTransformed->SetDirectory(nullptr);

    for (int i = 1; i <= fCosmicHistogramsTransformed->GetNbinsX(); i++) {
        for (int j = 1; j <= fCosmicHistogramsTransformed->GetNbinsY(); j++) {
            const double zenith = fCosmicHistogramsTransformed->GetYaxis()->GetBinCenter(j);
            
            double cosZenith = TMath::Cos(zenith * TMath::DegToRad());
            if (std::abs(cosZenith) > 1e-6) {
                const double value = fCosmicHistogramsTransformed->GetBinContent(i, j) / cosZenith;
                fCosmicHistogramsTransformed->SetBinContent(i, j, value);
            }
        }
    }

    file->Close();
    delete file;

    RESTInfo << "TRestGeant4ParticleSource::InitializeCosmics - Successfully loaded and transformed cosmic spectrum for particle: " 
             << fParticleName << " from file: " << rootFilename << RESTendl;
}
