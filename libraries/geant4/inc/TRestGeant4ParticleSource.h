#ifndef TRESTGEANT4PARTICLESOURCE_H
#define TRESTGEANT4PARTICLESOURCE_H

#include <Math/Vector3D.h>
#include <TF2.h>
#include <TH2D.h>
#include <TRandom.h>

#include <memory>
#include <string>
#include <vector>

#include "TRestGeant4AngularDistribution.h"
#include "TRestGeant4EnergyDistribution.h"
#include "TRestGeant4ParticleState.h"
#include "TRestMetadata.h"

/// \class TRestGeant4ParticleSource
/// \brief A `source` of the generator: the `particle` (with `excitedLevel` and `charge` for an ion) and
/// its `energy` and `angular` distributions, or a Decay0 file (`use`) which gives the particles. When
/// energy and angular have the type formula2, TH2D or cosmic, they are sampled together here. See
/// TRestGeant4PrimaryGenerator for the options.
class TRestGeant4ParticleSource : public TRestMetadata {
   public:
    std::string fParticleName;
    TRestWithUnits fExcitedLevel = 0.0;
    int fCharge = 0;
    /// Decay0 file, instead of the particle, energy and angular
    std::string fUse;

    TRestGeant4ParticleSource() = default;
    TRestGeant4ParticleSource(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestGeant4ParticleSource"; }

    /// \brief A copy with its own distributions not loaded yet (LoadDistributions), for a thread
    TRestGeant4ParticleSource Clone() const;

    /// \brief Builds the formulas and reads the histograms and the Decay0 file, needed only to sample
    void LoadDistributions();

    /// \brief The particles of an event, starting at `origin`. The angle reference is `direction` or, if
    /// `inward`, toward `center`
    std::vector<TRestGeant4ParticleState> Generate(TRandom& random, const ROOT::Math::XYZVector& origin,
                                                   const ROOT::Math::XYZVector& center, bool inward);

    inline bool IsDecay0() const { return !fUse.empty(); }
    inline const TRestGeant4EnergyDistribution* GetEnergyDistribution() const { return fEnergy.get(); }
    inline const TRestGeant4AngularDistribution* GetAngularDistribution() const { return fAngular.get(); }

   private:
    std::shared_ptr<TRestGeant4EnergyDistribution> fEnergy;    //!
    std::shared_ptr<TRestGeant4AngularDistribution> fAngular;  //!

    /// Energy and angle sampled together (formula2, TH2D, cosmic)
    std::string fJointType;                  //!
    std::string fJointFormula;               //! formula2
    std::string fJointFile, fJointName;      //! TH2D and cosmic (histogram name)
    std::shared_ptr<TF2> fJointFunction;     //!
    std::shared_ptr<TH2D> fJointHistogram;   //!
    double fJointEnergyUnit = 1;             //!
    double fJointAngleUnit = 1;              //!

    /// Decay0 events, read once and shared by the copies
    std::shared_ptr<const std::vector<std::vector<TRestGeant4ParticleState>>> fDecay0Events;  //!

    void CheckJoint();
    void LoadJointDistribution();
};

#endif
