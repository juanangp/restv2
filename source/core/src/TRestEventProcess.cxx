#include "TRestEventProcess.h"

#include "TRestRun.h"

static const bool TRestEventProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestEventProcess>("observables", &TRestEventProcess::fObservables);
    return true;
}();

/// \brief Constructs a generic readout metadata object with default name.
TRestEventProcess::TRestEventProcess() : TRestMetadata() { fName = "TRestEventProcess"; }

TRestEventProcess::TRestEventProcess(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestEventProcess::TRestEventProcess(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

/// \brief Initializes geometry and decoding from YAML.
void TRestEventProcess::LoadConfig() {
    UpdateParamsFromYAML<TRestEventProcess>(fNode);
    ReadYAMLVerbose(fNode);
    // Sync resolved parameters to the node
    UpdateYAMLFromParams<TRestEventProcess>(fNode);
}

Long64_t TRestEventProcess::GetInputEventCount() const { return fRunInfo ? fRunInfo->GetEntries() : -1; }

bool TRestEventProcess::ShouldRegisterObservable(const std::string& name) const {
    if (!fRunInfo) return false;
    for (const auto& obs : fObservables) {
        if (obs == "none") return false;
        if (obs == "all" || obs == name) return true;
    }
    return false;
}
