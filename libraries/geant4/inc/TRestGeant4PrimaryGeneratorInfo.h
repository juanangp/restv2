#pragma once

#include <string>
#include <utility>
#include <iostream>
#include <cmath>

// ROOT MathCore and Core dependencies
#include <Math/Vector3D.h>
#include <TF1.h>
#include <TF2.h>

namespace TRestGeant4PrimaryGeneratorTypes {

enum class SpatialGeneratorTypes {
    CUSTOM,
    VOLUME,
    SURFACE,
    POINT,
    COSMIC,
    SOURCE
};

std::string SpatialGeneratorTypesToString(SpatialGeneratorTypes type);
SpatialGeneratorTypes StringToSpatialGeneratorTypes(const std::string& typeStr);

enum class SpatialGeneratorShapes {
    GDML,
    WALL,
    CIRCLE,
    BOX,
    SPHERE,
    CYLINDER
};

std::string SpatialGeneratorShapesToString(SpatialGeneratorShapes shape);
SpatialGeneratorShapes StringToSpatialGeneratorShapes(const std::string& shapeStr);

enum class EnergyDistributionTypes {
    TH1D,
    TH2D,
    FORMULA,
    FORMULA2,
    MONO,
    FLAT,
    LOG
};

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

enum class AngularDistributionTypes {
    TH1D,
    TH2D,
    FORMULA,
    FORMULA2,
    ISOTROPIC,
    FLUX,
    BACK_TO_BACK
};

std::string AngularDistributionTypesToString(AngularDistributionTypes type);
AngularDistributionTypes StringToAngularDistributionTypes(const std::string& typeStr);

enum class AngularDistributionFormulas { 
    COS2, 
    COS3, 
    SIN_2THETA 
};

std::string AngularDistributionFormulasToString(AngularDistributionFormulas formula);
AngularDistributionFormulas StringToAngularDistributionFormulas(const std::string& formulaStr);
TF1 AngularDistributionFormulasToRootFormula(AngularDistributionFormulas formula);

enum class EnergyAndAngularDistributionFormulas {
    COSMIC_MUONS
};

std::string EnergyAndAngularDistributionFormulasToString(EnergyAndAngularDistributionFormulas formula);
EnergyAndAngularDistributionFormulas StringToEnergyAndAngularDistributionFormulas(const std::string& formulaStr);
TF2 EnergyAndAngularDistributionFormulasToRootFormula(EnergyAndAngularDistributionFormulas formula);

}  // namespace TRestGeant4PrimaryGeneratorTypes

/// \class TRestGeant4PrimaryGeneratorInfo
/// \brief Class to store global spatial parameters, geometry shapes, and boundaries for primary event generators.
/// \class TRestGeant4PrimaryGeneratorInfo
/// \brief Class to store global spatial parameters, geometry shapes, and boundaries for primary event generators.
class TRestGeant4PrimaryGeneratorInfo {
   public:
    std::string fSpatialGeneratorType = "point";
    std::string fSpatialGeneratorShape;
    std::string fSpatialGeneratorFrom;

    std::array<double, 3> fSpatialGeneratorPosition = {0.0, 0.0, 0.0};
    std::array<double, 3> fSpatialGeneratorRotationAxis = {0.0, 0.0, 0.0};
    double fSpatialGeneratorRotationValue = 0.0;
    std::array<double, 3> fSpatialGeneratorSize = {0.0, 0.0, 0.0};
    
    std::string fSpatialGeneratorSpatialDensityFunction;
    std::array<double, 3> fSpatialGeneratorWorldSize = {0.0, 0.0, 0.0};

    TRestGeant4PrimaryGeneratorInfo() = default;
    virtual ~TRestGeant4PrimaryGeneratorInfo() = default;

    void Print() const;
    // --- Inline Analytical Getters (Conversiones explícitas desde std::array a XYZVector) ---
    inline std::string GetSpatialGeneratorType() const { return fSpatialGeneratorType; }
    inline std::string GetSpatialGeneratorShape() const { return fSpatialGeneratorShape; }
    inline std::string GetSpatialGeneratorFrom() const { return fSpatialGeneratorFrom; }
    
    inline ROOT::Math::XYZVector GetSpatialGeneratorPosition() const { 
        return ROOT::Math::XYZVector(fSpatialGeneratorPosition[0], fSpatialGeneratorPosition[1], fSpatialGeneratorPosition[2]); 
    }
    inline ROOT::Math::XYZVector GetSpatialGeneratorRotationAxis() const { 
        return ROOT::Math::XYZVector(fSpatialGeneratorRotationAxis[0], fSpatialGeneratorRotationAxis[1], fSpatialGeneratorRotationAxis[2]); 
    }
    inline double GetSpatialGeneratorRotationValue() const { return fSpatialGeneratorRotationValue; }
    
    inline ROOT::Math::XYZVector GetSpatialGeneratorSize() const { 
        return ROOT::Math::XYZVector(fSpatialGeneratorSize[0], fSpatialGeneratorSize[1], fSpatialGeneratorSize[2]); 
    }
    inline ROOT::Math::XYZVector GetSpatialGeneratorWorldSize() const { 
        return ROOT::Math::XYZVector(fSpatialGeneratorWorldSize[0], fSpatialGeneratorWorldSize[1], fSpatialGeneratorWorldSize[2]); 
    }

    /// \brief Returns cosmic generator radius (mm) extracted from world size constraints.
    inline double GetSpatialGeneratorCosmicRadius() const { 
        double mag2 = fSpatialGeneratorWorldSize[0] * fSpatialGeneratorWorldSize[0] + 
                      fSpatialGeneratorWorldSize[1] * fSpatialGeneratorWorldSize[1] + 
                      fSpatialGeneratorWorldSize[2] * fSpatialGeneratorWorldSize[2];
        return std::sqrt(mag2); 
    }

    /// \brief Returns cosmic surface cross section term (cm2) for simulation exposure computations.
    inline double GetSpatialGeneratorCosmicSurfaceTermCm2() const {
        const double radius = GetSpatialGeneratorCosmicRadius();
        return M_PI * radius * radius * 0.01;
    }

    inline std::string GetSpatialGeneratorSpatialDensityFunction() const {
        return fSpatialGeneratorSpatialDensityFunction;
    }

    // Grants structural reflection and framework pipeline injection privileges
    friend class TRestGeant4Metadata;
    friend class DetectorConstruction;
    friend class TRestMetadataFieldRegistry;
};
