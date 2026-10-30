#include "TRestEventProcess.h"

#include <stdexcept>

#include "TRestCut.h"
#include "TRestManager.h"
#include "TRestRun.h"

static const bool TRestEventProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestEventProcess>("observables", &TRestEventProcess::fObservables);
    reg.RegisterField<TRestEventProcess>("cuts", &TRestEventProcess::fCutNames);
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

void TRestEventProcess::InitCuts() {
    fCuts.clear();
    if (fCutNames.empty()) return;
    if (!fManager) throw std::runtime_error(GetName() + ": cuts need a TRestManager with the TRestCut");
    if (!fRunInfo || !fRunInfo->fOutputAnalysisTree) {
        throw std::runtime_error(GetName() + ": cuts need the output analysis tree");
    }
    const auto cuts = fManager->GetMetadataOfType<TRestCut>();
    for (const auto& name : fCutNames) {
        TRestCut* cut = nullptr;
        for (auto* c : cuts) {
            if (c->GetName() == name) cut = c;
        }
        if (!cut) throw std::runtime_error(GetName() + ": TRestCut '" + name + "' not found in the manager");
        cut->Compile(fRunInfo->fOutputAnalysisTree);
        fCuts.push_back(cut);
    }
}

bool TRestEventProcess::PassCuts() const {
    for (const auto* cut : fCuts) {
        if (!cut->Evaluate()) return false;
    }
    return true;
}
