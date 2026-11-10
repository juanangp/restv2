#ifndef TRESTGEANT4ANGULARDISTRIBUTION_H
#define TRESTGEANT4ANGULARDISTRIBUTION_H

#include <Math/Vector3D.h>
#include <TF1.h>
#include <TH1D.h>
#include <TMath.h>
#include <TRandom.h>

#include <array>
#include <memory>
#include <string>
#include <utility>

#include "TRestMetadata.h"

/// \class TRestGeant4AngularDistribution
/// \brief Direction of the primaries of a source (the `angular` section): the `type` selects the derived
/// class, which reads its own fields. `direction` is the fixed direction or the reference of the angle
/// (as legacy, the reference points to the center for a sphere surface). Angles in rad. See
/// TRestGeant4PrimaryGenerator for the options.
///
///     angular:
///       type: "isotropic"           # flux, isotropic, formula, TH1D, formula2, TH2D or cosmic
///       direction: [0, -1, 0]
///       isotropicConeHalfAngle: 20 deg
///
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
class TRestGeant4AngularDistribution : public TRestMetadata {
   public:
    std::string fType;  ///< `type`, case insensitive (it selects the derived class in Create)
    /// `direction`: the fixed direction or the reference of the angle (not normalized, must not be zero)
    std::array<double, 3> fDirection = {0.0, -1.0, 0.0};

    /// \brief Empty distribution.
    TRestGeant4AngularDistribution() = default;
    /// \brief Keeps the node; the derived classes read their fields from it.
    TRestGeant4AngularDistribution(const std::string& name, const YAML::Node& node)
        : TRestMetadata(name, node) {}

    /// \brief The derived class of the `type` of the node, configured from it
    /// \param name Instance name.
    /// \param node The `angular` section; throws if `type` is missing or unknown.
    static std::unique_ptr<TRestGeant4AngularDistribution> Create(const std::string& name,
                                                                  const YAML::Node& node);

    /// \brief A copy without the loaded distribution (LoadDistribution builds its own), for a thread
    virtual std::unique_ptr<TRestGeant4AngularDistribution> Clone() const = 0;

    /// \brief Builds the formula or reads the histogram, needed only to sample
    virtual void LoadDistribution() {}

    /// \brief A direction, `axis` being the unit reference (`direction` or toward the center)
    /// \param random Random generator of the thread.
    /// \param axis Unit reference direction.
    /// \return A unit direction.
    virtual ROOT::Math::XYZVector Sample(TRandom& random, const ROOT::Math::XYZVector& axis) = 0;

    /// \brief True for the angle of a distribution of energy and angle (formula2, TH2D, cosmic), given
    /// by TRestGeant4ParticleSource together with the energy one
    virtual bool IsJoint() const { return false; }

    /// \brief The unit `direction`
    ROOT::Math::XYZVector GetDirection() const;

    /// \brief No-op.
    void Initialize() override {}

   protected:
    /// \brief Checks a non zero `direction` (throws std::runtime_error otherwise)
    void CheckDirection();
};

/// \class TRestGeant4AngularDistributionFlux
/// \brief `type: "flux"`: the fixed `direction`.
class TRestGeant4AngularDistributionFlux : public TRestGeant4AngularDistribution {
   public:
    TRestGeant4AngularDistributionFlux() = default;
    /// \brief Configured from the `angular` section.
    TRestGeant4AngularDistributionFlux(const std::string& name, const YAML::Node& node);
    /// \brief Reads `type` and `direction` (checked non zero) and writes them back.
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4AngularDistributionFlux"; }

    std::unique_ptr<TRestGeant4AngularDistribution> Clone() const override {
        return std::make_unique<TRestGeant4AngularDistributionFlux>(*this);
    }
    /// \brief Returns `axis`.
    ROOT::Math::XYZVector Sample(TRandom&, const ROOT::Math::XYZVector& axis) override { return axis; }
};

/// \class TRestGeant4AngularDistributionIsotropic
/// \brief `type: "isotropic"`: uniform in 4 pi or, with `isotropicConeHalfAngle`, in the solid angle of
/// the cone around `direction`.
class TRestGeant4AngularDistributionIsotropic : public TRestGeant4AngularDistribution {
   public:
    /// `isotropicConeHalfAngle` (rad, in [0, pi]); 0: uniform in 4 pi
    TRestWithUnits fIsotropicConeHalfAngle = 0.0;

    TRestGeant4AngularDistributionIsotropic() = default;
    /// \brief Configured from the `angular` section.
    TRestGeant4AngularDistributionIsotropic(const std::string& name, const YAML::Node& node);
    /// \brief Reads `type`, `direction` and `isotropicConeHalfAngle` and checks them.
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4AngularDistributionIsotropic"; }

