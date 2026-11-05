#include "TRestDetectorTransportManager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <tuple>

#include "TDirectory.h"
#include "TFile.h"
#include "TGraph.h"
#include "TH1D.h"
#include "TRestYAMLUtils.h"

static const bool TRestDetectorTransportManager_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using M = TRestDetectorTransportManager;
    reg.RegisterField<M>("medium", &M::fMediumNode);
    reg.RegisterField<M>("transport", &M::fTransportNode);
    reg.RegisterField<M>("gain", &M::fGainNode);

    using T = TRestDetectorTransportTable;
    reg.RegisterField<T>("pressure", &T::fPressure);
    reg.RegisterField<T>("temperatures", &T::fTemperatures);
    reg.RegisterField<T>("fieldRange", &T::fFieldRange);
    reg.RegisterField<T>("fieldNodes", &T::fFieldNodes);
    reg.RegisterField<T>("logField", &T::fLogField);
    reg.RegisterField<T>("nCollisions", &T::fCollisions);
    reg.RegisterField<T>("maxElectronEnergy", &T::fMaxElectronEnergy);
    reg.RegisterField<T>("usePenning", &T::fUsePenning);
    reg.RegisterField<T>("penningRate", &T::fPenningRate);
    reg.RegisterField<T>("overwrite", &T::fOverwrite);

    using G = TRestDetectorGainTable;
    reg.RegisterField<G>("gap", &G::fGap);
    reg.RegisterField<G>("pressures", &G::fPressures);
    reg.RegisterField<G>("temperatures", &G::fTemperatures);
    reg.RegisterField<G>("reducedFieldRange", &G::fReducedFieldRange);
    reg.RegisterField<G>("fieldNodes", &G::fFieldNodes);
    reg.RegisterField<G>("avalanches", &G::fAvalanches);
    reg.RegisterField<G>("usePenning", &G::fUsePenning);
    reg.RegisterField<G>("penningRate", &G::fPenningRate);
    reg.RegisterField<G>("overwrite", &G::fOverwrite);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorTransportManager", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorTransportManager>(instanceName, params);
        });
    return true;
}();

constexpr double kVcmPerVmm = 10.;
/// Entries of the transport and gain tables in the metadata of the medium
const std::string kTransportEntries = "tables";
const std::string kGainEntries = "amplifications";

/// Number without trailing zeros (99, 0.5, 293.15)
std::string Format(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%g", value);
    return buffer;
}

/// Entries (list `entries` of the metadata of the medium), empty if there are none
YAML::Node GetEntries(TFile* file, const std::string& medium, const std::string& entries) {
    const YAML::Node metadata = TRestMetadata::ReadMetadata(file, medium);
    if (!metadata || !metadata.IsMap() || !metadata[entries] || !metadata[entries].IsSequence()) {
        return YAML::Node(YAML::NodeType::Sequence);
    }
    return metadata[entries];
}

/// Entry of a key, a null node if there is none (check IsMap: a null node is defined for yaml-cpp)
YAML::Node FindEntry(const YAML::Node& entries, const std::string& key) {
    // Index loop: range-for gives YAML::detail::iterator_value, which cannot be assigned to a node
    YAML::Node entry;
    for (size_t i = 0; i < entries.size(); i++) {
        if (entries[i]["key"] && entries[i]["key"].as<std::string>() == key) entry = entries[i];
    }
    return entry;
}

