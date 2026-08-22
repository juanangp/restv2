#pragma once

class TRandom;

#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

// ROOT MathCore and Core dependencies
#include <Math/Vector3D.h>
#include <TF1.h>
#include <TF2.h>
#include <TMath.h>

#include "TRestGeant4PrimaryGeneratorTypes.h"
#include "TRestMetadata.h"

class TRestGeant4ParticleAngularDistribution : public TRestMetadata {
   public:
    std::string fType = "Flux";
    std::string fFilename="";
    std::string fNameInFile="";
    size_t fFormulaNPoints = 500;
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, TMath::Pi()};
    TRestWithUnits fIsotropicConeHalfAngle = 0.0;
    std::array<double, 3> fDirection = {1.0, 0.0, 0.0};

    TRestGeant4ParticleAngularDistribution();
    TRestGeant4ParticleAngularDistribution(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestGeant4ParticleAngularDistribution"; }
};

class TRestGeant4ParticleEnergyDistribution : public TRestMetadata {
   public:
    std::string fType = "Mono";
    std::string fFilename="";
    std::string fNameInFile="";
    size_t fFormulaNPoints = 5000;
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, 1.0E20};
    TRestWithUnits fEnergy = 0.0;

    TRestGeant4ParticleEnergyDistribution();
    TRestGeant4ParticleEnergyDistribution(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestGeant4ParticleEnergyDistribution"; }
};

/// \class TRestGeant4ParticleSource
/// \brief Configures and generates primary particles with energy/angular phase-space models.
class TRestGeant4ParticleSource : public TRestMetadata {

   public:
    TRandom* fDecayRandomMethod = nullptr;  //! External RNG for decay template selection
    inline void SetRandomMethod(TRandom* random) { fDecayRandomMethod = random; }
    inline TRandom* GetRandomMethod() const { return fDecayRandomMethod; }

    TRestGeant4ParticleAngularDistribution fAngularDistribution;
    TRestGeant4ParticleEnergyDistribution  fEnergyDistribution;
    // --- Angular Distribution Kinematics ---
    TF1* fAngularDistributionFunction = nullptr;
    TF1* fEnergyDistributionFunction = nullptr;
    // --- Combined Dimensional Distributions ---
    TF2* fEnergyAndAngularDistributionFunction = nullptr;
    std::string fGenFilename="";
    std::string fParticleName="";
    double fParticleExcitationLevel = 0.0;
    int fParticleCharge = 0;
    ROOT::Math::XYZVector fParticleOrigin = {0.0, 0.0, 0.0};

    /// Generated particle state is owned by TRestGeant4PrimaryGeneratorInfo.

    /// \brief Builds a particle source with default distributions and empty particle buffers.
    TRestGeant4ParticleSource();
    /// \brief Builds and configures a particle source from a YAML metadata node.
    TRestGeant4ParticleSource(const std::string& instanceName, const YAML::Node& node);
    TRestGeant4ParticleSource(const TRestGeant4ParticleSource& other);
    TRestGeant4ParticleSource& operator=(const TRestGeant4ParticleSource& other);
    TRestGeant4ParticleSource(TRestGeant4ParticleSource&& other) noexcept;
    TRestGeant4ParticleSource& operator=(TRestGeant4ParticleSource&& other) noexcept;
    /// \brief Releases owned ROOT distribution functions and source resources.
    virtual ~TRestGeant4ParticleSource();

    // --- REST Pipeline Life Cycle Hooks ---
    /// \brief Samples/updates internal particle state according to configured distributions.
    virtual void Update() {}

    /// \brief Factory method to instantiate a concrete source model by name.
    static TRestGeant4ParticleSource* instantiate(const std::string& model = "");

    inline std::string GetParticleName() const { return fParticleName; }
    inline double GetExcitationLevel() const { return fParticleExcitationLevel; }
    inline int GetParticleCharge() const { return fParticleCharge; }
    inline ROOT::Math::XYZVector GetOrigin() const { return fParticleOrigin; }
    inline void SetParticleName(const std::string& name) { fParticleName = name; }
    inline void SetExcitationLevel(double energy) { fParticleExcitationLevel = energy < 0.0 ? 0.0 : energy; }
    inline void SetParticleCharge(int charge) { fParticleCharge = charge; }
    inline void SetOrigin(const ROOT::Math::XYZVector& pos) { fParticleOrigin = pos; }

    /// \brief Returns a sampled direction vector based on the active angular model.
    // --- Energy & Angular Getters/Setters ---
    inline std::string GetEnergyDistributionType() const { return fEnergyDistribution.fType; }
    inline std::pair<double, double> GetEnergyDistributionRange() const { return {fEnergyDistribution.fRange.first.value, fEnergyDistribution.fRange.second.value}; }
    inline double GetEnergyDistributionRangeMin() const { return fEnergyDistribution.fRange.first.value; }
    inline double GetEnergyDistributionRangeMax() const { return fEnergyDistribution.fRange.second.value; }
    inline size_t GetEnergyDistributionFormulaNPoints() const { return fEnergyDistribution.fFormulaNPoints; }
    inline std::string GetEnergyDistributionFilename() const { return fEnergyDistribution.fFilename; }
    inline std::string GetEnergyDistributionNameInFile() const { return fEnergyDistribution.fNameInFile; }
    inline const TF1* GetEnergyDistributionFunction() const { return fEnergyDistributionFunction; }

