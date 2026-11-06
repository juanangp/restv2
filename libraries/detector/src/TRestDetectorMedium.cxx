#include "TRestDetectorMedium.h"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>

#include "TFile.h"
#include "TRandom.h"
#include "TRestDetectorTransportManager.h"
#include "TRestTools.h"
#include "TRestYAMLUtils.h"

using ROOT::Math::XYZVector;

static const bool TRestDetectorMedium_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using M = TRestDetectorMedium;
    reg.RegisterField<M>("state", &M::fState);
    reg.RegisterField<M>("components", &M::fComponents);
    reg.RegisterField<M>("mediumFile", &M::fMediumFile);
    reg.RegisterField<M>("pressure", &M::fPressure);
    reg.RegisterField<M>("temperature", &M::fTemperature);
    reg.RegisterField<M>("charge", &M::fChargeNode);
    reg.RegisterField<M>("light", &M::fLightNode);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorMedium", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorMedium>(instanceName, params);
        });
    return true;
}();

std::filesystem::path ExpandHome(const std::string& path) {
    const char* home = std::getenv("HOME");
    if (home && !path.empty() && path[0] == '~') return home + path.substr(1);
    return path;
}
}  // namespace

TRestDetectorMedium::TRestDetectorMedium() : TRestMetadata() { fName = "TRestDetectorMedium"; }