/// Adds or replaces the entry of a table in the metadata of the medium (definition stored if the file has
/// none) and returns the directory of its graphs, `directory/<key>`, as current directory
TDirectory* AddEntry(TFile* file, const std::string& medium, const YAML::Node& definition,
                     const std::string& entries, const YAML::Node& entry, const std::string& directory,
                     const std::string& user) {
    if (!file || file->IsZombie() || !file->IsWritable()) {
        throw std::runtime_error(user + ": the medium file is not writable");
    }
    const std::string key = entry["key"].as<std::string>();
    YAML::Node metadata = TRestDetectorMedium::GetFileMetadata(file, medium, definition);
    YAML::Node tables(YAML::NodeType::Sequence);
    const YAML::Node stored = GetEntries(file, medium, entries);
    for (size_t i = 0; i < stored.size(); i++) {
        if (!stored[i]["key"] || stored[i]["key"].as<std::string>() != key) tables.push_back(stored[i]);
    }
    tables.push_back(entry);
    metadata[entries] = tables;
    TRestMetadata::WriteMetadata(file, medium, metadata);

    TDirectory* tablesDir = file->GetDirectory(directory.c_str());
    if (!tablesDir) tablesDir = file->mkdir(directory.c_str());
    TDirectory* keyDir = tablesDir ? tablesDir->GetDirectory(key.c_str()) : nullptr;
    if (tablesDir && !keyDir) keyDir = tablesDir->mkdir(key.c_str());
    if (!keyDir) throw std::runtime_error(user + ": cannot create " + directory + "/" + key);
    keyDir->cd();
    return keyDir;
}

/// Values of a graph versus the field in V/cm (fields to V/mm, the same for all the graphs of a table).
/// false if there is no graph
bool ReadGraph(TDirectory* directory, const std::string& name, std::vector<double>& fields,
               std::vector<double>& values, const std::string& error) {
    auto* graph = dynamic_cast<TGraph*>(directory->Get(name.c_str()));
    if (!graph) return false;
    std::vector<double> graphFields(graph->GetX(), graph->GetX() + graph->GetN());
    for (auto& field : graphFields) field /= kVcmPerVmm;
    if (fields.empty()) {
        fields = graphFields;
    } else if (graphFields != fields) {
        throw std::runtime_error(error + "the graphs have different fields");
    }
    values = std::vector<double>(graph->GetY(), graph->GetY() + graph->GetN());
    delete graph;
    return true;
}

void CheckFields(const std::vector<double>& fields, const std::string& error) {
    if (fields.empty()) throw std::runtime_error(error + "no graphs");
    if (!std::is_sorted(fields.begin(), fields.end()) || fields.front() <= 0) {
        throw std::runtime_error(error + "the fields must be positive and increasing");
    }
}

/// Graph of a quantity versus the field (V/mm, stored in V/cm)
void WriteGraph(const std::string& name, const std::string& title, const std::vector<double>& fields,
                const std::vector<double>& values) {
    std::vector<double> fieldsVcm = fields;
    for (auto& field : fieldsVcm) field *= kVcmPerVmm;
    TGraph graph(fields.size(), fieldsVcm.data(), values.data());
    graph.SetName(name.c_str());
    graph.SetTitle(title.c_str());
    graph.Write(name.c_str(), TObject::kOverwrite);
}

bool HasKey(const YAML::Node& entries, const std::string& key) { return FindEntry(entries, key).IsMap(); }

/// Configurations of a key: a map (one) or a list of maps
template <class T>
std::vector<std::unique_ptr<T>> CreateTables(const YAML::Node& node, const std::string& key) {
    std::vector<std::unique_ptr<T>> tables;
    if (!node || node.IsNull()) return tables;
    std::vector<YAML::Node> configurations;
    if (node.IsMap()) {
        configurations.push_back(node);
    } else if (node.IsSequence()) {
        for (size_t i = 0; i < node.size(); i++) configurations.push_back(node[i]);
    }
    for (size_t i = 0; i < configurations.size(); i++) {
        if (!configurations[i].IsMap()) configurations.clear();
    }
    if (configurations.empty()) {
        throw std::runtime_error("TRestDetectorTransportManager: " + key +
                                 " must be a map or a list of maps");
    }
    for (size_t i = 0; i < configurations.size(); i++) {
        const std::string name = configurations.size() == 1 ? key : key + "_" + std::to_string(i);
        tables.push_back(std::make_unique<T>(name, configurations[i]));
        tables.back()->CheckConfig();
    }
    return tables;
}
}  // namespace

// ============================================================
// TRestDetectorTransportTable
// ============================================================

using Carrier = TRestDetectorTransportTable::Carrier;

