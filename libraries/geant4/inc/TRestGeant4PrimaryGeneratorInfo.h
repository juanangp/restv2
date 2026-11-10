#ifndef TRESTGEANT4PRIMARYGENERATORINFO_H
#define TRESTGEANT4PRIMARYGENERATORINFO_H

#include <Math/Vector3D.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "TRestGeant4ParticleSource.h"
#include "TRestGeant4SpatialGenerator.h"
#include "TRestMetadata.h"

/// \class TRestGeant4PrimaryGeneratorInfo
/// \brief The `generator` section of TRestGeant4Metadata: the spatial generator (its `type` selects a
/// TRestGeant4SpatialGenerator, which reads its fields from this same section), the optional
/// `spatialDensityFunction` and the `source` (one or a list of TRestGeant4ParticleSource; none for the
/// cry generator, which gives the particles). Each part is
/// configured with UpdateParamsFromYAML and written back with UpdateYAMLFromParams, so the stored YAML
/// has the values used (e.g. the cosmic radius). The options are documented in
/// TRestGeant4PrimaryGenerator, which samples the primaries.
///
///     generator:
///       type: "point"                  # point, volume, surface, cosmic or cry
///       position: [0, 0, -200 mm]
///       source:
///         particle: "e-"
///         angular:
///           type: "flux"
///           direction: [0, 0, 1]
///         energy:
///           type: "mono"
///           energy: 30 keV
///
/// \author Luis Obis <luis.antonio.obis@gmail.com>
/// \author JuanAn García <juanangp@unizar.es>
class TRestGeant4PrimaryGeneratorInfo : public TRestMetadata {
   public:
    /// TF3 formula of x, y, z (mm) in [0, 1], the probability to accept a volume or surface position
    std::string fSpatialDensityFunction;

    /// `source`: one or a list of TRestGeant4ParticleSource (none for the cry generator)
    std::vector<TRestGeant4ParticleSource> fSources;  //!

    TRestGeant4PrimaryGeneratorInfo() = default;
    /// \brief Configured from the `generator` section.
    TRestGeant4PrimaryGeneratorInfo(const std::string& name, const YAML::Node& node);
    /// \brief The copies have their own spatial generator and sources (Clone)
    TRestGeant4PrimaryGeneratorInfo(const TRestGeant4PrimaryGeneratorInfo& other);
    /// \brief The copy has its own spatial generator, sources and YAML section (Clone)
    TRestGeant4PrimaryGeneratorInfo& operator=(const TRestGeant4PrimaryGeneratorInfo& other);
    /// \brief Reads `source` and `spatialDensityFunction`, creates the spatial generator of `type` and
    /// checks them: the cry generator has no source and the others need one, the density needs a volume
    /// or surface generator and the cosmic one a single, not Decay0, source (std::runtime_error).
    void LoadConfig() override;
    /// \brief No-op.
    void Initialize() override {}
    /// \brief Returns "TRestGeant4PrimaryGeneratorInfo".
    std::string GetClassName() const override { return "TRestGeant4PrimaryGeneratorInfo"; }

    /// \brief The fields of the spatial generator, which reads this same section
    std::set<std::string> GetSharedNodeKeys() const override;

    /// \brief Builds the distributions of the sources and opens the cry showers file, needed only to
    /// generate (it checks them before the simulation, each thread loads its own copy of the
    /// distributions, the showers file is shared)
    void LoadDistributions();

    /// \brief The spatial generator of `type`
    inline const TRestGeant4SpatialGenerator* GetSpatialGenerator() const { return fSpatialGenerator.get(); }
    inline size_t GetNumberOfSources() const { return fSources.size(); }  ///< Number of sources
    /// \brief True if a source is a Decay0 file
    bool HasDecay0Sources() const;

    /// \brief True for the cosmic generator
    bool IsCosmic() const;
    /// \brief True for the cry generator (the showers of a restCRY file, no sources)
    bool IsCRY() const;
    /// \brief Showers in the file of the cry generator, once open (0 if not cry)
    int64_t GetCRYShowersInFile() const;
    /// \brief The corners of the CRY square of the cry generator in world coordinates (empty if not cry)
    std::vector<ROOT::Math::XYZVector> GetCRYPlaneCorners() const;
    /// \brief The cry generator: sets the showers used and their exposure from the showers taken
    void UpdateCRYExposure();
    /// \brief Showers of the cry generator used and their exposure (time simulated, us), 0 if not cry
    int64_t GetCRYShowersUsed() const;
    /// \brief Exposure of the showers used by the cry generator (time simulated, us), 0 if not cry
    double GetCRYExposureTime() const;
    /// \brief Sets the showers used and the exposure of the cry generator (e.g. a sum of runs)
    void SetCRYExposure(int64_t showers, double exposureTime);
    /// \brief True if the positions are in a GDML volume (`from`) or on its surface
    bool IsGdml() const;
    /// \brief True if the positions are on the surface of a GDML volume
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

    /// \brief Prints the generator class, the GDML volume, the cosmic surface, the cry file, the density and
    /// the sources.
    void Print() const;

   private:
    std::unique_ptr<TRestGeant4SpatialGenerator> fSpatialGenerator;  //!

    void CopyGenerator(const TRestGeant4PrimaryGeneratorInfo& other);
};

#endif
