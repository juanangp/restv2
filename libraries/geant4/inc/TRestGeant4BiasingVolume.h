#ifndef TRESTGEANT4BIASINGVOLUME_H
#define TRESTGEANT4BIASINGVOLUME_H

#include <iostream>
#include <string>
#include <utility>

// ROOT MathCore GenVector header
#include <Math/Vector3D.h>

#include "TRestMetadata.h"

/// \class TRestGeant4BiasingVolume
/// \brief Class containing properties and geometry tests for Geant4 biasing techniques.
class TRestGeant4BiasingVolume : public TRestMetadata {
   public:
    std::array<TRestWithUnits, 3> fVolumePosition = {0.0, 0.0, 0.0};
    TRestWithUnits fVolumeSize = 0.0;
    double fBiasingFactor = 1.0;
    std::pair<TRestWithUnits, TRestWithUnits> fEnergyRange = {0.0, 1.0E20};
    std::string fVolumeType = "virtualBox";

    // --- Getters ---
    inline double GetBiasingFactor() const { return fBiasingFactor; }
    inline double GetBiasingVolumeSize() const { return fVolumeSize; }
    inline std::string GetBiasingVolumeType() const { return fVolumeType; }
    inline ROOT::Math::XYZVector GetBiasingVolumePosition() const {
        return ROOT::Math::XYZVector(fVolumePosition[0], fVolumePosition[1], fVolumePosition[2]);
    }
    inline std::pair<double, double> GetEnergyRange() const { return fEnergyRange; }
    inline double GetMaxEnergy() const { return fEnergyRange.second; }
    inline double GetMinEnergy() const { return fEnergyRange.first; }

    // --- Setters ---
    inline void SetBiasingVolumeSize(double size) { fVolumeSize = size; }
    inline void SetBiasingVolumeType(const std::string& type) { fVolumeType = type; }
    inline void SetBiasingVolumePosition(const ROOT::Math::XYZVector& pos) {
        fVolumePosition = {pos.X(), pos.Y(), pos.Z()};
    }
    inline void SetBiasingFactor(double factor) { fBiasingFactor = factor; }
    inline void SetEnergyRange(const std::pair<double, double>& eRange) { fEnergyRange = eRange; }

    /// \brief Checks whether a given spatial point is located inside the biasing volume boundaries.
    /// \return 1 if inside, 0 otherwise.
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

    /// \brief Loads physics lists, options and production cuts from YAML.
    void LoadConfig() override;
    /// \brief No-op hook kept for REST metadata lifecycle compatibility.
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestGeant4BiasingVolume"; }

    // Constructors & Destructors
    /// \brief Constructs an empty physics-list metadata container.
    TRestGeant4BiasingVolume();
    /// \brief Loads physics list metadata from an RML/YAML configuration file section.
    TRestGeant4BiasingVolume(const std::string& configFilename, const std::string& name = "");
    /// \brief Builds metadata directly from REST v3 YAML node content.
    TRestGeant4BiasingVolume(const std::string& instanceName, const YAML::Node& node);
    /// \brief Destructor.
    virtual ~TRestGeant4BiasingVolume();
};

#endif
