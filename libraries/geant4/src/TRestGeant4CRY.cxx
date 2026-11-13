/// \class TRestGeant4CRY
/// The configuration of CRY (Cosmic-ray Shower Library, LLNL) for the `cry` generator of restG4
/// (TRestGeant4SpatialGeneratorCRY): a section of the TRestGeant4Manager, as the physics lists, linked to
/// the generator when the configuration is loaded (TRestGeant4Manager::LoadConfig; a cry generator without
/// this section is an error, the section is not used with another generator). restG4, built with CRY, calls
/// CRY during the simulation: each event is a shower with all its particles. This class does not depend on
/// CRY. It is stored in the output file with the other metadata.
///
/// ### Parameters
///
/// * particles: the particles returned by CRY, a list of muons, neutrons, protons, gammas, electrons
///   (electrons and positrons), pions and kaons (all by default), e.g. `["muons"]` for only the muons. The
///   showers keep only those particles (and a shower without any of them is not returned); the exposure is
///   the same (the time simulated by CRY does not depend on the particles kept). Legacy returnMuons... :
///   the particles with true.
/// * nParticlesMin, nParticlesMax: showers with less or more particles are not returned (1, 1000000).
/// * date: month-day-year, for the 11-year solar cycle ("7-1-2012").
/// * latitude: geomagnetic latitude in degrees, -90 to 90 (90).
/// * altitude: 0, 2100 m or 11300 m, the altitudes of the CRY data tables (0).
/// * subBoxLength: side of the horizontal square where the particles start, needed. It should be about the
///   size of the detector: the particles outside it are not returned, the ones far from the detector waste
///   time in restG4. The square must be inside the world (restG4 stops with an error otherwise).
/// * maxExposureTime: the simulation stops when CRY has simulated this time (0: no limit, only nEvents;
///   with nEvents 0 only this one).
///
/// The seed of CRY is the one of the simulation (TRestGeant4Metadata). The CRY offsets (xoffset, yoffset,
/// zoffset) are not used: the square is placed in the geometry by the `position` and `verticalAxis` of the
/// generator. The exposure simulated is stored in the generator section
/// (TRestGeant4Metadata::GetCRYExposureTime), to normalize to a rate.
///
/// Example:
///
///    g4Manager:
///      class: "TRestGeant4Manager"
///      ...
///      cry:
///        class: "TRestGeant4CRY"
///        particles: ["muons", "neutrons"]   # all by default
///        latitude: 42
///        altitude: 0
///        subBoxLength: 3 m
///        maxExposureTime: 1 day
///      geant4Metadata:
///        class: "TRestGeant4Metadata"
///        ...
///        generator:
///          type: "cry"
///          position: [0, 1 m, 0]      # center of the CRY square
///          verticalAxis: [0, 1, 0]    # the CRY up: the showers go down along -Y
///
/// Migrated from legacy TRestGeant4ParticleSourceCry: the options in their own section, the whole shower per
/// event and the exposure stored.

#include "TRestGeant4CRY.h"

#include <algorithm>
#include <cmath>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

static const bool TRestGeant4CRY_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using C = TRestGeant4CRY;
    reg.RegisterField<C>("particles", &C::fParticles);
    reg.RegisterField<C>("nParticlesMin", &C::fNParticlesMin);
    reg.RegisterField<C>("nParticlesMax", &C::fNParticlesMax);
    reg.RegisterField<C>("date", &C::fDate);
    reg.RegisterField<C>("latitude", &C::fLatitude);
    reg.RegisterField<C>("altitude", &C::fAltitude);
    reg.RegisterField<C>("subBoxLength", &C::fSubBoxLength);
    reg.RegisterField<C>("maxExposureTime", &C::fMaxExposureTime);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestGeant4CRY", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestGeant4CRY>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestGeant4CRY::TRestGeant4CRY(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

void TRestGeant4CRY::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4CRY>(fNode);

    const std::string error = GetClassName() + " '" + GetName() + "': ";
    if (fSubBoxLength <= 0) {
        throw std::runtime_error(error +
                                 "subBoxLength is needed: the side of the square where the particles "
                                 "start, about the size of the detector");
    }
    const std::set<std::string> known = {"muons",     "neutrons", "protons", "gammas",
                                         "electrons", "pions",    "kaons"};
    if (fParticles.empty()) throw std::runtime_error(error + "particles is empty (all by default)");
    for (const auto& particle : fParticles) {
        if (!known.count(particle)) {
            throw std::runtime_error(error + "unknown particles '" + particle +
                                     "' (muons, neutrons, protons, gammas, electrons, pions or kaons)");
        }
    }
    if (fNParticlesMin < 1 || fNParticlesMax < fNParticlesMin) {
        throw std::runtime_error(error + "it must be 1 <= nParticlesMin <= nParticlesMax");
    }
    if (std::abs(fLatitude) > 90) throw std::runtime_error(error + "latitude must be in [-90, 90]");
    const double altitude = fAltitude / 1000.0;  // m
    if (altitude != 0 && altitude != 2100 && altitude != 11300) {
        throw std::runtime_error(error + "altitude must be 0, 2100 m or 11300 m (the CRY data tables)");
    }
    if (!std::regex_match(fDate, std::regex(R"(\d{1,2}-\d{1,2}-\d{4})"))) {
        throw std::runtime_error(error + "date must be month-day-year, e.g. 7-1-2012");
    }
    if (fMaxExposureTime < 0) throw std::runtime_error(error + "maxExposureTime must not be negative");

    UpdateYAMLFromParams<TRestGeant4CRY>(fNode);
}

std::string TRestGeant4CRY::GetCRYSetup() const {
    std::ostringstream setup;
    setup.precision(10);
    setup << "returnNeutrons " << Returns("neutrons") << " returnProtons " << Returns("protons")
          << " returnGammas " << Returns("gammas") << " returnElectrons " << Returns("electrons")
          << " returnMuons " << Returns("muons") << " returnPions " << Returns("pions") << " returnKaons "
          << Returns("kaons") << " nParticlesMin " << fNParticlesMin << " nParticlesMax " << fNParticlesMax
          << " date " << fDate << " latitude " << fLatitude << " altitude " << fAltitude / 1000.0
          << " subboxLength " << fSubBoxLength / 1000.0;
    return setup.str();
}

bool TRestGeant4CRY::Returns(const std::string& name) const {
    return std::find(fParticles.begin(), fParticles.end(), name) != fParticles.end();
}
