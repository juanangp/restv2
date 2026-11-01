#ifndef TRESTGEANT4CRYSHOWERS_H
#define TRESTGEANT4CRYSHOWERS_H

#include <cstdint>
#include <string>

#include "TRestMetadata.h"

class TFile;

/// \class TRestGeant4CRYShowers
/// \brief Configuration and result of a CRY simulation of cosmic-ray showers: restCRY (packages/restG4,
/// the only part that needs CRY) runs it and writes the showers file, which the `cry` generator of restG4
/// reads. See TRestGeant4CRYShowers.cxx for the options and the file contents.
class TRestGeant4CRYShowers : public TRestMetadata {
   public:
    /// Particles returned by CRY
    bool fReturnNeutrons = true;
    bool fReturnProtons = true;
    bool fReturnGammas = true;
    bool fReturnElectrons = true;
    bool fReturnMuons = true;
    bool fReturnPions = true;
    bool fReturnKaons = true;

    /// Showers with less or more particles are not returned
    int fNParticlesMin = 1;
    int fNParticlesMax = 1000000;

    /// Date for the 11-year solar cycle, month-day-year
    std::string fDate = "7-1-2012";
    /// Geomagnetic latitude in degrees, -90 to 90
    double fLatitude = 90.0;
    /// Altitude: 0, 2100 m or 11300 m (the CRY data tables)
    TRestWithUnits fAltitude = 0.0;
    /// Side of the horizontal square where the particles are returned
    TRestWithUnits fSubBoxLength = 100000.0;

    /// The simulation stops at the first reached of nShowers and exposureTime (0: no limit, one is needed)
    int64_t fNShowers = 0;
    TRestWithUnits fExposureTime = 0.0;

    /// Seed of the random generator (0: a random one, written back)
    long fSeed = 0;

    std::string fOutputFileName = "cry_showers.root";

    /// Results, written by restCRY: showers in the file and time simulated by CRY (exposure)
    int64_t fShowersGenerated = 0;
    TRestWithUnits fTimeSimulated = 0.0;
    std::string fProgram;
    std::string fCreationDate;

    static constexpr const char* kTreeName = "showers";

    TRestGeant4CRYShowers() = default;
    TRestGeant4CRYShowers(const std::string& instanceName, const YAML::Node& node);
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestGeant4CRYShowers"; }

    /// \brief The configuration string of CRYSetup (lengths in m)
    std::string GetCRYSetup() const;

    /// \brief Sets the seed (e.g. a random one when it is 0), also in the YAML node to be stored
    void SetSeed(long seed);

    /// \brief Sets the results and writes them to the YAML node to be stored
    void SetResults(int64_t showers, double timeSimulated, const std::string& program);

    /// \brief The TRestGeant4CRYShowers stored in a showers file (throws if there is none)
    static TRestGeant4CRYShowers ReadFromFile(TFile* file);
};

#endif
