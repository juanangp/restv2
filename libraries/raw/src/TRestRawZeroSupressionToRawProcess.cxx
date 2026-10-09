#include "TRestRawZeroSupressionToRawProcess.h"

#include "TRestRawSignalEvent.h"

// Registration in MetadataClassRegistry
namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestRawZeroSupressionToRawProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestRawZeroSupressionToRawProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace


/// \brief Default constructor for FEMDAQ-to-signal conversion.
TRestRawZeroSupressionToRawProcess::TRestRawZeroSupressionToRawProcess() : TRestEventProcess() {
    fName = "TRestRawZeroSupressionToRawProcess";
}

/// \brief Constructor from an in-memory YAML node.
TRestRawZeroSupressionToRawProcess::TRestRawZeroSupressionToRawProcess(const std::string& instanceName,
                                                             const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

/// \brief Constructor from a file and YAML section name.
TRestRawZeroSupressionToRawProcess::TRestRawZeroSupressionToRawProcess(const std::string& fileName,
                                                             const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

/// \brief Loads process parameters and synchronizes resolved values back to YAML.
void TRestRawZeroSupressionToRawProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestRawZeroSupressionToRawProcess::LoadConfig YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestRawZeroSupressionToRawProcess>(fNode);
    // Sync resolved parameters to the node
    UpdateYAMLFromParams<TRestRawZeroSupressionToRawProcess>(fNode);
}

/// \brief Opens input trees, imports run timing metadata, and prepares branch bindings.
void TRestRawZeroSupressionToRawProcess::InitProcess() { }

/// \brief Converts one tree entry into a TRestRawSignalEvent and fills per-channel samples.
bool TRestRawZeroSupressionToRawProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    output.CopyFrom(&input);

    auto* rawEvent = dynamic_cast<TRestRawSignalEvent*>(&output);
    if (!rawEvent) {
        throw std::runtime_error("TRestRawZeroSupressionToRawProcess: output is not a TRestRawSignalEvent");
    }

    TRestRawZeroSupressionToRawProcess::ZeroSuppressionToRaw(rawEvent);


    return true;
}

void TRestRawZeroSupressionToRawProcess::ZeroSuppressionToRaw(TRestRawSignalEvent* rawEvent) {
    if (!rawEvent) return;

    int nSignals = rawEvent->GetNumberOfSignals();
    for (int i = 0; i < nSignals; ++i) {
        auto& signal = rawEvent->GetSignal(i);
        int nPoints = signal.GetNPoints();
        
        short offset = 0;
        for (int j = 0; j < nPoints; ++j) {
            short val = signal.GetPoint(j);
            
            if (val == 0) continue;
            if (offset == 0) offset = val;
            
            signal.IncreaseBinBy(j, -offset);
        }
    }
}

void TRestRawZeroSupressionToRawProcess::EndProcess() { }

