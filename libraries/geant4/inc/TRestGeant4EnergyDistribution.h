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
///
///     energy:
///       type: "flat"                # mono, flat, log, formula, TH1D, formula2, TH2D or cosmic
///       range: [1 MeV, 10 MeV]
class TRestGeant4EnergyDistribution : public TRestMetadata {
   public:
    std::string fType;  ///< `type`, case insensitive (it selects the derived class in Create)

    /// \brief Empty distribution.
    TRestGeant4EnergyDistribution() = default;
    /// \brief Keeps the node; the derived classes read their fields from it.
    TRestGeant4EnergyDistribution(const std::string& name, const YAML::Node& node)
        : TRestMetadata(name, node) {}

    /// \brief The derived class of the `type` of the node, configured from it
    /// \param name Instance name.
    /// \param node The `energy` section; throws if `type` is missing or unknown.
    static std::unique_ptr<TRestGeant4EnergyDistribution> Create(const std::string& name,
                                                                 const YAML::Node& node);

    /// \brief A copy without the loaded distribution (LoadDistribution builds its own), for a thread
    virtual std::unique_ptr<TRestGeant4EnergyDistribution> Clone() const = 0;

    /// \brief Builds the formula or reads the histogram, needed only to sample
    virtual void LoadDistribution() {}

    /// \brief A kinetic energy (keV)
    /// \param random Random generator of the thread.
    virtual double Sample(TRandom& random) = 0;

    /// \brief True for the energy of a distribution of energy and angle (formula2, TH2D, cosmic), given
    /// by TRestGeant4ParticleSource together with the angular one
    virtual bool IsJoint() const { return false; }

    /// \brief No-op.
    void Initialize() override {}
};

/// \class TRestGeant4EnergyDistributionMono
/// \brief `type: "mono"`: `energy`.
class TRestGeant4EnergyDistributionMono : public TRestGeant4EnergyDistribution {
   public:
    TRestWithUnits fEnergy = 0.0;  ///< `energy` (keV), must be positive

    TRestGeant4EnergyDistributionMono() = default;
    /// \brief Configured from the `energy` section.
    TRestGeant4EnergyDistributionMono(const std::string& name, const YAML::Node& node);
    /// \brief Reads `type` and `energy` and checks it.
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4EnergyDistributionMono"; }

    std::unique_ptr<TRestGeant4EnergyDistribution> Clone() const override {
        return std::make_unique<TRestGeant4EnergyDistributionMono>(*this);
    }
    /// \brief Returns `energy`.
    double Sample(TRandom&) override { return fEnergy; }
};

/// \class TRestGeant4EnergyDistributionFlat
/// \brief `type: "flat"`: uniform in `range`.
class TRestGeant4EnergyDistributionFlat : public TRestGeant4EnergyDistribution {
   public:
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, 0.0};  ///< `range` (keV)

    TRestGeant4EnergyDistributionFlat() = default;
    /// \brief Configured from the `energy` section.
    TRestGeant4EnergyDistributionFlat(const std::string& name, const YAML::Node& node);
    /// \brief Reads `type` and `range` and checks it.
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4EnergyDistributionFlat"; }

    std::unique_ptr<TRestGeant4EnergyDistribution> Clone() const override {
        return std::make_unique<TRestGeant4EnergyDistributionFlat>(*this);
    }
    /// \brief Uniform in `range`.
    double Sample(TRandom& random) override;
};

/// \class TRestGeant4EnergyDistributionLog
/// \brief `type: "log"`: uniform in log(E) in `range` (minimum > 0).
class TRestGeant4EnergyDistributionLog : public TRestGeant4EnergyDistribution {
   public:
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, 0.0};  ///< `range` (keV), minimum > 0

    TRestGeant4EnergyDistributionLog() = default;
    /// \brief Configured from the `energy` section.
    TRestGeant4EnergyDistributionLog(const std::string& name, const YAML::Node& node);
    /// \brief Reads `type` and `range` and checks it.
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4EnergyDistributionLog"; }

    std::unique_ptr<TRestGeant4EnergyDistribution> Clone() const override {
        return std::make_unique<TRestGeant4EnergyDistributionLog>(*this);
    }
    /// \brief Uniform in log(E) in `range`.
    double Sample(TRandom& random) override;
};

