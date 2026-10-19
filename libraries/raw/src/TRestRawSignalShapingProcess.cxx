#include "TRestRawSignalShapingProcess.h"
#include "TRestRawSignalEvent.h"
#include "TRestPulseShapeAnalysis.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

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

std::vector<double> TRestRawSignalShapingProcess::BuildResponse(const std::string& type, double shapingTime,
                                                                double gain, int& center) {
    center = 0;
    if (!(shapingTime > 0)) return {};

    std::vector<double> r;

    if (type == "gaus") {
        const int cBin = (int)(shapingTime * 3.5);
        r.resize(2 * cBin);
        center = cBin;
        for (int i = 0; i < 2 * cBin; ++i) {
            const double x = (i - cBin) / shapingTime;
            r[i] = std::exp(-0.5 * x * x);
        }
    } else if (type == "exponential" || type == "shaper" || type == "shaperSin") {
        r.resize((int)(5 * shapingTime));
        for (size_t i = 0; i < r.size(); ++i) {
            const double c = (double)i / shapingTime;
            if (type == "exponential")
                r[i] = std::exp(-c);
            else
                r[i] = TRestPulseShapeAnalysis::AgetShape(c, type == "shaperSin");
        }
    } else {
        return {};
    }

    const double sum = std::accumulate(r.begin(), r.end(), 0.0);
    if (!(sum > 0)) return {};
    for (double& v : r) v *= gain / sum;
    return r;
}

std::vector<double> TRestRawSignalShapingProcess::Convolve(const std::vector<double>& in,
                                                           const std::vector<double>& response, int center,
                                                           bool ignoreNegative) {
    const int nBins = (int)in.size();
    const int nr = (int)response.size();
    std::vector<double> out(nBins, 0.0);

    for (int m = 0; m < nBins; ++m) {
        const double v = in[m];
        if (v == 0 || (ignoreNegative && v < 0)) continue;

        const int kMin = std::max(0, center - m);
        const int kMax = std::min(nr, nBins - m + center);
        for (int k = kMin; k < kMax; ++k) out[m + k - center] += response[k] * v;
    }
    return out;
}

void TRestRawSignalShapingProcess::InitProcess() {
    fResponse = BuildResponse(fShapingType, fShapingTime, fShapingGain, fResponseCenter);

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

    if (&input != &output) {
        outEvent->CopyFrom(&input);
    }

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

        std::vector<double> shaped = Convolve(centered, fResponse, fResponseCenter, !useBaseline);
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
