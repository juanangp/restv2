/// \class TRestGeant4PrimaryGenerator
/// Primary generator of restG4: samples, for each event, the primary particles (name, excitation,
/// charge, kinetic energy, direction and origin) described by the `generator` section of
/// TRestGeant4Metadata. Units: keV, mm and rad; any value can be given with units (e.g. `5.3 MeV`,
/// `2 cm`, `30 deg`).
///
/// ### Structure
///
/// Each section has a `type`, which selects the class that reads it; each class reads only its own
/// fields with UpdateParamsFromYAML, checks them (an error with a message if a needed one is missing or
/// not valid) and writes them back with UpdateYAMLFromParams, so the stored metadata has the values used.
///
///     generator:                      TRestGeant4PrimaryGeneratorInfo
///       type: "..."                   TRestGeant4SpatialGenerator: point, volume, surface or cosmic
///       ...                           its fields
///       spatialDensityFunction: "..." optional
///       source:                       TRestGeant4ParticleSource, one or a list
///         particle: "..."
///         energy:                     TRestGeant4EnergyDistribution
///           type: "..."               mono, flat, log, formula, TH1D (or formula2, TH2D, cosmic)
///         angular:                    TRestGeant4AngularDistribution
///           type: "..."               flux, isotropic, formula, TH1D (or formula2, TH2D, cosmic)
///
/// The types are case insensitive. All the sources of an event start at the same position (one primary
/// vertex per particle).
///
/// ### Generator type (TRestGeant4SpatialGenerator)
///
/// * **point** (TRestGeant4SpatialGeneratorPoint): `position: [x, y, z]`.
/// * **volume** (TRestGeant4SpatialGeneratorVolume): uniform inside
///   - a GDML volume, `from: "name"` (a physical volume, or a logical one with a single physical
///   volume), excluding its daughters; its placement in the world is taken from the geometry;
///   - or a `shape`: `box` (`size: [x, y, z]`), `sphere` (`size: [radius, 0, 0]`) or `cylinder`
///   (`size: [radius, length, 0]`, axis Z).
/// * **surface** (TRestGeant4SpatialGeneratorSurface): on the surface of
///   - a GDML volume, `from: "name"` (G4VSolid::GetPointOnSurface, not uniform for every solid);
///   - or a `shape`: `sphere` (`size: [radius, 0, 0]`), `cylinder` (`size: [radius, length, 0]`, lateral
///   surface, axis Z), `circle` (`size: [radius, 0, 0]`, XY plane) or `wall` (`size: [x, y, 0]`, XY
///   plane). On a sphere, as legacy, the angular `direction` is replaced by the one to its center.
///
///   The shapes are centered at `position` ([0, 0, 0] by default) and rotated by `rotationValue` (angle)
///   around `rotationAxis` ([0, 0, 1] by default).
/// * **cosmic** (TRestGeant4SpatialGeneratorCosmic): the direction is sampled first and the origin is a
/// uniform point of the disk of radius `radius` through the world center normal to that direction, moved
/// back to the sphere of that radius, so the flux through any surface inside the sphere is the one of the
/// distributions. `radius` is, if not given, the one of the sphere circumscribing the world box (written
/// to the stored metadata); the generation surface, to normalize to a flux, is pi radius^2
/// (TRestGeant4Metadata::GetCosmicGeneratorSurfaceCm2). It needs a single source, not a Decay0 one.
///
/// `spatialDensityFunction` (volume and surface) is a TF3 formula of x, y, z (mm, world coordinates)
/// with values in [0, 1]: each position is accepted with that probability (an error if it is > 1).
///
/// ### Source (TRestGeant4ParticleSource)
///
/// * `particle`: Geant4 name (e.g. "alpha", "e-", "gamma", "neutron", "mu-") or ion name (e.g. "Pb210"),
/// with `excitedLevel` (excitation energy, 0 by default) and `charge` (in units of e, 0 by default).
/// * `energy` and `angular`, below.
/// * Or only `use: "file.dat"`: the particles (gamma, e-, e+ and alpha, with their energy and direction)
/// of a random event of a Decay0 file.
///
/// ### Energy type (TRestGeant4EnergyDistribution)
///
/// * **mono**: `energy`.
/// * **flat**: uniform in `range: [min, max]`.
/// * **log**: uniform in log(E) in `range` (min > 0).
/// * **formula**: the named `formula` (CosmicNeutrons, FissionNeutronsU238, EnvironmentalGammas or
/// EnvironmentalNeutrons) in `range` (intersected with the formula one, an error if they do not overlap),
/// sampled with `formulaNPoints` (5000 by default).
/// * **TH1D**: the histogram `nameInFile` of `filename`, the bins with the center in `range`; the x axis
/// is in MeV or GeV if its title says so, otherwise in keV; uniform inside the bin.
///
/// ### Angular type (TRestGeant4AngularDistribution)
///
/// All of them have `direction` ([0, -1, 0] by default): the fixed direction or the reference of the
/// angle (the azimuth around it is uniform).
///
/// * **flux**: the fixed `direction`.
/// * **isotropic**: uniform in 4 pi or, with `isotropicConeHalfAngle`, in the solid angle of the cone
/// around `direction`.
/// * **formula**: the angle to `direction` follows the named `formula` (Cos2, Cos3 or Sin2theta, 0 above
/// 90 deg; its density in the angle, not per solid angle) in `range` ([0, 180 deg] by default),
/// sampled with `formulaNPoints` (500 by default).
/// * **TH1D**: the angle to `direction`, in rad, from the histogram `nameInFile` of `filename`, the bins
/// with the center in `range`.
///
/// ### Energy and angle together
///
/// When `energy` and `angular` have the same type `formula2`, `TH2D` or `cosmic`, the energy and the
/// angle to the angular `direction` are sampled together (each section gives its `range`; `formula`,
/// `filename` and `nameInFile` can be in either):
///
/// * **formula2**: the named `formula` (CosmicMuons: energy in keV, zenith angle in rad), sampled with the
/// energy and angular `formulaNPoints`.
/// * **TH2D**: the histogram `nameInFile` of `filename`, energy (x) in MeV and angle (y) in deg.
/// * **cosmic**: as TH2D, the histogram `<nameInFile>_energy_zenith`, `nameInFile` being the particle key
/// (neutron, proton, gamma, electron_minus, electron_plus, muon_minus, muon_plus, neutron_below_1MeV,
/// neutron_between_1MeV_and_10GeV, neutron_above_10GeV), which gives the particle if not given. Each bin
/// is divided by cos(angle): the flux through a horizontal surface to the one of the cosmic generator.
///
/// ### Examples
///
/// Alphas from a thin source layer of the geometry:
///
///     generator:
///       type: "volume"
///       from: "source"
///       source:
///         particle: "alpha"
///         angular:
///           type: "isotropic"
///         energy:
///           type: "mono"
///           energy: 5.3 MeV
///
/// Pencil beam of electrons:
///
///     generator:
///       type: "point"
///       position: [0.0, 0.0, -200.0 mm]
///       source:
///         particle: "e-"
///         angular:
///           type: "flux"
///           direction: [0.0, 0.0, 1.0]
///         energy:
///           type: "mono"
///           energy: 30.0 keV
///
/// Cosmic neutrons:
///
///     generator:
///       type: "cosmic"
///       source:
///         particle: "neutron"
///         angular:
///           type: "formula"
///           formula: "Cos2"
///           direction: [0, -1, 0]
///           formulaNPoints: 1000
///         energy:
///           type: "formula"
///           formula: "CosmicNeutrons"
///           formulaNPoints: 10000
///           range: [10 MeV, 10 GeV]
///
/// Cosmic muons with correlated energy and zenith angle:
///
///     generator:
///       type: "cosmic"
///       radius: 2 m
///       source:
///         particle: "mu-"
///         angular:
///           type: "formula2"
///           formula: "CosmicMuons"
///           direction: [0, -1, 0]
///           range: [0, 90 deg]
///         energy:
///           type: "formula2"
///
/// Cosmic flux from a file of energy-zenith histograms:
///
///     generator:
///       type: "cosmic"
///       source:
///         angular:
///           type: "cosmic"
///           direction: [0, -1, 0]
///         energy:
///           type: "cosmic"
///           filename: "cosmics.root"
///           nameInFile: "neutron"
///
/// Gammas from a rotated circle, in a cone around its normal, and a Decay0 event at the same point:
///
///     generator:
///       type: "surface"
///       shape: "circle"
///       size: [5 cm, 0, 0]
///       position: [0, 0, 10 cm]
///       rotationAxis: [1, 0, 0]
///       rotationValue: 30 deg
///       source:
///         - particle: "gamma"
///           angular:
///             type: "isotropic"
///             direction: [0, 0, -1]
///             isotropicConeHalfAngle: 20 deg
///           energy:
///             type: "TH1D"
///             filename: "spectrum.root"
///             nameInFile: "gammas"
///             range: [10 keV, 3 MeV]
///         - use: "Tl208.dat"
///
/// Ions in a box of gas, with more of them near z = 0:
///
///     generator:
///       type: "volume"
///       shape: "box"
///       size: [10 cm, 10 cm, 10 cm]
///       spatialDensityFunction: "exp(-abs(z) / 20)"
///       source:
///         particle: "Pb210"
///         angular:
///           type: "isotropic"
///         energy:
///           type: "mono"
///           energy: 1 keV
///
/// ### Implementation
///
/// TRestGeant4PrimaryGeneratorInfo::LoadDistributions loads the formulas, histograms and Decay0 files
/// once (restG4 master thread) to report any error before the simulation. Each thread has its own
/// TRestGeant4PrimaryGenerator, with its own random generator (seed: metadata seed + thread ID + 1) and
/// copies (Clone) of the spatial generator and of the sources, which load their own distributions, so no
/// locks are needed. The GDML positions need the Geant4 geometry: restG4 implements SampleGdmlPosition;
/// everything else does not depend on Geant4. restG4 (PrimaryGeneratorAction) only fills the particle gun.
///
/// ### Differences with legacy
///
/// * The YAML keys are the legacy ones; each section only uses the fields of its type.
/// * One cosmic position algorithm for any distribution (legacy had a different one for the cosmic
/// histograms).
/// * Removed: back to back (not implemented), the `custom` and `source` generator types (not
/// configurable) and the Decay0 seed (the metadata seed is used).
/// * New: sphere and cylinder volumes, the cone sampled directly, the `radius` of the cosmic generator,
/// e+ and alpha in Decay0 files (an unknown particle is an error).
/// * Fixed: a generator with `from` has no shape (it used the box one, with size 0: every primary at the
/// volume center), the GDML surfaces (the solid was not set), the bounding box of the GDML volumes
/// (scaled by 1.1, wrong for a box not centered at the origin) and the placement of the GDML volume (see
/// TRestGeant4GeometryInfo::GetPosition).

