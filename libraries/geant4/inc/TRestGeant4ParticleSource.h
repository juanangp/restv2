#pragma once

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

// ROOT MathCore and Core dependencies
#include <Math/Vector3D.h>
#include <TF1.h>
#include <TF2.h>
#include <TMath.h>

#include "TRestGeant4Particle.h"
#include "TRestGeant4PrimaryGeneratorInfo.h"
#include "TRestMetadata.h"

/// \class TRestGeant4ParticleSource
/// \brief Configures and generates primary particles with energy/angular phase-space models.
class TRestGeant4ParticleSource : public TRestMetadata {

   private:
    /// \brief Loads event-wise particle templates from an external event data file.
    void ReadEventDataFile(const std::string& filename);
    /// \brief Parses DECAY0 generator output and converts it into source templates.
    bool ReadDecay0File(const std::string& filename);

   public:
    // --- Angular Distribution Kinematics ---
    std::string fAngularDistributionType = "Flux";
    std::string fAngularDistributionFilename;
    std::string fAngularDistributionNameInFile;
    size_t fAngularDistributionFormulaNPoints = 500;
    TF1* fAngularDistributionFunction = nullptr;
    std::pair<double, double> fAngularDistributionRange = {0.0, TMath::Pi()};
    double fAngularDistributionIsotropicConeHalfAngle = 0.0;

    // --- Energy Distribution Kinematics ---
    std::string fEnergyDistributionType = "Mono";
    std::string fEnergyDistributionFilename;
    std::string fEnergyDistributionNameInFile;
    size_t fEnergyDistributionFormulaNPoints = 5000;
    TF1* fEnergyDistributionFunction = nullptr;
    std::pair<double, double> fEnergyDistributionRange = {0.0, 1.0E20};

    // --- Combined Dimensional Distributions ---
    TF2* fEnergyAndAngularDistributionFunction = nullptr;

    std::string fGenFilename;

    /// Stored set of generated particles
    std::vector<TRestGeant4Particle> fParticles;

    // --- Composition: Internal Particle State Encapsulation ---
    TRestGeant4Particle fParticle;

    /// Stored list of particle templates
    std::vector<std::vector<TRestGeant4Particle>> fParticlesTemplate;  //! Non-persisted template buffers

    /// Modern functional container
    std::function<double()> fRandomMethod = nullptr;  //! Non-persisted functional hook

    /// \brief Builds a particle source with default distributions and empty particle buffers.
    TRestGeant4ParticleSource();
    /// \brief Builds and configures a particle source from a YAML metadata node.
    TRestGeant4ParticleSource(const std::string& instanceName, const YAML::Node& node);
    /// \brief Releases owned ROOT distribution functions and source resources.
    virtual ~TRestGeant4ParticleSource();

    // --- REST Pipeline Life Cycle Hooks ---
    /// \brief Samples/updates internal particle state according to configured distributions.
    virtual void Update();

    /// \brief Factory method to instantiate a concrete source model by name.
    static TRestGeant4ParticleSource* instantiate(const std::string& model = "");

    // --- Exposing Inner Particle State (Maintains Interface Compatibility) ---
    inline std::string GetParticleName() const { return fParticle.GetParticleName(); }
    inline Double_t GetExcitationLevel() const { return fParticle.GetExcitationLevel(); }
    inline Double_t GetEnergy() const { return fParticle.GetEnergy(); }
    inline ROOT::Math::XYZVector GetMomentumDirection() const { return fParticle.GetMomentumDirection(); }
    inline Int_t GetParticleCharge() const { return fParticle.GetParticleCharge(); }
    inline ROOT::Math::XYZVector GetOrigin() const { return fParticle.GetOrigin(); }

    inline void SetParticle(const TRestGeant4Particle& particle) { fParticle.SetParticle(particle); }
    inline void SetParticleName(const std::string& name) { fParticle.SetParticleName(name); }
    inline void SetExcitationLevel(Double_t energy) { fParticle.SetExcitationLevel(energy); }
    inline void SetParticleCharge(Int_t charge) { fParticle.SetParticleCharge(charge); }
    inline void SetDirection(const ROOT::Math::XYZVector& dir) { fParticle.SetDirection(dir); }
    inline void SetEnergy(Double_t en) { fParticle.SetEnergy(en); }
    inline void SetOrigin(const ROOT::Math::XYZVector& pos) { fParticle.SetOrigin(pos); }

    inline TRestGeant4Particle& GetBaseParticle() { return fParticle; }
    inline const TRestGeant4Particle& GetBaseParticle() const { return fParticle; }

