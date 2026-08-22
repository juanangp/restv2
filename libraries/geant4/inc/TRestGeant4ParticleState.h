#pragma once

#include <Math/Vector3D.h>

#include <iostream>
#include <string>

/// \class TRestGeant4ParticleState
/// \brief Stores one primary/secondary particle state used by generators and track metadata.
class TRestGeant4ParticleState {
   protected:
    std::string fParticleName;
    double fExcitationLevel = 0;
    ROOT::Math::XYZVector fDirection = {1.0, 0.0, 0.0};
    double fEnergy = 0;
    int fCharge = 0;
    ROOT::Math::XYZVector fOrigin;

   public:
    inline std::string GetParticleName() const { return fParticleName; }
    inline double GetExcitationLevel() const { return fExcitationLevel; }
    inline double GetEnergy() const { return fEnergy; }
    inline ROOT::Math::XYZVector GetMomentumDirection() const { return fDirection; }
    inline int GetParticleCharge() const { return fCharge; }
    inline ROOT::Math::XYZVector GetOrigin() const { return fOrigin; }
    inline ROOT::Math::XYZVector GetDirection() const { return fDirection; }

    /// \brief Copies all particle fields from another particle instance.
    void SetParticle(const TRestGeant4ParticleState& particle);

    void SetParticleName(const std::string& particle) { fParticleName = particle; }

    void SetExcitationLevel(double excitationEnergy) {
        fExcitationLevel = excitationEnergy;
        if (fExcitationLevel < 0) fExcitationLevel = 0;
    }

    void SetParticleCharge(int charge) { fCharge = charge; }

    void SetDirection(const ROOT::Math::XYZVector& dir) { fDirection = dir.Unit(); }
    void SetEnergy(double en) { fEnergy = en; }
    void SetOrigin(const ROOT::Math::XYZVector& pos) { fOrigin = pos; }

    /// \brief Prints particle identity, energy, direction and origin.
    void Print() const;

    // Constructor
    /// \brief Constructor.
    TRestGeant4ParticleState();
    // Destructor
    /// \brief Destructor.
    virtual ~TRestGeant4ParticleState();
};
