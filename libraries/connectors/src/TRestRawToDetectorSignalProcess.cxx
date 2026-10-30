/// \class TRestRawToDetectorSignalProcess
/// Transforms a TRestRawSignalEvent into a TRestDetectorSignalEvent. The bin `p` of each raw signal
/// becomes a point at time `triggerStarts + sampling * p` with amplitude `gain * value`.
///
/// Without zero suppression, the points whose raw value is over `threshold` are transferred. With
/// zero suppression, only the points over threshold found as in TRestRawSignalAnalysisProcess
/// (baselineRange, baselineOption, integralRange, pointsThreshold, signalThreshold,
/// pointsOverThreshold, nPointsFlat) are transferred, with the baseline subtracted.
///
/// Signals without points are not added to the output. Events without signals are rejected.
///
/// Observables: NSignalsRejected (number of signals without points).
///
/// Example:
///
///    rawToSignal:
///      class: "TRestRawToDetectorSignalProcess"
///      sampling: 0.2
///      triggerStarts: 20
///      gain: 0.02
///      zeroSuppression: true
///      baselineRange: [20, 140]
///      integralRange: [150, 450]
///      pointsThreshold: 3
///      signalThreshold: 7
///      pointsOverThreshold: 7
///      observables:
///        - "NSignalsRejected"
///
/// Migrated from the legacy connectors library (Javier Gracia, Javier Galan, JuanAn Garcia).

#include "TRestRawToDetectorSignalProcess.h"

#include <stdexcept>
#include <vector>

#include "TRestDetectorSignalEvent.h"
#include "TRestPulseShapeAnalysis.h"
#include "TRestRawSignalEvent.h"
#include "TRestRun.h"

using namespace TRestPulseShapeAnalysis;

static const bool TRestRawToDetectorSignalProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestRawToDetectorSignalProcess;
    reg.RegisterField<P>("sampling", &P::fSampling);
    reg.RegisterField<P>("triggerStarts", &P::fTriggerStarts);
    reg.RegisterField<P>("gain", &P::fGain);
    reg.RegisterField<P>("threshold", &P::fThreshold);
    reg.RegisterField<P>("zeroSuppression", &P::fZeroSuppression);
    reg.RegisterField<P>("baselineRange", &P::fBaselineRange);
    reg.RegisterField<P>("baselineOption", &P::fBaselineOption);
    reg.RegisterField<P>("integralRange", &P::fIntegralRange);
    reg.RegisterField<P>("pointsThreshold", &P::fPointsThreshold);
    reg.RegisterField<P>("signalThreshold", &P::fSignalThreshold);
    reg.RegisterField<P>("pointsOverThreshold", &P::fPointsOverThreshold);
    reg.RegisterField<P>("nPointsFlat", &P::fNPointsFlat);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestRawToDetectorSignalProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestRawToDetectorSignalProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestRawToDetectorSignalProcess::TRestRawToDetectorSignalProcess() : TRestEventProcess() {
    fName = "TRestRawToDetectorSignalProcess";
}

TRestRawToDetectorSignalProcess::TRestRawToDetectorSignalProcess(const std::string& instanceName,
                                                                 const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestRawToDetectorSignalProcess::TRestRawToDetectorSignalProcess(const std::string& fileName,
                                                                 const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestRawToDetectorSignalProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestRawToDetectorSignalProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestRawToDetectorSignalProcess>(fNode);
    UpdateYAMLFromParams<TRestRawToDetectorSignalProcess>(fNode);
}

void TRestRawToDetectorSignalProcess::InitProcess() {
    RegisterObservable("NSignalsRejected", fNSignalsRejected);
}

bool TRestRawToDetectorSignalProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* rawEvent = dynamic_cast<const TRestRawSignalEvent*>(&input);
    auto* signalEvent = dynamic_cast<TRestDetectorSignalEvent*>(&output);
    if (!rawEvent || !signalEvent) {
        throw std::runtime_error(
            "TRestRawToDetectorSignalProcess: input/output is not a TRestRawSignalEvent/"
            "TRestDetectorSignalEvent");
    }

    signalEvent->Initialize();
    signalEvent->SetEventInfo(&input);

    fNSignalsRejected = 0;
    const auto thrParams = std::make_pair(fPointsThreshold, fSignalThreshold);

    for (int s = 0; s < rawEvent->GetNumberOfSignals(); s++) {
        const auto& rawSignal = rawEvent->GetSignal(s);
        const auto data = rawSignal.GetData();

        std::vector<double> times, amplitudes;
        if (fZeroSuppression) {
            double baseline, baselineSigma;
            auto corrected = CalculateBaselineAndSigma(data, fBaselineRange.first, fBaselineRange.second,
                                                       baseline, baselineSigma, fBaselineOption);
            auto range = fIntegralRange;  // clamped to the signal size by GetPointsOverThreshold
            for (const auto& [bin, value] : GetPointsOverThreshold(
                     corrected, range, thrParams, fPointsOverThreshold, fNPointsFlat, baselineSigma)) {
                times.push_back(fTriggerStarts + fSampling * bin);
                amplitudes.push_back(fGain * value);
            }
        } else {
            for (size_t p = 0; p < data.size(); p++) {
                if (data[p] > fThreshold) {
                    times.push_back(fTriggerStarts + fSampling * p);
                    amplitudes.push_back(fGain * data[p]);
                }
            }
        }

        if (times.empty()) {
            fNSignalsRejected++;
            continue;
        }
        signalEvent->AddSignal(rawSignal.GetSignalID(), times, amplitudes);
    }

    return signalEvent->GetNumberOfSignals() > 0;
}