/// \class TRestGeant4EnergyDistributionFormula
/// \brief `type: "formula"`: the named `formula` (TRestGeant4PrimaryGeneratorTypes) in `range`
/// (intersected with the formula one), sampled with `formulaNPoints`.
class TRestGeant4EnergyDistributionFormula : public TRestGeant4EnergyDistribution {
   public:
    /// `formula`: CosmicNeutrons, FissionNeutronsU238, EnvironmentalGammas or EnvironmentalNeutrons
    std::string fFormula;
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, 1.0E20};  ///< `range` (keV)
    size_t fFormulaNPoints = 5000;  ///< `formulaNPoints`: points of the TF1 used to sample

    TRestGeant4EnergyDistributionFormula() = default;
    /// \brief Configured from the `energy` section.
    TRestGeant4EnergyDistributionFormula(const std::string& name, const YAML::Node& node);
    /// \brief Reads `type`, `formula`, `range` and `formulaNPoints` and checks them (an unknown formula
    /// name or CosmicGammas, not implemented, throws).
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4EnergyDistributionFormula"; }

    std::unique_ptr<TRestGeant4EnergyDistribution> Clone() const override;
    /// \brief Builds the TF1 of the formula in `range` intersected with the formula one (throws if they do
    /// not overlap).
    void LoadDistribution() override;
    /// \brief An energy sampled from the formula (throws if it is not loaded).
    double Sample(TRandom& random) override;

   private:
    std::shared_ptr<TF1> fFunction;  //!
};

/// \class TRestGeant4EnergyDistributionTH1D
/// \brief `type: "TH1D"`: the histogram `nameInFile` of `filename`, the bins with the center in `range`.
/// The x axis is in MeV or GeV if its title says so, otherwise in keV; uniform inside the bin.
class TRestGeant4EnergyDistributionTH1D : public TRestGeant4EnergyDistribution {
   public:
    std::string fFilename;    ///< `filename`: ROOT file with the histogram
    std::string fNameInFile;  ///< `nameInFile`: name of the TH1D in the file
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, 1.0E20};  ///< `range` (keV)

    TRestGeant4EnergyDistributionTH1D() = default;
    /// \brief Configured from the `energy` section.
    TRestGeant4EnergyDistributionTH1D(const std::string& name, const YAML::Node& node);
    /// \brief Reads `type`, `filename`, `nameInFile` and `range` and checks them.
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4EnergyDistributionTH1D"; }

    std::unique_ptr<TRestGeant4EnergyDistribution> Clone() const override;
    /// \brief Reads the histogram, takes the unit of the x axis from its title and empties the bins with
    /// the center out of `range` (throws if it is not a TH1D or it is empty in `range`).
    void LoadDistribution() override;
    /// \brief An energy sampled from the histogram, in keV (throws if it is not loaded).
    double Sample(TRandom& random) override;

   private:
    std::shared_ptr<TH1D> fHistogram;  //!
    double fUnit = 1;                  //!
};

/// \class TRestGeant4EnergyDistributionJoint
/// \brief `type: "formula2"`, `"TH2D"` or `"cosmic"`: the energy axis of a distribution of energy and
/// angle, which must have the same type in `angular` (see TRestGeant4ParticleSource): `formula`,
/// `filename`, `nameInFile`, `range` and `formulaNPoints`.
class TRestGeant4EnergyDistributionJoint : public TRestGeant4EnergyDistribution {
   public:
    std::string fFormula;     ///< `formula` (formula2): CosmicMuons
    std::string fFilename;    ///< `filename` (TH2D, cosmic)
    std::string fNameInFile;  ///< `nameInFile` (TH2D: histogram; cosmic: particle key)
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, 1.0E20};  ///< `range` of the energy (keV)
    size_t fFormulaNPoints = 5000;  ///< `formulaNPoints` of the energy axis (formula2)

    TRestGeant4EnergyDistributionJoint() = default;
    /// \brief Configured from the `energy` section.
    TRestGeant4EnergyDistributionJoint(const std::string& name, const YAML::Node& node);
    /// \brief Reads `type`, `range` and the joint fields and checks the range.
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4EnergyDistributionJoint"; }

    std::unique_ptr<TRestGeant4EnergyDistribution> Clone() const override {
        return std::make_unique<TRestGeant4EnergyDistributionJoint>(*this);
    }
    /// \brief Not used, TRestGeant4ParticleSource samples energy and angle together (it throws)
    double Sample(TRandom& random) override;
    /// \brief Returns true.
    bool IsJoint() const override { return true; }
};

#endif
