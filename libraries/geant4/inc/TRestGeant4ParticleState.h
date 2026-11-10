#ifndef TRESTGEANT4PARTICLESTATE_H
#define TRESTGEANT4PARTICLESTATE_H

#include <Math/Vector3D.h>

#include <string>

/// \class TRestGeant4ParticleState
/// \brief Stores one primary/secondary particle state used by generators and track metadata.
///
/// A primary particle given by TRestGeant4PrimaryGenerator to restG4: name, excitation level and charge
/// (ions), kinetic energy (keV), unit direction, origin (mm) and start time (us).
///
/// \author JuanAn García <juanangp@unizar.es>
class TRestGeant4ParticleState {
   protected:
    std::string fParticleName;                           ///< Geant4 particle or ion name
    double fExcitationLevel = 0;                         ///< Excitation energy of an ion (keV, >= 0)
    ROOT::Math::XYZVector fDirection = {1.0, 0.0, 0.0};  ///< Unit momentum direction
    double fEnergy = 0;                                  ///< Kinetic energy (keV)
    int fCharge = 0;                                     ///< Charge of an ion (e)
    ROOT::Math::XYZVector fOrigin;                       ///< Origin (mm)
    /// Start time (us), e.g. of the particles of a cosmic shower relative to its first one
    double fTime = 0;

   public:
    inline std::string GetParticleName() const { return fParticleName; }   ///< Particle name
    inline double GetExcitationLevel() const { return fExcitationLevel; }  ///< Excitation energy (keV)
    inline double GetEnergy() const { return fEnergy; }                    ///< Kinetic energy (keV)
    /// \brief Unit momentum direction (as GetDirection)
    inline ROOT::Math::XYZVector GetMomentumDirection() const { return fDirection; }
    inline int GetParticleCharge() const { return fCharge; }                  ///< Charge (e)
    inline ROOT::Math::XYZVector GetOrigin() const { return fOrigin; }        ///< Origin (mm)
    inline ROOT::Math::XYZVector GetDirection() const { return fDirection; }  ///< Unit direction
    inline double GetTime() const { return fTime; }                           ///< Start time (us)

    /// \brief Copies all particle fields from another particle instance.
    void SetParticle(const TRestGeant4ParticleState& particle);

    /// \brief Sets the particle name
    void SetParticleName(const std::string& particle) { fParticleName = particle; }

    /// \brief Sets the excitation energy (keV), negative values become 0
    void SetExcitationLevel(double excitationEnergy) {
        fExcitationLevel = excitationEnergy;
        if (fExcitationLevel < 0) fExcitationLevel = 0;
    }

    void SetParticleCharge(int charge) { fCharge = charge; }  ///< Sets the charge (e)

    /// \brief Sets the direction (normalized)
    void SetDirection(const ROOT::Math::XYZVector& dir) { fDirection = dir.Unit(); }
    void SetEnergy(double en) { fEnergy = en; }                          ///< Sets the kinetic energy (keV)
    void SetOrigin(const ROOT::Math::XYZVector& pos) { fOrigin = pos; }  ///< Sets the origin (mm)
    void SetTime(double time) { fTime = time; }                          ///< Sets the start time (us)

    /// \brief Prints particle identity, energy, direction and origin.
    void Print() const;

    // Constructor
    /// \brief Constructor.
    TRestGeant4ParticleState();
    // Destructor
    /// \brief Destructor.
    virtual ~TRestGeant4ParticleState();
};

#endif
