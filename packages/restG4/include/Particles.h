
#ifndef Particles_h
#define Particles_h 1

#include <G4VPhysicsConstructor.hh>
#include <globals.hh>

/// \class Particles
/// \brief Physics constructor that only defines particles: gamma, leptons, mesons, baryons, light ions,
/// the generic ion and the short-lived particles. It has no processes.
class Particles : public G4VPhysicsConstructor {
   public:
    /// \param name name of the physics constructor
    Particles(const G4String& name = "particles");

    virtual ~Particles();

    /// \brief Defines the particles
    virtual void ConstructParticle();

    // This method is dummy
    virtual void ConstructProcess() {};
};
#endif
