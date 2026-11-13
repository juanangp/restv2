#ifndef TRESTGEANT4SPATIALGENERATOR_H
#define TRESTGEANT4SPATIALGENERATOR_H

#include <Math/Vector3D.h>
#include <TRandom.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
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
///
///     generator:
///       type: "volume"                 # point, volume, surface, cosmic or cry
///       shape: "box"
///       size: [10 cm, 10 cm, 10 cm]
///       position: [0, 0, 0]
///       rotationAxis: [0, 0, 1]
///       rotationValue: 0 deg
///       source: ...                    # TRestGeant4ParticleSource
///
/// \author JuanAn García <juanangp@unizar.es>
class TRestGeant4SpatialGenerator : public TRestMetadata {
   public:
    std::string fType;  ///< `type`, case insensitive (it selects the derived class in Create)

    /// \brief Empty generator.
    TRestGeant4SpatialGenerator() = default;
    /// \brief Keeps the node (the `generator` section); the derived classes read their fields from it.
    TRestGeant4SpatialGenerator(const std::string& name, const YAML::Node& node)
        : TRestMetadata(name, node) {}

    /// \brief The derived class of the `type` of the node, configured from it
    /// \param name Instance name.
    /// \param node The `generator` section; throws if `type` is missing or unknown.
    static std::unique_ptr<TRestGeant4SpatialGenerator> Create(const std::string& name,
                                                               const YAML::Node& node);

    /// \brief A copy, e.g. for a thread
    virtual std::unique_ptr<TRestGeant4SpatialGenerator> Clone() const = 0;

    /// \brief A position (for a GDML volume it needs the Geant4 geometry, see IsGdml)
    virtual ROOT::Math::XYZVector Sample(TRandom& random) = 0;

    /// \brief The generator center (the reference of an inward direction)
    virtual ROOT::Math::XYZVector GetCenter() const { return {}; }

    /// \brief True if the positions are given by the Geant4 geometry (TRestGeant4PrimaryGenerator)
    virtual bool IsGdml() const { return false; }

    /// \brief True if the direction reference is toward the center (as legacy, a sphere surface)
    virtual bool IsInward() const { return false; }

    /// \brief No-op.
    void Initialize() override {}
};

/// \class TRestGeant4SpatialGeneratorPoint
/// \brief `type: "point"`: all the primaries start at `position`.
class TRestGeant4SpatialGeneratorPoint : public TRestGeant4SpatialGenerator {
   public:
    std::array<TRestWithUnits, 3> fPosition = {0.0, 0.0, 0.0};  ///< `position` (mm)

    TRestGeant4SpatialGeneratorPoint() = default;
    /// \brief Configured from the `generator` section.
    TRestGeant4SpatialGeneratorPoint(const std::string& name, const YAML::Node& node);
    /// \brief Reads `type` and `position` and writes them back.
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4SpatialGeneratorPoint"; }

    std::unique_ptr<TRestGeant4SpatialGenerator> Clone() const override {
        return std::make_unique<TRestGeant4SpatialGeneratorPoint>(*this);
    }
    /// \brief Returns `position`.
    ROOT::Math::XYZVector Sample(TRandom& random) override;
    /// \brief Returns `position`.
    ROOT::Math::XYZVector GetCenter() const override;
};

/// \class TRestGeant4SpatialGeneratorShape
/// \brief Base of the volume and surface generators: a GDML volume (`from`) or a `shape` with `size`,
/// centered at `position` and rotated by `rotationValue` around `rotationAxis`.
class TRestGeant4SpatialGeneratorShape : public TRestGeant4SpatialGenerator {
   public:
    std::string fFrom;   ///< `from`: GDML physical (or logical) volume; if given, no shape
    std::string fShape;  ///< `shape` (lower case), see the derived classes
    std::array<TRestWithUnits, 3> fSize = {0.0, 0.0, 0.0};          ///< `size` (mm)
    std::array<TRestWithUnits, 3> fPosition = {0.0, 0.0, 0.0};      ///< `position`: center (mm)
    std::array<TRestWithUnits, 3> fRotationAxis = {0.0, 0.0, 1.0};  ///< `rotationAxis`
    TRestWithUnits fRotationValue = 0.0;                            ///< `rotationValue` (rad)

    TRestGeant4SpatialGeneratorShape() = default;
    /// \brief Keeps the node; the derived classes read their fields from it.
    TRestGeant4SpatialGeneratorShape(const std::string& name, const YAML::Node& node)
        : TRestGeant4SpatialGenerator(name, node) {}

    /// \brief Returns `position`.
    ROOT::Math::XYZVector GetCenter() const override;
    /// \brief True with `from`.
    bool IsGdml() const override { return !fFrom.empty(); }

