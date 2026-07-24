#pragma once

#include <iostream>
#include <string>
#include <utility>

// ROOT MathCore GenVector header
#include <Math/Vector3D.h>

#include "TRestMetadata.h"

/// \class TRestGeant4BiasingVolume
/// \brief Class containing properties and geometry tests for Geant4 biasing techniques.
class TRestGeant4BiasingVolume {
   public:
    std::array<double, 3> fVolumePosition = {0.0, 0.0, 0.0};
    double fVolumeSize = 0.0;
    std::string fBiasingVolumeType = "virtualBox";  // Obsolete duplicate field kept for compatibility
    double fBiasingFactor = 1.0;
    std::pair<double, double> fEnergyRange = {0.0, 1.0E20};
    std::string fVolumeType = "virtualBox";

    TRestGeant4BiasingVolume();
    virtual ~TRestGeant4BiasingVolume();

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
    inline void SetBiasingVolumeType(const std::string& type) {
        fVolumeType = type;
        fBiasingVolumeType = type;
    }
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

    void PrintBiasingVolume() const;

    // Grants reflection registration macro privileges
    friend class TRestMetadataFieldRegistry;
};
