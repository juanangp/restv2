/// \class TRestDetectorSignalToRawSignalProcess
/// Transforms a TRestDetectorSignalEvent into a TRestRawSignalEvent: each signal becomes nPoints samples
/// of width sampling, the first one starting at trigger time - triggerDelay, with the ADC value
/// offset + gain * charge rounded and clipped to [-32768, maxValue] (ADC saturation, counted in the
/// observable NSaturatedSignals). All the signals of the event are digitized, also those without points in
/// the acquisition window (flat at offset).
///
/// The charge of each sample depends on the mode:
/// - integrate: the charge of the points inside the sample [t, t + sampling) is added (signals made of
///   charges, e.g. from TRestDetectorHitsToSignalProcess);
/// - sample: the signal is linearly interpolated at the start of the sample, 0 outside the signal
///   (waveforms, e.g. from TRestDetectorSignalShapingProcess, evaluated on a finer grid).
///
/// Trigger time (triggerMode), observable TriggerTime:
/// - firstDeposit: time of the first point with positive charge of the event;
/// - integralThreshold: first time at which the charge of the event in the previous integralWindow (by
///   default half the acquisition window, as legacy) reaches integralThreshold. Events that do not reach
///   it are rejected;
/// - fixed: triggerFixedStartTime.
///
/// Instead of gain and offset, a linear calibration can be given: the energies calibrationEnergy go to the
/// fractions calibrationRange of the ADC range (0: -32768, 1: 32767). Events without signals are rejected.
/// The noise and the shaping are not done here: use TRestDetectorSignalShapingProcess before (in physical
/// units) and TRestRawSignalAddNoiseProcess after, as for the data.
///
/// Differences with legacy: no shaping or noise inside the process, no parameters per channel type and no
/// ...TPC trigger modes (each detector goes in its own pipeline), no observable trigger mode, triggerDelay
/// in time units instead of samples, the sample mode, the saturation maxValue and the calibration offset
/// (legacy multiplied the gain by the ADC range twice).
///
/// Parameters: sampling (mandatory), nPoints (512), mode (integrate), triggerMode (firstDeposit),
/// triggerDelay (0), integralThreshold (0), integralWindow (0), triggerFixedStartTime (0), gain (1),
/// offset (0), calibrationEnergy and calibrationRange ([0, 0]: not used), maxValue (32767).
///
/// Example:
///
///    signalToRaw:
///      class: "TRestDetectorSignalToRawSignalProcess"
///      sampling: 0.04 us
///      nPoints: 512
///      mode: "sample"
///      triggerMode: "firstDeposit"
///      triggerDelay: 4 us
///      gain: 10
///      offset: 250
///      maxValue: 4095
///      observables:
///        - "TriggerTime"

#include "TRestDetectorSignalToRawSignalProcess.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "TRestDetectorSignalEvent.h"
#include "TRestRawSignalEvent.h"
#include "TRestRun.h"

static const bool TRestDetectorSignalToRawSignalProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorSignalToRawSignalProcess;
    reg.RegisterField<P>("sampling", &P::fSampling);
    reg.RegisterField<P>("nPoints", &P::fNPoints);
    reg.RegisterField<P>("mode", &P::fMode);
    reg.RegisterField<P>("triggerMode", &P::fTriggerMode);
    reg.RegisterField<P>("triggerDelay", &P::fTriggerDelay);
    reg.RegisterField<P>("integralThreshold", &P::fIntegralThreshold);
    reg.RegisterField<P>("integralWindow", &P::fIntegralWindow);
    reg.RegisterField<P>("triggerFixedStartTime", &P::fTriggerFixedStartTime);
    reg.RegisterField<P>("gain", &P::fGain);
    reg.RegisterField<P>("offset", &P::fOffset);
    reg.RegisterField<P>("calibrationEnergy", &P::fCalibrationEnergy);
    reg.RegisterField<P>("calibrationRange", &P::fCalibrationRange);
    reg.RegisterField<P>("maxValue", &P::fMaxValue);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorSignalToRawSignalProcess",
        [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorSignalToRawSignalProcess>(instanceName, params);
        });
    return true;
}();

constexpr double kMinADC = std::numeric_limits<short>::min();
constexpr double kMaxADC = std::numeric_limits<short>::max();
}  // namespace

TRestDetectorSignalToRawSignalProcess::TRestDetectorSignalToRawSignalProcess() : TRestEventProcess() {
    fName = "TRestDetectorSignalToRawSignalProcess";
}

TRestDetectorSignalToRawSignalProcess::TRestDetectorSignalToRawSignalProcess(const std::string& instanceName,
                                                                             const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestDetectorSignalToRawSignalProcess::TRestDetectorSignalToRawSignalProcess(const std::string& fileName,
                                                                             const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorSignalToRawSignalProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestDetectorSignalToRawSignalProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }
    UpdateParamsFromYAML<TRestDetectorSignalToRawSignalProcess>(fNode);
    UpdateYAMLFromParams<TRestDetectorSignalToRawSignalProcess>(fNode);
}

