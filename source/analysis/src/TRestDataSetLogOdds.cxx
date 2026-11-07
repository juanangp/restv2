#include "TRestDataSetLogOdds.h"

#include <TDirectory.h>

#include <cmath>
#include <stdexcept>

#include "TRestDataSet.h"

/// \class TRestDataSetLogOdds
///
/// Log odds of observables, a process of TRestDataSetProcessManager. The probability density function
/// (PDF) of each observable is built in the reference (referenceFileName with the cuts, see
/// TRestDataSetProcess), a histogram in its range normalized to 1; for each entry of the dataset, with p the
/// probability of the bin of its value, the column `<name>_<observable>` = log(1 - p) - log(p) is added
/// (1000 if p is 0, e.g. outside the range, and -1000 if p is 1, a single bin with entries), and
/// `<name>_total` with their sum. The lower the value, the more likely the entry is in the distribution of
/// the reference.
///
/// ### Parameters
/// * observables: list of `observable` (column), `range` of the PDF and `nBins` (100).
/// * referenceFileName, cuts: see TRestDataSetProcess.
///
/// ### Results
/// * observables: `pdf`, the probability of each bin.
/// * referenceUsed, parametersFileName: see TRestDataSetProcess.
///
/// Objects (folder RESTDataSetStore/TRestDataSetLogOdds/<name> of the output dataset): the PDF of each
/// observable, `pdf_<observable>`.
///
/// Example (in a TRestDataSetProcessManager, the energy cut defined in the manager):
///
///    odds:
///      class: "TRestDataSetLogOdds"
///      referenceFileName: "Dataset_Calibration_Calibrated.root"
///      cuts: ["energy"]
///      observables:
///        - {observable: "trackAnalysis_MaxTrack_XYZ_SigmaZ2", range: [0, 20], nBins: 100}
///        - {observable: "trackAnalysis_MaxTrackEnergyBalanceXY", range: [-5, 5], nBins: 100}
///
/// Differences with legacy (TRestDataSetOdds): a process of a dataset pipeline (no ComputeLogOdds nor
/// dataSetName/outputFileName); oddsFile replaced by referenceFileName (a dataset processed by this process
/// reuses its PDFs, stored in its configuration, otherwise they are built); the cut is TRestCut of the
/// manager by name (`cuts`) instead of a TRestCut inside; `observables` with `observable` instead of the
/// `observable` elements with `name`.

static const bool TRestDataSetLogOddsObservable_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestDataSetLogOddsObservable>("observable",
                                                     &TRestDataSetLogOddsObservable::fObservable);
    reg.RegisterField<TRestDataSetLogOddsObservable>("range", &TRestDataSetLogOddsObservable::fRange);
    reg.RegisterField<TRestDataSetLogOddsObservable>("nBins", &TRestDataSetLogOddsObservable::fNBins);
    reg.RegisterField<TRestDataSetLogOddsObservable>("pdf", &TRestDataSetLogOddsObservable::fPDF);
    return true;
}();

static const bool TRestDataSetLogOdds_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestDataSetLogOdds>("observables", &TRestDataSetLogOdds::fObservables);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDataSetLogOdds", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDataSetLogOdds>(instanceName, params);
        });
    return true;
}();

/// log(1 - p) - log(p), finite and decreasing: 1000 for p <= 0, -1000 for p >= 1 (all the entries of the
/// reference in one bin), instead of +-infinity
double LogOdds(double probability) {
    if (probability <= 0) return 1000.;
    if (probability >= 1) return -1000.;
    return std::log(1. - probability) - std::log(probability);
}
}  // namespace

TRestDataSetLogOddsObservable::TRestDataSetLogOddsObservable() : TRestMetadata() {
    fName = "TRestDataSetLogOddsObservable";
}

TRestDataSetLogOddsObservable::TRestDataSetLogOddsObservable(const std::string& name, const YAML::Node& node)
    : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestDataSetLogOddsObservable::LoadConfig() {
    UpdateParamsFromYAML<TRestDataSetLogOddsObservable>(fNode);
    if (fObservable.empty()) throw std::runtime_error("TRestDataSetLogOddsObservable: no observable");
    const std::string context = "TRestDataSetLogOddsObservable '" + fObservable + "'";
    if (!(fRange.first < fRange.second)) throw std::runtime_error(context + ": range [min, max] is needed");
    if (fNBins <= 0) throw std::runtime_error(context + ": nBins must be positive");
    if (!fPDF.empty() && static_cast<int>(fPDF.size()) != fNBins)
        throw std::runtime_error(context + ": the pdf has not nBins values");
    UpdateYAMLFromParams<TRestDataSetLogOddsObservable>(fNode);
}

