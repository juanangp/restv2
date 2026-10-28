/// \class TRestSelectionProcess
/// Selects the events that pass the TRestCut given in `cuts` (names of TRestCut defined in the manager),
/// for any event type: the output event is a copy of the input one. The cuts are applied after the
/// process as for any process (TRestEventProcess::PassCuts), so they can use the observables of the
/// previous processes and of the input file. Any process accepts `cuts`; this one is for selecting
/// without processing, e.g. at the start of the pipeline on the observables of the input file.
///
/// Parameters: event (event type, empty: the output event of the previous process; required if it is the
/// first process) and cuts.
///
/// Example:
///
///    fiducial:
///      class: "TRestCut"
///      conditions:
///        originX: "trackLineAnalysis_OriginX > 20 && trackLineAnalysis_OriginX < 225"
///    pipeline:
///      class: "TRestProcessManager"
///      select:
///        class: "TRestSelectionProcess"
///        event: "TRestTrackEvent"
///        cuts: ["fiducial"]
///
/// Differences with legacy (TRestEventSelectionProcess): the selection is by TRestCut, not by a list of
/// event IDs.

#include "TRestSelectionProcess.h"

#include <stdexcept>

namespace {
const bool kFieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestSelectionProcess>("event", &TRestSelectionProcess::fEvent);
    return true;
}();

const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestSelectionProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestSelectionProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestSelectionProcess::TRestSelectionProcess() : TRestEventProcess() { fName = "TRestSelectionProcess"; }

TRestSelectionProcess::TRestSelectionProcess(const std::string& instanceName, const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestSelectionProcess::TRestSelectionProcess(const std::string& fileName, const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestSelectionProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestSelectionProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }
    UpdateParamsFromYAML<TRestSelectionProcess>(fNode);
    UpdateYAMLFromParams<TRestSelectionProcess>(fNode);
}

void TRestSelectionProcess::InitProcess() {
    if (fCutNames.empty()) {
        RESTWarning << "TRestSelectionProcess '" << GetName() << "': no cuts, all the events are selected"
                    << RESTendl;
    }
}

bool TRestSelectionProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    output.CopyFrom(&input);
    return true;
}
