/// \class TRestGeant4CRYShowers
/// Cosmic-ray showers generated with CRY (Cosmic-ray Shower Library, LLNL): restCRY (packages/restG4)
/// runs CRY with this configuration and writes the showers file; the `cry` generator of restG4
/// (TRestGeant4SpatialGeneratorCRY) reads it, one shower (all its particles) per Geant4 event. This class
/// does not depend on CRY: only restCRY is linked to it.
///
/// ### Parameters
///
/// * returnNeutrons, returnProtons, returnGammas, returnElectrons, returnMuons, returnPions, returnKaons:
///   particles returned by CRY (true by default).
/// * nParticlesMin, nParticlesMax: showers with less or more particles are not returned (1, 1000000).
/// * date: month-day-year, for the 11-year solar cycle ("7-1-2012").
/// * latitude: geomagnetic latitude in degrees, -90 to 90 (90).
/// * altitude: 0, 2100 m or 11300 m, the altitudes of the CRY data tables (0).
/// * subBoxLength: side of the horizontal square, centered at the origin, where the particles are returned
///   (100 m). It should be about the size of the geometry: the particles outside it are not returned, the
///   ones far from the detector waste time in restG4.
/// * nShowers, exposureTime: the simulation stops at the first one reached (0: no limit, at least one is
///   needed); exposureTime is the time simulated by CRY.
/// * seed: of the random generator (0: a random one, written back to the stored metadata).
/// * outputFileName: the showers file, overwritten if it exists ("cry_showers.root").
///
/// The CRY offsets (xoffset, yoffset, zoffset) are not used: the showers are placed in the geometry by
/// the `position` and `verticalAxis` of the restG4 generator.
///
/// ### Showers file
///
/// * TTree `showers`, one entry per shower, with a vector per branch (one element per particle):
///   `particle` (Geant4 name), `charge` (e), `energy` (kinetic, keV), `positionX`, `positionY`,
///   `positionZ` (mm, CRY coordinates: horizontal plane z = 0, +Z up), `directionX`, `directionY`,
///   `directionZ` and `time` (us, relative to the first particle of the shower).
/// * RESTMetadataStore: this metadata, with the results: `showersGenerated` (entries of the tree),
///   `timeSimulated` (the exposure, time simulated by CRY), `program` (CRY version) and `creationDate`.
///
/// Example:
///
///    cryShowers:
///      class: "TRestGeant4CRYShowers"
///      returnNeutrons: true
///      returnProtons: true
///      returnGammas: true
///      returnElectrons: true
///      returnMuons: true
///      returnPions: true
///      returnKaons: true
///      date: "7-1-2012"
///      latitude: 90
///      altitude: 0
///      subBoxLength: 5 m
///      nShowers: 1000000
///      exposureTime: 1 day
///      seed: 137
///      outputFileName: "cry_showers.root"
///
/// Migrated from legacy TRestGeant4ParticleSourceCry (a particle source calling CRY during the Geant4
/// simulation): CRY now runs before, in its own program, and the exposure (time simulated) is stored.

#include "TRestGeant4CRYShowers.h"

#include <TFile.h>

#include <cmath>
#include <ctime>
#include <regex>
#include <sstream>
#include <stdexcept>

#include "TRestTools.h"

static const bool TRestGeant4CRYShowers_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using C = TRestGeant4CRYShowers;
    reg.RegisterField<C>("returnNeutrons", &C::fReturnNeutrons);
    reg.RegisterField<C>("returnProtons", &C::fReturnProtons);
    reg.RegisterField<C>("returnGammas", &C::fReturnGammas);
    reg.RegisterField<C>("returnElectrons", &C::fReturnElectrons);
    reg.RegisterField<C>("returnMuons", &C::fReturnMuons);
    reg.RegisterField<C>("returnPions", &C::fReturnPions);
    reg.RegisterField<C>("returnKaons", &C::fReturnKaons);
    reg.RegisterField<C>("nParticlesMin", &C::fNParticlesMin);
    reg.RegisterField<C>("nParticlesMax", &C::fNParticlesMax);
    reg.RegisterField<C>("date", &C::fDate);
    reg.RegisterField<C>("latitude", &C::fLatitude);
    reg.RegisterField<C>("altitude", &C::fAltitude);
    reg.RegisterField<C>("subBoxLength", &C::fSubBoxLength);
    reg.RegisterField<C>("nShowers", &C::fNShowers);
    reg.RegisterField<C>("exposureTime", &C::fExposureTime);
    reg.RegisterField<C>("seed", &C::fSeed);
    reg.RegisterField<C>("outputFileName", &C::fOutputFileName);
    reg.RegisterField<C>("showersGenerated", &C::fShowersGenerated);
    reg.RegisterField<C>("timeSimulated", &C::fTimeSimulated);
    reg.RegisterField<C>("program", &C::fProgram);
    reg.RegisterField<C>("creationDate", &C::fCreationDate);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestGeant4CRYShowers", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestGeant4CRYShowers>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestGeant4CRYShowers::TRestGeant4CRYShowers(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