TRestDetectorMedium::TRestDetectorMedium(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestDetectorMedium::TRestDetectorMedium(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

TRestDetectorMedium::~TRestDetectorMedium() = default;

void TRestDetectorMedium::LoadConfig() {
    UpdateParamsFromYAML<TRestDetectorMedium>(fNode);
    const std::string prefix = "TRestDetectorMedium '" + fName + "': ";
    if (!fNode["name"]) throw std::runtime_error("TRestDetectorMedium: the medium needs a name");
    fState = TRestTools::NormalizeOption("state", fState, kStates);
    for (const auto& [component, fraction] : fComponents) {
        if (fraction <= 0) throw std::runtime_error(prefix + "the fraction of " + component + " must be > 0");
    }
    if (fPressure <= 0 || fTemperature <= 0) {
        throw std::runtime_error(prefix + "pressure and temperature must be > 0");
    }

    // Medium file: the definition (from the file, or checked against it), given to the responses
    std::unique_ptr<TFile> file;
    if (!fMediumFile.empty()) {
        const std::string path = GetMediumFile().string();
        if (std::filesystem::is_regular_file(path)) {
            file.reset(TFile::Open(path.c_str(), "READ"));
            if (!file || file->IsZombie()) throw std::runtime_error(prefix + "cannot open " + path);
            fMediumFile = path;
            const YAML::Node metadata = TRestMetadata::ReadMetadata(file.get(), fName);
            if (!metadata || !metadata.IsMap()) {
                throw std::runtime_error(prefix + "no medium '" + fName + "' in " + path);
            }
            if (metadata["components"]) {
                const auto components = metadata["components"].as<std::map<std::string, double>>();
                const std::string state = TRestYAMLUtils::GetParam<std::string>(metadata, "state", fState);
                if (fNode["components"] && (components != fComponents || state != fState)) {
                    throw std::runtime_error(prefix + "state or components different from those of " + path);
                }
                fComponents = components;
                fState = state;
            }
        } else {
            RESTWarning << prefix << "medium file " << fMediumFile
                        << " not found: no tables, the fixed parameters of the responses are used"
                        << RESTendl;
        }
    }
    // The pressure only applies to gases
    if (!IsGas() && fNode["pressure"]) {
        throw std::runtime_error(prefix + "pressure is only for gases (state " + fState + ")");
    }

    fCharge.reset();
    if (fChargeNode && !fChargeNode.IsNull()) {
        if (!fChargeNode.IsMap()) throw std::runtime_error(prefix + "charge must be a map");
        fCharge = std::make_shared<TRestDetectorChargeResponse>("charge", fChargeNode);
        fCharge->CheckConfig();
        fCharge->SetMedium(*this, file.get());
    }
    fLight.reset();
    if (fLightNode && !fLightNode.IsNull()) {
        if (!fLightNode.IsMap()) throw std::runtime_error(prefix + "light must be a map");
        fLight = std::make_shared<TRestDetectorLightResponse>("light", fLightNode);
        fLight->CheckConfig();
    }
    UpdateYAMLFromParams<TRestDetectorMedium>(fNode);
}

const TRestDetectorLightResponse& TRestDetectorMedium::GetLightResponse(const std::string& user) const {
    if (!fLight) {
        throw std::runtime_error(user + ": the medium '" + fName + "' has no light response (key 'light')");
    }
    return *fLight;
}

const TRestDetectorChargeResponse& TRestDetectorMedium::GetChargeResponse(const std::string& user) const {
    if (!fCharge) {
        throw std::runtime_error(user + ": the medium '" + fName + "' has no charge response (key 'charge')");
    }
    return *fCharge;
}

YAML::Node TRestDetectorMedium::GetDefinition() const {
    YAML::Node definition;
    definition["name"] = fName;
    definition["state"] = fState;
    definition["components"] = fComponents;
    return definition;
}

std::filesystem::path TRestDetectorMedium::GetMediumFile() const {
    return ExpandHome(fMediumFile.empty() ? fName + ".root" : fMediumFile).lexically_normal();
}

YAML::Node TRestDetectorMedium::GetFileMetadata(TFile* file, const std::string& name,
                                                const YAML::Node& definition) {
    // ReadMetadata gives a null node (defined for yaml-cpp) if the file has no medium yet
    YAML::Node metadata = TRestMetadata::ReadMetadata(file, name);
    if (!metadata || !metadata.IsMap()) metadata = YAML::Node(YAML::NodeType::Map);
    if (metadata["components"]) {
        if (definition["components"] && definition["components"].as<std::map<std::string, double>>() !=
                                            metadata["components"].as<std::map<std::string, double>>()) {
            throw std::runtime_error("TRestDetectorMedium: " + std::string(file->GetName()) +
                                     " belongs to another medium (different components)");
        }
    } else {
        // First table, or a file written without the definition: the definition is stored
        if (definition["state"]) metadata["state"] = YAML::Clone(definition["state"]);
        if (definition["components"]) metadata["components"] = YAML::Clone(definition["components"]);
    }
    metadata["name"] = name;
    return metadata;
}

// ============================================================
// TRestDetectorLightResponse
// ============================================================

static const bool TRestDetectorLightResponse_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using L = TRestDetectorLightResponse;
    reg.RegisterField<L>("attenuationLength", &L::fAttenuationLength);
    reg.RegisterField<L>("effectiveLightSpeed", &L::fEffectiveLightSpeed);
    return true;
}();

TRestDetectorLightResponse::TRestDetectorLightResponse() : TRestMetadata() {
    fName = "TRestDetectorLightResponse";
}

TRestDetectorLightResponse::TRestDetectorLightResponse(const std::string& instanceName,
                                                       const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

void TRestDetectorLightResponse::LoadConfig() {
    UpdateParamsFromYAML<TRestDetectorLightResponse>(fNode);
    if (fAttenuationLength < 0 || fEffectiveLightSpeed < 0) {
        throw std::runtime_error(
            "TRestDetectorLightResponse: attenuationLength and effectiveLightSpeed must be >= 0");
    }
    UpdateYAMLFromParams<TRestDetectorLightResponse>(fNode);
}

// ============================================================
// TRestDetectorChargeResponse
// ============================================================

static const bool TRestDetectorChargeResponse_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using C = TRestDetectorChargeResponse;
    reg.RegisterField<C>("wValue", &C::fWValue);
    reg.RegisterField<C>("fanoFactor", &C::fFanoFactor);
    reg.RegisterField<C>("driftField", &C::fDriftField);
    reg.RegisterField<C>("driftVelocity", &C::fDriftVelocity);
    reg.RegisterField<C>("longDiff", &C::fLongitudinalDiffusion);
    reg.RegisterField<C>("transDiff", &C::fTransversalDiffusion);
    reg.RegisterField<C>("attachment", &C::fAttachment);
    reg.RegisterField<C>("transportTable", &C::fTransportTable);
    reg.RegisterField<C>("loadedTables", &C::fLoadedTables);
    return true;
}();

namespace {
bool SamePressure(double a, double b) { return std::abs(a - b) <= 1e-6 * b; }
bool SameTemperature(double a, double b) { return std::abs(a - b) <= 0.01; }
}  // namespace

TRestDetectorChargeResponse::TRestDetectorChargeResponse() : TRestMetadata() {
    fName = "TRestDetectorChargeResponse";
}

TRestDetectorChargeResponse::TRestDetectorChargeResponse(const std::string& instanceName,
                                                         const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestDetectorChargeResponse::~TRestDetectorChargeResponse() = default;

void TRestDetectorChargeResponse::LoadConfig() {
    UpdateParamsFromYAML<TRestDetectorChargeResponse>(fNode);
    if (fWValue < 0 || fFanoFactor < 0 || fDriftField < 0 || fDriftVelocity < 0 ||
        fLongitudinalDiffusion < 0 || fTransversalDiffusion < 0 || fAttachment < 0) {
        throw std::runtime_error(
            "TRestDetectorChargeResponse: wValue, fanoFactor, driftField, driftVelocity, longDiff, transDiff "
            "and attachment must be >= 0");
    }
    UpdateYAMLFromParams<TRestDetectorChargeResponse>(fNode);
}

void TRestDetectorChargeResponse::SetMedium(const TRestDetectorMedium& medium, TFile* file) {
    fMediumName = medium.GetName();
    fPressure = medium.GetPressure();
    fTemperature = medium.GetTemperature();
    fTransportTables.clear();
    fGainTables.clear();
    fLoadedTables.clear();
    fRangeWarned->store(false);
    if (file) LoadTables(file);
    UpdateYAMLFromParams<TRestDetectorChargeResponse>(fNode);
}

void TRestDetectorChargeResponse::LoadTables(TFile* file) {
    const std::string prefix = "TRestDetectorChargeResponse of '" + fMediumName + "': ";
    // Transport tables: at pressure and temperature, else (gases) the closest pressure
    const YAML::Node transport = TRestDetectorTransportTable::GetTables(file, fMediumName);
    std::vector<std::string> keys;
    if (!fTransportTable.empty()) {
        keys.push_back(fTransportTable);
    } else {
        double closest = -1;
        for (size_t i = 0; i < transport.size(); i++) {
            const double pressure = transport[i]["pressure"].as<double>();
            const double temperature = transport[i]["temperature"].as<double>();
            const bool scaling = TRestYAMLUtils::GetParam<bool>(transport[i], "pressureScaling", true);
            if (!SameTemperature(temperature, fTemperature) ||
                (!scaling && !SamePressure(pressure, fPressure)))
                continue;
            if (closest < 0 ||
                std::abs(std::log(pressure / fPressure)) < std::abs(std::log(closest / fPressure)) - 1e-9) {
                closest = pressure;
            }
        }
        for (size_t i = 0; i < transport.size(); i++) {
            if (closest > 0 && SamePressure(transport[i]["pressure"].as<double>(), closest) &&
                SameTemperature(transport[i]["temperature"].as<double>(), fTemperature)) {
                keys.push_back(transport[i]["key"].as<std::string>());
            }
        }
        if (closest > 0 && !SamePressure(closest, fPressure)) {
            RESTDebug << prefix << "transport tables at " << closest << " bar scaled with E/p to "
                      << fPressure << " bar" << RESTendl;
        }
    }
    for (const auto& key : keys) {
        auto table = std::make_shared<const TRestDetectorTransportTable>(
            TRestDetectorTransportTable::Read(file, fMediumName, key));
        if (!table->HasCarrier("electron")) {
            throw std::runtime_error(prefix + "transport table " + key + " has no electrons");
        }
        if (!SameTemperature(table->fTemperature, fTemperature)) {
            RESTWarning << prefix << "temperature " << fTemperature << " K but transport table " << key
                        << " is at " << table->fTemperature << " K" << RESTendl;
        }
        fTransportTables.push_back(table);
        fLoadedTables.push_back(key);
    }

    // Gain tables: only at pressure and temperature
    const YAML::Node gain = TRestDetectorGainTable::GetTables(file, fMediumName);
    for (size_t i = 0; i < gain.size(); i++) {
        if (!SamePressure(gain[i]["pressure"].as<double>(), fPressure) ||
            !SameTemperature(gain[i]["temperature"].as<double>(), fTemperature)) {
            continue;
        }
        const auto key = gain[i]["key"].as<std::string>();
        fGainTables.push_back(std::make_shared<const TRestDetectorGainTable>(
            TRestDetectorGainTable::Read(file, fMediumName, key)));
        fLoadedTables.push_back(key);
    }
    if (fLoadedTables.empty()) {
        RESTWarning << prefix << "no tables at " << fPressure << " bar and " << fTemperature << " K in "
                    << file->GetName() << ", the fixed transport parameters are used" << RESTendl;
    }
}

const TRestDetectorTransportTable* TRestDetectorChargeResponse::GetTransportTable(double field) const {
    const TRestDetectorTransportTable* best = nullptr;
    double bestWidth = 0;
    for (const auto& table : fTransportTables) {
        const auto [minField, maxField] = table->GetFieldRange(fPressure);
        if (field < minField || field > maxField) continue;
        const double width = maxField / minField;
        if (!best || width < bestWidth) {
            best = table.get();
            bestWidth = width;
        }
    }
    if (best || fTransportTables.empty()) return best;

    // Out of all of them: the closest range
    double bestDistance = std::numeric_limits<double>::max();
    for (const auto& table : fTransportTables) {
        const auto [minField, maxField] = table->GetFieldRange(fPressure);
        const double distance = std::abs(std::log(field < minField ? minField / field : field / maxField));
        if (distance < bestDistance) {
            best = table.get();
            bestDistance = distance;
        }
    }
    if (!fRangeWarned->exchange(true)) {
        RESTWarning << "TRestDetectorChargeResponse of '" << fMediumName << "': field " << field * 10
                    << " V/cm out of the range of the transport tables, using the closest one ("
                    << best->GetKey() << ", constant out of its range)" << RESTendl;
    }
    return best;
}

void TRestDetectorChargeResponse::CheckCarrier(const std::string& carrier) const {
    if (carrier != "electron") {
        throw std::runtime_error("TRestDetectorChargeResponse of '" + fMediumName + "': no " + carrier +
                                 " transport without a transport table (mediumFile)");
    }
}

double TRestDetectorChargeResponse::GetDriftVelocity(double field, const std::string& carrier) const {
    if (const auto* table = GetTransportTable(field))
        return table->GetDriftVelocity(field, fPressure, carrier);
    CheckCarrier(carrier);
    return fDriftVelocity;
}

double TRestDetectorChargeResponse::GetDriftFieldForTables() const {
    if (HasTransportTables() && fDriftField <= 0) {
        throw std::runtime_error("TRestDetectorChargeResponse of '" + fMediumName +
                                 "': the transport tables need the drift field (driftField in charge)");
    }
    return fDriftField;
}

double TRestDetectorChargeResponse::GetDriftVelocity() const {
    return GetDriftVelocity(GetDriftFieldForTables());
}

double TRestDetectorChargeResponse::GetLongitudinalDiffusion() const {
    return GetLongitudinalDiffusion(GetDriftFieldForTables());
}

double TRestDetectorChargeResponse::GetTransversalDiffusion() const {
    return GetTransversalDiffusion(GetDriftFieldForTables());
}

double TRestDetectorChargeResponse::GetAttachment() const { return GetAttachment(GetDriftFieldForTables()); }

double TRestDetectorChargeResponse::GetLongitudinalDiffusion(double field, const std::string& carrier) const {
    if (const auto* table = GetTransportTable(field)) {
        return table->GetLongitudinalDiffusion(field, fPressure, carrier);
    }
    CheckCarrier(carrier);
    return fLongitudinalDiffusion;
}

double TRestDetectorChargeResponse::GetTransversalDiffusion(double field, const std::string& carrier) const {
    if (const auto* table = GetTransportTable(field)) {
        return table->GetTransversalDiffusion(field, fPressure, carrier);
    }
    CheckCarrier(carrier);
    return fTransversalDiffusion;
}

double TRestDetectorChargeResponse::GetTownsend(double field, const std::string& carrier) const {
    if (const auto* table = GetTransportTable(field)) return table->GetTownsend(field, fPressure, carrier);
    CheckCarrier(carrier);
    return 0;
}

double TRestDetectorChargeResponse::GetAttachment(double field, const std::string& carrier) const {
    if (const auto* table = GetTransportTable(field)) return table->GetAttachment(field, fPressure, carrier);
    CheckCarrier(carrier);
    return fAttachment;
}

const TRestDetectorGainTable& TRestDetectorChargeResponse::GetGainTable(double gap,
                                                                        const std::string& key) const {
    std::vector<const TRestDetectorGainTable*> candidates;
    std::string available;
    for (const auto& table : fGainTables) {
        available += (available.empty() ? "" : ", ") + table->GetKey();
        if (std::abs(table->fTableGap - gap) > 1e-6 * gap) continue;
        if (!key.empty() && table->GetKey() != key) continue;
        candidates.push_back(table.get());
    }
    const std::string prefix = "TRestDetectorChargeResponse of '" + fMediumName + "': ";
    if (candidates.empty()) {
        throw std::runtime_error(prefix + "no gain table with gap " + std::to_string(gap * 1000) + " um" +
                                 (key.empty() ? "" : " and key " + key) + " at " + std::to_string(fPressure) +
                                 " bar and " + std::to_string(fTemperature) +
                                 " K (available: " + (available.empty() ? "none" : available) +
                                 "; generate it with restGarfield)");
    }
    if (candidates.size() > 1) {
        std::string keys;
        for (const auto* table : candidates) keys += (keys.empty() ? "" : ", ") + table->GetKey();
        throw std::runtime_error(prefix + "several gain tables with that gap (" + keys +
                                 "): choose one with the key");
    }
    return *candidates.front();
}

bool TRestDetectorChargeResponse::DiffuseElectron(XYZVector& position, const XYZVector& normal,
                                                  double distance, double longDiff, double transDiff,
                                                  double attachment, TRandom& random) {
    // Coefficients in sqrt(cm) and 1/cm, distances in mm
    const double distanceCm = distance / 10.;
    if (attachment > 0 && random.Uniform() > std::exp(-attachment * distanceCm)) return false;

    const double sigmaL = 10. * longDiff * std::sqrt(distanceCm);
    const double sigmaT = 10. * transDiff * std::sqrt(distanceCm);
    // Transverse basis of the normal
    const XYZVector u =
        (std::abs(normal.X()) < 0.9 ? XYZVector(1, 0, 0) : XYZVector(0, 1, 0)).Cross(normal).Unit();
    const XYZVector v = normal.Cross(u);
    position += random.Gaus(0, sigmaT) * u + random.Gaus(0, sigmaT) * v + random.Gaus(0, sigmaL) * normal;
    return true;
}
