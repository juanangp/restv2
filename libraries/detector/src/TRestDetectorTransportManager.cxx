#include "TRestDetectorTransportManager.h"

#include <stdexcept>

namespace {
const bool kRegistered = []() {
    // The medium and the transport simulations
    TRestMetadataFieldRegistry::Instance().RegisterChildMetadata<TRestDetectorTransportManager>();
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorTransportManager", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorTransportManager>(instanceName, params);
        });
    return true;
}();
}  // namespace

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
    fMedium.reset();
    fSimulations.clear();

    for (const auto& element : fNode) {
        const auto key = element.first.as<std::string>();
        auto value = element.second;
        if (!value || !value.IsMap() || !value["class"]) continue;

        auto meta = MetadataClassRegistry::Instance().Create(key, value);
        if (auto* medium = dynamic_cast<TRestDetectorMedium*>(meta.get())) {
            if (fMedium) {
                throw std::runtime_error("TRestDetectorTransportManager: only one medium is allowed");
            }
            meta.release();
            fMedium.reset(medium);
        } else {
            fSimulations.push_back(std::move(meta));
        }
    }
    if (!fMedium) throw std::runtime_error("TRestDetectorTransportManager: no TRestDetectorMedium");
}

void TRestDetectorTransportManager::PrintMetadata() const {
    RESTMetadata << "=== TRestDetectorTransportManager ===" << RESTendl;
    if (fMedium) fMedium->PrintMetadata();
    for (const auto& simulation : fSimulations) simulation->PrintMetadata();
}
