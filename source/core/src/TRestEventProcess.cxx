#include "TRestEventProcess.h"
#include "TRestTools.h"

static const bool TRestEventProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestEventProcess>("observables", &TRestEventProcess::fObservables);
    return true;
}();

/// \brief Constructs a generic readout metadata object with default name.
TRestEventProcess::TRestEventProcess() : TRestMetadata() {
    fName = "TRestEventProcess";
}

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
    //Sync resolved parameters to the node
    UpdateYAMLFromParams<TRestEventProcess>(fNode);
}

