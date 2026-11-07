#include "TRestRawDAQMetadata.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "TRestLogManager.h"

namespace {
const bool kFieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestRawDAQFEM>("id", &TRestRawDAQFEM::fId);
    reg.RegisterField<TRestRawDAQFEM>("IP", &TRestRawDAQFEM::fIP);
    reg.RegisterField<TRestRawDAQFEM>("FECs", &TRestRawDAQFEM::fFECs);

    reg.RegisterField<TRestRawDAQMetadata>("electronics", &TRestRawDAQMetadata::fElectronics);
    reg.RegisterField<TRestRawDAQMetadata>("fems", &TRestRawDAQMetadata::fFEMs);
    reg.RegisterField<TRestRawDAQMetadata>("TCM", &TRestRawDAQMetadata::fTCM);
    reg.RegisterField<TRestRawDAQMetadata>("nEvents", &TRestRawDAQMetadata::fNEvents);
    reg.RegisterField<TRestRawDAQMetadata>("time", &TRestRawDAQMetadata::fTime);
    reg.RegisterField<TRestRawDAQMetadata>("maxFileSize", &TRestRawDAQMetadata::fMaxFileSize);
    reg.RegisterField<TRestRawDAQMetadata>("updateRate", &TRestRawDAQMetadata::fUpdateRate);
    reg.RegisterField<TRestRawDAQMetadata>("prometheusAddress", &TRestRawDAQMetadata::fPrometheusAddress);

    MetadataClassRegistry::Instance().Register(
        "TRestRawDAQMetadata", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestRawDAQMetadata>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestRawDAQFEM::TRestRawDAQFEM() : TRestMetadata() { fName = "TRestRawDAQFEM"; }

TRestRawDAQFEM::TRestRawDAQFEM(const std::string& name, const YAML::Node& node) : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestRawDAQFEM::LoadConfig() {
    UpdateParamsFromYAML<TRestRawDAQFEM>(fNode);
    UpdateYAMLFromParams<TRestRawDAQFEM>(fNode);
}

TRestRawDAQMetadata::TRestRawDAQMetadata() : TRestMetadata() { fName = "TRestRawDAQMetadata"; }

TRestRawDAQMetadata::TRestRawDAQMetadata(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestRawDAQMetadata::TRestRawDAQMetadata(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

void TRestRawDAQMetadata::LoadConfig() {
    UpdateParamsFromYAML<TRestRawDAQMetadata>(fNode);
    if (fUpdateRate < 1e6) fUpdateRate = 1e6;  // at least 1 s
    GetMaxFileSizeBytes();                     // throws if not valid
    UpdateYAMLFromParams<TRestRawDAQMetadata>(fNode);
}

uint64_t TRestRawDAQMetadata::ParseSizeToBytes(const std::string& size) {
    std::string text;
    for (const char c : size) {
        if (!std::isspace(static_cast<unsigned char>(c))) text += std::tolower(static_cast<unsigned char>(c));
    }
    size_t pos = 0;
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) pos++;
    if (pos == 0) throw std::runtime_error("TRestRawDAQMetadata: no number in the size \"" + size + "\"");

    const uint64_t value = std::stoull(text.substr(0, pos));
    const std::string unit = text.substr(pos);
    if (unit.empty() || unit == "b") return value;
    if (unit == "kb" || unit == "k") return value * 1024ull;
    if (unit == "mb" || unit == "m") return value * 1024ull * 1024ull;
    if (unit == "gb" || unit == "g") return value * 1024ull * 1024ull * 1024ull;
    throw std::runtime_error("TRestRawDAQMetadata: unknown unit \"" + unit + "\" in the size \"" + size +
                             "\" (B, kB, MB or GB)");
}

void TRestRawDAQMetadata::PrintMetadata() const {
    RESTMetadata << "=== TRestRawDAQMetadata: " << fName << " ===" << RESTendl;
    RESTMetadata << " - Electronics: " << fElectronics << RESTendl;
    for (const auto& fem : fFEMs) {
        std::string fecs;
        for (const int fec : fem.fFECs) fecs += " " + std::to_string(fec);
        RESTMetadata << " - FEM " << fem.fId << ": " << fem.fIP << (fecs.empty() ? "" : ", FECs" + fecs)
                     << RESTendl;
    }
    if (!fTCM.empty()) RESTMetadata << " - TCM: " << fTCM << RESTendl;
    RESTMetadata << " - Events: " << fNEvents << ", time: " << GetMaxTimeSeconds()
                 << " s (0: no limit), max file size: " << fMaxFileSize << RESTendl;
    RESTMetadata << " - Update rate: " << GetUpdateRateSeconds() << " s, Prometheus: " << fPrometheusAddress
                 << RESTendl;
}
