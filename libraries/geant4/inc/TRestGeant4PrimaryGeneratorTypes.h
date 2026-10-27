#ifndef TRESTGEANT4PRIMARYGENERATORTYPES_H
#define TRESTGEANT4PRIMARYGENERATORTYPES_H

#include <Math/Vector3D.h>

#include <memory>
#include <string>

class TF1;
class TF2;
class TH1;
class TRandom;

/// Named formulas of the primary generator: energy (keV), angle (rad) and energy and angle; the names are
/// case insensitive and an unknown one throws std::invalid_argument. Also the helpers shared by the
/// generator classes (TRestGeant4SpatialGenerator, TRestGeant4EnergyDistribution,
/// TRestGeant4AngularDistribution, TRestGeant4ParticleSource).
namespace TRestGeant4PrimaryGeneratorTypes {

enum class EnergyDistributionFormulas {
    COSMIC_NEUTRONS,
    COSMIC_GAMMAS,
    FISSION_NEUTRONS_U238,
    ENVIRONMENTAL_GAMMAS,
    ENVIRONMENTAL_NEUTRONS
};
std::string EnergyDistributionFormulasToString(EnergyDistributionFormulas formula);
EnergyDistributionFormulas StringToEnergyDistributionFormulas(const std::string& formulaStr);
TF1 EnergyDistributionFormulasToRootFormula(EnergyDistributionFormulas formula);

enum class AngularDistributionFormulas { COS2, COS3, SIN_2THETA };
std::string AngularDistributionFormulasToString(AngularDistributionFormulas formula);
AngularDistributionFormulas StringToAngularDistributionFormulas(const std::string& formulaStr);
TF1 AngularDistributionFormulasToRootFormula(AngularDistributionFormulas formula);

enum class EnergyAndAngularDistributionFormulas { COSMIC_MUONS };
std::string EnergyAndAngularDistributionFormulasToString(EnergyAndAngularDistributionFormulas formula);
EnergyAndAngularDistributionFormulas StringToEnergyAndAngularDistributionFormulas(
    const std::string& formulaStr);
TF2 EnergyAndAngularDistributionFormulasToRootFormula(EnergyAndAngularDistributionFormulas formula);

/// \brief A copy (not attached to any file) of the histogram `name` of the ROOT file, an error if missing
std::unique_ptr<TH1> ReadHistogram(const std::string& fileName, const std::string& name);

/// \brief A uniform direction in 4 pi
ROOT::Math::XYZVector IsotropicDirection(TRandom& random);

/// \brief A direction at `angle` from the unit vector `axis`, with uniform azimuth around it
ROOT::Math::XYZVector DirectionFromAxis(TRandom& random, const ROOT::Math::XYZVector& axis, double angle);

}  // namespace TRestGeant4PrimaryGeneratorTypes

#endif
