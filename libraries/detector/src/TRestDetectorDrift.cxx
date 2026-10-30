#include "TRestDetectorDrift.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <stdexcept>

#include "TFile.h"
#include "TRandom.h"
#include "TRestDetectorTransport.h"

using ROOT::Math::XYZVector;

static const bool TRestDetectorDrift_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using M = TRestDetectorDrift;
    reg.RegisterField<M>("electricField", &M::fElectricField);
    reg.RegisterField<M>("pressure", &M::fPressure);
    reg.RegisterField<M>("temperature", &M::fTemperature);
    reg.RegisterField<M>("wValue", &M::fWValue);
    reg.RegisterField<M>("fanoFactor", &M::fFanoFactor);
    reg.RegisterField<M>("driftVelocity", &M::fDriftVelocity);
    reg.RegisterField<M>("longDiff", &M::fLongitudinalDiffusion);
    reg.RegisterField<M>("transDiff", &M::fTransversalDiffusion);
    reg.RegisterField<M>("attachment", &M::fAttachment);
    reg.RegisterField<M>("medium", &M::fMedium);
    reg.RegisterField<M>("mediumFile", &M::fMediumFile);
    reg.RegisterField<M>("table", &M::fTable);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorDrift", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorDrift>(instanceName, params);
        });
    return true;
}();

std::filesystem::path ExpandHome(const std::string& path) {
    const char* home = std::getenv("HOME");
    if (home && !path.empty() && path[0] == '~') return home + path.substr(1);
    return path;
}
}  // namespace

TRestDetectorDrift::TRestDetectorDrift() : TRestMetadata() { fName = "TRestDetectorDrift"; }