    /// \brief Placement of the GDML volume in the world, set from the geometry (restG4): it sets `position`,
    /// `rotationAxis` and `rotationValue`.
    void SetGdmlPlacement(const ROOT::Math::XYZVector& center, const ROOT::Math::XYZVector& axis,
                          double angle);

   protected:
    /// \brief Checks `from` or `shape` (one of `shapes`) and the number of positive `size` values
    /// \param shapes Valid shape names and their number of `size` values (throws std::runtime_error).
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
    /// \brief Configured from the `generator` section.
    TRestGeant4SpatialGeneratorVolume(const std::string& name, const YAML::Node& node);
    /// \brief Reads the shape fields and checks them.
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4SpatialGeneratorVolume"; }

    std::unique_ptr<TRestGeant4SpatialGenerator> Clone() const override {
        return std::make_unique<TRestGeant4SpatialGeneratorVolume>(*this);
    }
    /// \brief A uniform position inside the shape (throws for a GDML volume, see IsGdml).
    ROOT::Math::XYZVector Sample(TRandom& random) override;
};

/// \class TRestGeant4SpatialGeneratorSurface
/// \brief `type: "surface"`: on the surface of a GDML volume (`from`) or a shape: `sphere` (size:
/// radius), `cylinder` (size: radius, length; lateral surface, axis Z), `circle` (size: radius; XY plane)
/// or `wall` (size: x, y; XY plane). On a sphere the direction reference points to its center.
class TRestGeant4SpatialGeneratorSurface : public TRestGeant4SpatialGeneratorShape {
   public:
    TRestGeant4SpatialGeneratorSurface() = default;
    /// \brief Configured from the `generator` section.
    TRestGeant4SpatialGeneratorSurface(const std::string& name, const YAML::Node& node);
    /// \brief Reads the shape fields and checks them.
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4SpatialGeneratorSurface"; }

    std::unique_ptr<TRestGeant4SpatialGenerator> Clone() const override {
        return std::make_unique<TRestGeant4SpatialGeneratorSurface>(*this);
    }
    /// \brief A uniform position on the surface of the shape (throws for a GDML volume, see IsGdml).
    ROOT::Math::XYZVector Sample(TRandom& random) override;
    /// \brief True for a sphere.
    bool IsInward() const override { return fShape == "sphere"; }
};

/// \class TRestGeant4SpatialGeneratorCosmic
/// \brief `type: "cosmic"`: the origin depends on the direction, a uniform point of the disk of radius
/// `radius` through the world center normal to the direction, moved back to the sphere of that radius.
/// The radius is, if not given, the one of the sphere circumscribing the world (written to the YAML).
class TRestGeant4SpatialGeneratorCosmic : public TRestGeant4SpatialGenerator {
   public:
    TRestWithUnits fRadius = 0.0;  ///< `radius` (mm, 0: the world one, set by restG4)

    TRestGeant4SpatialGeneratorCosmic() = default;
    /// \brief Configured from the `generator` section.
    TRestGeant4SpatialGeneratorCosmic(const std::string& name, const YAML::Node& node);
    /// \brief Reads `type` and `radius` (not negative).
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4SpatialGeneratorCosmic"; }

    std::unique_ptr<TRestGeant4SpatialGenerator> Clone() const override {
        return std::make_unique<TRestGeant4SpatialGeneratorCosmic>(*this);
    }
    /// \brief Not used, the origin needs the direction (SampleFromDirection); it throws
    ROOT::Math::XYZVector Sample(TRandom& random) override;
    /// \brief The origin of a particle with this direction: a uniform point of the disk of radius
    /// `radius` through the world center normal to the direction, moved back along it to the sphere
    /// (throws if the radius is not set).
    ROOT::Math::XYZVector SampleFromDirection(TRandom& random, const ROOT::Math::XYZVector& direction) const;

    /// \brief Sets the radius if not given, also in the YAML node to be stored
    void SetDefaultRadius(double radius);
    /// \brief Generation surface (the sphere cross section) in cm2, to normalize to a flux
    double GetSurfaceCm2() const { return M_PI * fRadius * fRadius * 0.01; }
};

class TRestGeant4CRY;

/// \class TRestGeant4CRYEngine
/// \brief The CRY library behind the cry generator. This library does not depend on CRY: restG4, built with
/// CRY, gives the engine (TRestGeant4SpatialGeneratorCRY::SetEngineFactory).
class TRestGeant4CRYEngine {
   public:
    virtual ~TRestGeant4CRYEngine() = default;
    /// \brief The particles of a new shower, in CRY coordinates (horizontal plane z = 0, +Z up): Geant4
    /// names, positions in mm, kinetic energies in keV, times in us relative to the first particle
    virtual std::vector<TRestGeant4ParticleState> Generate() = 0;
    /// \brief Time simulated by CRY so far (us): the exposure of the showers generated
    virtual double GetTimeSimulated() const = 0;
};

