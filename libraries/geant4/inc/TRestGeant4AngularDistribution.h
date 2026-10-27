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
class TRestGeant4AngularDistribution : public TRestMetadata {
   public:
    std::string fType;
    std::array<double, 3> fDirection = {0.0, -1.0, 0.0};

    TRestGeant4AngularDistribution() = default;
    TRestGeant4AngularDistribution(const std::string& name, const YAML::Node& node)
        : TRestMetadata(name, node) {}

    /// \brief The derived class of the `type` of the node, configured from it
    static std::unique_ptr<TRestGeant4AngularDistribution> Create(const std::string& name,
                                                                 const YAML::Node& node);

    /// \brief A copy without the loaded distribution (LoadDistribution builds its own), for a thread
    virtual std::unique_ptr<TRestGeant4AngularDistribution> Clone() const = 0;

    /// \brief Builds the formula or reads the histogram, needed only to sample
    virtual void LoadDistribution() {}

    /// \brief A direction, `axis` being the unit reference (`direction` or toward the center)
    virtual ROOT::Math::XYZVector Sample(TRandom& random, const ROOT::Math::XYZVector& axis) = 0;

    /// \brief True for the angle of a distribution of energy and angle (formula2, TH2D, cosmic), given
    /// by TRestGeant4ParticleSource together with the energy one
    virtual bool IsJoint() const { return false; }

    /// \brief The unit `direction`
    ROOT::Math::XYZVector GetDirection() const;

    void Initialize() override {}

   protected:
    /// \brief Checks a non zero `direction`
    void CheckDirection();
};

/// \class TRestGeant4AngularDistributionFlux
/// \brief `type: "flux"`: the fixed `direction`.
class TRestGeant4AngularDistributionFlux : public TRestGeant4AngularDistribution {
   public:
    TRestGeant4AngularDistributionFlux() = default;
    TRestGeant4AngularDistributionFlux(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4AngularDistributionFlux"; }

    std::unique_ptr<TRestGeant4AngularDistribution> Clone() const override {
        return std::make_unique<TRestGeant4AngularDistributionFlux>(*this);
    }
    ROOT::Math::XYZVector Sample(TRandom&, const ROOT::Math::XYZVector& axis) override { return axis; }
};

/// \class TRestGeant4AngularDistributionIsotropic
/// \brief `type: "isotropic"`: uniform in 4 pi or, with `isotropicConeHalfAngle`, in the solid angle of
/// the cone around `direction`.
class TRestGeant4AngularDistributionIsotropic : public TRestGeant4AngularDistribution {
   public:
    TRestWithUnits fIsotropicConeHalfAngle = 0.0;

    TRestGeant4AngularDistributionIsotropic() = default;
    TRestGeant4AngularDistributionIsotropic(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4AngularDistributionIsotropic"; }

    std::unique_ptr<TRestGeant4AngularDistribution> Clone() const override {
        return std::make_unique<TRestGeant4AngularDistributionIsotropic>(*this);
    }
    ROOT::Math::XYZVector Sample(TRandom& random, const ROOT::Math::XYZVector& axis) override;
};

/// \class TRestGeant4AngularDistributionFormula
/// \brief `type: "formula"`: the angle to `direction` follows the named `formula` (its density in the
/// angle) in `range`, sampled with `formulaNPoints`; uniform azimuth.
class TRestGeant4AngularDistributionFormula : public TRestGeant4AngularDistribution {
   public:
    std::string fFormula;
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, TMath::Pi()};
    size_t fFormulaNPoints = 500;

    TRestGeant4AngularDistributionFormula() = default;
    TRestGeant4AngularDistributionFormula(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4AngularDistributionFormula"; }

    std::unique_ptr<TRestGeant4AngularDistribution> Clone() const override;
    void LoadDistribution() override;
    ROOT::Math::XYZVector Sample(TRandom& random, const ROOT::Math::XYZVector& axis) override;

   private:
    std::shared_ptr<TF1> fFunction;  //!
};

/// \class TRestGeant4AngularDistributionTH1D
/// \brief `type: "TH1D"`: the angle to `direction` (rad) from the histogram `nameInFile` of `filename`,
/// the bins with the center in `range`; uniform azimuth.
class TRestGeant4AngularDistributionTH1D : public TRestGeant4AngularDistribution {
   public:
    std::string fFilename;
    std::string fNameInFile;
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, TMath::Pi()};

    TRestGeant4AngularDistributionTH1D() = default;
    TRestGeant4AngularDistributionTH1D(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4AngularDistributionTH1D"; }

    std::unique_ptr<TRestGeant4AngularDistribution> Clone() const override;
    void LoadDistribution() override;
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
    std::string fFormula;
    std::string fFilename;
    std::string fNameInFile;
    std::pair<TRestWithUnits, TRestWithUnits> fRange = {0.0, TMath::Pi()};
    size_t fFormulaNPoints = 500;

    TRestGeant4AngularDistributionJoint() = default;
    TRestGeant4AngularDistributionJoint(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4AngularDistributionJoint"; }

    std::unique_ptr<TRestGeant4AngularDistribution> Clone() const override {
        return std::make_unique<TRestGeant4AngularDistributionJoint>(*this);
    }
    /// \brief Not used, TRestGeant4ParticleSource samples energy and angle together
    ROOT::Math::XYZVector Sample(TRandom& random, const ROOT::Math::XYZVector& axis) override;
    bool IsJoint() const override { return true; }
};

#endif
