#include "TRestRawBaseLineCorrectionProcess.h"

#include <stdexcept>

#include "TRestPulseShapeAnalysis.h"
#include "TRestRawSignalEvent.h"

static const bool TRestRawBaseLineCorrectionProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestRawBaseLineCorrectionProcess>(
        "smoothingWindow", &TRestRawBaseLineCorrectionProcess::fSmoothingWindow);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestRawBaseLineCorrectionProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestRawBaseLineCorrectionProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestRawBaseLineCorrectionProcess::TRestRawBaseLineCorrectionProcess() : TRestEventProcess() {
    fName = "TRestRawBaseLineCorrectionProcess";
}

TRestRawBaseLineCorrectionProcess::TRestRawBaseLineCorrectionProcess(const std::string& instanceName,
                                                                     const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestRawBaseLineCorrectionProcess::TRestRawBaseLineCorrectionProcess(const std::string& fileName,
                                                                     const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestRawBaseLineCorrectionProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestRawBaseLineCorrectionProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestRawBaseLineCorrectionProcess>(fNode);

    if (fSmoothingWindow < 1) {
        throw std::runtime_error("TRestRawBaseLineCorrectionProcess: smoothingWindow must be at least 1");
    }

    UpdateYAMLFromParams<TRestRawBaseLineCorrectionProcess>(fNode);
}

bool TRestRawBaseLineCorrectionProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    output.CopyFrom(&input);

    auto* signalEvent = dynamic_cast<TRestRawSignalEvent*>(&output);
    if (!signalEvent) {
        throw std::runtime_error("TRestRawBaseLineCorrectionProcess: output is not a TRestRawSignalEvent");
    }

    for (int s = 0; s < signalEvent->GetNumberOfSignals(); s++) {
        auto& signal = signalEvent->GetSignal(s);
        const auto data = signal.GetData();
        double baseline, baselineSigma;
        const auto smoothed = TRestPulseShapeAnalysis::GetSignalSmoothed_ExcludeOutliers(
            data, fSmoothingWindow, baseline, baselineSigma);
        // Truncated like legacy: data[b] becomes short(data[b] - smoothed[b])
        for (int b = 0; b < (int)data.size(); b++) {
            signal.IncreaseBinBy(b, short(data[b] - smoothed[b]) - data[b]);
        }
    }

    return true;
}
