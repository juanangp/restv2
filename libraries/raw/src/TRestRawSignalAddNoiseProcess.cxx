#include "TRestRawSignalAddNoiseProcess.h"

#include <TRandom.h>

#include <vector>

#include "TRestPulseShapeAnalysis.h"
#include "TRestRawSignalEvent.h"

using namespace TRestPulseShapeAnalysis;

//
// Example
//
//    addNoise:
//      class: "TRestRawSignalAddNoiseProcess"
//      noise:
//        - method: "white"
//          amplitude: 15
//        - method: "sin"
//          amplitude: 100
//          period: 55

static const bool TRestNoiseParameters_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestNoiseParameters>("method", &TRestNoiseParameters::fNoiseMethod);
    reg.RegisterField<TRestNoiseParameters>("amplitude", &TRestNoiseParameters::fNoiseAmplitude);
    reg.RegisterField<TRestNoiseParameters>("period", &TRestNoiseParameters::fNoisePeriod);
    return true;
}();

TRestNoiseParameters::TRestNoiseParameters() : TRestMetadata() { fName = "TRestNoiseParameters"; }

TRestNoiseParameters::TRestNoiseParameters(const std::string& name, const YAML::Node& node)
    : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestNoiseParameters::LoadConfig() {
    UpdateParamsFromYAML<TRestNoiseParameters>(fNode);
    UpdateYAMLFromParams<TRestNoiseParameters>(fNode);
}

// --- TRestRawSignalAddNoiseProcess ---

static const bool TRestRawSignalAddNoiseProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestRawSignalAddNoiseProcess>("noise", &TRestRawSignalAddNoiseProcess::fNoiseParams);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestRawSignalAddNoiseProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestRawSignalAddNoiseProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestRawSignalAddNoiseProcess::TRestRawSignalAddNoiseProcess() : TRestEventProcess() {
    fName = "TRestRawSignalAddNoiseProcess";
}

TRestRawSignalAddNoiseProcess::TRestRawSignalAddNoiseProcess(const std::string& instanceName,
                                                             const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestRawSignalAddNoiseProcess::TRestRawSignalAddNoiseProcess(const std::string& fileName,
                                                             const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestRawSignalAddNoiseProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestRawSignalAddNoiseProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestRawSignalAddNoiseProcess>(fNode);
    UpdateYAMLFromParams<TRestRawSignalAddNoiseProcess>(fNode);
}

bool TRestRawSignalAddNoiseProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    output.CopyFrom(&input);

    const auto* inEvent = dynamic_cast<const TRestRawSignalEvent*>(&input);
    auto* signalEvent = dynamic_cast<TRestRawSignalEvent*>(&output);
    if (!inEvent || !signalEvent) {
        throw std::runtime_error("TRestRawSignalAddNoiseProcess: input/output is not a TRestRawSignalEvent");
    }

    std::vector<short> noiseData;
    for (const auto& noisePar : fNoiseParams) {
        const std::string method = noisePar.fNoiseMethod;
        const double noiseAmp = noisePar.fNoiseAmplitude;
        if (method == "white") {
            for (int s = 0; s < signalEvent->GetNumberOfSignals(); s++) {
                auto& sig = signalEvent->GetSignal(s);
                noiseData.resize(sig.GetNPoints());
                GetWhiteNoise(noiseData, noiseAmp);
                for (int b = 0; b < sig.GetNPoints(); b++) sig.IncreaseBinBy(b, noiseData[b]);
            }
        } else if (method == "sin" || method == "sinusoidal") {
            const double phase = gRandom->Uniform(0.0, 2.0 * TMath::Pi());  // Same phase for the event
            const double period = noisePar.fNoisePeriod;
            for (int s = 0; s < signalEvent->GetNumberOfSignals(); s++) {
                auto& sig = signalEvent->GetSignal(s);
                noiseData.resize(sig.GetNPoints());

                GetSinNoise(noiseData, noiseAmp, period, phase);
                for (int b = 0; b < sig.GetNPoints(); b++) sig.IncreaseBinBy(b, noiseData[b]);
            }
        } else {
            RESTWarning << "TRestRawSignalAddNoiseProcess: method " << method
                        << "not implemented. Implemented methods: 'white' and 'sin'" << RESTendl;
        }
    }

    return true;
}
