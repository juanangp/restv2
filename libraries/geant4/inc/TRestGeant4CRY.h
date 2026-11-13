#ifndef TRESTGEANT4CRY_H
#define TRESTGEANT4CRY_H

#include <string>
#include <vector>

#include "TRestMetadata.h"

/// \class TRestGeant4CRY
/// \brief The configuration of CRY (Cosmic-ray Shower Library, LLNL) for the `cry` generator of restG4: a
/// section of the TRestGeant4Manager, as the physics lists; the generator only places its plane in the
/// geometry. See TRestGeant4CRY.cxx for the options.
///
/// Each field is the YAML key without the `f` prefix and with the first letter in lower case (e.g.
/// fSubBoxLength is `subBoxLength`).
///
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
class TRestGeant4CRY : public TRestMetadata {
   public:
    /// `particles`: the particles returned by CRY, of muons, neutrons, protons, gammas, electrons (and
    /// positrons), pions and kaons (all by default)
    std::vector<std::string> fParticles = {"muons",     "neutrons", "protons", "gammas",
                                           "electrons", "pions",    "kaons"};

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
    /// Side of the horizontal square where the particles start (needed)
    TRestWithUnits fSubBoxLength = 0.0;

    /// The simulation stops when CRY has simulated this time (us; 0: no limit, only nEvents)
    TRestWithUnits fMaxExposureTime = 0.0;

    TRestGeant4CRY() = default;
    /// \brief Configured from a YAML node.
    TRestGeant4CRY(const std::string& instanceName, const YAML::Node& node);
    /// \brief Reads the fields and checks them (throws std::runtime_error if subBoxLength is not positive,
    /// or particles, nParticlesMin/Max, latitude, altitude, date or maxExposureTime are not valid), then
    /// writes them back to the node.
    void LoadConfig() override;
    /// \brief No-op.
    void Initialize() override {}
    /// \brief Returns "TRestGeant4CRY".
    std::string GetClassName() const override { return "TRestGeant4CRY"; }

    /// \brief The configuration string of CRYSetup (lengths in m)
    std::string GetCRYSetup() const;
    /// \brief True if CRY returns the particles `name` (one of `particles`)
    bool Returns(const std::string& name) const;
};

#endif