    std::unique_ptr<TRestGeant4AngularDistribution> Clone() const override {
        return std::make_unique<TRestGeant4AngularDistributionIsotropic>(*this);
    }
    /// \brief Uniform in 4 pi, or in the solid angle of the cone around `axis`.
    ROOT::Math::XYZVector Sample(TRandom& random, const ROOT::Math::XYZVector& axis) override;
};

/// \class TRestGeant4AngularDistributionFormula
/// \brief `type: "formula"`: the angle to `direction` follows the named `formula` (its density in the
/// angle) in `range`, sampled with `formulaNPoints`; uniform azimuth.
class TRestGeant4AngularDistributionFormula : public TRestGeant4AngularDistribution {
   public:
    /// `formula`: Cos2, Cos3 or Sin2theta (TRestGeant4PrimaryGeneratorTypes::AngularDistributionFormulas)
    std::string fFormula;
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, TMath::Pi()};  ///< `range` (rad)
    size_t fFormulaNPoints = 500;  ///< `formulaNPoints`: points of the TF1 used to sample

    TRestGeant4AngularDistributionFormula() = default;
    /// \brief Configured from the `angular` section.
    TRestGeant4AngularDistributionFormula(const std::string& name, const YAML::Node& node);
    /// \brief Reads `type`, `direction`, `formula`, `range` and `formulaNPoints` and checks them.
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4AngularDistributionFormula"; }

    std::unique_ptr<TRestGeant4AngularDistribution> Clone() const override;
    /// \brief Builds the TF1 of the formula in `range` intersected with the formula one (throws if they do
    /// not overlap).
    void LoadDistribution() override;
    /// \brief A direction at an angle to `axis` sampled from the formula, with uniform azimuth.
    ROOT::Math::XYZVector Sample(TRandom& random, const ROOT::Math::XYZVector& axis) override;

   private:
    std::shared_ptr<TF1> fFunction;  //!
};

/// \class TRestGeant4AngularDistributionTH1D
/// \brief `type: "TH1D"`: the angle to `direction` (rad) from the histogram `nameInFile` of `filename`,
/// the bins with the center in `range`; uniform azimuth.
class TRestGeant4AngularDistributionTH1D : public TRestGeant4AngularDistribution {
   public:
    std::string fFilename;    ///< `filename`: ROOT file with the histogram
    std::string fNameInFile;  ///< `nameInFile`: name of the TH1D in the file
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, TMath::Pi()};  ///< `range` (rad)

    TRestGeant4AngularDistributionTH1D() = default;
    /// \brief Configured from the `angular` section.
    TRestGeant4AngularDistributionTH1D(const std::string& name, const YAML::Node& node);
    /// \brief Reads `type`, `direction`, `filename`, `nameInFile` and `range` and checks them.
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4AngularDistributionTH1D"; }

    std::unique_ptr<TRestGeant4AngularDistribution> Clone() const override;
    /// \brief Reads the histogram from the file and empties the bins with the center out of `range`
    /// (throws if it is not a TH1D or it is empty in `range`).
    void LoadDistribution() override;
    /// \brief A direction at an angle to `axis` sampled from the histogram, with uniform azimuth.
    ROOT::Math::XYZVector Sample(TRandom& random, const ROOT::Math::XYZVector& axis) override;

   private:
    std::shared_ptr<TH1D> fHistogram;  //!
};

/// \class TRestGeant4AngularDistributionJoint
/// \brief `type: "formula2"`, `"TH2D"` or `"cosmic"`: the angle axis of a distribution of energy and
/// angle, which must have the same type in `energy` (see TRestGeant4ParticleSource): `formula`,
/// `filename`, `nameInFile`, `range` and `formulaNPoints`; the angle is to `direction`.
class TRestGeant4AngularDistributionJoint : public TRestGeant4AngularDistribution {
   public:
    std::string fFormula;     ///< `formula` (formula2): CosmicMuons
    std::string fFilename;    ///< `filename` (TH2D, cosmic)
    std::string fNameInFile;  ///< `nameInFile` (TH2D: histogram; cosmic: particle key)
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, TMath::Pi()};  ///< `range` of the angle (rad)
    size_t fFormulaNPoints = 500;  ///< `formulaNPoints` of the angle axis (formula2)

    TRestGeant4AngularDistributionJoint() = default;
    /// \brief Configured from the `angular` section.
    TRestGeant4AngularDistributionJoint(const std::string& name, const YAML::Node& node);
    /// \brief Reads `type`, `direction` and the joint fields and checks them.
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4AngularDistributionJoint"; }

    std::unique_ptr<TRestGeant4AngularDistribution> Clone() const override {
        return std::make_unique<TRestGeant4AngularDistributionJoint>(*this);
    }
    /// \brief Not used, TRestGeant4ParticleSource samples energy and angle together (it throws)
    ROOT::Math::XYZVector Sample(TRandom& random, const ROOT::Math::XYZVector& axis) override;
    /// \brief Returns true.
    bool IsJoint() const override { return true; }
};

#endif