    /// \brief Returns a sampled direction vector based on the active angular model.
    ROOT::Math::XYZVector GetDirection() const;
    // --- Energy & Angular Getters/Setters ---
    inline std::string GetEnergyDistributionType() const { return fEnergyDistributionType; }
    inline std::pair<double, double> GetEnergyDistributionRange() const { return fEnergyDistributionRange; }
    inline double GetEnergyDistributionRangeMin() const { return fEnergyDistributionRange.first; }
    inline double GetEnergyDistributionRangeMax() const { return fEnergyDistributionRange.second; }
    inline size_t GetEnergyDistributionFormulaNPoints() const { return fEnergyDistributionFormulaNPoints; }
    inline std::string GetEnergyDistributionFilename() const { return fEnergyDistributionFilename; }
    inline std::string GetEnergyDistributionNameInFile() const { return fEnergyDistributionNameInFile; }
    inline const TF1* GetEnergyDistributionFunction() const { return fEnergyDistributionFunction; }

    inline std::string GetAngularDistributionType() const { return fAngularDistributionType; }
    inline std::pair<double, double> GetAngularDistributionRange() const { return fAngularDistributionRange; }
    inline double GetAngularDistributionRangeMin() const { return fAngularDistributionRange.first; }
    inline double GetAngularDistributionRangeMax() const { return fAngularDistributionRange.second; }
    inline size_t GetAngularDistributionFormulaNPoints() const { return fAngularDistributionFormulaNPoints; }
    inline std::string GetAngularDistributionFilename() const { return fAngularDistributionFilename; }
    inline std::string GetAngularDistributionNameInFile() const { return fAngularDistributionNameInFile; }
    inline const TF1* GetAngularDistributionFunction() const { return fAngularDistributionFunction; }
    inline double GetAngularDistributionIsotropicConeHalfAngle() const {
        return fAngularDistributionIsotropicConeHalfAngle;
    }

    inline const TF2* GetEnergyAndAngularDistributionFunction() const {
        return fEnergyAndAngularDistributionFunction;
    }
    inline std::string GetGenFilename() const { return fGenFilename; }
    inline std::vector<TRestGeant4Particle> GetParticles() const { return fParticles; }

    inline void SetAngularDistributionIsotropicConeHalfAngle(double angle) {
        if (angle < 0.0 || angle > TMath::Pi()) {
            exit(1);
        }
        fAngularDistributionIsotropicConeHalfAngle = angle;
    }

    inline void SetAngularDistributionType(const std::string& type) {
        fAngularDistributionType = TRestGeant4PrimaryGeneratorTypes::AngularDistributionTypesToString(
            TRestGeant4PrimaryGeneratorTypes::StringToAngularDistributionTypes(type));
    }

    inline void SetAngularDistributionRange(const std::pair<double, double>& range) {
        fAngularDistributionRange = range;
    }

    inline void SetAngularDistributionFormula(const std::string& formula) {
        if (fAngularDistributionFunction) delete fAngularDistributionFunction;
        fAngularDistributionFunction = static_cast<TF1*>(
            TRestGeant4PrimaryGeneratorTypes::AngularDistributionFormulasToRootFormula(
                TRestGeant4PrimaryGeneratorTypes::StringToAngularDistributionFormulas(formula))
                .Clone());
    }

    inline void SetAngularDistributionFormulaNPoints(size_t nPoints) {
        fAngularDistributionFormulaNPoints = std::min(nPoints, (size_t)10000);
    }
    inline void SetAngularDistributionFilename(const std::string& filename) {
        fAngularDistributionFilename = filename;
    }
    inline void SetAngularDistributionNameInFile(const std::string& name) {
        fAngularDistributionNameInFile = name;
    }

    inline void SetEnergyDistributionType(const std::string& type) {
        fEnergyDistributionType = TRestGeant4PrimaryGeneratorTypes::EnergyDistributionTypesToString(
            TRestGeant4PrimaryGeneratorTypes::StringToEnergyDistributionTypes(type));
    }

    inline void SetEnergyDistributionRange(const std::pair<double, double>& range) {
        fEnergyDistributionRange = range;
    }
    inline void SetEnergyDistributionFormulaNPoints(size_t nPoints) {
        fEnergyDistributionFormulaNPoints = std::min(nPoints, (size_t)10000);
    }
    inline void SetEnergyDistributionFilename(const std::string& filename) {
        fEnergyDistributionFilename = filename;
    }
    inline void SetEnergyDistributionNameInFile(const std::string& name) {
        fEnergyDistributionNameInFile = name;
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
    inline void SetRandomMethod(std::function<double()> method) { fRandomMethod = method; }
    inline void AddParticle(const TRestGeant4Particle& particle) { fParticles.push_back(particle); }
    inline void RemoveParticles() { fParticles.clear(); }
    inline void RemoveTemplates() { fParticlesTemplate.clear(); }
    inline void FlushParticlesTemplate() {
        fParticlesTemplate.push_back(fParticles);
        fParticles.clear();
    }

    /// \brief Clones this source including configuration state.
    virtual TRestGeant4ParticleSource* Clone() const;

    // --- Framework and Identification Overrides ---
    std::string GetClassName() const override { return "TRestGeant4ParticleSource"; }
    /// \brief Prints source configuration and active distribution settings.
    void PrintMetadata() override;
    /// \brief Loads source configuration from the current YAML metadata node.
    void LoadConfig() override;
    /// \brief No-op hook kept for REST metadata lifecycle compatibility.
    void Initialize() override {};
};
