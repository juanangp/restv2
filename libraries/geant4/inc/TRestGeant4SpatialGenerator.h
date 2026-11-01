#ifndef TRESTGEANT4SPATIALGENERATOR_H
#define TRESTGEANT4SPATIALGENERATOR_H

#include <Math/Vector3D.h>
#include <TRandom.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "TRestGeant4ParticleState.h"
#include "TRestMetadata.h"

/// \class TRestGeant4SpatialGenerator
/// \brief Where the primaries start: the generator `type` (point, volume, surface, cosmic or cry) selects the
/// derived class, which reads its own fields from the `generator` section. Positions in mm, world
/// coordinates. See TRestGeant4PrimaryGenerator for the options.
class TRestGeant4SpatialGenerator : public TRestMetadata {
   public:
    std::string fType;

    TRestGeant4SpatialGenerator() = default;
    TRestGeant4SpatialGenerator(const std::string& name, const YAML::Node& node)
        : TRestMetadata(name, node) {}

    /// \brief The derived class of the `type` of the node, configured from it
    static std::unique_ptr<TRestGeant4SpatialGenerator> Create(const std::string& name,
                                                               const YAML::Node& node);

    virtual std::unique_ptr<TRestGeant4SpatialGenerator> Clone() const = 0;

    /// \brief A position (for a GDML volume it needs the Geant4 geometry, see IsGdml)
    virtual ROOT::Math::XYZVector Sample(TRandom& random) = 0;

    /// \brief The generator center (the reference of an inward direction)
    virtual ROOT::Math::XYZVector GetCenter() const { return {}; }

    /// \brief True if the positions are given by the Geant4 geometry (TRestGeant4PrimaryGenerator)
    virtual bool IsGdml() const { return false; }

    /// \brief True if the direction reference is toward the center (as legacy, a sphere surface)
    virtual bool IsInward() const { return false; }

    void Initialize() override {}
};

/// \class TRestGeant4SpatialGeneratorPoint
/// \brief `type: "point"`: all the primaries start at `position`.
class TRestGeant4SpatialGeneratorPoint : public TRestGeant4SpatialGenerator {
   public:
    std::array<TRestWithUnits, 3> fPosition = {0.0, 0.0, 0.0};

    TRestGeant4SpatialGeneratorPoint() = default;
    TRestGeant4SpatialGeneratorPoint(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4SpatialGeneratorPoint"; }

    std::unique_ptr<TRestGeant4SpatialGenerator> Clone() const override {
        return std::make_unique<TRestGeant4SpatialGeneratorPoint>(*this);
    }
    ROOT::Math::XYZVector Sample(TRandom& random) override;
    ROOT::Math::XYZVector GetCenter() const override;
};

/// \class TRestGeant4SpatialGeneratorShape
/// \brief Base of the volume and surface generators: a GDML volume (`from`) or a `shape` with `size`,
/// centered at `position` and rotated by `rotationValue` around `rotationAxis`.
class TRestGeant4SpatialGeneratorShape : public TRestGeant4SpatialGenerator {
   public:
    std::string fFrom;
    std::string fShape;
    std::array<TRestWithUnits, 3> fSize = {0.0, 0.0, 0.0};
    std::array<TRestWithUnits, 3> fPosition = {0.0, 0.0, 0.0};
    std::array<TRestWithUnits, 3> fRotationAxis = {0.0, 0.0, 1.0};
    TRestWithUnits fRotationValue = 0.0;

    TRestGeant4SpatialGeneratorShape() = default;
    TRestGeant4SpatialGeneratorShape(const std::string& name, const YAML::Node& node)
        : TRestGeant4SpatialGenerator(name, node) {}

    ROOT::Math::XYZVector GetCenter() const override;
    bool IsGdml() const override { return !fFrom.empty(); }

    /// \brief Placement of the GDML volume in the world, set from the geometry (restG4)
    void SetGdmlPlacement(const ROOT::Math::XYZVector& center, const ROOT::Math::XYZVector& axis,
                          double angle);

