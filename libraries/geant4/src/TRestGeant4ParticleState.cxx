
#include "TRestGeant4ParticleState.h"

using namespace std;

TRestGeant4ParticleState::TRestGeant4ParticleState() {
    // TRestGeant4ParticleState default constructor
}

TRestGeant4ParticleState::~TRestGeant4ParticleState() {
    // TRestGeant4ParticleState destructor
}

///////////////////////////////////////////////
/// \brief A copy method updated for modern REST references
///
void TRestGeant4ParticleState::SetParticle(const TRestGeant4ParticleState& particle) {
    fExcitationLevel = particle.GetExcitationLevel();
    fParticleName = particle.GetParticleName();
    fEnergy = particle.GetEnergy();
    fDirection = particle.GetMomentumDirection();
    fOrigin = particle.GetOrigin();
    fCharge = particle.GetParticleCharge();
}

///////////////////////////////////////////////
/// \brief Prints on screen the details about the Geant4 simulation
/// conditions, stored in TRestGeant4Metadata.
///
void TRestGeant4ParticleState::Print() const {
    std::cout << "Particle name : " << GetParticleName() << std::endl;
    std::cout << "Charge : " << GetParticleCharge() << std::endl;
    std::cout << "Energy : " << GetEnergy() << " keV" << std::endl;
    std::cout << "Excitation level : " << GetExcitationLevel() << std::endl;
    std::cout << "X : " << GetOrigin().X() << "mm Y : " << GetOrigin().Y() << "mm Z : " << GetOrigin().Z()
              << "mm" << std::endl;
    std::cout << "Px : " << GetMomentumDirection().X() << " Py : " << GetMomentumDirection().Y()
              << " Pz : " << GetMomentumDirection().Z() << std::endl;
    std::cout << " ---------------------- " << std::endl;
}