double TRestDataSetLogOddsObservable::GetProbability(double value) const {
    if (fPDF.empty() || !(value >= fRange.first && value < fRange.second)) return 0;
    const auto bin = static_cast<size_t>((value - fRange.first) / (fRange.second - fRange.first) * fNBins);
    return bin < fPDF.size() ? fPDF[bin] : 0;
}

std::unique_ptr<TH1D> TRestDataSetLogOddsObservable::GetHistogram(const std::string& name) const {
    auto histogram = std::make_unique<TH1D>(name.c_str(), (";" + fObservable + ";Probability").c_str(),
                                            fNBins, fRange.first, fRange.second);
    histogram->SetDirectory(nullptr);
    for (size_t i = 0; i < fPDF.size(); i++) histogram->SetBinContent(i + 1, fPDF[i]);
    return histogram;
}

TRestDataSetLogOdds::TRestDataSetLogOdds() : TRestDataSetProcess() { fName = "TRestDataSetLogOdds"; }

TRestDataSetLogOdds::TRestDataSetLogOdds(const std::string& instanceName, const YAML::Node& node)
    : TRestDataSetProcess(instanceName, node) {
    LoadConfig();
}

void TRestDataSetLogOdds::LoadConfig() {
    UpdateParamsFromYAML<TRestDataSetLogOdds>(fNode);
    if (fObservables.empty()) throw std::runtime_error("TRestDataSetLogOdds '" + fName + "': no observables");
    UpdateYAMLFromParams<TRestDataSetLogOdds>(fNode);
}

void TRestDataSetLogOdds::ComputeParameters(const TRestDataSet&, ROOT::RDF::RNode frame) {
    // All the histograms booked before the event loop
    std::vector<ROOT::RDF::RResultPtr<TH1D>> histograms;
    for (const auto& observable : fObservables) {
        histograms.push_back(frame.Histo1D({("pdf_" + observable.fObservable).c_str(), "", observable.fNBins,
                                            observable.fRange.first, observable.fRange.second},
                                           observable.fObservable));
    }
    for (size_t i = 0; i < fObservables.size(); i++) {
        auto& observable = fObservables[i];
        const TH1D& histogram = *histograms[i];
        const double integral = histogram.Integral();
        if (integral <= 0) {
            throw std::runtime_error("TRestDataSetLogOdds '" + fName + "': no entries of " +
                                     observable.fObservable + " in its range");
        }
        observable.fPDF.assign(observable.fNBins, 0);
        for (int bin = 0; bin < observable.fNBins; bin++)
            observable.fPDF[bin] = histogram.GetBinContent(bin + 1) / integral;
        RESTDebug << "TRestDataSetLogOdds '" << fName << "': PDF of " << observable.fObservable << " from "
                  << integral << " entries" << RESTendl;
    }
    UpdateYAMLFromParams<TRestDataSetLogOdds>(fNode);
}

void TRestDataSetLogOdds::Apply(TRestDataSet& dataSet) const {
    ROOT::RDF::RNode frame = dataSet.GetDataFrame();
    std::vector<std::string> columns;
    std::string total;
    for (const auto& observable : fObservables) {
        const std::string column = GetColumn(observable.fObservable);
        // The observable copied: the data frame can be used after this process
        auto logOdds = [observable](double value) { return LogOdds(observable.GetProbability(value)); };
        const std::string input = AsDouble(frame, observable.fObservable);
        frame = DefineColumn(frame, column, logOdds, {input});
        columns.push_back(column);
        if (!total.empty()) total += " + ";
        total += column;
    }
    frame = DefineColumn(frame, GetTotalColumn(), total);
    columns.push_back(GetTotalColumn());
    dataSet.SetDataFrame(frame, columns);
}

void TRestDataSetLogOdds::WriteObjects(TDirectory* directory) const {
    for (const auto& observable : fObservables) {
        const std::string name = "pdf_" + observable.fObservable;
        directory->WriteObject(observable.GetHistogram(name).get(), name.c_str());
    }
}
