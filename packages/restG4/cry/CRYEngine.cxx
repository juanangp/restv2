/// \file CRYEngine.cxx
/// \brief The CRY library behind the cry generator of restG4 (TRestGeant4SpatialGeneratorCRY, geant4 library,
/// which does not depend on CRY). Built only with CRY (REST_CRY), in the restG4 executable: CRY builds only
/// a static library, maybe without -fPIC, which cannot go in the shared library of restG4.
///
/// The CRY particles are given the Geant4 names of legacy TRestGeant4ParticleSourceCry; positions in mm
/// (CRY: m), kinetic energies in keV (CRY: MeV), times in us relative to the first particle of the shower
/// (CRY: s from the start of the simulation). The data tables are those of $CRYDATAPATH (CRY setup script)
/// or of the build.

#include <TRandom3.h>
#include <TRestGeant4SpatialGenerator.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "CRYGenerator.h"
#include "CRYParticle.h"
#include "CRYSetup.h"

namespace {

/// The random generator of CRY, a function without arguments (a single CRY generator, shared by the threads
/// under the lock of TRestGeant4SpatialGeneratorCRY)
TRandom3 gCRYRandom;
double CRYRandom() { return gCRYRandom.Rndm(); }

/// Geant4 name of a CRY particle, as legacy
std::string ParticleName(CRYParticle& particle) {
    const int charge = particle.charge();
    switch (particle.id()) {
        case CRYParticle::Neutron:
            return "neutron";
        case CRYParticle::Proton:
            return "proton";
        case CRYParticle::Pion:
            return charge > 0 ? "pi+" : charge < 0 ? "pi-" : "pi0";
        case CRYParticle::Kaon:
            return charge > 0 ? "kaon+" : charge < 0 ? "kaon-" : "kaon0";
        case CRYParticle::Muon:
            return charge > 0 ? "mu+" : "mu-";
        case CRYParticle::Electron:
            return charge > 0 ? "e+" : "e-";
        case CRYParticle::Gamma:
            return "gamma";
    }
    throw std::runtime_error("CRY: unknown particle " + std::to_string(particle.id()));
}

class CRYEngine : public TRestGeant4CRYEngine {
   public:
    CRYEngine(const std::string& setupString, long seed) {
        gCRYRandom.SetSeed(seed);
        const char* dataPath = std::getenv("CRYDATAPATH");
        fSetup = std::make_unique<CRYSetup>(setupString, dataPath && *dataPath ? dataPath : CRY_DATA_PATH);
        fSetup->setRandomFunction(CRYRandom);
        fGenerator = std::make_unique<CRYGenerator>(fSetup.get());
    }

    std::vector<TRestGeant4ParticleState> Generate() override {
        constexpr double kUsPerS = 1e6;
        std::vector<CRYParticle*> cryParticles;
        fGenerator->genEvent(&cryParticles);

        double firstTime = 0;
        for (size_t i = 0; i < cryParticles.size(); i++) {
            firstTime = i == 0 ? cryParticles[i]->t() : std::min(firstTime, cryParticles[i]->t());
        }
        std::vector<TRestGeant4ParticleState> particles;
        for (auto* cryParticle : cryParticles) {
            TRestGeant4ParticleState particle;
            particle.SetParticleName(ParticleName(*cryParticle));
            particle.SetParticleCharge(cryParticle->charge());
            particle.SetEnergy(cryParticle->ke() * 1000);  // MeV -> keV
            particle.SetTime((cryParticle->t() - firstTime) * kUsPerS);
            particle.SetOrigin({cryParticle->x() * 1000, cryParticle->y() * 1000, cryParticle->z() * 1000});
            particle.SetDirection({cryParticle->u(), cryParticle->v(), cryParticle->w()});
            particles.push_back(particle);
            delete cryParticle;
        }
        return particles;
    }

    double GetTimeSimulated() const override { return fGenerator->timeSimulated() * 1e6; }

   private:
    std::unique_ptr<CRYSetup> fSetup;
    std::unique_ptr<CRYGenerator> fGenerator;
};

}  // namespace

/// Gives the CRY engine to the cry generator (main.cxx, before the simulation)
void RegisterCRYEngine() {
    TRestGeant4SpatialGeneratorCRY::SetEngineFactory([](const std::string& setup, long seed) {
        return std::unique_ptr<TRestGeant4CRYEngine>(std::make_unique<CRYEngine>(setup, seed));
    });
}