void TRestDetectorSignalToRawSignalProcess::InitProcess() {
    const std::string prefix = "TRestDetectorSignalToRawSignalProcess: ";
    if (fSampling <= 0 || fNPoints <= 0) {
        throw std::runtime_error(prefix + "sampling and nPoints must be > 0");
    }
    if (kModes.count(fMode) == 0) {
        throw std::runtime_error(prefix + "unknown mode '" + fMode + "' (integrate or sample)");
    }
    if (kTriggerModes.count(fTriggerMode) == 0) {
        throw std::runtime_error(prefix + "unknown triggerMode '" + fTriggerMode +
                                 "' (firstDeposit, integralThreshold or fixed)");
    }
    if (fTriggerMode == "integralThreshold" && fIntegralThreshold <= 0) {
        throw std::runtime_error(prefix + "triggerMode integralThreshold needs integralThreshold > 0");
    }
    if (fIntegralWindow < 0) throw std::runtime_error(prefix + "integralWindow must be >= 0");
    if (fMaxValue <= kMinADC || fMaxValue > kMaxADC) {
        throw std::runtime_error(prefix + "maxValue must be in (-32768, 32767]");
    }

    fADCGain = fGain;
    fADCOffset = fOffset;
    const auto& [minEnergy, maxEnergy] = fCalibrationEnergy;
    const auto& [minRange, maxRange] = fCalibrationRange;
    if (minEnergy != maxEnergy || minRange != maxRange) {
        if (minEnergy == maxEnergy || minRange == maxRange) {
            throw std::runtime_error(prefix +
                                     "calibrationEnergy and calibrationRange need two different values");
        }
        // ADC(E) = min + range * (minRange + (E - minEnergy) * (maxRange - minRange) / (maxEnergy -
        // minEnergy))
        const double range = kMaxADC - kMinADC;
        fADCGain = range * (maxRange - minRange) / (maxEnergy - minEnergy);
        fADCOffset = kMinADC + range * minRange - fADCGain * minEnergy;
    }

    RegisterObservable("TriggerTime", fTriggerTime);
    RegisterObservable("NSaturatedSignals", fNSaturatedSignals);
}

bool TRestDetectorSignalToRawSignalProcess::GetTriggerTime(const TRestDetectorSignalEvent& event,
                                                           double& time) const {
    if (fTriggerMode == "fixed") {
        time = fTriggerFixedStartTime;
        return true;
    }

    // Points with positive charge of all the signals, sorted in time
    std::vector<std::pair<double, double>> points;
    for (int s = 0; s < event.GetNumberOfSignals(); s++) {
        for (const auto& point : event.GetSignal(s).GetPointsVector()) {
            if (point.second > 0) points.push_back(point);
        }
    }
    if (points.empty()) return false;
    std::sort(points.begin(), points.end());

    if (fTriggerMode == "firstDeposit") {
        time = points.front().first;
        return true;
    }

    // integralThreshold: charge in (t - window, t] at each point
    const double window = fIntegralWindow > 0 ? (double)fIntegralWindow : fNPoints * fSampling / 2.;
    double charge = 0;
    size_t first = 0;
    for (const auto& [t, q] : points) {
        charge += q;
        while (points[first].first <= t - window) charge -= points[first++].second;
        if (charge >= fIntegralThreshold) {
            time = t;
            return true;
        }
    }
    return false;
}

bool TRestDetectorSignalToRawSignalProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* signalEvent = dynamic_cast<const TRestDetectorSignalEvent*>(&input);
    auto* rawEvent = dynamic_cast<TRestRawSignalEvent*>(&output);
    if (!signalEvent || !rawEvent) {
        throw std::runtime_error(
            "TRestDetectorSignalToRawSignalProcess: input/output is not a TRestDetectorSignalEvent/"
            "TRestRawSignalEvent");
    }

    rawEvent->Initialize();
    rawEvent->SetEventInfo(signalEvent);
    fNSaturatedSignals = 0;
    fTriggerTime = 0;

    if (signalEvent->GetNumberOfSignals() <= 0) return false;
    if (!GetTriggerTime(*signalEvent, fTriggerTime)) {
        RESTDebug << "TRestDetectorSignalToRawSignalProcess: event " << signalEvent->GetID()
                  << " without trigger" << RESTendl;
        return false;
    }
    const double startTime = fTriggerTime - fTriggerDelay;

    for (int s = 0; s < signalEvent->GetNumberOfSignals(); s++) {
        const auto& signal = signalEvent->GetSignal(s);
        auto points = signal.GetPointsVector();
        std::sort(points.begin(), points.end());

        std::vector<double> charges(fNPoints, 0);
        if (fMode == "integrate") {
            for (const auto& [t, q] : points) {
                const double bin = std::floor((t - startTime) / fSampling);
                if (bin >= 0 && bin < fNPoints) charges[(int)bin] += q;
            }
        } else if (!points.empty()) {
            size_t i = 0;
            for (int b = 0; b < fNPoints; b++) {
                const double t = startTime + b * fSampling;
                if (t < points.front().first || t > points.back().first) continue;
                while (i + 1 < points.size() && points[i + 1].first < t) i++;
                if (i + 1 >= points.size()) {
                    charges[b] = points[i].second;
                    continue;
                }
                const auto& [t0, q0] = points[i];
                const auto& [t1, q1] = points[i + 1];
                charges[b] = t1 > t0 ? q0 + (t - t0) / (t1 - t0) * (q1 - q0) : q0;
            }
        }

        std::vector<short> samples(fNPoints);
        bool saturated = false;
        for (int b = 0; b < fNPoints; b++) {
            double value = std::round(fADCOffset + fADCGain * charges[b]);
            if (value < kMinADC || value > fMaxValue) {
                saturated = true;
                value = std::clamp(value, kMinADC, (double)fMaxValue);
            }
            samples[b] = (short)value;
        }
        if (saturated) fNSaturatedSignals++;
        rawEvent->AddSignal(signal.GetSignalID(), samples);
    }

    return rawEvent->GetNumberOfSignals() > 0;
}