#include "TRestGeant4PrimaryGenerator.h"

#include <stdexcept>
#include <string>

using namespace std;
using ROOT::Math::XYZVector;

TRestGeant4PrimaryGenerator::TRestGeant4PrimaryGenerator(const TRestGeant4PrimaryGeneratorInfo& info,
                                                         unsigned int seed)
    : fInfo(&info), fRandom(seed) {
    if (!info.GetSpatialGenerator()) {
        throw runtime_error("TRestGeant4PrimaryGenerator: no generator configured");
    }
    fSpatial = info.GetSpatialGenerator()->Clone();
    for (const auto& source : info.fSources) {
        fSources.push_back(source.Clone());
        fSources.back().LoadDistributions();
    }
    if (!info.fSpatialDensityFunction.empty()) {
        const string name = "TRestGeant4PrimaryGeneratorDensity" + to_string(seed);
        fDensity = make_unique<TF3>(name.c_str(), info.fSpatialDensityFunction.c_str());
        if (!fDensity->IsValid()) {
            throw runtime_error("TRestGeant4PrimaryGenerator: invalid spatialDensityFunction '" +
                                info.fSpatialDensityFunction + "'");
        }
    }
}

vector<TRestGeant4ParticleState> TRestGeant4PrimaryGenerator::Generate() {
    const auto* cosmic = dynamic_cast<const TRestGeant4SpatialGeneratorCosmic*>(fSpatial.get());
    // The cosmic origin depends on the direction, it is set below
    const XYZVector position = cosmic ? XYZVector() : SamplePosition();

    vector<TRestGeant4ParticleState> particles;
    for (auto& source : fSources) {
        const XYZVector center = fSpatial->GetCenter();
        auto sourceParticles = source.Generate(fRandom, position, center, fSpatial->IsInward());
        for (auto& particle : sourceParticles) {
            if (cosmic) particle.SetOrigin(cosmic->SampleFromDirection(fRandom, particle.GetDirection()));
            particles.push_back(particle);
        }
    }
    return particles;
}

XYZVector TRestGeant4PrimaryGenerator::SampleGdmlPosition() {
    throw runtime_error("TRestGeant4PrimaryGenerator: the GDML positions need the Geant4 geometry (restG4)");
}

XYZVector TRestGeant4PrimaryGenerator::SamplePosition() {
    const auto sample = [this]() {
        return fSpatial->IsGdml() ? SampleGdmlPosition() : fSpatial->Sample(fRandom);
    };
    if (!fDensity) return sample();

    // The density accepts each position with its probability
    constexpr int maxAttempts = 10000000;
    for (int attempt = 0; attempt < maxAttempts; attempt++) {
        const XYZVector position = sample();
        const double density = fDensity->Eval(position.X(), position.Y(), position.Z());
        if (density > 1) {
            throw runtime_error("TRestGeant4PrimaryGenerator: spatialDensityFunction > 1 at (" +
                                to_string(position.X()) + ", " + to_string(position.Y()) + ", " +
                                to_string(position.Z()) + ")");
        }
        if (fRandom.Uniform() < density) return position;
    }
    throw runtime_error("TRestGeant4PrimaryGenerator: no position accepted by spatialDensityFunction");
}
