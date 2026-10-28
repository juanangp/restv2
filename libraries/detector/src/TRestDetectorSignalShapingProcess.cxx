/// \class TRestDetectorSignalShapingProcess
/// Shapes each signal of a TRestDetectorSignalEvent with the response h of the electronics, in physical
/// time units and in double precision, before the digitization (so there is no rounding or saturation
/// before the shaping). Each point (t_i, q_i) of the signal, e.g. the charge collected at a time
/// (TRestDetectorHitsToSignalProcess), contributes with its response:
///
///     s(t) = gain / max(h) * sum_i q_i * h((t - t_i) / shapingTime)
///
/// normalized to its maximum, so that an isolated charge q gives a pulse of height gain * q, independent of
/// the sampling of the digitization, as the amplitude of a real shaper. The response types are those of
/// TRestPulseShapeAnalysis::ShapingFunction: gaus (sigma shapingTime, centred), exponential, shaper and
/// shaperSin (AGET response, time constant shapingTime).
///
/// The shaped signal is a waveform evaluated on a fine grid (multiples of shapingTime / 10) from the first
/// point to the end of the response of the last one. The sampling of the electronics is given only in
/// TRestDetectorSignalToRawSignalProcess, in `sample` mode, which interpolates the waveform.
///
/// Difference with TRestRawSignalShapingProcess (for data, in bins): the raw response is normalized so that
/// its samples add up to gain, which depends on the sampling.
///
/// Parameters: shapingType (shaperSin), shapingTime (1 us), gain (1).
///
/// Example:
///
///    signalShaping:
///      class: "TRestDetectorSignalShapingProcess"
///      shapingType: "shaperSin"
///      shapingTime: 200 ns

#include "TRestDetectorSignalShapingProcess.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "TRestDetectorSignalEvent.h"
#include "TRestPulseShapeAnalysis.h"

static const bool TRestDetectorSignalShapingProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorSignalShapingProcess;
    reg.RegisterField<P>("shapingType", &P::fShapingType);
    reg.RegisterField<P>("shapingTime", &P::fShapingTime);
    reg.RegisterField<P>("gain", &P::fGain);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorSignalShapingProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorSignalShapingProcess>(instanceName, params);
        });
    return true;
}();

/// Steps of the tabulated response
constexpr int kSteps = 10000;
/// Points of the shaped waveform per shapingTime
constexpr int kPointsPerShapingTime = 10;
}  // namespace

TRestDetectorSignalShapingProcess::TRestDetectorSignalShapingProcess() : TRestEventProcess() {
    fName = "TRestDetectorSignalShapingProcess";
}

TRestDetectorSignalShapingProcess::TRestDetectorSignalShapingProcess(const std::string& instanceName,
                                                                     const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestDetectorSignalShapingProcess::TRestDetectorSignalShapingProcess(const std::string& fileName,
                                                                     const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorSignalShapingProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestDetectorSignalShapingProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }
    UpdateParamsFromYAML<TRestDetectorSignalShapingProcess>(fNode);
    UpdateYAMLFromParams<TRestDetectorSignalShapingProcess>(fNode);
}

void TRestDetectorSignalShapingProcess::InitProcess() {
    if (fShapingTime <= 0) {
        throw std::runtime_error("TRestDetectorSignalShapingProcess: shapingTime must be > 0");
    }
    // Throws for an unknown type
    TRestPulseShapeAnalysis::ShapingFunction(fShapingType, 0);
    fSupport = TRestPulseShapeAnalysis::ShapingSupport(fShapingType);

    // Tabulated response and its maximum
    const double du = (fSupport.second - fSupport.first) / kSteps;
    fResponse.assign(kSteps + 1, 0);
    double maximum = 0;
    for (int i = 0; i <= kSteps; i++) {
        fResponse[i] = TRestPulseShapeAnalysis::ShapingFunction(fShapingType, fSupport.first + i * du);
        maximum = std::max(maximum, fResponse[i]);
    }
    if (!(maximum > 0)) {
        throw std::runtime_error("TRestDetectorSignalShapingProcess: the response '" + fShapingType +
                                 "' has no positive maximum");
    }
    fNormalization = fGain / maximum;
}

double TRestDetectorSignalShapingProcess::Response(double u) const {
    const double x = (u - fSupport.first) / (fSupport.second - fSupport.first) * kSteps;
    if (x < 0 || x > kSteps) return 0;
    const int i = std::min((int)x, kSteps - 1);
    return fResponse[i] + (x - i) * (fResponse[i + 1] - fResponse[i]);
}

bool TRestDetectorSignalShapingProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* inputEvent = dynamic_cast<const TRestDetectorSignalEvent*>(&input);
    auto* outputEvent = dynamic_cast<TRestDetectorSignalEvent*>(&output);
    if (!inputEvent || !outputEvent) {
        throw std::runtime_error(
            "TRestDetectorSignalShapingProcess: input/output is not a TRestDetectorSignalEvent");
    }

    outputEvent->Initialize();
    outputEvent->SetEventInfo(inputEvent);

    for (int s = 0; s < inputEvent->GetNumberOfSignals(); s++) {
        const auto& signal = inputEvent->GetSignal(s);
        const auto points = signal.GetPointsVector();
        if (points.empty()) continue;

        double minTime = points.front().first, maxTime = minTime;
        for (const auto& [time, charge] : points) {
            minTime = std::min(minTime, time);
            maxTime = std::max(maxTime, time);
        }
        // Grid of multiples of step covering the response of all the points
        const double step = fShapingTime / kPointsPerShapingTime;
        const long first = std::lround(std::floor((minTime + fSupport.first * fShapingTime) / step));
        const long last = std::lround(std::ceil((maxTime + fSupport.second * fShapingTime) / step));

        std::vector<double> times, values;
        times.reserve(last - first + 1);
        values.reserve(last - first + 1);
        for (long k = first; k <= last; k++) {
            const double t = k * step;
            double value = 0;
            for (const auto& [time, charge] : points) {
                const double u = (t - time) / fShapingTime;
                if (u < fSupport.first || u > fSupport.second) continue;
                value += charge * Response(u);
            }
            times.push_back(t);
            values.push_back(fNormalization * value);
        }
        outputEvent->AddSignal(signal.GetSignalID(), times, values);
    }

    return outputEvent->GetNumberOfSignals() > 0;
}