void TRestGeant4CRYShowers::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4CRYShowers>(fNode);

    const std::string error = GetClassName() + " '" + GetName() + "': ";
    if (fNShowers < 0 || fExposureTime < 0) {
        throw std::runtime_error(error + "nShowers and exposureTime must not be negative");
    }
    if (fNShowers == 0 && fExposureTime == 0) {
        throw std::runtime_error(error + "nShowers or exposureTime is needed (the first one reached stops)");
    }
    if (fNParticlesMin < 1 || fNParticlesMax < fNParticlesMin) {
        throw std::runtime_error(error + "it must be 1 <= nParticlesMin <= nParticlesMax");
    }
    if (std::abs(fLatitude) > 90) throw std::runtime_error(error + "latitude must be in [-90, 90]");
    const double altitude = fAltitude / 1000.0;  // m
    if (altitude != 0 && altitude != 2100 && altitude != 11300) {
        throw std::runtime_error(error + "altitude must be 0, 2100 m or 11300 m (the CRY data tables)");
    }
    if (fSubBoxLength <= 0) throw std::runtime_error(error + "subBoxLength must be positive");
    if (!std::regex_match(fDate, std::regex(R"(\d{1,2}-\d{1,2}-\d{4})"))) {
        throw std::runtime_error(error + "date must be month-day-year, e.g. 7-1-2012");
    }
    if (fOutputFileName.empty()) throw std::runtime_error(error + "outputFileName is empty");

    UpdateYAMLFromParams<TRestGeant4CRYShowers>(fNode);
}

std::string TRestGeant4CRYShowers::GetCRYSetup() const {
    std::ostringstream setup;
    setup.precision(10);
    setup << "returnNeutrons " << fReturnNeutrons << " returnProtons " << fReturnProtons << " returnGammas "
          << fReturnGammas << " returnElectrons " << fReturnElectrons << " returnMuons " << fReturnMuons
          << " returnPions " << fReturnPions << " returnKaons " << fReturnKaons << " nParticlesMin "
          << fNParticlesMin << " nParticlesMax " << fNParticlesMax << " date " << fDate << " latitude "
          << fLatitude << " altitude " << fAltitude / 1000.0 << " subboxLength " << fSubBoxLength / 1000.0;
    return setup.str();
}

void TRestGeant4CRYShowers::SetSeed(long seed) {
    fSeed = seed;
    UpdateYAMLFromParams<TRestGeant4CRYShowers>(fNode);
}

void TRestGeant4CRYShowers::SetResults(int64_t showers, double timeSimulated, const std::string& program) {
    fShowersGenerated = showers;
    fTimeSimulated = timeSimulated;
    fProgram = program;
    const std::time_t now = std::time(nullptr);
    char date[32];
    std::strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    fCreationDate = date;
    UpdateYAMLFromParams<TRestGeant4CRYShowers>(fNode);
}

TRestGeant4CRYShowers TRestGeant4CRYShowers::ReadFromFile(TFile* file) {
    const auto section =
        TRestTools::GetMetadataClass(TRestTools::ReadMetadataStore(file), "TRestGeant4CRYShowers");
    if (!section.second) {
        throw std::runtime_error("TRestGeant4CRYShowers: no TRestGeant4CRYShowers in " +
                                 std::string(file ? file->GetName() : "(no file)") +
                                 ", is it a restCRY file?");
    }
    return TRestGeant4CRYShowers(section.first, section.second);
}
