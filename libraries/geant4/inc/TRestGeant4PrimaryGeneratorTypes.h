#ifndef TRESTGEANT4PRIMARYGENERATORTYPES_H
#define TRESTGEANT4PRIMARYGENERATORTYPES_H

#include <Math/Vector3D.h>

#include <memory>
#include <string>

class TF1;
class TF2;
class TH1;
class TH1D;
class TH2D;
class TRandom;

/// Named formulas of the primary generator: energy (keV), angle (rad) and energy and angle; the names are
/// case insensitive and an unknown one throws std::invalid_argument. Also the helpers shared by the
/// generator classes (TRestGeant4SpatialGenerator, TRestGeant4EnergyDistribution,
/// TRestGeant4AngularDistribution, TRestGeant4ParticleSource).
///
/// \author Luis Obis <luis.antonio.obis@gmail.com>
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
namespace TRestGeant4PrimaryGeneratorTypes {

/// \brief Named energy formulas (keV), `formula` of the energy type formula
enum class EnergyDistributionFormulas {
    COSMIC_NEUTRONS,        ///< "CosmicNeutrons": cosmic neutrons at sea level, 100 keV to 10 GeV
    COSMIC_GAMMAS,          ///< "CosmicGammas": not implemented (rejected by the energy type formula)
    FISSION_NEUTRONS_U238,  ///< "FissionNeutronsU238": Watt spectrum of U238 fission, 0 to 10 MeV
    ENVIRONMENTAL_GAMMAS,   ///< "EnvironmentalGammas": E^-1.5 exp(-E / 1.5 MeV), 250 keV to 10 MeV
    ENVIRONMENTAL_NEUTRONS  ///< "EnvironmentalNeutrons": exp(-E / 2 MeV), 0 to 10 MeV
};
/// \brief The name of an energy formula
std::string EnergyDistributionFormulasToString(EnergyDistributionFormulas formula);
/// \brief The energy formula of a name (case insensitive; throws std::invalid_argument if unknown)
EnergyDistributionFormulas StringToEnergyDistributionFormulas(const std::string& formulaStr);
/// \brief The normalized TF1 of an energy formula, x in keV in the formula range
TF1 EnergyDistributionFormulasToRootFormula(EnergyDistributionFormulas formula);

/// \brief Named angular formulas: density in the angle (rad) to the reference direction, 0 above 90 deg
enum class AngularDistributionFormulas {
    COS2,       ///< "Cos2": cos^2
    COS3,       ///< "Cos3": cos^3
    SIN_2THETA  ///< "Sin2theta": sin(2 theta)
};
/// \brief The name of an angular formula
std::string AngularDistributionFormulasToString(AngularDistributionFormulas formula);
/// \brief The angular formula of a name (case insensitive; throws std::invalid_argument if unknown)
AngularDistributionFormulas StringToAngularDistributionFormulas(const std::string& formulaStr);
/// \brief The TF1 of an angular formula, x the angle in rad
TF1 AngularDistributionFormulasToRootFormula(AngularDistributionFormulas formula);

/// \brief Named formulas of energy and angle together (type formula2)
enum class EnergyAndAngularDistributionFormulas {
    COSMIC_MUONS  ///< "CosmicMuons": Guan modified Gaisser parametrization at sea level (arXiv:1509.06176)
};
/// \brief The name of an energy and angle formula
std::string EnergyAndAngularDistributionFormulasToString(EnergyAndAngularDistributionFormulas formula);
/// \brief The energy and angle formula of a name (case insensitive; throws std::invalid_argument if unknown)
EnergyAndAngularDistributionFormulas StringToEnergyAndAngularDistributionFormulas(
    const std::string& formulaStr);
/// \brief The TF2 of an energy and angle formula: x the kinetic energy (keV), y the zenith angle (rad)
TF2 EnergyAndAngularDistributionFormulasToRootFormula(EnergyAndAngularDistributionFormulas formula);

/// \brief A copy (not attached to any file) of the histogram `name` of the ROOT file, an error if missing
std::unique_ptr<TH1> ReadHistogram(const std::string& fileName, const std::string& name);
/// \brief ReadHistogram as a TH1D: any 1D histogram (TH1F, TH1I...) is converted (same bins, contents and
/// errors); an error if it is not 1D
std::unique_ptr<TH1D> ReadHistogram1D(const std::string& fileName, const std::string& name);
/// \brief ReadHistogram as a TH2D: any 2D histogram (TH2F, TH2I...) is converted (same bins, contents and
/// errors); an error if it is not 2D
std::unique_ptr<TH2D> ReadHistogram2D(const std::string& fileName, const std::string& name);

/// \brief A uniform direction in 4 pi
ROOT::Math::XYZVector IsotropicDirection(TRandom& random);

/// \brief A direction at `angle` from the unit vector `axis`, with uniform azimuth around it
ROOT::Math::XYZVector DirectionFromAxis(TRandom& random, const ROOT::Math::XYZVector& axis, double angle);

}  // namespace TRestGeant4PrimaryGeneratorTypes

#endif
