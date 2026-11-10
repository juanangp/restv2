#ifndef TRESTGEANT4BIASINGVOLUME_H
#define TRESTGEANT4BIASINGVOLUME_H

#include <array>
#include <string>
#include <utility>

// ROOT MathCore GenVector header
#include <Math/Vector3D.h>

#include "TRestMetadata.h"

/// \class TRestGeant4BiasingVolume
/// \brief A virtual volume (box or sphere) of the volume biasing of restG4.
///
/// An entry of the `biasingVolumes` list of TRestGeant4Metadata. The volumes are not part of the Geant4
/// geometry: they are geometric tests (inside an axis-aligned box or a sphere) applied by restG4 to the steps
/// of the particles. They must be nested and listed from the outermost to the innermost (each one strictly
/// inside the previous one, without touching it, checked by TRestGeant4Metadata::CheckBiasingVolumes).
///
/// restG4 runs the simulation in stages (one per volume, plus a last one). In stage k a particle entering
/// volume k is stopped and, if its kinetic energy is inside `energyRange`, its state at the crossing point is
/// stored; stage k+1 simulates each stored particle `biasingFactor` times. Particles entering with an energy
/// outside `energyRange` are stopped and not stored (their contribution is discarded). See
/// TRestGeant4Metadata and the restG4 README for the stages and the weights.
///
///     biasingVolumes:
///       - type: "virtualSphere"     # virtualBox (default) or virtualSphere
///         position: [0, 0, 0]       # center (mm)
///         size: 50 cm               # box side or sphere radius (mm), > 0
///         biasingFactor: 10         # integer >= 1: replicas of each particle stored
///         energyRange: [0, 10 MeV]  # keV: particles stored (inclusive range)
///
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
class TRestGeant4BiasingVolume : public TRestMetadata {
   public:
    std::array<TRestWithUnits, 3> fVolumePosition = {0.0, 0.0, 0.0};  ///< `position`: center (mm)
    TRestWithUnits fVolumeSize = 0.0;  ///< `size`: side of the box or radius of the sphere (mm)
    double fBiasingFactor = 1.0;       ///< `biasingFactor`
    /// `energyRange` (keV) of the particles to bias
    std::pair<TRestWithUnits, TRestWithUnits> fEnergyRange = {0.0, 1.0E20};
    std::string fVolumeType = "virtualBox";  ///< `type`: virtualBox or virtualSphere

    // --- Getters ---
    inline double GetBiasingFactor() const { return fBiasingFactor; }        ///< `biasingFactor`
    inline double GetBiasingVolumeSize() const { return fVolumeSize; }       ///< `size` (mm)
    inline std::string GetBiasingVolumeType() const { return fVolumeType; }  ///< `type`
    /// \brief The center, `position` (mm)
    inline ROOT::Math::XYZVector GetBiasingVolumePosition() const {
        return ROOT::Math::XYZVector(fVolumePosition[0], fVolumePosition[1], fVolumePosition[2]);
    }
    /// \brief `energyRange` (keV)
    inline std::pair<double, double> GetEnergyRange() const { return fEnergyRange; }
    inline double GetMaxEnergy() const { return fEnergyRange.second; }  ///< Maximum of `energyRange`
    inline double GetMinEnergy() const { return fEnergyRange.first; }   ///< Minimum of `energyRange`

    // --- Setters ---
    inline void SetBiasingVolumeSize(double size) { fVolumeSize = size; }  ///< Sets `size` (mm)
    /// \brief Sets `type` (not checked)
    inline void SetBiasingVolumeType(const std::string& type) { fVolumeType = type; }
    /// \brief Sets `position` (mm)
    inline void SetBiasingVolumePosition(const ROOT::Math::XYZVector& pos) {
        fVolumePosition = {pos.X(), pos.Y(), pos.Z()};
    }
    inline void SetBiasingFactor(double factor) { fBiasingFactor = factor; }  ///< Sets `biasingFactor`
    /// \brief Sets `energyRange` (keV)
    inline void SetEnergyRange(const std::pair<double, double>& eRange) { fEnergyRange = eRange; }

    /// \brief Checks whether a given spatial point is located inside the biasing volume boundaries.
    /// \param x, y, z Position (mm).
    /// \return 1 if inside, 0 otherwise (also for an unknown type).
    inline int isInside(double x, double y, double z) const {
        // Shift global point coordinates relative to the volume center position
        double dx = x - fVolumePosition[0];
        double dy = y - fVolumePosition[1];
        double dz = z - fVolumePosition[2];

        if (fVolumeType == "virtualBox") {
            double halfSize = fVolumeSize / 2.0;
            if (dx < halfSize && dx > -halfSize) {
                if (dy < halfSize && dy > -halfSize) {
                    if (dz < halfSize && dz > -halfSize) return 1;
                }
            }
        } else if (fVolumeType == "virtualSphere") {
            double r2 = dx * dx + dy * dy + dz * dz;
            if (r2 < fVolumeSize * fVolumeSize) return 1;
        }
        return 0;
    }

    /// \brief Fraction t (0 to 1) of the segment from `from` to `to` (mm) where it enters the volume.
    ///
    /// The segment is a straight line (the chord of a Geant4 step). Returns -1 if `from` is inside the
    /// volume, or if the segment does not enter it (also when it only touches its surface); otherwise the
    /// entry point is from + t (to - from), also when the segment goes through the volume and leaves it.
    double GetEntryFraction(const ROOT::Math::XYZVector& from, const ROOT::Math::XYZVector& to) const;

    /// \brief True if `inner` is strictly inside this volume (their surfaces do not touch)
    bool Contains(const TRestGeant4BiasingVolume& inner) const;

    /// \brief Reads the fields from YAML, checks `type` (virtualBox or virtualSphere), `size` (> 0),
    /// `biasingFactor` (an integer >= 1) and `energyRange` (minimum <= maximum), and writes them back.
    void LoadConfig() override;
    /// \brief No-op hook kept for REST metadata lifecycle compatibility.
    void Initialize() override {}
    /// \brief Returns "TRestGeant4BiasingVolume".
    std::string GetClassName() const override { return "TRestGeant4BiasingVolume"; }

    // Constructors & Destructors
    /// \brief Constructs a biasing volume with the default values.
    TRestGeant4BiasingVolume();
    /// \brief Loads the biasing volume from a section of a YAML configuration file.
    TRestGeant4BiasingVolume(const std::string& configFilename, const std::string& name = "");
    /// \brief Builds the biasing volume from a YAML node (an entry of `biasingVolumes`).
    TRestGeant4BiasingVolume(const std::string& instanceName, const YAML::Node& node);
    /// \brief Destructor.
    virtual ~TRestGeant4BiasingVolume();
};

#endif