/// \class TRestGeant4SpatialGeneratorCRY
/// \brief `type: "cry"`: the cosmic-ray showers of CRY, generated during the simulation, one per event with
/// all its particles, with the options of the TRestGeant4CRY section (SetCRY). The CRY plane (z = 0, the
/// square of subBoxLength) is centered at `position` and its +Z (up) is `verticalAxis`. The copies
/// (threads) share the CRY generator; the generator has no sources.
class TRestGeant4SpatialGeneratorCRY : public TRestGeant4SpatialGenerator {
   public:
    std::array<TRestWithUnits, 3> fPosition = {0.0, 0.0, 0.0};      ///< `position`: CRY plane center (mm)
    std::array<TRestWithUnits, 3> fVerticalAxis = {0.0, 1.0, 0.0};  ///< `verticalAxis`: the CRY +Z (up)

    /// Results, written at the end of the simulation: showers simulated and their exposure (time simulated
    /// by CRY), to normalize to a rate
    int64_t fShowersUsed = 0;
    TRestWithUnits fExposureTime = 0.0;  ///< `exposureTime`: exposure of the showers used (us)

    /// The engine of CRY for a configuration (CRYSetup string, GetCRYSetup) and a seed
    using EngineFactory = std::function<std::unique_ptr<TRestGeant4CRYEngine>(const std::string&, long)>;

    TRestGeant4SpatialGeneratorCRY() = default;
    /// \brief Configured from the `generator` section.
    TRestGeant4SpatialGeneratorCRY(const std::string& name, const YAML::Node& node);
    /// \brief Reads `type`, `position`, `verticalAxis` and the results, and checks them.
    void LoadConfig() override;
    std::string GetClassName() const override { return "TRestGeant4SpatialGeneratorCRY"; }

    /// \brief A copy sharing the CRY generator (once started)
    std::unique_ptr<TRestGeant4SpatialGenerator> Clone() const override {
        return std::make_unique<TRestGeant4SpatialGeneratorCRY>(*this);
    }
    /// \brief Not used, the positions are the ones of the showers (NextShower); it throws
    ROOT::Math::XYZVector Sample(TRandom& random) override;
    /// \brief Returns `position`.
    ROOT::Math::XYZVector GetCenter() const override;

    /// \brief Sets the engine factory (restG4 built with CRY)
    static void SetEngineFactory(EngineFactory factory);
    /// \brief True if there is an engine factory (restG4 built with CRY)
    static bool HasEngine();
    /// \brief The CRY options of the TRestGeant4CRY section (TRestGeant4Manager::LoadConfig)
    void SetCRY(const TRestGeant4CRY& cry);
    /// \brief True once SetCRY was called
    bool HasCRY() const { return !fCRYSetup.empty(); }
    /// \brief maxExposureTime of the TRestGeant4CRY (us, 0: no limit)
    double GetMaxExposureTime() const { return fMaxExposureTime; }
    /// \brief Starts CRY with a seed, shared by the copies made after it (restG4 master thread). Throws if
    /// there is no TRestGeant4CRY or no engine (restG4 built without CRY)
    void Start(long seed);
    /// \brief The particles of a new shower, in world coordinates (thread safe). False if maxExposureTime
    /// was reached
    bool NextShower(std::vector<TRestGeant4ParticleState>& particles);

    /// \brief The configuration string of CRYSetup (lengths in m), of the TRestGeant4CRY
    inline const std::string& GetCRYSetup() const { return fCRYSetup; }

    /// \brief The corners of the CRY square (subBoxLength) in world coordinates, to check that it is inside
    /// the world
    std::vector<ROOT::Math::XYZVector> GetPlaneCorners() const;

    /// \brief Sets showersUsed and exposureTime from the showers generated (all the copies), also in the YAML
    void UpdateExposure();
    /// \brief Sets showersUsed and exposureTime, also in the YAML (e.g. a sum of runs)
    void SetExposure(int64_t showers, double exposureTime);

   private:
    /// From the TRestGeant4CRY (SetCRY): the CRYSetup string, subBoxLength and maxExposureTime (mm, us)
    std::string fCRYSetup;        //!
    double fSubBoxLength = 0;     //!
    double fMaxExposureTime = 0;  //!

    struct State;
    std::shared_ptr<State> fState;  //!

    /// \brief From CRY coordinates (+Z up) to the world: rotation of +Z to verticalAxis
    ROOT::Math::XYZVector RotateToWorld(const ROOT::Math::XYZVector& v) const;
};

#endif
