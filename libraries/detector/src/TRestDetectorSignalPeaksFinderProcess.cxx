/// \class TRestDetectorSignalPeaksFinderProcess
/// Finds all the peaks of each signal with TRestPulseShapeAnalysis::GetPeaks and groups the peaks of the
/// event in time coincidence windows. It replaces the legacy TRestRawPeaksFinderProcess: it works on
/// detector signals, whose baseline is already subtracted and whose times are physical, so there is no
/// baseline, time conversion or calibration; and channels are not selected by type, the signals of each
/// detector (e.g. vetoes and Micromegas) are split beforehand with TRestRawSignalRemoveChannelsProcess
/// and analysed separately. Legacy veto signals used halfWindow 2 and maxGreaterEqual 0. Legacy windows
/// contained the peaks up to window / 2 after the first one.
///
/// Observables, the peaks sorted by time and then by signal ID:
/// - PeaksSignalID, PeaksTime, PeaksAmplitude, PeaksWindowIndex: one entry per peak.
/// - WindowTime (time of the first peak), WindowMultiplicity (number of peaks), WindowAmplitude (sum of
///   the peak amplitudes): one entry per window.
/// - NPeaks, NPeaksSignals (signals with at least one peak), NWindows, PeaksAmplitudeSum.
///
/// Parameters: threshold (100), distance (0.4 us), halfWindow (5), maxGreaterEqual (3), window (0.2 us).
///
/// Example:
///
///    peaksFinder:
///      class: "TRestDetectorSignalPeaksFinderProcess"
///      threshold: 50
///      distance: 400ns
///      window: 200ns

#include "TRestDetectorSignalPeaksFinderProcess.h"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <tuple>

#include "TRestDetectorSignalEvent.h"
#include "TRestPulseShapeAnalysis.h"
#include "TRestRun.h"

static const bool TRestDetectorSignalPeaksFinderProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorSignalPeaksFinderProcess;
    reg.RegisterField<P>("threshold", &P::fThreshold);
    reg.RegisterField<P>("distance", &P::fDistance);
    reg.RegisterField<P>("halfWindow", &P::fHalfWindow);
    reg.RegisterField<P>("maxGreaterEqual", &P::fMaxGreaterEqual);
    reg.RegisterField<P>("window", &P::fWindow);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorSignalPeaksFinderProcess",
        [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorSignalPeaksFinderProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestDetectorSignalPeaksFinderProcess::TRestDetectorSignalPeaksFinderProcess() : TRestEventProcess() {
    fName = "TRestDetectorSignalPeaksFinderProcess";
}

TRestDetectorSignalPeaksFinderProcess::TRestDetectorSignalPeaksFinderProcess(const std::string& instanceName,
                                                                             const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestDetectorSignalPeaksFinderProcess::TRestDetectorSignalPeaksFinderProcess(const std::string& fileName,
                                                                             const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorSignalPeaksFinderProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestDetectorSignalPeaksFinderProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestDetectorSignalPeaksFinderProcess>(fNode);
    UpdateYAMLFromParams<TRestDetectorSignalPeaksFinderProcess>(fNode);
}

void TRestDetectorSignalPeaksFinderProcess::InitProcess() {
    if (fDistance < 0 || fWindow < 0 || fHalfWindow < 1 || fMaxGreaterEqual < 0) {
        throw std::runtime_error(
            "TRestDetectorSignalPeaksFinderProcess: distance, window and maxGreaterEqual must be >= 0 and "
            "halfWindow >= 1");
    }

    RegisterObservable("PeaksSignalID", fObs.PeaksSignalID);
    RegisterObservable("PeaksTime", fObs.PeaksTime);
    RegisterObservable("PeaksAmplitude", fObs.PeaksAmplitude);
    RegisterObservable("PeaksWindowIndex", fObs.PeaksWindowIndex);

    RegisterObservable("WindowTime", fObs.WindowTime);
    RegisterObservable("WindowMultiplicity", fObs.WindowMultiplicity);
    RegisterObservable("WindowAmplitude", fObs.WindowAmplitude);

    RegisterObservable("NPeaks", fObs.NPeaks);
    RegisterObservable("NPeaksSignals", fObs.NPeaksSignals);
    RegisterObservable("NWindows", fObs.NWindows);
    RegisterObservable("PeaksAmplitudeSum", fObs.PeaksAmplitudeSum);
}

bool TRestDetectorSignalPeaksFinderProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    output.CopyFrom(&input);

    const auto* signalEvent = dynamic_cast<const TRestDetectorSignalEvent*>(&input);
    if (!signalEvent) {
        throw std::runtime_error(
            "TRestDetectorSignalPeaksFinderProcess: input is not a TRestDetectorSignalEvent");
    }

    fObs.clear();

    // (time, signal ID, amplitude) of all the peaks of the event
    std::vector<std::tuple<double, int, double>> peaks;
    std::set<int> peakSignals;
    for (int s = 0; s < signalEvent->GetNumberOfSignals(); s++) {
        const auto& signal = signalEvent->GetSignal(s);
        for (const auto& [time, amplitude] : TRestPulseShapeAnalysis::GetPeaks(
                 signal.GetPointsVector(), fThreshold, fDistance, fHalfWindow, fMaxGreaterEqual)) {
            peaks.emplace_back(time, signal.GetSignalID(), amplitude);
            peakSignals.insert(signal.GetSignalID());
        }
    }
    std::sort(peaks.begin(), peaks.end());

    for (const auto& [time, signalID, amplitude] : peaks) {
        // Peaks are sorted by time, so a peak opens a new window if it is too far from the current one
        if (fObs.WindowTime.empty() || time - fObs.WindowTime.back() > fWindow) {
            fObs.WindowTime.push_back(time);
            fObs.WindowMultiplicity.push_back(0);
            fObs.WindowAmplitude.push_back(0);
        }
        fObs.WindowMultiplicity.back()++;
        fObs.WindowAmplitude.back() += amplitude;

        fObs.PeaksSignalID.push_back(signalID);
        fObs.PeaksTime.push_back(time);
        fObs.PeaksAmplitude.push_back(amplitude);
        fObs.PeaksWindowIndex.push_back(fObs.WindowTime.size() - 1);
        fObs.PeaksAmplitudeSum += amplitude;
    }

    fObs.NPeaks = peaks.size();
    fObs.NPeaksSignals = peakSignals.size();
    fObs.NWindows = fObs.WindowTime.size();

    return true;
}
