#include "TRestRawDAQMetadata.h"

#include <algorithm>
#include <cctype>
#include <cmath>
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
    // Number: digits with an optional decimal part (e.g. "1.5GB"), parsed here (independent of the locale)
    size_t pos = 0;
    size_t nDigits = 0;
    bool point = false;
    long double value = 0;
    long double scale = 1;
    while (pos < text.size()) {
        const unsigned char c = text[pos];
        if (std::isdigit(c)) {
            nDigits++;
            if (point) {
                scale /= 10;
                value += (c - '0') * scale;
            } else {
                value = value * 10 + (c - '0');
            }
        } else if (c == '.' && !point) {
            point = true;
        } else {
            break;
        }
        pos++;
    }
    if (nDigits == 0) throw std::runtime_error("TRestRawDAQMetadata: no number in the size \"" + size + "\"");

    const std::string unit = text.substr(pos);
    long double factor = 0;
    if (unit.empty() || unit == "b") {
        factor = 1;
    } else if (unit == "kb" || unit == "k") {
        factor = 1024.;
    } else if (unit == "mb" || unit == "m") {
        factor = 1024. * 1024.;
    } else if (unit == "gb" || unit == "g") {
        factor = 1024. * 1024. * 1024.;
    }
    // Rounded to bytes
    if (factor > 0) return static_cast<uint64_t>(std::llround(value * factor));
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