    inline std::string GetAngularDistributionType() const { return fAngularDistribution.fType; }
    inline std::pair<double, double> GetAngularDistributionRange() const { return {fAngularDistribution.fRange.first.value, fAngularDistribution.fRange.second.value}; }
    inline double GetAngularDistributionRangeMin() const { return fAngularDistribution.fRange.first.value; }
    inline double GetAngularDistributionRangeMax() const { return fAngularDistribution.fRange.second.value; }
    inline size_t GetAngularDistributionFormulaNPoints() const { return fAngularDistribution.fFormulaNPoints; }
    inline std::string GetAngularDistributionFilename() const { return fAngularDistribution.fFilename; }
    inline std::string GetAngularDistributionNameInFile() const { return fAngularDistribution.fNameInFile; }
    inline const TF1* GetAngularDistributionFunction() const { return fAngularDistributionFunction; }
    inline double GetAngularDistributionIsotropicConeHalfAngle() const {
        return fAngularDistribution.fIsotropicConeHalfAngle.value;
    }

    inline const TF2* GetEnergyAndAngularDistributionFunction() const {
        return fEnergyAndAngularDistributionFunction;
    }
    inline std::string GetGenFilename() const { return fGenFilename; }

    inline void SetAngularDistributionIsotropicConeHalfAngle(double angle) {
        if (angle < 0.0 || angle > TMath::Pi()) {
            exit(1);
        }
        fAngularDistribution.fIsotropicConeHalfAngle = angle;
    }

    inline void SetAngularDistributionType(const std::string& type) {
        fAngularDistribution.fType = TRestGeant4PrimaryGeneratorTypes::AngularDistributionTypesToString(
            TRestGeant4PrimaryGeneratorTypes::StringToAngularDistributionTypes(type));
    }

    inline void SetAngularDistributionRange(const std::pair<double, double>& range) {
        fAngularDistribution.fRange.first = range.first;
        fAngularDistribution.fRange.second = range.second;
    }

    inline void SetAngularDistributionFormula(const std::string& formula) {
        if (fAngularDistributionFunction) delete fAngularDistributionFunction;
        fAngularDistributionFunction = static_cast<TF1*>(
            TRestGeant4PrimaryGeneratorTypes::AngularDistributionFormulasToRootFormula(
                TRestGeant4PrimaryGeneratorTypes::StringToAngularDistributionFormulas(formula))
                .Clone());
    }

    inline void SetAngularDistributionFormulaNPoints(size_t nPoints) {
        fAngularDistribution.fFormulaNPoints = std::min(nPoints, (size_t)10000);
    }
    inline void SetAngularDistributionFilename(const std::string& filename) {
        fAngularDistribution.fFilename = filename;
    }
    inline void SetAngularDistributionNameInFile(const std::string& name) {
        fAngularDistribution.fNameInFile = name;
    }

    inline void SetEnergyDistributionType(const std::string& type) {
        fEnergyDistribution.fType = TRestGeant4PrimaryGeneratorTypes::EnergyDistributionTypesToString(
            TRestGeant4PrimaryGeneratorTypes::StringToEnergyDistributionTypes(type));
    }

    inline void SetEnergyDistributionRange(const std::pair<double, double>& range) {
        fEnergyDistribution.fRange.first = range.first;
        fEnergyDistribution.fRange.second = range.second;
    }
    inline void SetEnergyDistributionFormulaNPoints(size_t nPoints) {
        fEnergyDistribution.fFormulaNPoints = std::min(nPoints, (size_t)10000);
    }
    inline void SetEnergyDistributionFilename(const std::string& filename) {
        fEnergyDistribution.fFilename = filename;
    }
    inline void SetEnergyDistributionNameInFile(const std::string& name) {
        fEnergyDistribution.fNameInFile = name;
    }

    inline void SetEnergyDistributionFormula(const std::string& formula) {
        if (fEnergyDistributionFunction) delete fEnergyDistributionFunction;
        fEnergyDistributionFunction = static_cast<TF1*>(
            TRestGeant4PrimaryGeneratorTypes::EnergyDistributionFormulasToRootFormula(
                TRestGeant4PrimaryGeneratorTypes::StringToEnergyDistributionFormulas(formula))
                .Clone());
    }

    inline void SetEnergyAndAngularDistributionFormula(const std::string& formula) {
        if (fEnergyAndAngularDistributionFunction) delete fEnergyAndAngularDistributionFunction;
        fEnergyAndAngularDistributionFunction = static_cast<TF2*>(
            TRestGeant4PrimaryGeneratorTypes::EnergyAndAngularDistributionFormulasToRootFormula(
                TRestGeant4PrimaryGeneratorTypes::StringToEnergyAndAngularDistributionFormulas(formula))
                .Clone());
    }

    inline void SetGenFilename(const std::string& name) { fGenFilename = name; }
    // --- Framework and Identification Overrides ---
    std::string GetClassName() const override { return "TRestGeant4ParticleSource"; }
    /// \brief Loads source configuration from the current YAML metadata node.
    void LoadConfig() override;
    /// \brief No-op hook kept for REST metadata lifecycle compatibility.
    void Initialize() override {};
};
