#include "TRestDetectorTransport.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <stdexcept>

#include "TDirectory.h"
#include "TFile.h"
#include "TGraph.h"
#include "TObjString.h"
#include "TRestDetectorMedium.h"

static const bool TRestDetectorTransport_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using T = TRestDetectorTransport;
    reg.RegisterField<T>("pressures", &T::fPressures);
    reg.RegisterField<T>("temperatures", &T::fTemperatures);
    reg.RegisterField<T>("fieldRange", &T::fFieldRange);
    reg.RegisterField<T>("fieldNodes", &T::fFieldNodes);
    reg.RegisterField<T>("logField", &T::fLogField);
    reg.RegisterField<T>("nCollisions", &T::fCollisions);
    reg.RegisterField<T>("maxElectronEnergy", &T::fMaxElectronEnergy);
    reg.RegisterField<T>("usePenning", &T::fUsePenning);
    reg.RegisterField<T>("penningRate", &T::fPenningRate);
    reg.RegisterField<T>("outputPath", &T::fOutputPath);
    reg.RegisterField<T>("overwrite", &T::fOverwrite);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorTransport", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorTransport>(instanceName, params);
        });
    return true;
}();

constexpr double kVcmPerVmm = 10.;

/// Number without trailing zeros (99, 0.5, 293.15)
std::string Format(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%g", value);
    return buffer;
}

/// Quantities stored as graphs: name and axis title
using Carrier = TRestDetectorTransport::Carrier;
struct Quantity {
    const char* name;
    const char* title;
    std::vector<double> Carrier::*values;
};
const Quantity kQuantities[] = {
    {"driftVelocity", "drift velocity (mm/#mus)", &Carrier::fDriftVelocity},
    {"longitudinalDiffusion", "longitudinal diffusion (#sqrt{cm})", &Carrier::fLongitudinalDiffusion},
    {"transversalDiffusion", "transversal diffusion (#sqrt{cm})", &Carrier::fTransversalDiffusion},
    {"townsend", "Townsend coefficient (1/cm)", &Carrier::fTownsend},
    {"attachment", "attachment coefficient (1/cm)", &Carrier::fAttachment},
};

/// Graph of a quantity of a carrier. Tables of the first format (no carriers) have no carrier in the name
std::string GraphName(const std::string& medium, const std::string& key, const std::string& carrier,
                      const std::string& quantity) {
    return medium + "_" + key + "_" + (carrier.empty() ? "" : carrier + "_") + quantity;
}
}  // namespace

TRestDetectorTransport::TRestDetectorTransport() : TRestMetadata() { fName = "TRestDetectorTransport"; }

