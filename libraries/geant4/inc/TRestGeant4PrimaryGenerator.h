#ifndef TRESTGEANT4PRIMARYGENERATOR_H
#define TRESTGEANT4PRIMARYGENERATOR_H

#include <Math/Vector3D.h>
#include <TF3.h>
#include <TRandom3.h>

#include <memory>
#include <vector>

#include "TRestGeant4ParticleSource.h"
#include "TRestGeant4ParticleState.h"
#include "TRestGeant4PrimaryGeneratorInfo.h"
#include "TRestGeant4SpatialGenerator.h"

/// \class TRestGeant4PrimaryGenerator
/// \brief Samples the primaries of an event from a TRestGeant4PrimaryGeneratorInfo: one per thread, with
/// its own random generator and copies of the spatial generator and the sources. The positions in GDML
/// volumes need the Geant4 geometry, they are given by SampleGdmlPosition, which restG4 implements.
/// Units: keV, mm. The generator options (YAML) and examples are documented in
/// TRestGeant4PrimaryGenerator.cxx.
class TRestGeant4PrimaryGenerator {
   public:
    TRestGeant4PrimaryGenerator(const TRestGeant4PrimaryGeneratorInfo& info, unsigned int seed);
    virtual ~TRestGeant4PrimaryGenerator() = default;

    /// \brief The primaries of a new event: name, excitation, charge, energy, direction and origin
    std::vector<TRestGeant4ParticleState> Generate();

    inline TRandom& GetRandom() { return fRandom; }
    inline const TRestGeant4PrimaryGeneratorInfo& GetInfo() const { return *fInfo; }

   protected:
    /// \brief A uniform position in the GDML volume (or on its surface), in world coordinates
    virtual ROOT::Math::XYZVector SampleGdmlPosition();

   private:
    const TRestGeant4PrimaryGeneratorInfo* fInfo;           //!
    TRandom3 fRandom;                                       //!
    std::unique_ptr<TRestGeant4SpatialGenerator> fSpatial;  //!
    std::vector<TRestGeant4ParticleSource> fSources;        //!
    std::unique_ptr<TF3> fDensity;                          //!

    /// \brief A position of the spatial generator accepted by the density, if any
    ROOT::Math::XYZVector SamplePosition();
};

#endif
