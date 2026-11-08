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
///
///     source:
///       particle: "Pb210"         # Geant4 particle or ion name
///       excitedLevel: 0 keV
///       charge: 0
///       energy:                   # TRestGeant4EnergyDistribution
///         type: "mono"
///         energy: 1 keV
///       angular:                  # TRestGeant4AngularDistribution
///         type: "isotropic"
///
/// or a Decay0 file, `source: {use: "Xe136bb0n.dat"}`, without particle, energy and angular.
class TRestGeant4ParticleSource : public TRestMetadata {
   public:
    std::string fParticleName;           ///< `particle`: Geant4 particle or ion name
    TRestWithUnits fExcitedLevel = 0.0;  ///< `excitedLevel`: excitation energy of an ion (keV)
    int fCharge = 0;                     ///< `charge`: charge of an ion (e)
    /// Decay0 file, instead of the particle, energy and angular
    std::string fUse;
    /// `energy` and `angular` sections, of the distribution class of their `type`
    std::unique_ptr<TRestGeant4EnergyDistribution> fEnergy;    //!
    std::unique_ptr<TRestGeant4AngularDistribution> fAngular;  //!

    /// Only movable: a copy (with its own distributions) is made with Clone
    TRestGeant4ParticleSource() = default;
    /// \brief Source configured from a `source` entry.
    TRestGeant4ParticleSource(const std::string& name, const YAML::Node& node);
    TRestGeant4ParticleSource(TRestGeant4ParticleSource&&) = default;
    TRestGeant4ParticleSource& operator=(TRestGeant4ParticleSource&&) = default;
    /// \brief Reads the fields and the `energy` and `angular` sections and checks them: a Decay0 source has
    /// only `use`; otherwise `particle`, `energy` and `angular` are needed, and a joint type must be in
    /// both sections. Throws std::runtime_error if not valid.
    void LoadConfig() override;
    /// \brief No-op.
    void Initialize() override {}
    /// \brief Returns "TRestGeant4ParticleSource".
    std::string GetClassName() const override { return "TRestGeant4ParticleSource"; }

    /// \brief A copy with its own distributions not loaded yet (LoadDistributions), e.g. for a thread: built
    /// again from its section, which has the values used. The Decay0 events are shared
    TRestGeant4ParticleSource Clone() const;

    /// \brief Builds the formulas and reads the histograms and the Decay0 file, needed only to sample
    void LoadDistributions();

    /// \brief The particles of an event, starting at `origin`. The angle reference is `direction` or, if
    /// `inward`, toward `center`
    /// \param random Random generator of the thread.
    /// \param origin Position of the particles (mm).
    /// \param center Center of the spatial generator (mm), used if `inward`.
    /// \param inward True if the reference direction points to `center`.
    /// \return One particle, or those of a random event of the Decay0 file.
    std::vector<TRestGeant4ParticleState> Generate(TRandom& random, const ROOT::Math::XYZVector& origin,
                                                   const ROOT::Math::XYZVector& center, bool inward);

    inline bool IsDecay0() const { return !fUse.empty(); }  ///< True for a Decay0 source (`use`)
    /// \brief The energy distribution (nullptr for a Decay0 source)
    inline const TRestGeant4EnergyDistribution* GetEnergyDistribution() const { return fEnergy.get(); }
    /// \brief The angular distribution (nullptr for a Decay0 source)
    inline const TRestGeant4AngularDistribution* GetAngularDistribution() const { return fAngular.get(); }

   private:
    /// Energy and angle sampled together (formula2, TH2D, cosmic)
    std::string fJointType;               //!
    std::string fJointFormula;            //! formula2
    std::string fJointFile, fJointName;   //! TH2D and cosmic (histogram name)
    std::unique_ptr<TF2> fJointFunction;  //!
    /// Cumulative integral of fJointFunction over its Npx x Npy cells (normalized to 1), to sample it:
    /// built once and shared by the copies (TF2::GetRandom2 integrates each cell adaptively, minutes for
    /// the 5000 x 500 cells of the cosmic muons)
    std::shared_ptr<const std::vector<double>> fJointCumulative;  //!
    std::unique_ptr<TH2D> fJointHistogram;                        //!
    double fJointEnergyUnit = 1;                                  //!
    double fJointAngleUnit = 1;                                   //!

    /// Decay0 events, read once and shared by the copies
    std::shared_ptr<const std::vector<std::vector<TRestGeant4ParticleState>>> fDecay0Events;  //!

    void CheckJoint();
    void LoadJointDistribution();
    /// \brief fJointCumulative of fJointFunction (2 x 2 Gauss-Legendre points per cell)
    void BuildJointCumulative();
    /// \brief Energy (x) and angle (y) sampled from fJointFunction with fJointCumulative
    void SampleJointFunction(TRandom& random, double& x, double& y) const;
};

#endif
