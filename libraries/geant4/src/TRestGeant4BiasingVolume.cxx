#include "TRestGeant4BiasingVolume.h"

// Modern REST v3 field registry reflection hook
static const bool TRestGeant4BiasingVolume_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestGeant4BiasingVolume>("position", &TRestGeant4BiasingVolume::fVolumePosition);
    reg.RegisterField<TRestGeant4BiasingVolume>("size", &TRestGeant4BiasingVolume::fVolumeSize);
    reg.RegisterField<TRestGeant4BiasingVolume>("biasingFactor", &TRestGeant4BiasingVolume::fBiasingFactor);
    reg.RegisterField<TRestGeant4BiasingVolume>("energyRange", &TRestGeant4BiasingVolume::fEnergyRange);
    reg.RegisterField<TRestGeant4BiasingVolume>("type", &TRestGeant4BiasingVolume::fVolumeType);
    return true;
}();

TRestGeant4BiasingVolume::TRestGeant4BiasingVolume() : TRestMetadata() { fName = "TRestGeant4BiasingVolume"; }

TRestGeant4BiasingVolume::TRestGeant4BiasingVolume(const std::string& configFilename, const std::string& name)
    : TRestMetadata(configFilename, name) {
    LoadConfig();
}

TRestGeant4BiasingVolume::TRestGeant4BiasingVolume(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestGeant4BiasingVolume::~TRestGeant4BiasingVolume() = default;

void TRestGeant4BiasingVolume::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4BiasingVolume>(fNode);
    UpdateYAMLFromParams<TRestGeant4BiasingVolume>(fNode);
}
