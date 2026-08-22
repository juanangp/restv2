#pragma once

#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <random>
#include <limits>

// ROOT MathCore and Core dependencies
#include <Math/Vector3D.h>
#include <TF1.h>
#include <TF2.h>

#include "TRestGeant4ParticleSource.h"
#include "TRestGeant4ParticleState.h"
#include "TRestGeant4PrimaryGeneratorTypes.h"

#include "TRestMetadata.h"

/// \class TRestGeant4PrimaryGeneratorInfo
/// \brief Class to store global spatial parameters, geometry shapes, and boundaries for primary event
/// generators.
/// \class TRestGeant4PrimaryGeneratorInfo
/// \brief Class to store global spatial parameters, geometry shapes, and boundaries for primary event
/// generators.
class TRestGeant4PrimaryGeneratorInfo : public TRestMetadata {
   public:
    std::string fSpatialGeneratorType = "point";
    std::string fSpatialGeneratorShape = "box";
    std::string fSpatialGeneratorFrom = "World";

    std::array<TRestWithUnits, 3> fSpatialGeneratorPosition = {0.0, 0.0, 0.0};
    std::array<TRestWithUnits, 3> fSpatialGeneratorRotationAxis = {0.0, 0.0, 1.0};
    TRestWithUnits fSpatialGeneratorRotationValue = 0.0;
    std::array<TRestWithUnits, 3> fSpatialGeneratorSize = {0.0, 0.0, 0.0};
    std::array<TRestWithUnits, 3> fSpatialGeneratorWorldSize = {0.0, 0.0, 0.0};
    std::string fSpatialGeneratorSpatialDensityFunction = "";

    // 2. COMPOSITION UNIFICATION: The particle sources now live inside the generator info!
    std::vector<TRestGeant4ParticleSource> fParticleSources;  //! YAML-managed runtime sources
    std::vector<std::vector<TRestGeant4ParticleState>> fGeneratedParticles;  //! Current runtime state
    std::vector<std::vector<std::vector<TRestGeant4ParticleState>>> fGeneratedParticleTemplates;  //! DECAY0 templates
    std::mt19937 fTemplateRandom;
    long fSeed = 0;

    // Standard constructor definitions matching the pipeline
    TRestGeant4PrimaryGeneratorInfo();
    TRestGeant4PrimaryGeneratorInfo(const std::string& name, const YAML::Node& node);
    virtual ~TRestGeant4PrimaryGeneratorInfo();

    void LoadConfig() override;
    void Initialize() override {}
    void RemoveParticleSources();
    void SetGeneratedParticleSeed(long seed);
    void UpdateGeneratedParticles();
    bool HasGeneratedParticleTemplates() const;
    const std::vector<TRestGeant4ParticleState>& GetGeneratedParticles(size_t sourceIndex) const;

    void Print() const;
    // --- Inline analytical getters (explicit conversions from std::array to XYZVector) ---
    inline std::string GetSpatialGeneratorType() const { return fSpatialGeneratorType; }
    inline std::string GetSpatialGeneratorShape() const { return fSpatialGeneratorShape; }
    inline std::string GetSpatialGeneratorFrom() const { return fSpatialGeneratorFrom; }

    inline ROOT::Math::XYZVector GetSpatialGeneratorPosition() const {
        return ROOT::Math::XYZVector(fSpatialGeneratorPosition[0].value, fSpatialGeneratorPosition[1].value,
                                     fSpatialGeneratorPosition[2].value);
    }
    inline ROOT::Math::XYZVector GetSpatialGeneratorRotationAxis() const {
        return ROOT::Math::XYZVector(fSpatialGeneratorRotationAxis[0].value, fSpatialGeneratorRotationAxis[1].value,
                                     fSpatialGeneratorRotationAxis[2].value);
    }
    inline double GetSpatialGeneratorRotationValue() const { return fSpatialGeneratorRotationValue.value; }

    inline ROOT::Math::XYZVector GetSpatialGeneratorSize() const {
        return ROOT::Math::XYZVector(fSpatialGeneratorSize[0].value, fSpatialGeneratorSize[1].value,
                                     fSpatialGeneratorSize[2].value);
    }
    inline ROOT::Math::XYZVector GetSpatialGeneratorWorldSize() const {
        return ROOT::Math::XYZVector(fSpatialGeneratorWorldSize[0].value, fSpatialGeneratorWorldSize[1].value,
                                     fSpatialGeneratorWorldSize[2].value);
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
