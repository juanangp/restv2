#include "TRestDataSetSignalSelection.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "TRestDataSet.h"

/// \class TRestDataSetSignalSelection
///
/// Selection of signal-like entries, a process of TRestDataSetProcessManager. The range of an `observable`
/// between two quantiles of its distribution in the reference (the signal, e.g. a calibration:
/// referenceFileName with the cuts, see TRestDataSetProcess) is computed, and only the entries of the
/// dataset processed (e.g. a background) with the observable in that range are kept: the signal efficiency
/// is fixed and applied to the background. The kept entries of each run are its `selectedEntries` in the
/// output dataset.
///
/// The observable is any column, e.g. the log odds of TRestDataSetLogOdds (`logOdds_total`, lower values
/// for the signal) or the score of TRestDataSetTMVA (`bdt_score`, higher values for the signal); the
/// `quantiles` tell which part of the signal is kept:
/// * [0, 0.8]: the 80% of the signal with the lowest values (observable <= max), e.g. log odds.
/// * [0.2, 1]: the 80% with the highest values (observable >= min), e.g. the score of a BDT.
/// * [0.1, 0.9]: the central 80% (min <= observable <= max).
///
/// ### Parameters
/// * observable: column of the datasets.
/// * quantiles: [lower, upper] quantiles of the reference kept, 0 <= lower < upper <= 1 (0 and 1: no limit
///   on that side). The efficiency is upper - lower (up to ties of the values).
/// * referenceFileName, cuts: the signal, see TRestDataSetProcess.
///
/// ### Results
/// * range: [min, max] of the observable kept (-.inf and .inf: no limit on that side).
/// * efficiency: fraction of the reference in the range.
/// * referenceUsed, parametersFileName: see TRestDataSetProcess.
///
/// Example (in a TRestDataSetProcessManager, the energy cut defined in the manager):
///
///    bdtSelection:
///      class: "TRestDataSetSignalSelection"
///      referenceFileName: "Dataset_Calibration_TMVA.root"
///      cuts: ["energy"]
///      observable: "bdt_score"
///      quantiles: [0.2, 1]

static const bool TRestDataSetSignalSelection_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestDataSetSignalSelection>("observable", &TRestDataSetSignalSelection::fObservable);
    reg.RegisterField<TRestDataSetSignalSelection>("quantiles", &TRestDataSetSignalSelection::fQuantiles);
    reg.RegisterField<TRestDataSetSignalSelection>("range", &TRestDataSetSignalSelection::fRange);
    reg.RegisterField<TRestDataSetSignalSelection>("efficiency", &TRestDataSetSignalSelection::fEfficiency);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDataSetSignalSelection", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDataSetSignalSelection>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestDataSetSignalSelection::TRestDataSetSignalSelection() : TRestDataSetProcess() {
    fName = "TRestDataSetSignalSelection";
}

TRestDataSetSignalSelection::TRestDataSetSignalSelection(const std::string& instanceName,
                                                         const YAML::Node& node)
    : TRestDataSetProcess(instanceName, node) {
    LoadConfig();
}

void TRestDataSetSignalSelection::LoadConfig() {
    UpdateParamsFromYAML<TRestDataSetSignalSelection>(fNode);
    const std::string context = "TRestDataSetSignalSelection '" + fName + "'";
    if (fObservable.empty()) throw std::runtime_error(context + ": no observable");
    if (!(fQuantiles.first >= 0 && fQuantiles.first < fQuantiles.second && fQuantiles.second <= 1))
        throw std::runtime_error(context + ": quantiles [lower, upper] with 0 <= lower < upper <= 1 needed");
    UpdateYAMLFromParams<TRestDataSetSignalSelection>(fNode);
}

void TRestDataSetSignalSelection::ComputeParameters(const TRestDataSet&, ROOT::RDF::RNode frame) {
    const std::string context = "TRestDataSetSignalSelection '" + fName + "'";
    std::vector<double> values = *frame.Take<double>(AsDouble(frame, fObservable));
    if (values.empty()) throw std::runtime_error(context + ": no entries in the reference");
    std::sort(values.begin(), values.end());
    const size_t n = values.size();
    constexpr double infinity = std::numeric_limits<double>::infinity();

    // The lowest value above the lower fraction of the entries, the highest below the upper one
    fRange = {-infinity, infinity};
    if (fQuantiles.first > 0) {
        const auto index = static_cast<size_t>(std::floor(fQuantiles.first * n));
        fRange.first = values[std::min(index, n - 1)];
    }
    if (fQuantiles.second < 1) {
        const auto index = static_cast<size_t>(std::ceil(fQuantiles.second * n));
        fRange.second = values[index > 0 ? index - 1 : 0];
    }
    const auto first = std::lower_bound(values.begin(), values.end(), fRange.first);
    const auto last = std::upper_bound(values.begin(), values.end(), fRange.second);
    fEfficiency = last > first ? static_cast<double>(last - first) / n : 0;

    RESTInfo << context << ": " << fObservable << " in [" << fRange.first << ", " << fRange.second
             << "], efficiency " << fEfficiency << " of " << n << " reference entries" << RESTendl;
    UpdateYAMLFromParams<TRestDataSetSignalSelection>(fNode);
}

void TRestDataSetSignalSelection::Apply(TRestDataSet& dataSet) const {
    ROOT::RDF::RNode frame = dataSet.GetDataFrame();
    const std::string input = AsDouble(frame, fObservable);
    const auto range = fRange;
    frame = frame.Filter([range](double value) { return value >= range.first && value <= range.second; },
                         {input}, fName);
    dataSet.SetDataFrame(frame, {});
}
