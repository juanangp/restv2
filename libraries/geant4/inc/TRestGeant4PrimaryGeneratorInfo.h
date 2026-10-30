#ifndef TRESTGEANT4PRIMARYGENERATORINFO_H
#define TRESTGEANT4PRIMARYGENERATORINFO_H

#include <Math/Vector3D.h>

#include <memory>
#include <string>
#include <vector>

#include "TRestGeant4ParticleSource.h"
#include "TRestGeant4SpatialGenerator.h"
#include "TRestMetadata.h"

/// \class TRestGeant4PrimaryGeneratorInfo
/// \brief The `generator` section of TRestGeant4Metadata: the spatial generator (its `type` selects a
/// TRestGeant4SpatialGenerator, which reads its fields from this same section), the optional
/// `spatialDensityFunction` and the `source` (one or a list of TRestGeant4ParticleSource). Each part is
/// configured with UpdateParamsFromYAML and written back with UpdateYAMLFromParams, so the stored YAML
/// has the values used (e.g. the cosmic radius). The options are documented in
/// TRestGeant4PrimaryGenerator, which samples the primaries.
class TRestGeant4PrimaryGeneratorInfo : public TRestMetadata {
   public:
    /// TF3 formula of x, y, z (mm) in [0, 1], the probability to accept a volume or surface position
    std::string fSpatialDensityFunction;

    std::vector<TRestGeant4ParticleSource> fSources;  //!

    TRestGeant4PrimaryGeneratorInfo() = default;
    TRestGeant4PrimaryGeneratorInfo(const std::string& name, const YAML::Node& node);
    /// \brief The copies have their own spatial generator and sources (Clone)
    TRestGeant4PrimaryGeneratorInfo(const TRestGeant4PrimaryGeneratorInfo& other);
    TRestGeant4PrimaryGeneratorInfo& operator=(const TRestGeant4PrimaryGeneratorInfo& other);
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestGeant4PrimaryGeneratorInfo"; }

    /// \brief The fields of the spatial generator, which reads this same section
    std::set<std::string> GetSharedNodeKeys() const override;

    /// \brief Builds the distributions of the sources, needed only to generate (it checks them before
    /// the simulation, each thread loads its own copy)
    void LoadDistributions();

    inline const TRestGeant4SpatialGenerator* GetSpatialGenerator() const { return fSpatialGenerator.get(); }
    inline size_t GetNumberOfSources() const { return fSources.size(); }
    bool HasDecay0Sources() const;

    bool IsCosmic() const;
    /// \brief True if the positions are in a GDML volume (`from`) or on its surface
    bool IsGdml() const;
    bool IsGdmlSurface() const;
    /// \brief The `from` volume of a GDML generator
    std::string GetGdmlVolume() const;

    /// \brief Placement of the GDML volume in the world, from the geometry (restG4)
    void SetGdmlPlacement(const ROOT::Math::XYZVector& center, const ROOT::Math::XYZVector& axis,
                          double angle);
    /// \brief The cosmic radius if not given (restG4: the sphere circumscribing the world)
    void SetCosmicRadius(double radius);
    /// \brief Cosmic generation surface in cm2 (0 if not cosmic)
    double GetCosmicSurfaceCm2() const;

    void Print() const;

   private:
    std::unique_ptr<TRestGeant4SpatialGenerator> fSpatialGenerator;  //!

    void CopyGenerator(const TRestGeant4PrimaryGeneratorInfo& other);
};

#endif
