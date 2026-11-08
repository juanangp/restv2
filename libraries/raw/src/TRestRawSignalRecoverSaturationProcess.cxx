/// \class TRestRawSignalRecoverSaturationProcess
/// Recovers signals that saturated the ADC by fitting the AGET response function (without the sine
/// term, see TRestPulseShapeAnalysis::FitAget) to the non-saturated bins, with the baseline fixed.
/// Saturated bins are increased up to the fit values when these are above the saturation value. Signals whose
/// fit is below the saturation value (minus the baseline sigma) in any saturated bin are left unchanged,
/// since the fit does not describe the pulse.
///
/// The fit range is the span of the points over threshold of each signal plus fitMargin bins on each side.
///
/// Signals whose maximum is below minSaturationValue are skipped unless processAllSignals is true. With
/// processAllSignals, signals with fewer than minSaturatedBins flat bins at the maximum are also processed:
/// the nBinsIfNotSaturated bins around the maximum are replaced by the fit values (each bin is moved by
/// the difference between the fit and its own value). Recovered values are clamped to the short range.
///
/// Parameters: minSaturatedBins (3), minSaturationValue (4095), processAllSignals (false),
/// nBinsIfNotSaturated (20), baselineRange ([5,55]), baselineOption (""), pointsThreshold (2),
/// signalThreshold (5), pointsOverThreshold (5), fitMargin (10), nPointsFlat (512).
///
/// Observables: AddedAmplitude, AddedIntegral, SignalsSaturated, SignalsRecovered, SignalsIDsRecovered.
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
#include <climits>
#include <cmath>
#include <memory>

#include "TF1.h"
#include "TGraph.h"
#include "TRestPulseShapeAnalysis.h"
#include "TRestRawSignalEvent.h"
#include "TRestRun.h"
#include "TRestTools.h"

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

TRestRawSignalRecoverSaturationProcess::TRestRawSignalRecoverSaturationProcess(const std::string& fileName,
                                                                               const std::string& sectionName)
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
    fBaselineOption = TRestTools::NormalizeOption("baselineOption", fBaselineOption,
                                                  TRestPulseShapeAnalysis::kBaselineOptions);
    UpdateYAMLFromParams<TRestRawSignalRecoverSaturationProcess>(fNode);
}

void TRestRawSignalRecoverSaturationProcess::InitProcess() {
    RegisterObservable("AddedAmplitude", fObs.AddedAmplitude);
    RegisterObservable("AddedIntegral", fObs.AddedIntegral);
    RegisterObservable("SignalsSaturated", fObs.SignalsSaturated);
    RegisterObservable("SignalsRecovered", fObs.SignalsRecovered);
    RegisterObservable("SignalsIDsRecovered", fObs.SignalsIDsRecovered);
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
    const bool debug = GetVerboseLevel() >= TRestLogManager::REST_Verbose_Level::REST_Debug;
    auto skip = [&](const TRestRawSignal& sig, const std::string& reason) {
        if (debug)
            std::cout << "recSat: event " << sigEvent->GetID() << " signal " << sig.GetSignalID()
                      << " not recovered: " << reason << std::endl;
    };

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

        if (maxValue < fMinSaturationValue && !fProcessAllSignals) continue;

        if ((int)saturatedBins.size() < fMinSaturatedBins && !fProcessAllSignals) {
            skip(signal, "only " + std::to_string(saturatedBins.size()) + " consecutive bins at maximum " +
                             std::to_string(maxValue));
            continue;
        }

        fObs.SignalsSaturated++;

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
                              fBaselineOption)) {
            skip(signal, "baseline calculation failed");
            continue;
        }

        std::vector<float> subtracted(nPoints);
        for (int i = 0; i < nPoints; i++) subtracted[i] = data[i] - baseline;
        std::pair<int, int> range = {0, nPoints};
        const auto pointsOverThr =
            GetPointsOverThreshold(subtracted, range, thrParams, fPointsOverThreshold, fNPointsFlat, sigma);

        if (pointsOverThr.empty()) {
            skip(signal, "no points over threshold (baseline " + std::to_string(baseline) + ", sigma " +
                             std::to_string(sigma) + ")");
            continue;
        }

        // Graph with the pulse range (points over threshold plus a margin) without the saturated bins
        std::vector<bool> isSaturated(nPoints, false);
        for (int b : saturatedBins) isSaturated[b] = true;
        TGraph g;
        const int fitStart = std::max(0, pointsOverThr.front().first - fFitMargin);
        const int fitEnd = std::min(nPoints - 1, pointsOverThr.back().first + fFitMargin);
        for (int i = fitStart; i <= fitEnd; i++) {
            if (!isSaturated[i]) g.AddPoint(i, data[i]);
        }

        const auto fit = FitAget(g, false, baseline);
        if (!fit) {
            skip(signal, "fit failed or invalid (" + std::to_string(g.GetN()) + " points, " +
                             std::to_string(saturatedBins.size()) + " saturated bins)");
            continue;
        }

        // A saturated pulse is at or above the saturation value in all the saturated bins, a fit that is
        // below it (beyond the baseline fluctuations) in any of them does not describe the pulse
        const bool isSaturatedSignal = (int)saturatedBins.size() >= fMinSaturatedBins;
        if (isSaturatedSignal) {
            auto below = std::find_if(saturatedBins.begin(), saturatedBins.end(),
                                      [&](int b) { return fit->Eval(b) < maxValue - sigma; });
            if (below != saturatedBins.end()) {
                std::string pars;
                for (int i = 0; i < fit->GetNpar(); i++)
                    pars +=
                        std::string(" ") + fit->GetParName(i) + "=" + std::to_string(fit->GetParameter(i));
                skip(signal, "bad fit, below saturation at bin " + std::to_string(*below) + " (fit " +
                                 std::to_string(fit->Eval(*below)) + ", saturation " +
                                 std::to_string(maxValue) + ", saturated bins " +
                                 std::to_string(saturatedBins.front()) + "-" +
                                 std::to_string(saturatedBins.back()) + ";" + pars + ")");
                continue;
            }
        }

        bool anyBinRecovered = false;
        double signalAddedAmplitude = 0, signalAddedIntegral = 0;
        std::vector<int> deltas(nPoints, 0);
        for (int b : saturatedBins) {
            // Difference to each bin value (the window of processAllSignals is not flat), keeping the
            // recovered value within the range of a short sample
            const double value =
                std::clamp(fit->Eval(b) - data[b], (double)SHRT_MIN - data[b], (double)SHRT_MAX - data[b]);
            if (value > 0 || fProcessAllSignals) {
                deltas[b] = (int)value;
                anyBinRecovered = true;
                signalAddedIntegral += value;
                signalAddedAmplitude = std::max(signalAddedAmplitude, value);
            }
        }

        if (!anyBinRecovered) {
            skip(signal, "fit does not exceed saturation " + std::to_string(maxValue));
            continue;
        }

        for (int b : saturatedBins) {
            if (deltas[b] != 0) signal.IncreaseBinByClamped(b, deltas[b]);
        }
        fObs.SignalsIDsRecovered.push_back(signal.GetSignalID());
        fObs.SignalsRecovered++;
        fObs.AddedAmplitude += signalAddedAmplitude;
        fObs.AddedIntegral += signalAddedIntegral;
    }

    return true;
}