namespace {
/// Quantities stored as graphs: name and axis title
struct Quantity {
    const char* name;
    const char* title;
    std::vector<double> Carrier::* values;
};
const Quantity kQuantities[] = {
    {"driftVelocity", "drift velocity (mm/#mus)", &Carrier::fDriftVelocity},
    {"longitudinalDiffusion", "longitudinal diffusion (#sqrt{cm})", &Carrier::fLongitudinalDiffusion},
    {"transversalDiffusion", "transversal diffusion (#sqrt{cm})", &Carrier::fTransversalDiffusion},
    {"townsend", "Townsend coefficient (1/cm)", &Carrier::fTownsend},
    {"attachment", "attachment coefficient (1/cm)", &Carrier::fAttachment},
};
}  // namespace

TRestDetectorTransportTable::TRestDetectorTransportTable() : TRestMetadata() {
    fName = "TRestDetectorTransportTable";
}

TRestDetectorTransportTable::TRestDetectorTransportTable(const std::string& instanceName,
                                                         const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

void TRestDetectorTransportTable::LoadConfig() {
    UpdateParamsFromYAML<TRestDetectorTransportTable>(fNode);
    const std::string prefix = "TRestDetectorTransportTable '" + fName + "': ";
    if (fFieldNodes <= 0 || fFieldRange.first <= 0 || fFieldRange.first >= fFieldRange.second) {
        throw std::runtime_error(prefix + "fieldRange must be 0 < min < max and fieldNodes > 0");
    }
    if (fCollisions <= 0 || fMaxElectronEnergy <= 0) {
        throw std::runtime_error(prefix + "nCollisions and maxElectronEnergy must be > 0");
    }
    if (fPressure <= 0) throw std::runtime_error(prefix + "pressure must be > 0");
    for (double t : fTemperatures) {
        if (t <= 0) throw std::runtime_error(prefix + "temperatures must be > 0");
    }
    if (fUsePenning && (fPenningRate <= 0 || fPenningRate > 1)) {
        throw std::runtime_error(prefix + "usePenning needs a penningRate in (0, 1]");
    }
    UpdateYAMLFromParams<TRestDetectorTransportTable>(fNode);
}

std::vector<double> TRestDetectorTransportTable::GetTemperatures(const TRestDetectorMedium& medium) const {
    if (fTemperatures.empty()) return {medium.GetTemperature()};
    return fTemperatures;
}

std::vector<double> TRestDetectorTransportTable::GetFieldGrid() const {
    return GetFieldGrid(fFieldRange.first, fFieldRange.second, fFieldNodes, fLogField);
}

std::vector<double> TRestDetectorTransportTable::GetFieldGrid(double minField, double maxField, int nodes,
                                                              bool logField) {
    std::vector<double> fields;
    for (int i = 0; i < nodes; i++) {
        const double t = nodes > 1 ? i / (nodes - 1.) : 0;
        fields.push_back(logField ? minField * std::pow(maxField / minField, t)
                                  : minField + t * (maxField - minField));
    }
    return fields;
}

std::string TRestDetectorTransportTable::GetKey(double pressure, double temperature, double minField,
                                                double maxField, double penning) {
    return Format(pressure) + "bar_" + Format(temperature) + "K_E" + Format(minField * kVcmPerVmm) + "-" +
           Format(maxField * kVcmPerVmm) + "Vcm" + (penning > 0 ? "_Penning" + Format(penning) : "");
}

std::string TRestDetectorTransportTable::GetKey() const {
    if (fFields.empty()) throw std::runtime_error("TRestDetectorTransportTable: empty table");
    return GetKey(fPressure, fTemperature, fFields.front(), fFields.back(), fPenning);
}

std::pair<double, double> TRestDetectorTransportTable::GetFieldRange(double pressure) const {
    if (fFields.empty()) return {0, 0};
    const double scale = fPressureScaling ? pressure / fPressure : 1;
    return {fFields.front() * scale, fFields.back() * scale};
}

double TRestDetectorTransportTable::Interpolate(const std::vector<double>& values, double field,
                                                double pressure, bool logValues) const {
    const size_t n = std::min(values.size(), fFields.size());
    if (n == 0) return 0;
    // Field of the table with the same E/p
    const double x = fPressureScaling ? field * fPressure / pressure : field;
    if (n == 1 || x <= fFields.front()) return values.front();
    if (x >= fFields[n - 1]) return values[n - 1];
    const size_t i = std::upper_bound(fFields.begin(), fFields.begin() + n, x) - fFields.begin() - 1;
    const double t = std::log(x / fFields[i]) / std::log(fFields[i + 1] / fFields[i]);
    if (logValues && values[i] > 0 && values[i + 1] > 0) {
        return values[i] * std::pow(values[i + 1] / values[i], t);
    }
    return values[i] + t * (values[i + 1] - values[i]);
}

const Carrier& TRestDetectorTransportTable::GetCarrier(const std::string& carrier) const {
    const auto it = fCarriers.find(carrier);
    if (it == fCarriers.end()) {
        throw std::runtime_error("TRestDetectorTransportTable: the table of '" + fMedium + "' has no " +
                                 carrier + " transport");
    }
    return it->second;
}

double TRestDetectorTransportTable::GetDriftVelocity(double field, double pressure,
                                                     const std::string& carrier) const {
    return Interpolate(GetCarrier(carrier).fDriftVelocity, field, pressure, false);
}

double TRestDetectorTransportTable::GetLongitudinalDiffusion(double field, double pressure,
                                                             const std::string& carrier) const {
    const double scale = fPressureScaling ? std::sqrt(fPressure / pressure) : 1;
    return std::max(0., Interpolate(GetCarrier(carrier).fLongitudinalDiffusion, field, pressure, false)) *
           scale;
}

double TRestDetectorTransportTable::GetTransversalDiffusion(double field, double pressure,
                                                            const std::string& carrier) const {
    const double scale = fPressureScaling ? std::sqrt(fPressure / pressure) : 1;
    return std::max(0., Interpolate(GetCarrier(carrier).fTransversalDiffusion, field, pressure, false)) *
           scale;
}

double TRestDetectorTransportTable::GetTownsend(double field, double pressure,
                                                const std::string& carrier) const {
    const double scale = fPressureScaling ? pressure / fPressure : 1;
    return std::max(0., Interpolate(GetCarrier(carrier).fTownsend, field, pressure, true)) * scale;
}

double TRestDetectorTransportTable::GetAttachment(double field, double pressure,
                                                  const std::string& carrier) const {
    const double scale = fPressureScaling ? pressure / fPressure : 1;
    return std::max(0., Interpolate(GetCarrier(carrier).fAttachment, field, pressure, true)) * scale;
}

YAML::Node TRestDetectorTransportTable::GetTables(TFile* file, const std::string& medium) {
    return GetEntries(file, medium, kTransportEntries);
}

bool TRestDetectorTransportTable::HasTable(TFile* file, const std::string& medium, const std::string& key) {
    return HasKey(GetTables(file, medium), key);
}

TRestDetectorTransportTable TRestDetectorTransportTable::Read(TFile* file, const std::string& medium,
                                                              const std::string& key) {
    const std::string error = "TRestDetectorTransportTable: table '" + key + "' of '" + medium + "': ";
    const YAML::Node entry = FindEntry(GetTables(file, medium), key);
    if (!entry.IsMap()) throw std::runtime_error(error + "not in the metadata of the medium");

    TRestDetectorTransportTable table;
    table.fMedium = medium;
    table.fPressure = entry["pressure"].as<double>();
    table.fTemperature = entry["temperature"].as<double>();
    table.fPressureScaling = TRestYAMLUtils::GetParam<bool>(entry, "pressureScaling", true);
    table.fPenning = TRestYAMLUtils::GetParam<double>(entry, "penningRate", 0);

    auto* directory = file->GetDirectory((kTablesDir + "/" + key).c_str());
    if (!directory) throw std::runtime_error(error + "no directory " + kTablesDir + "/" + key);
    if (!entry["carriers"]) throw std::runtime_error(error + "no carriers in its entry");
    for (const auto& carrier : entry["carriers"].as<std::vector<std::string>>()) {
        auto& values = table.fCarriers[carrier];
        for (const auto& quantity : kQuantities) {
            ReadGraph(directory, medium + "_" + key + "_" + carrier + "_" + quantity.name, table.fFields,
                      values.*quantity.values, error);
        }
    }
    CheckFields(table.fFields, error);
    return table;
}

void TRestDetectorTransportTable::Write(TFile* file, const YAML::Node& medium,
                                        const YAML::Node& generator) const {
    const std::string key = GetKey();
    YAML::Node entry;
    entry["key"] = key;
    entry["pressure"] = fPressure;
    entry["temperature"] = fTemperature;
    entry["fieldRange"] = std::vector<double>{fFields.front(), fFields.back()};
    entry["pressureScaling"] = fPressureScaling;
    entry["penningRate"] = fPenning;
    std::vector<std::string> carriers;
    for (const auto& [carrier, values] : fCarriers) carriers.push_back(carrier);
    entry["carriers"] = carriers;
    entry["generator"] = YAML::Clone(generator);
    AddEntry(file, fMedium, medium, kTransportEntries, entry, kTablesDir, "TRestDetectorTransportTable");

    std::string composition;
    if (medium["components"]) {
        for (const auto& [component, fraction] : medium["components"].as<std::map<std::string, double>>()) {
            composition += (composition.empty() ? " (" : ", ") + component + " " + Format(fraction) + "%";
        }
        if (!composition.empty()) composition += ")";
    }
    const std::string condition = fMedium + composition + ", " + Format(fPressure) + " bar, " +
                                  Format(fTemperature) + " K" +
                                  (fPenning > 0 ? ", Penning " + Format(fPenning) : "");
    for (const auto& [carrier, carrierValues] : fCarriers) {
        for (const auto& quantity : kQuantities) {
            const auto& values = carrierValues.*quantity.values;
            if (values.size() != fFields.size()) continue;
            WriteGraph(fMedium + "_" + key + "_" + carrier + "_" + quantity.name,
                       condition + ";E (V/cm);" + carrier + " " + quantity.title, fFields, values);
        }
    }
}

// ============================================================
// TRestDetectorGainTable
// ============================================================

TRestDetectorGainTable::TRestDetectorGainTable() : TRestMetadata() { fName = "TRestDetectorGainTable"; }

TRestDetectorGainTable::TRestDetectorGainTable(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

void TRestDetectorGainTable::LoadConfig() {
    UpdateParamsFromYAML<TRestDetectorGainTable>(fNode);
    const std::string prefix = "TRestDetectorGainTable '" + fName + "': ";
    if (fGap <= 0) throw std::runtime_error(prefix + "gap must be > 0");
    if (fFieldNodes <= 0 || fReducedFieldRange.first <= 0 ||
        fReducedFieldRange.first > fReducedFieldRange.second ||
        (fFieldNodes > 1 && fReducedFieldRange.first == fReducedFieldRange.second)) {
        throw std::runtime_error(prefix + "reducedFieldRange must be 0 < min < max and fieldNodes > 0");
    }
    if (fAvalanches < 2) {
        throw std::runtime_error(prefix + "avalanches must be >= 2 (theta from the moments)");
    }
    for (double p : fPressures) {
        if (p <= 0) throw std::runtime_error(prefix + "pressures must be > 0");
    }
    for (double t : fTemperatures) {
        if (t <= 0) throw std::runtime_error(prefix + "temperatures must be > 0");
    }
    if (fUsePenning && (fPenningRate <= 0 || fPenningRate > 1)) {
        throw std::runtime_error(prefix + "usePenning needs a penningRate in (0, 1]");
    }
    UpdateYAMLFromParams<TRestDetectorGainTable>(fNode);
}

std::vector<double> TRestDetectorGainTable::GetPressures(const TRestDetectorMedium& medium) const {
    if (fPressures.empty()) return {medium.GetPressure()};
    return std::vector<double>(fPressures.begin(), fPressures.end());
}

std::vector<double> TRestDetectorGainTable::GetTemperatures(const TRestDetectorMedium& medium) const {
    if (fTemperatures.empty()) return {medium.GetTemperature()};
    return fTemperatures;
}

std::vector<double> TRestDetectorGainTable::GetFieldGrid(double pressure) const {
    auto fields = TRestDetectorTransportTable::GetFieldGrid(fReducedFieldRange.first,
                                                            fReducedFieldRange.second, fFieldNodes, false);
    for (auto& field : fields) field *= pressure;  // E/p -> E
    return fields;
}

std::string TRestDetectorGainTable::GetKey(double pressure, double temperature, double gap, double minField,
                                           double maxField, double penning) {
    return Format(pressure) + "bar_" + Format(temperature) + "K_gap" + Format(gap * 1000) + "um_E" +
           Format(minField * kVcmPerVmm) + "-" + Format(maxField * kVcmPerVmm) + "Vcm" +
           (penning > 0 ? "_Penning" + Format(penning) : "");
}

std::string TRestDetectorGainTable::GetKey() const {
    if (fFields.empty()) throw std::runtime_error("TRestDetectorGainTable: empty table");
    return GetKey(fPressure, fTemperature, fTableGap, fFields.front(), fFields.back(), fPenning);
}

std::pair<double, double> TRestDetectorGainTable::GetFieldRange() const {
    if (fFields.empty()) return {0, 0};
    return {fFields.front(), fFields.back()};
}

bool TRestDetectorGainTable::InRange(double field) const {
    if (fFields.empty()) return false;
    const double tolerance = 1e-9 * fFields.back();
    return field >= fFields.front() - tolerance && field <= fFields.back() + tolerance;
}

namespace {
/// Linear interpolation in the field (values of a table in range)
double InterpolateLinear(const std::vector<double>& fields, const std::vector<double>& values, double x) {
    if (fields.size() == 1 || x <= fields.front()) return values.front();
    if (x >= fields.back()) return values.back();
    const size_t i = std::upper_bound(fields.begin(), fields.end(), x) - fields.begin() - 1;
    const double t = (x - fields[i]) / (fields[i + 1] - fields[i]);
    return values[i] + t * (values[i + 1] - values[i]);
}
}  // namespace

double TRestDetectorGainTable::GetGain(double field) const {
    if (!InRange(field)) {
        throw std::runtime_error("TRestDetectorGainTable: amplification field " + Format(field * kVcmPerVmm) +
                                 " V/cm out of the range of table " + GetKey());
    }
    std::vector<double> logGain;
    for (const double gain : fGain) logGain.push_back(std::log(std::max(gain, 1e-300)));
    return std::exp(InterpolateLinear(fFields, logGain, field));
}

double TRestDetectorGainTable::GetTheta(double field) const {
    if (!InRange(field)) {
        throw std::runtime_error("TRestDetectorGainTable: amplification field " + Format(field * kVcmPerVmm) +
                                 " V/cm out of the range of table " + GetKey());
    }
    return InterpolateLinear(fFields, fTheta, field);
}

YAML::Node TRestDetectorGainTable::GetTables(TFile* file, const std::string& medium) {
    return GetEntries(file, medium, kGainEntries);
}

bool TRestDetectorGainTable::HasTable(TFile* file, const std::string& medium, const std::string& key) {
    return HasKey(GetTables(file, medium), key);
}

TRestDetectorGainTable TRestDetectorGainTable::Read(TFile* file, const std::string& medium,
                                                    const std::string& key) {
    const std::string error = "TRestDetectorGainTable: table '" + key + "' of '" + medium + "': ";
    const YAML::Node entry = FindEntry(GetTables(file, medium), key);
    if (!entry.IsMap()) throw std::runtime_error(error + "not in the metadata of the medium");

    TRestDetectorGainTable table;
    table.fMedium = medium;
    table.fPressure = entry["pressure"].as<double>();
    table.fTemperature = entry["temperature"].as<double>();
    table.fTableGap = entry["gap"].as<double>();
    table.fPenning = TRestYAMLUtils::GetParam<double>(entry, "penningRate", 0);

    auto* directory = file->GetDirectory((kTablesDir + "/" + key).c_str());
    if (!directory) throw std::runtime_error(error + "no directory " + kTablesDir + "/" + key);
    const std::pair<const char*, std::vector<double>*> quantities[] = {{"gain", &table.fGain},
                                                                       {"theta", &table.fTheta}};
    for (const auto& [quantity, values] : quantities) {
        if (!ReadGraph(directory, medium + "_" + key + "_" + quantity, table.fFields, *values, error)) {
            throw std::runtime_error(error + "no " + quantity + " graph");
        }
    }
    CheckFields(table.fFields, error);
    return table;
}

void TRestDetectorGainTable::Write(TFile* file, const YAML::Node& medium, const YAML::Node& generator,
                                   const std::vector<std::vector<double>>& sizes) const {
    if (fGain.size() != fFields.size() || fTheta.size() != fFields.size()) {
        throw std::runtime_error("TRestDetectorGainTable: gain and theta must have a value per field");
    }
    const std::string key = GetKey();
    YAML::Node entry;
    entry["key"] = key;
    entry["pressure"] = fPressure;
    entry["temperature"] = fTemperature;
    entry["gap"] = fTableGap;
    entry["fieldRange"] = std::vector<double>{fFields.front(), fFields.back()};
    entry["penningRate"] = fPenning;
    entry["avalanches"] = sizes.empty() ? 0 : static_cast<int>(sizes.front().size());
    entry["generator"] = YAML::Clone(generator);
    AddEntry(file, fMedium, medium, kGainEntries, entry, kTablesDir, "TRestDetectorGainTable");

    const std::string condition = fMedium + ", " + Format(fPressure) + " bar, " + Format(fTemperature) +
                                  " K, gap " + Format(fTableGap * 1000) + " #mum" +
                                  (fPenning > 0 ? ", Penning " + Format(fPenning) : "");
    const std::tuple<const char*, const char*, const std::vector<double>*> quantities[] = {
        {"gain", "mean gain", &fGain}, {"theta", "Polya #theta", &fTheta}};
    for (const auto& [quantity, title, values] : quantities) {
        WriteGraph(fMedium + "_" + key + "_" + quantity, condition + ";E (V/cm);" + title, fFields, *values);
    }
    for (size_t i = 0; i < sizes.size() && i < fFields.size(); i++) {
        if (sizes[i].empty()) continue;
        const double maxSize = *std::max_element(sizes[i].begin(), sizes[i].end());
        const std::string name = fMedium + "_" + key + "_size_" + std::to_string(i);
        TH1D histogram(
            name.c_str(),
            (condition + ", E " + Format(fFields[i] * kVcmPerVmm) + " V/cm;avalanche size;avalanches")
                .c_str(),
            100, 0, 1.05 * maxSize + 1);
        histogram.SetDirectory(nullptr);
        for (const double size : sizes[i]) histogram.Fill(size);
        histogram.Write(name.c_str(), TObject::kOverwrite);
    }
}

// ============================================================
// TRestDetectorTransportManager
// ============================================================

TRestDetectorTransportManager::TRestDetectorTransportManager(const std::string& instanceName,
                                                             const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestDetectorTransportManager::TRestDetectorTransportManager(const std::string& fileName,
                                                             const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

TRestDetectorTransportManager::~TRestDetectorTransportManager() = default;

void TRestDetectorTransportManager::LoadConfig() {
    UpdateParamsFromYAML<TRestDetectorTransportManager>(fNode);
    if (!fMediumNode || !fMediumNode.IsMap()) {
        throw std::runtime_error("TRestDetectorTransportManager: no medium (key 'medium')");
    }
    fMedium = std::make_unique<TRestDetectorMedium>("medium", fMediumNode);
    fMedium->CheckConfig();
    fTransport = CreateTables<TRestDetectorTransportTable>(fTransportNode, "transport");
    fGain = CreateTables<TRestDetectorGainTable>(fGainNode, "gain");
    UpdateYAMLFromParams<TRestDetectorTransportManager>(fNode);
}

void TRestDetectorTransportManager::PrintMetadata() const {
    RESTMetadata << "=== TRestDetectorTransportManager ===" << RESTendl;
    if (fMedium) fMedium->PrintMetadata();
    for (const auto& table : fTransport) table->PrintMetadata();
    for (const auto& table : fGain) table->PrintMetadata();
}
