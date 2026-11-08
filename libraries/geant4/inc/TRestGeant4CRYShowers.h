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
///
/// Each field is the YAML key without the `f` prefix and with the first letter in lower case (e.g.
/// fNShowers is `nShowers`); the results (`showersGenerated`, `timeSimulated`, `program`,
/// `creationDate`) are written by restCRY.
class TRestGeant4CRYShowers : public TRestMetadata {
   public:
    bool fReturnNeutrons = true;   ///< `returnNeutrons`: CRY returns neutrons
    bool fReturnProtons = true;    ///< `returnProtons`: CRY returns protons
    bool fReturnGammas = true;     ///< `returnGammas`: CRY returns gammas
    bool fReturnElectrons = true;  ///< `returnElectrons`: CRY returns electrons and positrons
    bool fReturnMuons = true;      ///< `returnMuons`: CRY returns muons
    bool fReturnPions = true;      ///< `returnPions`: CRY returns pions
    bool fReturnKaons = true;      ///< `returnKaons`: CRY returns kaons

    /// Showers with less particles are not returned (at least 1)
    int fNParticlesMin = 1;
    /// Showers with more particles are not returned
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
    /// Time simulated by CRY at which the simulation stops (us; 0: no limit)
    TRestWithUnits fExposureTime = 0.0;

    /// Seed of the random generator (0: a random one, written back)
    long fSeed = 0;

    /// The showers file written by restCRY (overwritten if it exists)
    std::string fOutputFileName = "cry_showers.root";

    /// Results, written by restCRY: showers in the file and time simulated by CRY (exposure)
    int64_t fShowersGenerated = 0;
    TRestWithUnits fTimeSimulated = 0.0;  ///< Time simulated by CRY (us), the exposure of the showers
    std::string fProgram;                 ///< Program that wrote the file (CRY version)
    std::string fCreationDate;            ///< Date when the results were set (YYYY-MM-DD HH:MM:SS)

    /// Name of the TTree of the showers in the file
    static constexpr const char* kTreeName = "showers";

    TRestGeant4CRYShowers() = default;
    /// \brief Configured from a YAML node.
    TRestGeant4CRYShowers(const std::string& instanceName, const YAML::Node& node);
    /// \brief Reads the fields and checks them (throws std::runtime_error if nShowers and exposureTime are
    /// both 0 or negative, nParticlesMin/Max, latitude, altitude, subBoxLength, date or outputFileName are
    /// not valid), then writes them back to the node.
    void LoadConfig() override;
    /// \brief No-op.
    void Initialize() override {}
    /// \brief Returns "TRestGeant4CRYShowers".
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