TRestDetectorTransport::TRestDetectorTransport(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestDetectorTransport::TRestDetectorTransport(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorTransport::LoadConfig() {
    UpdateParamsFromYAML<TRestDetectorTransport>(fNode);
    const std::string prefix = "TRestDetectorTransport '" + fName + "': ";
    if (fFieldNodes <= 0 || fFieldRange.first <= 0 || fFieldRange.first >= fFieldRange.second) {
        throw std::runtime_error(prefix + "fieldRange must be 0 < min < max and fieldNodes > 0");
    }
    if (fCollisions <= 0 || fMaxElectronEnergy <= 0) {
        throw std::runtime_error(prefix + "nCollisions and maxElectronEnergy must be > 0");
    }
    if (fPressures.empty() || fTemperatures.empty()) {
        throw std::runtime_error(prefix + "pressures and temperatures cannot be empty");
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
    UpdateYAMLFromParams<TRestDetectorTransport>(fNode);
}

std::filesystem::path TRestDetectorTransport::GetMediumFile(const TRestDetectorMedium& medium) const {
    std::string path = fOutputPath;
    const char* home = std::getenv("HOME");
    if (home && !path.empty() && path[0] == '~') path = home + path.substr(1);
    std::filesystem::create_directories(path);
    return (std::filesystem::path(path) / (medium.GetName() + ".root")).lexically_normal();
}

/// As Garfield::Medium::SetFieldGrid
std::vector<double> TRestDetectorTransport::GetFieldGrid() const {
    const double minField = fFieldRange.first, maxField = fFieldRange.second;
    std::vector<double> fields;
    for (int i = 0; i < fFieldNodes; i++) {
        const double t = fFieldNodes > 1 ? i / (fFieldNodes - 1.) : 0;
        fields.push_back(fLogField ? minField * std::pow(maxField / minField, t)
                                   : minField + t * (maxField - minField));
    }
    return fields;
}

std::string TRestDetectorTransport::GetKey(double pressure, double temperature, double minField,
                                           double maxField, double penning) {
    return Format(pressure) + "bar_" + Format(temperature) + "K_E" + Format(minField * kVcmPerVmm) + "-" +
           Format(maxField * kVcmPerVmm) + "Vcm" + (penning > 0 ? "_Penning" + Format(penning) : "");
}

std::string TRestDetectorTransport::GetKey() const {
    if (fFields.empty()) throw std::runtime_error("TRestDetectorTransport: empty table");
    return GetKey(fPressure, fTemperature, fFields.front(), fFields.back(), fPenning);
}

std::pair<double, double> TRestDetectorTransport::GetFieldRange(double pressure) const {
    if (fFields.empty()) return {0, 0};
    const double scale = fPressureScaling ? pressure / fPressure : 1;
    return {fFields.front() * scale, fFields.back() * scale};
}

double TRestDetectorTransport::Interpolate(const std::vector<double>& values, double field,
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

const TRestDetectorTransport::Carrier& TRestDetectorTransport::GetCarrier(const std::string& carrier) const {
    const auto it = fCarriers.find(carrier);
    if (it == fCarriers.end()) {
        throw std::runtime_error("TRestDetectorTransport: the table of '" + fMedium + "' has no " + carrier +
                                 " transport");
    }
    return it->second;
}

double TRestDetectorTransport::GetDriftVelocity(double field, double pressure,
                                                const std::string& carrier) const {
    return Interpolate(GetCarrier(carrier).fDriftVelocity, field, pressure, false);
}

double TRestDetectorTransport::GetLongitudinalDiffusion(double field, double pressure,
                                                        const std::string& carrier) const {
    const double scale = fPressureScaling ? std::sqrt(fPressure / pressure) : 1;
    return std::max(0., Interpolate(GetCarrier(carrier).fLongitudinalDiffusion, field, pressure, false)) *
           scale;
}

double TRestDetectorTransport::GetTransversalDiffusion(double field, double pressure,
                                                       const std::string& carrier) const {
    const double scale = fPressureScaling ? std::sqrt(fPressure / pressure) : 1;
    return std::max(0., Interpolate(GetCarrier(carrier).fTransversalDiffusion, field, pressure, false)) *
           scale;
}

double TRestDetectorTransport::GetTownsend(double field, double pressure, const std::string& carrier) const {
    const double scale = fPressureScaling ? pressure / fPressure : 1;
    return std::max(0., Interpolate(GetCarrier(carrier).fTownsend, field, pressure, true)) * scale;
}

double TRestDetectorTransport::GetAttachment(double field, double pressure,
                                             const std::string& carrier) const {
    const double scale = fPressureScaling ? pressure / fPressure : 1;
    return std::max(0., Interpolate(GetCarrier(carrier).fAttachment, field, pressure, true)) * scale;
}

YAML::Node TRestDetectorTransport::GetTables(TFile* file, const std::string& medium) {
    const YAML::Node metadata = TRestMetadata::ReadMetadata(file, medium);
    if (!metadata || !metadata["tables"] || !metadata["tables"].IsSequence()) {
        return YAML::Node(YAML::NodeType::Sequence);
    }
    return metadata["tables"];
}

bool TRestDetectorTransport::HasTable(TFile* file, const std::string& medium, const std::string& key) {
    const YAML::Node tables = GetTables(file, medium);
    for (size_t i = 0; i < tables.size(); i++) {
        if (tables[i]["key"] && tables[i]["key"].as<std::string>() == key) return true;
    }
    return false;
}

TRestDetectorTransport TRestDetectorTransport::Read(TFile* file, const std::string& medium,
                                                              const std::string& key) {
    const std::string error = "TRestDetectorTransport: table '" + key + "' of '" + medium + "': ";
    // Index loop: range-for gives YAML::detail::iterator_value, which cannot be assigned to a node
    YAML::Node entry;
    const YAML::Node tables = GetTables(file, medium);
    for (size_t i = 0; i < tables.size(); i++) {
        if (tables[i]["key"] && tables[i]["key"].as<std::string>() == key) entry = tables[i];
    }
    if (!entry) throw std::runtime_error(error + "not in the metadata of the medium");

    TRestDetectorTransport table;
    table.fMedium = medium;
    table.fPressure = entry["pressure"].as<double>();
    table.fTemperature = entry["temperature"].as<double>();
    table.fPressureScaling = entry["pressureScaling"] ? entry["pressureScaling"].as<bool>() : true;
    table.fPenning = entry["penningRate"] ? entry["penningRate"].as<double>() : 0;

    auto* directory = file->GetDirectory((kTablesDir + "/" + key).c_str());
    if (!directory) throw std::runtime_error(error + "no directory " + kTablesDir + "/" + key);
    // First format: no carriers, electron graphs without the carrier in the name
    const bool firstFormat = !entry["carriers"];
    const auto carriers =
        firstFormat ? std::vector<std::string>{"electron"} : entry["carriers"].as<std::vector<std::string>>();
    for (const auto& carrier : carriers) {
        auto& values = table.fCarriers[carrier];
        for (const auto& quantity : kQuantities) {
            const std::string name = GraphName(medium, key, firstFormat ? "" : carrier, quantity.name);
            auto* graph = dynamic_cast<TGraph*>(directory->Get(name.c_str()));
            if (!graph) continue;
            std::vector<double> fields(graph->GetX(), graph->GetX() + graph->GetN());
            for (auto& field : fields) field /= kVcmPerVmm;
            if (table.fFields.empty()) {
                table.fFields = fields;
            } else if (fields != table.fFields) {
                throw std::runtime_error(error + "the graphs have different fields");
            }
            values.*quantity.values = std::vector<double>(graph->GetY(), graph->GetY() + graph->GetN());
            delete graph;
        }
    }
    if (table.fFields.empty()) throw std::runtime_error(error + "no graphs");
    if (!std::is_sorted(table.fFields.begin(), table.fFields.end()) || table.fFields.front() <= 0) {
        throw std::runtime_error(error + "the fields must be positive and increasing");
    }
    return table;
}

void TRestDetectorTransport::Write(TFile* file, const YAML::Node& medium, const YAML::Node& generator,
                                        const std::string& garfieldTable) const {
    if (!file || file->IsZombie() || !file->IsWritable()) {
        throw std::runtime_error("TRestDetectorTransport: the medium file is not writable");
    }
    const std::string key = GetKey();

    // Metadata of the medium: stored with the first table, then it must agree
    YAML::Node metadata = TRestMetadata::ReadMetadata(file, fMedium);
    if (!metadata) {
        metadata = YAML::Clone(medium);
    } else if (medium["components"] && metadata["components"] &&
               medium["components"].as<std::map<std::string, double>>() !=
                   metadata["components"].as<std::map<std::string, double>>()) {
        throw std::runtime_error("TRestDetectorTransport: " + std::string(file->GetName()) +
                                 " belongs to another medium (different components)");
    }
    metadata["name"] = fMedium;

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
    YAML::Node tables(YAML::NodeType::Sequence);
    const YAML::Node stored = GetTables(file, fMedium);
    for (size_t i = 0; i < stored.size(); i++) {
        if (!stored[i]["key"] || stored[i]["key"].as<std::string>() != key) tables.push_back(stored[i]);
    }
    tables.push_back(entry);
    metadata["tables"] = tables;

    // Graphs
    TDirectory* tablesDir = file->GetDirectory(kTablesDir.c_str());
    if (!tablesDir) tablesDir = file->mkdir(kTablesDir.c_str());
    TDirectory* directory = tablesDir ? tablesDir->GetDirectory(key.c_str()) : nullptr;
    if (tablesDir && !directory) directory = tablesDir->mkdir(key.c_str());
    if (!directory) {
        throw std::runtime_error("TRestDetectorTransport: cannot create " + kTablesDir + "/" + key);
    }
    directory->cd();

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
    std::vector<double> fieldsVcm = fFields;
    for (auto& field : fieldsVcm) field *= kVcmPerVmm;
    for (const auto& [carrier, carrierValues] : fCarriers) {
        for (const auto& quantity : kQuantities) {
            const auto& values = carrierValues.*quantity.values;
            if (values.size() != fFields.size()) continue;
            const std::string name = GraphName(fMedium, key, carrier, quantity.name);
            TGraph graph(fFields.size(), fieldsVcm.data(), values.data());
            graph.SetName(name.c_str());
            graph.SetTitle((condition + ";E (V/cm);" + carrier + " " + quantity.title).c_str());
            graph.Write(name.c_str(), TObject::kOverwrite);
        }
    }
    if (!garfieldTable.empty()) TObjString(garfieldTable.c_str()).Write("garfieldTable", TObject::kOverwrite);

    TRestMetadata::WriteMetadata(file, fMedium, metadata);
}
