#ifndef TRESTGEANT4ENERGYDISTRIBUTION_H
#define TRESTGEANT4ENERGYDISTRIBUTION_H

#include <TF1.h>
#include <TH1D.h>
#include <TRandom.h>

#include <memory>
#include <string>
#include <utility>

#include "TRestMetadata.h"

/// \class TRestGeant4EnergyDistribution
/// \brief Kinetic energy of the primaries of a source (the `energy` section): the `type` selects the
/// derived class, which reads its own fields. Energies in keV. See TRestGeant4PrimaryGenerator for the
/// options.
class TRestGeant4EnergyDistribution : public TRestMetadata {
   public:
    std::string fType;

    TRestGeant4EnergyDistribution() = default;
    TRestGeant4EnergyDistribution(const std::string& name, const YAML::Node& node)
        : TRestMetadata(name, node) {}

    /// \brief The derived class of the `type` of the node, configured from it
    static std::unique_ptr<TRestGeant4EnergyDistribution> Create(const std::string& name,
                                                                const YAML::Node& node);

    /// \brief A copy without the loaded distribution (LoadDistribution builds its own), for a thread
    virtual std::unique_ptr<TRestGeant4EnergyDistribution> Clone() const = 0;

    /// \brief Builds the formula or reads the histogram, needed only to sample
    virtual void LoadDistribution() {}

    virtual double Sample(TRandom& random) = 0;

    /// \brief True for the energy of a distribution of energy and angle (formula2, TH2D, cosmic), given
    /// by TRestGeant4ParticleSource together with the angular one
    virtual bool IsJoint() const { return false; }

    void Initialize() override {}
};

/// \class TRestGeant4EnergyDistributionMono
/// \brief `type: "mono"`: `energy`.
class TRestGeant4EnergyDistributionMono : public TRestGeant4EnergyDistribution {
   public:
    TRestWithUnits fEnergy = 0.0;

    TRestGeant4EnergyDistributionMono() = default;
    TRestGeant4EnergyDistributionMono(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4EnergyDistributionMono"; }

    std::unique_ptr<TRestGeant4EnergyDistribution> Clone() const override {
        return std::make_unique<TRestGeant4EnergyDistributionMono>(*this);
    }
    double Sample(TRandom&) override { return fEnergy; }
};

/// \class TRestGeant4EnergyDistributionFlat
/// \brief `type: "flat"`: uniform in `range`.
class TRestGeant4EnergyDistributionFlat : public TRestGeant4EnergyDistribution {
   public:
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, 0.0};

    TRestGeant4EnergyDistributionFlat() = default;
    TRestGeant4EnergyDistributionFlat(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4EnergyDistributionFlat"; }

    std::unique_ptr<TRestGeant4EnergyDistribution> Clone() const override {
        return std::make_unique<TRestGeant4EnergyDistributionFlat>(*this);
    }
    double Sample(TRandom& random) override;
};

/// \class TRestGeant4EnergyDistributionLog
/// \brief `type: "log"`: uniform in log(E) in `range` (minimum > 0).
class TRestGeant4EnergyDistributionLog : public TRestGeant4EnergyDistribution {
   public:
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, 0.0};

    TRestGeant4EnergyDistributionLog() = default;
    TRestGeant4EnergyDistributionLog(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4EnergyDistributionLog"; }

    std::unique_ptr<TRestGeant4EnergyDistribution> Clone() const override {
        return std::make_unique<TRestGeant4EnergyDistributionLog>(*this);
    }
    double Sample(TRandom& random) override;
};

/// \class TRestGeant4EnergyDistributionFormula
/// \brief `type: "formula"`: the named `formula` (TRestGeant4PrimaryGeneratorTypes) in `range`
/// (intersected with the formula one), sampled with `formulaNPoints`.
class TRestGeant4EnergyDistributionFormula : public TRestGeant4EnergyDistribution {
   public:
    std::string fFormula;
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, 1.0E20};
    size_t fFormulaNPoints = 5000;

    TRestGeant4EnergyDistributionFormula() = default;
    TRestGeant4EnergyDistributionFormula(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4EnergyDistributionFormula"; }

    std::unique_ptr<TRestGeant4EnergyDistribution> Clone() const override;
    void LoadDistribution() override;
    double Sample(TRandom& random) override;

   private:
    std::shared_ptr<TF1> fFunction;  //!
};

/// \class TRestGeant4EnergyDistributionTH1D
/// \brief `type: "TH1D"`: the histogram `nameInFile` of `filename`, the bins with the center in `range`.
/// The x axis is in MeV or GeV if its title says so, otherwise in keV; uniform inside the bin.
class TRestGeant4EnergyDistributionTH1D : public TRestGeant4EnergyDistribution {
   public:
    std::string fFilename;
    std::string fNameInFile;
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, 1.0E20};

    TRestGeant4EnergyDistributionTH1D() = default;
    TRestGeant4EnergyDistributionTH1D(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4EnergyDistributionTH1D"; }

    std::unique_ptr<TRestGeant4EnergyDistribution> Clone() const override;
    void LoadDistribution() override;
    double Sample(TRandom& random) override;

   private:
    std::shared_ptr<TH1D> fHistogram;  //!
    double fUnit = 1;                 //!
};

/// \class TRestGeant4EnergyDistributionJoint
/// \brief `type: "formula2"`, `"TH2D"` or `"cosmic"`: the energy axis of a distribution of energy and
/// angle, which must have the same type in `angular` (see TRestGeant4ParticleSource): `formula`,
/// `filename`, `nameInFile`, `range` and `formulaNPoints`.
class TRestGeant4EnergyDistributionJoint : public TRestGeant4EnergyDistribution {
   public:
    std::string fFormula;
    std::string fFilename;
    std::string fNameInFile;
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, 1.0E20};
    size_t fFormulaNPoints = 5000;

    TRestGeant4EnergyDistributionJoint() = default;
    TRestGeant4EnergyDistributionJoint(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4EnergyDistributionJoint"; }

    std::unique_ptr<TRestGeant4EnergyDistribution> Clone() const override {
        return std::make_unique<TRestGeant4EnergyDistributionJoint>(*this);
    }
    /// \brief Not used, TRestGeant4ParticleSource samples energy and angle together
    double Sample(TRandom& random) override;
    bool IsJoint() const override { return true; }
};

#endif
