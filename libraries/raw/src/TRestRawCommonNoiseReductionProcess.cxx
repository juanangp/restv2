#include "TRestRawCommonNoiseReductionProcess.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "TRestPulseShapeAnalysis.h"

using namespace TRestPulseShapeAnalysis;

// --- TRestCommonNoiseBlock ---
//
// Example 
//
//    commonNoiseReduction:
//      class: "TRestRawCommonNoiseReductionProcess"
//      channels:
//        - channelRange: [[4320, 4391]]
//          mode: 1
//          centerWidth: 10
//        - channelRange: [[4392, 4463]]
//          mode: 1
//          centerWidth: 10
//        - channelRange: [[4464, 4535]]
//          mode: 1
//          centerWidth: 10
//        - channelRange: [[4536, 4607]]
//          mode: 1
//          centerWidth: 10

static const bool TRestCommonNoiseBlock_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestCommonNoiseBlock>("channelRange", &TRestCommonNoiseBlock::fChannelRange);
    reg.RegisterField<TRestCommonNoiseBlock>("mode", &TRestCommonNoiseBlock::fMode);
    reg.RegisterField<TRestCommonNoiseBlock>("centerWidth", &TRestCommonNoiseBlock::fCenterWidth);
    reg.RegisterField<TRestCommonNoiseBlock>("minSignalsRequired",
                                             &TRestCommonNoiseBlock::fMinSignalsRequired);
    reg.RegisterField<TRestCommonNoiseBlock>("baselineRange", &TRestCommonNoiseBlock::fBaselineRange);
    reg.RegisterField<TRestCommonNoiseBlock>("baselineSigmaThreshold", &TRestCommonNoiseBlock::fBaselineSigmaThreshold);
    reg.RegisterField<TRestCommonNoiseBlock>("signalSigmaThreshold",
                                         &TRestCommonNoiseBlock::fSignalSigmaThreshold);
    return true;
}();

TRestCommonNoiseBlock::TRestCommonNoiseBlock() : TRestMetadata() { fName = "TRestCommonNoiseBlock"; }

TRestCommonNoiseBlock::TRestCommonNoiseBlock(const std::string& name, const YAML::Node& node)
    : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestCommonNoiseBlock::LoadConfig() {
    UpdateParamsFromYAML<TRestCommonNoiseBlock>(fNode);
    UpdateYAMLFromParams<TRestCommonNoiseBlock>(fNode);
}

bool TRestCommonNoiseBlock::ContainsID(int signalID) const {
    if (fChannelRange.empty()) return true;
    for (const auto& [lo, hi] : fChannelRange) {
        if (signalID >= lo && signalID <= hi) return true;
    }
    return false;
}

// --- TRestRawCommonNoiseReductionProcess ---

static const bool TRestRawCommonNoiseReductionProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestRawCommonNoiseReductionProcess>(
        "channels", &TRestRawCommonNoiseReductionProcess::fBlocks);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestRawCommonNoiseReductionProcess",
        [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestRawCommonNoiseReductionProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestRawCommonNoiseReductionProcess::TRestRawCommonNoiseReductionProcess() : TRestEventProcess() {
    fName = "TRestRawCommonNoiseReductionProcess";
}

TRestRawCommonNoiseReductionProcess::TRestRawCommonNoiseReductionProcess(const std::string& instanceName,
                                                                         const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestRawCommonNoiseReductionProcess::TRestRawCommonNoiseReductionProcess(const std::string& fileName,
                                                                         const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestRawCommonNoiseReductionProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestRawCommonNoiseReductionProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestRawCommonNoiseReductionProcess>(fNode);
    UpdateYAMLFromParams<TRestRawCommonNoiseReductionProcess>(fNode);
}

/// \brief Corrects one block's signals in place inside outEvent: computes each
/// signal's baseline, keeps only the ones above the sigma threshold as "to be
/// corrected" (the rest are passed through untouched), and for each time bin
/// estimates the common-mode value from the block's own signals (median, or an
/// average window around it) and subtracts it.
void TRestRawCommonNoiseReductionProcess::ProcessBlock(const TRestRawSignalEvent& inEvent,
                                                       TRestRawSignalEvent& outEvent,
                                                       const TRestCommonNoiseBlock& block) const {

    std::vector<int> members;
    int nBins = -1;
    for (int s = 0; s < inEvent.GetNumberOfSignals(); s++) {
        const auto& sig = inEvent.GetSignal(s);
        if (!block.ContainsID(sig.GetSignalID())) continue;
        const int n = sig.GetNPoints();
        if (nBins < 0) nBins = n;
        if (n != nBins) continue;
        members.push_back(s);
    }
    if (nBins <= 0 || (int)members.size() < block.fMinSignalsRequired) return;

    std::vector<int> active;
    std::vector<double> activeBaseline;
    std::vector<std::vector<short>> activeData;
    std::vector<size_t> cleanIdx;

    for (int pos : members) {
        std::vector<short> data = inEvent.GetSignal(pos).GetData();

        double baseline = 0, sigma = 0;
        GetBaselineSigma(data, block.fBaselineRange.first, block.fBaselineRange.second, baseline, sigma);

        if (sigma < block.fBaselineSigmaThreshold) continue;

        bool hasSignal = false;
        if (sigma > 0) {
            const double limit = block.fSignalSigmaThreshold * sigma;
            const double hi = baseline + limit, lo = baseline - limit;
            for (short v : data) {
                if (v > hi || v < lo) {
                    hasSignal = true;
                    break;
                }
            }
        }

        if (!hasSignal) cleanIdx.push_back(active.size());
        active.push_back(pos);
        activeBaseline.push_back(baseline);
        activeData.push_back(std::move(data));
    }

    const int nClean = (int)cleanIdx.size();
    if (nClean < block.fMinSignalsRequired) return;

    double baselineMean = 0;
    for (double b : activeBaseline) baselineMean += b;
    baselineMean /= (double)active.size();

    std::vector<short> matrix((size_t)nBins * nClean);
    for (int k = 0; k < nClean; k++) {
        const auto& d = activeData[cleanIdx[k]];
        for (int b = 0; b < nBins; b++) matrix[(size_t)b * nClean + k] = d[b];
    }

    int begin = nClean / 2, end = nClean / 2;
    if (block.fMode == 1) {
        const int middle = nClean / 2;
        const int half = (int)(nClean * block.fCenterWidth * 0.01);
        begin = std::max(0, middle - half);
        end = std::min(nClean - 1, middle + half);
    }
    const double norm = (double)(end - begin + 1);

    std::vector<short> deltas(nBins);
    for (int b = 0; b < nBins; b++) {
        short* row = matrix.data() + (size_t)b * nClean;

        std::nth_element(row, row + begin, row + nClean);
        double binCorrection;
        if (end == begin) {
            binCorrection = row[begin];
        } else {
            std::nth_element(row + begin + 1, row + end, row + nClean);
            double sum = 0;
            for (int i = begin; i <= end; i++) sum += row[i];
            binCorrection = sum / norm;
        }

        deltas[b] = (short)std::lround(baselineMean - binCorrection);
    }

    for (int pos : active) {
        auto& sig = outEvent.GetSignal(pos);
        for (int b = 0; b < nBins; b++) sig.IncreaseBinBy(b, deltas[b]);
    }
}

bool TRestRawCommonNoiseReductionProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    output.CopyFrom(&input);

    const auto* inEvent = dynamic_cast<const TRestRawSignalEvent*>(&input);
    auto* outEvent = dynamic_cast<TRestRawSignalEvent*>(&output);
    if (!inEvent || !outEvent) {
        throw std::runtime_error(
            "TRestRawCommonNoiseReductionProcess: input/output is not a TRestRawSignalEvent");
    }

    if (fBlocks.empty()) {
        ProcessBlock(*inEvent, *outEvent, fDefaultBlock);
    } else {
        for (const auto& block : fBlocks) {
            ProcessBlock(*inEvent, *outEvent, block);
        }
    }

    return true;
}
