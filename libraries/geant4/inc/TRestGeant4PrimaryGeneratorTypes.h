#pragma once

#include <string>

class TF1;
class TF2;

namespace TRestGeant4PrimaryGeneratorTypes {

enum class SpatialGeneratorTypes { CUSTOM, VOLUME, SURFACE, POINT, COSMIC, SOURCE };
std::string SpatialGeneratorTypesToString(SpatialGeneratorTypes type);
SpatialGeneratorTypes StringToSpatialGeneratorTypes(const std::string& typeStr);

enum class SpatialGeneratorShapes { GDML, WALL, CIRCLE, BOX, SPHERE, CYLINDER };
std::string SpatialGeneratorShapesToString(SpatialGeneratorShapes shape);
SpatialGeneratorShapes StringToSpatialGeneratorShapes(const std::string& shapeStr);

enum class EnergyDistributionTypes { TH1D, TH2D, FORMULA, FORMULA2, MONO, FLAT, LOG, COSMIC };
std::string EnergyDistributionTypesToString(EnergyDistributionTypes type);
EnergyDistributionTypes StringToEnergyDistributionTypes(const std::string& typeStr);

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

enum class AngularDistributionTypes { TH1D, TH2D, FORMULA, FORMULA2, ISOTROPIC, FLUX, BACK_TO_BACK, COSMIC };
std::string AngularDistributionTypesToString(AngularDistributionTypes type);
AngularDistributionTypes StringToAngularDistributionTypes(const std::string& typeStr);

enum class AngularDistributionFormulas { COS2, COS3, SIN_2THETA };
std::string AngularDistributionFormulasToString(AngularDistributionFormulas formula);
AngularDistributionFormulas StringToAngularDistributionFormulas(const std::string& formulaStr);
TF1 AngularDistributionFormulasToRootFormula(AngularDistributionFormulas formula);

enum class EnergyAndAngularDistributionFormulas { COSMIC_MUONS };
std::string EnergyAndAngularDistributionFormulasToString(EnergyAndAngularDistributionFormulas formula);
EnergyAndAngularDistributionFormulas StringToEnergyAndAngularDistributionFormulas(const std::string& formulaStr);
TF2 EnergyAndAngularDistributionFormulasToRootFormula(EnergyAndAngularDistributionFormulas formula);

}  // namespace TRestGeant4PrimaryGeneratorTypes