TRestDetectorDrift::TRestDetectorDrift(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestDetectorDrift::TRestDetectorDrift(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorDrift::LoadConfig() {
    UpdateParamsFromYAML<TRestDetectorDrift>(fNode);
    fTransport.reset();
    if (!fMediumFile.empty()) LoadTransportTable();
    CheckParameters();
    UpdateYAMLFromParams<TRestDetectorDrift>(fNode);
}

std::string TRestDetectorDrift::SelectTable(const YAML::Node& tables) const {
    struct Candidate {
        std::string key;
        double pressure, temperature, minField, maxField;
        bool scaling;
    };
    std::vector<Candidate> candidates;
    for (const auto& table : tables) {
        const auto range = table["fieldRange"].as<std::vector<double>>();
        candidates.push_back({table["key"].as<std::string>(), table["pressure"].as<double>(),
                              table["temperature"].as<double>(), range.at(0), range.at(1),
                              TRestTools::GetYAMLParam<bool>(table, "pressureScaling", true)});
    }
    auto sameCondition = [&](const Candidate& c) {
        return std::abs(c.pressure - fPressure) <= 1e-6 * fPressure &&
               std::abs(c.temperature - fTemperature) <= 0.01;
    };
    // Range at the pressure of the medium (scaled with E/p for gases)
    auto contains = [&](const Candidate& c) {
        const double scale = sameCondition(c) ? 1 : fPressure / c.pressure;
        return fElectricField >= c.minField * scale && fElectricField <= c.maxField * scale;
    };

    const Candidate* best = nullptr;
    for (const auto& c : candidates) {
        if (!sameCondition(c) || !contains(c)) continue;
        if (!best || c.maxField / c.minField < best->maxField / best->minField) best = &c;
    }
    if (best) return best->key;

    for (const auto& c : candidates) {
        if (!c.scaling || std::abs(c.temperature - fTemperature) > 0.01 || !contains(c)) continue;
        const double distance = std::abs(std::log(c.pressure / fPressure));
        if (!best || distance < std::abs(std::log(best->pressure / fPressure))) best = &c;
    }
    if (best) {
        RESTWarning << "TRestDetectorDrift '" << fName << "': no table at " << fPressure << " bar, using "
                    << best->key << " scaled with E/p" << RESTendl;
        return best->key;
    }
    throw std::runtime_error("TRestDetectorDrift '" + fName + "': no table of medium '" + fMedium +
                             "' at the pressure, temperature and electricField (generate it with "
                             "restGarfield or set 'table')");
}

void TRestDetectorDrift::LoadTransportTable() {
    const std::string file = ExpandHome(fMediumFile).string();
    if (!std::filesystem::is_regular_file(file)) {
        if (fDriftVelocity <= 0) {
            throw std::runtime_error("TRestDetectorDrift '" + fName + "': medium file " + fMediumFile +
                                     " not found (generate it with restGarfield)");
        }
        RESTWarning << "TRestDetectorDrift '" << fName << "': medium file " << fMediumFile
                    << " not found, using the transport parameters of the YAML" << RESTendl;
        return;
    }
    if (fMedium.empty()) fMedium = std::filesystem::path(file).stem().string();

    std::unique_ptr<TFile> rootFile(TFile::Open(file.c_str(), "READ"));
    if (!rootFile || rootFile->IsZombie()) {
        throw std::runtime_error("TRestDetectorDrift '" + fName + "': cannot open " + file);
    }
    const auto tables = TRestDetectorTransport::GetTables(rootFile.get(), fMedium);
    const std::string key = fTable.empty() ? SelectTable(tables) : fTable;
    fTransport = std::make_shared<const TRestDetectorTransport>(
        TRestDetectorTransport::Read(rootFile.get(), fMedium, key));
    const auto& table = *fTransport;
    if (!table.HasCarrier("electron")) {
        throw std::runtime_error("TRestDetectorDrift '" + fName + "': table " + key + " has no electrons");
    }

    const auto [minField, maxField] = table.GetFieldRange(fPressure);
    if (fElectricField < minField || fElectricField > maxField) {
        RESTWarning << "TRestDetectorDrift '" << fName << "': electricField out of the range of table " << key
                    << ", the transport parameters are extrapolated" << RESTendl;
    }
    if (std::abs(fTemperature - table.fTemperature) > 0.01) {
        RESTWarning << "TRestDetectorDrift '" << fName << "': temperature " << fTemperature << " K but table "
                    << key << " is at " << table.fTemperature << " K" << RESTendl;
    }
    fDriftVelocity = table.GetDriftVelocity(fElectricField, fPressure);
    fLongitudinalDiffusion = table.GetLongitudinalDiffusion(fElectricField, fPressure);
    fTransversalDiffusion = table.GetTransversalDiffusion(fElectricField, fPressure);
    fAttachment = table.GetAttachment(fElectricField, fPressure);
    fMediumFile = file;
    fTable = key;
}

namespace {
/// Without table only the electron parameters of the YAML exist
void CheckYAMLCarrier(const std::string& name, const std::string& carrier) {
    if (carrier != "electron") {
        throw std::runtime_error("TRestDetectorDrift '" + name + "': no " + carrier +
                                 " transport without a table (mediumFile)");
    }
}
}  // namespace

double TRestDetectorDrift::GetDriftVelocity(double field, const std::string& carrier) const {
    if (fTransport) return fTransport->GetDriftVelocity(field, fPressure, carrier);
    CheckYAMLCarrier(fName, carrier);
    return fDriftVelocity;
}

double TRestDetectorDrift::GetLongitudinalDiffusion(double field, const std::string& carrier) const {
    if (fTransport) return fTransport->GetLongitudinalDiffusion(field, fPressure, carrier);
    CheckYAMLCarrier(fName, carrier);
    return fLongitudinalDiffusion;
}

double TRestDetectorDrift::GetTransversalDiffusion(double field, const std::string& carrier) const {
    if (fTransport) return fTransport->GetTransversalDiffusion(field, fPressure, carrier);
    CheckYAMLCarrier(fName, carrier);
    return fTransversalDiffusion;
}

double TRestDetectorDrift::GetTownsend(double field, const std::string& carrier) const {
    if (fTransport) return fTransport->GetTownsend(field, fPressure, carrier);
    CheckYAMLCarrier(fName, carrier);
    return 0;
}

double TRestDetectorDrift::GetAttachment(double field, const std::string& carrier) const {
    if (fTransport) return fTransport->GetAttachment(field, fPressure, carrier);
    CheckYAMLCarrier(fName, carrier);
    return fAttachment;
}

void TRestDetectorDrift::CheckParameters() const {
    const std::string prefix = GetClassName() + " '" + fName + "': ";
    if (fWValue <= 0) throw std::runtime_error(prefix + "wValue must be > 0");
    if (fFanoFactor < 0 || fDriftVelocity < 0 || fLongitudinalDiffusion < 0 || fTransversalDiffusion < 0 ||
        fAttachment < 0) {
        throw std::runtime_error(
            prefix + "fanoFactor, driftVelocity, longDiff, transDiff and attachment must be >= 0");
    }
    if (fPressure <= 0 || fTemperature <= 0) {
        throw std::runtime_error(prefix + "pressure and temperature must be > 0");
    }
}

bool TRestDetectorDrift::DiffuseElectron(XYZVector& position, const XYZVector& normal, double distance,
                                         TRandom& random) const {
    // Coefficients in sqrt(cm) and 1/cm, distances in mm
    const double distanceCm = distance / 10.;
    if (fAttachment > 0 && random.Uniform() > std::exp(-fAttachment * distanceCm)) return false;

    const double sigmaL = 10. * fLongitudinalDiffusion * std::sqrt(distanceCm);
    const double sigmaT = 10. * fTransversalDiffusion * std::sqrt(distanceCm);
    // Transverse basis of the normal
    const XYZVector u =
        (std::abs(normal.X()) < 0.9 ? XYZVector(1, 0, 0) : XYZVector(0, 1, 0)).Cross(normal).Unit();
    const XYZVector v = normal.Cross(u);
    position += random.Gaus(0, sigmaT) * u + random.Gaus(0, sigmaT) * v + random.Gaus(0, sigmaL) * normal;
    return true;
}