   protected:
    /// \brief Checks `from` or `shape` (one of `shapes`) and the number of positive `size` values
    void CheckShape(const std::map<std::string, size_t>& shapes);
    /// \brief From shape coordinates to world coordinates
    ROOT::Math::XYZVector ToWorld(const ROOT::Math::XYZVector& local) const;
};

/// \class TRestGeant4SpatialGeneratorVolume
/// \brief `type: "volume"`: uniform inside a GDML volume (`from`) or a shape: `box` (size: x, y, z),
/// `sphere` (size: radius) or `cylinder` (size: radius, length; axis Z).
class TRestGeant4SpatialGeneratorVolume : public TRestGeant4SpatialGeneratorShape {
   public:
    TRestGeant4SpatialGeneratorVolume() = default;
    TRestGeant4SpatialGeneratorVolume(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4SpatialGeneratorVolume"; }

    std::unique_ptr<TRestGeant4SpatialGenerator> Clone() const override {
        return std::make_unique<TRestGeant4SpatialGeneratorVolume>(*this);
    }
    ROOT::Math::XYZVector Sample(TRandom& random) override;
};

/// \class TRestGeant4SpatialGeneratorSurface
/// \brief `type: "surface"`: on the surface of a GDML volume (`from`) or a shape: `sphere` (size:
/// radius), `cylinder` (size: radius, length; lateral surface, axis Z), `circle` (size: radius; XY plane)
/// or `wall` (size: x, y; XY plane). On a sphere the direction reference points to its center.
class TRestGeant4SpatialGeneratorSurface : public TRestGeant4SpatialGeneratorShape {
   public:
    TRestGeant4SpatialGeneratorSurface() = default;
    TRestGeant4SpatialGeneratorSurface(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4SpatialGeneratorSurface"; }

    std::unique_ptr<TRestGeant4SpatialGenerator> Clone() const override {
        return std::make_unique<TRestGeant4SpatialGeneratorSurface>(*this);
    }
    ROOT::Math::XYZVector Sample(TRandom& random) override;
    bool IsInward() const override { return fShape == "sphere"; }
};

/// \class TRestGeant4SpatialGeneratorCosmic
/// \brief `type: "cosmic"`: the origin depends on the direction, a uniform point of the disk of radius
/// `radius` through the world center normal to the direction, moved back to the sphere of that radius.
/// The radius is, if not given, the one of the sphere circumscribing the world (written to the YAML).
class TRestGeant4SpatialGeneratorCosmic : public TRestGeant4SpatialGenerator {
   public:
    TRestWithUnits fRadius = 0.0;

    TRestGeant4SpatialGeneratorCosmic() = default;
    TRestGeant4SpatialGeneratorCosmic(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4SpatialGeneratorCosmic"; }

    std::unique_ptr<TRestGeant4SpatialGenerator> Clone() const override {
        return std::make_unique<TRestGeant4SpatialGeneratorCosmic>(*this);
    }
    /// \brief Not used, the origin needs the direction (SampleFromDirection)
    ROOT::Math::XYZVector Sample(TRandom& random) override;
    ROOT::Math::XYZVector SampleFromDirection(TRandom& random, const ROOT::Math::XYZVector& direction) const;

    /// \brief Sets the radius if not given, also in the YAML node to be stored
    void SetDefaultRadius(double radius);
    /// \brief Generation surface (the sphere cross section) in cm2, to normalize to a flux
    double GetSurfaceCm2() const { return M_PI * fRadius * fRadius * 0.01; }
};

/// \class TRestGeant4SpatialGeneratorCRY
/// \brief `type: "cry"`: the particles and positions of the showers of a restCRY file (`filename`, see
/// TRestGeant4CRYShowers), one per event, in order. The CRY plane (z = 0) is centered at `position` and its
/// +Z (up) is `verticalAxis`. The showers are read by all the copies (threads) from the same file, each one
/// once; the generator has no sources.
class TRestGeant4SpatialGeneratorCRY : public TRestGeant4SpatialGenerator {
   public:
    std::string fFilename;
    std::array<TRestWithUnits, 3> fPosition = {0.0, 0.0, 0.0};
    std::array<TRestWithUnits, 3> fVerticalAxis = {0.0, 1.0, 0.0};

    /// Results, written at the end of the simulation: showers simulated and their exposure (time simulated
    /// by CRY times the fraction of the showers of the file simulated)
    int64_t fShowersUsed = 0;
    TRestWithUnits fExposureTime = 0.0;

    TRestGeant4SpatialGeneratorCRY() = default;
    TRestGeant4SpatialGeneratorCRY(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4SpatialGeneratorCRY"; }

    /// \brief A copy reading the same file (the showers are shared)
    std::unique_ptr<TRestGeant4SpatialGenerator> Clone() const override {
        return std::make_unique<TRestGeant4SpatialGeneratorCRY>(*this);
    }
    /// \brief Not used, the positions are the ones of the showers (NextShower)
    ROOT::Math::XYZVector Sample(TRandom& random) override;
    ROOT::Math::XYZVector GetCenter() const override;

    /// \brief Opens the file, shared by the copies made after it (restG4 master thread)
    void Open();
    /// \brief The particles of the next shower not simulated yet, in world coordinates (thread safe).
    /// False if all the showers of the file were simulated
    bool NextShower(std::vector<TRestGeant4ParticleState>& particles);

    /// \brief Showers in the file (the file must be open)
    int64_t GetShowersInFile() const;

    /// \brief The corners of the CRY square (subBoxLength of the file) in world coordinates, to check that
    /// it is inside the world (the file must be open)
    std::vector<ROOT::Math::XYZVector> GetPlaneCorners() const;

    /// \brief Sets showersUsed and exposureTime from the showers taken (all the copies), also in the YAML
    void UpdateExposure();
    /// \brief Sets showersUsed and exposureTime, also in the YAML (e.g. a sum of runs)
    void SetExposure(int64_t showers, double exposureTime);

   private:
    struct Reader;
    std::shared_ptr<Reader> fReader;  //!

    /// \brief From CRY coordinates (+Z up) to the world: rotation of +Z to verticalAxis
    ROOT::Math::XYZVector RotateToWorld(const ROOT::Math::XYZVector& v) const;
};

#endif
