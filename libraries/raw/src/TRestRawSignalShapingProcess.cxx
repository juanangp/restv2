#include "TRestRawSignalShapingProcess.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

#include "TRestPulseShapeAnalysis.h"
#include "TRestRawSignalEvent.h"

static const bool TRestRawSignalShapingProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestRawSignalShapingProcess>("shapingType",
                                                    &TRestRawSignalShapingProcess::fShapingType);
    reg.RegisterField<TRestRawSignalShapingProcess>("shapingTime",
                                                    &TRestRawSignalShapingProcess::fShapingTime);
    reg.RegisterField<TRestRawSignalShapingProcess>("gain", &TRestRawSignalShapingProcess::fShapingGain);
    reg.RegisterField<TRestRawSignalShapingProcess>("baselineRange",
                                                    &TRestRawSignalShapingProcess::fBaselineRange);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestRawSignalShapingProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestRawSignalShapingProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestRawSignalShapingProcess::TRestRawSignalShapingProcess() : TRestEventProcess() {
    fName = "TRestRawSignalShapingProcess";
}

TRestRawSignalShapingProcess::TRestRawSignalShapingProcess(const std::string& instanceName,
                                                           const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestRawSignalShapingProcess::TRestRawSignalShapingProcess(const std::string& fileName,
                                                           const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestRawSignalShapingProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestRawSignalShapingProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestRawSignalShapingProcess>(fNode);
    UpdateYAMLFromParams<TRestRawSignalShapingProcess>(fNode);
}

void TRestRawSignalShapingProcess::InitProcess() {
    fResponse =
        TRestPulseShapeAnalysis::BuildResponse(fShapingType, fShapingTime, fShapingGain, fResponseCenter);

    if (fResponse.empty()) {
        throw std::runtime_error("TRestRawSignalShapingProcess: cannot build the response for shapingType '" +
                                 fShapingType + "' with shapingTime " + std::to_string(fShapingTime) +
                                 ". Valid types: gaus, exponential, shaper, shaperSin; shapingTime must give "
                                 "a kernel with at least one non-null sample.");
    }
}

bool TRestRawSignalShapingProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* inEvent = dynamic_cast<const TRestRawSignalEvent*>(&input);
    auto* outEvent = dynamic_cast<TRestRawSignalEvent*>(&output);
    if (!inEvent || !outEvent) {
        throw std::runtime_error("TRestRawSignalShapingProcess: input/output is not a TRestRawSignalEvent");
    }

    const int nSignals = inEvent->GetNumberOfSignals();
    if (nSignals <= 0) return false;

    outEvent->CopyFrom(&input);

    constexpr long kMin = std::numeric_limits<short>::min();
    constexpr long kMax = std::numeric_limits<short>::max();

    const bool useBaseline = fBaselineRange.second > fBaselineRange.first;

    for (int s = 0; s < nSignals; ++s) {
        const std::vector<short> data = inEvent->GetSignal(s).GetData();

        double baseline = 0, sigma = 0;
        std::vector<double> centered(data.begin(), data.end());
        if (useBaseline) {
            TRestPulseShapeAnalysis::GetBaselineSigma(data, fBaselineRange.first, fBaselineRange.second,
                                                      baseline, sigma);
            for (double& v : centered) v -= baseline;
        }

        std::vector<double> shaped =
            TRestPulseShapeAnalysis::Convolve(centered, fResponse, fResponseCenter, !useBaseline);
        if (useBaseline) {
            for (double& v : shaped) v += baseline;
        }

        auto& outSig = outEvent->GetSignal(s);
        const int nPoints = std::min((int)shaped.size(), outSig.GetNPoints());
        for (int b = 0; b < nPoints; ++b) {
            const short newSample = static_cast<short>(std::clamp(std::lround(shaped[b]), kMin, kMax));
            outSig.IncreaseBinBy(b, newSample - outSig.GetPoint(b));
        }
    }

    return true;
}
