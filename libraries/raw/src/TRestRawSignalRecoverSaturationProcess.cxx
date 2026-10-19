/// \class TRestRawSignalRecoverSaturationProcess
/// Recovers signals that saturated the ADC by fitting the AGET response function (without the sine
/// term, see TRestPulseShapeAnalysis::FitAget) to the non-saturated bins, with the baseline fixed.
/// Saturated bins are increased up to the fit values when these are above the saturation value.
///
/// The fit range is the span of the points over threshold of each signal plus fitMargin bins on each side.
///
/// Parameters: minSaturatedBins (3), minSaturationValue (4095), processAllSignals (false),
/// nBinsIfNotSaturated (20), baselineRange ([5,55]), baselineOption (""), pointsThreshold (2),
/// signalThreshold (5), pointsOverThreshold (5), fitMargin (20), nPointsFlat (512).
///
/// Observables: AddedAmplitude, AddedIntegral, SignalsSaturated, SignalsRecovered.
///
/// Example:
///
///    recSat:
///      class: "TRestRawSignalRecoverSaturationProcess"
///      minSaturationValue: 3500
///      baselineRange: [20, 150]
///      pointsOverThreshold: 7

#include "TRestRawSignalRecoverSaturationProcess.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "TF1.h"
#include "TGraph.h"
#include "TRestPulseShapeAnalysis.h"
#include "TRestRawSignalEvent.h"
#include "TRestRun.h"

using namespace TRestPulseShapeAnalysis;

static const bool TRestRawSignalRecoverSaturationProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestRawSignalRecoverSaturationProcess;
    reg.RegisterField<P>("minSaturatedBins", &P::fMinSaturatedBins);
    reg.RegisterField<P>("processAllSignals", &P::fProcessAllSignals);
    reg.RegisterField<P>("nBinsIfNotSaturated", &P::fNBinsIfNotSaturated);
    reg.RegisterField<P>("minSaturationValue", &P::fMinSaturationValue);
    reg.RegisterField<P>("baselineRange", &P::fBaselineRange);
    reg.RegisterField<P>("baselineOption", &P::fBaselineOption);
    reg.RegisterField<P>("pointsThreshold", &P::fPointsThreshold);
    reg.RegisterField<P>("signalThreshold", &P::fSignalThreshold);
    reg.RegisterField<P>("pointsOverThreshold", &P::fPointsOverThreshold);
    reg.RegisterField<P>("fitMargin", &P::fFitMargin);
    reg.RegisterField<P>("nPointsFlat", &P::fNPointsFlat);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestRawSignalRecoverSaturationProcess",
        [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestRawSignalRecoverSaturationProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestRawSignalRecoverSaturationProcess::TRestRawSignalRecoverSaturationProcess() : TRestEventProcess() {
    fName = "TRestRawSignalRecoverSaturationProcess";
}

TRestRawSignalRecoverSaturationProcess::TRestRawSignalRecoverSaturationProcess(
    const std::string& instanceName, const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestRawSignalRecoverSaturationProcess::TRestRawSignalRecoverSaturationProcess(
    const std::string& fileName, const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestRawSignalRecoverSaturationProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestRawSignalRecoverSaturationProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestRawSignalRecoverSaturationProcess>(fNode);
    UpdateYAMLFromParams<TRestRawSignalRecoverSaturationProcess>(fNode);
}

void TRestRawSignalRecoverSaturationProcess::InitProcess() {
    RegisterObservable("AddedAmplitude", fObs.AddedAmplitude);
    RegisterObservable("AddedIntegral", fObs.AddedIntegral);
    RegisterObservable("SignalsSaturated", fObs.SignalsSaturated);
    RegisterObservable("SignalsRecovered", fObs.SignalsRecovered);
    RegisterObservable("SignalsRecovered", fObs.SignalsIDsRecovered);
}

bool TRestRawSignalRecoverSaturationProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    output.CopyFrom(&input);

    auto* sigEvent = dynamic_cast<TRestRawSignalEvent*>(&output);
    if (!sigEvent) {
        throw std::runtime_error(
            "TRestRawSignalRecoverSaturationProcess: output is not a TRestRawSignalEvent");
    }

    fObs.clear();

    const auto thrParams = std::make_pair(fPointsThreshold, fSignalThreshold);

    for (int s = 0; s < sigEvent->GetNumberOfSignals(); s++) {
        auto& signal = sigEvent->GetSignal(s);
        const std::vector<short> data = signal.GetData();
        const int nPoints = (int)data.size();
        if (nPoints == 0) continue;

        const int maxPeakBin = (int)(std::max_element(data.begin(), data.end()) - data.begin());
        short maxValue = data[maxPeakBin];

        // Saturated bins: consecutive bins equal to the maximum, starting at its first occurrence
        std::vector<int> saturatedBins;
        for (int i = maxPeakBin; i < nPoints && data[i] == maxValue; i++) saturatedBins.push_back(i);

        if ((int)saturatedBins.size() < fMinSaturatedBins && !fProcessAllSignals) continue;

        fObs.SignalsSaturated++;

        if (maxValue < fMinSaturationValue) continue;

        if (fProcessAllSignals && (int)saturatedBins.size() < fMinSaturatedBins) {
            saturatedBins.clear();
            const int from = std::max(0, maxPeakBin - fNBinsIfNotSaturated / 2);
            const int to = std::min(nPoints, maxPeakBin + fNBinsIfNotSaturated / 2);
            for (int i = from; i < to; i++) saturatedBins.push_back(i);
            if (saturatedBins.empty()) continue;
            maxValue = data[saturatedBins.front()];
        }

        double baseline = 0, sigma = 0;
        if (!GetBaselineSigma(data, fBaselineRange.first, fBaselineRange.second, baseline, sigma,
                              fBaselineOption))
            continue;

        std::vector<float> subtracted(nPoints);
        for (int i = 0; i < nPoints; i++) subtracted[i] = data[i] - baseline;
        std::pair<int, int> range = {0, nPoints};
        const auto pointsOverThr = GetPointsOverThreshold(subtracted, range, thrParams, fPointsOverThreshold,
                                                          fNPointsFlat, sigma);

        if (pointsOverThr.empty()) continue;

        // Graph with the pulse range (points over threshold plus a margin) without the saturated bins
        std::vector<bool> isSaturated(nPoints, false);
        for (int b : saturatedBins) isSaturated[b] = true;
        TGraph g;
        const int fitStart = std::max(0, pointsOverThr.front().first - fFitMargin);
        const int fitEnd = std::min(nPoints - 1, pointsOverThr.back().first + fFitMargin);
        for (int i = fitStart; i <= fitEnd; i++) {
            if (!isSaturated[i]) g.AddPoint(i, data[i]);
        }

        const auto fit = FitAget(g, false, baseline,
                                 GetVerboseLevel() >= TRestLogManager::REST_Verbose_Level::REST_Debug);
        if (!fit) continue;

        bool anyBinRecovered = false;
        double signalAddedAmplitude = 0, signalAddedIntegral = 0;
        std::vector<short> deltas(nPoints, 0);
        for (int b : saturatedBins) {
            const double value = fit->Eval(b) - maxValue;
            if (value > 0 || fProcessAllSignals) {
                deltas[b] = (short)value;
                anyBinRecovered = true;
                signalAddedIntegral += value;
                signalAddedAmplitude = std::max(signalAddedAmplitude, value);
            }
        }

        if (!anyBinRecovered) continue;

        for (int b : saturatedBins) {
            if (deltas[b] != 0) signal.IncreaseBinBy(b, deltas[b]);
        }
        fObs.SignalsIDsRecovered.push_back(signal.GetSignalID());
        fObs.SignalsRecovered++;
        fObs.AddedAmplitude += signalAddedAmplitude;
        fObs.AddedIntegral += signalAddedIntegral;
    }

    return true;
}
