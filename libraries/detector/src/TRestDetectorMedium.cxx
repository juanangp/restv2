#include "TRestDetectorMedium.h"

#include <stdexcept>

static const bool TRestDetectorMedium_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using M = TRestDetectorMedium;
    reg.RegisterField<M>("state", &M::fState);
    reg.RegisterField<M>("components", &M::fComponents);
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

void TRestDetectorMedium::LoadConfig() {
    UpdateParamsFromYAML<TRestDetectorMedium>(fNode);
    const std::string prefix = "TRestDetectorMedium '" + fName + "': ";
    if (!fNode["name"]) throw std::runtime_error("TRestDetectorMedium: the medium needs a name");
    if (kStates.count(fState) == 0) {
        throw std::runtime_error(prefix + "unknown state '" + fState + "' (gas, liquid or solid)");
    }
    if (fComponents.empty()) throw std::runtime_error(prefix + "no components");
    for (const auto& [component, fraction] : fComponents) {
        if (fraction <= 0) throw std::runtime_error(prefix + "the fraction of " + component + " must be > 0");
    }
    UpdateYAMLFromParams<TRestDetectorMedium>(fNode);
}

YAML::Node TRestDetectorMedium::GetDefinition() const {
    YAML::Node definition;
    definition["name"] = fName;
    definition["state"] = fState;
    definition["components"] = fComponents;
    return definition;
}
