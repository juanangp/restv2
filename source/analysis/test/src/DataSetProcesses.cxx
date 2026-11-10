/// Unit tests of the dataset processes of the analysis library (GoogleTest, BUILD_TESTING):
/// TRestDataSetCalibration, TRestDataSetLogOdds, TRestDataSetSignalSelection and TRestDataSetTMVA. Each
/// test writes small dataset files with synthetic columns (fixed seeds) in its own directory of the work
/// directory and opens them with TRestDataSet(fileName), as restManager does; the processes are created
/// from YAML configurations and run as TRestDataSetProcessManager runs them (ObtainParameters, then Apply).
///
/// A dataset file, as TRestDataSet::Open reads it, is an AnalysisTree with the columns and the TObjString
/// RESTDataSetInfo with the list of runs (TRestDataSet::ReadRuns); RESTMetadataStore is optional.

#include <TFile.h>
#include <TH1D.h>
#include <TObjString.h>
#include <TRandom3.h>
#include <TTree.h>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "TRestDataSet.h"
#include "TRestDataSetCalibration.h"
#include "TRestDataSetLogOdds.h"
#include "TRestDataSetSignalSelection.h"
#include "TRestDataSetTMVA.h"
#include "TRestPeakFit.h"

namespace fs = std::filesystem;

namespace {

/// A column of a synthetic dataset
struct Column {
    std::string name;
    std::vector<double> values;
};

/// Directory of the current test in the work directory, created empty
fs::path TestDirectory() {
    const auto* test = ::testing::UnitTest::GetInstance()->current_test_info();
    const fs::path directory =
        fs::path(TESTS_WORK_DIR) / (std::string(test->test_suite_name()) + "_" + test->name());
    fs::remove_all(directory);
    fs::create_directories(directory);
    return directory;
}

/// Writes a dataset file with the columns (doubles, all of the same size), plus runIndex (0) and eventID
/// (the entry, an int), and a single run in RESTDataSetInfo. Returns its path.
std::string WriteDataSet(const fs::path& path, const std::vector<Column>& columns) {
    const size_t entries = columns.empty() ? 0 : columns.front().values.size();
    std::unique_ptr<TFile> file(TFile::Open(path.string().c_str(), "RECREATE"));
    if (!file || file->IsZombie()) throw std::runtime_error("cannot create " + path.string());

    // The tree belongs to the file (deleted when it is closed)
    auto* tree = new TTree("AnalysisTree", "AnalysisTree");
    int runIndex = 0, eventID = 0;
    std::vector<double> row(columns.size());
    tree->Branch("runIndex", &runIndex, "runIndex/I");
    tree->Branch("eventID", &eventID, "eventID/I");
    for (size_t i = 0; i < columns.size(); i++)
        tree->Branch(columns[i].name.c_str(), &row[i], (columns[i].name + "/D").c_str());
    for (size_t entry = 0; entry < entries; entry++) {
        eventID = static_cast<int>(entry);
        for (size_t i = 0; i < columns.size(); i++) row[i] = columns[i].values.at(entry);
        tree->Fill();
    }
    tree->Write();

    const std::string runs =
        "- run: {runNumber: 1, runTag: Synthetic}\n  fileName: synthetic.root\n  entries: " +
        std::to_string(entries) + "\n  selectedEntries: " + std::to_string(entries) + "\n  processing: 0\n";
    TObjString text(runs.c_str());
    file->WriteTObject(&text, "RESTDataSetInfo");
    file->Close();
    return path.string();
}

/// Values of a double column of the data frame of a dataset
std::vector<double> Values(const TRestDataSet& dataSet, const std::string& column) {
    return *dataSet.GetDataFrame().Take<double>(column);
}

bool HasColumn(const TRestDataSet& dataSet, const std::string& column) {
    const auto& columns = dataSet.GetColumns();
    return std::find(columns.begin(), columns.end(), column) != columns.end();
}

bool SameFile(const std::string& a, const std::string& b) {
    std::error_code error;
    return fs::equivalent(a, b, error);
}

// ---------------------------------------------------------------------------
// TRestDataSetCalibration
// ---------------------------------------------------------------------------

/// Charge of a calibration with a gain of 1000 per keV: 20000 entries of the peak of 5.9 keV (sigma 200)
/// and 10000 of the peak of 8 keV (sigma 250), in a random order
std::vector<double> CalibrationCharges() {
    TRandom3 random(2024);
    std::vector<double> charges;
    for (int i = 0; i < 30000; i++)
        charges.push_back(i % 3 == 2 ? random.Gaus(8000, 250) : random.Gaus(5900, 200));
    return charges;
}

const std::string kCalibrationConfig = R"(
class: TRestDataSetCalibration
verbose: warning
observable: charge
nBins: 600
range: [0, 12000]
peaks:
  - {energy: "5.9 keV"}
  - {energy: "8.0 keV"}
)";

/// The calibration dataset of the test directory
std::string WriteCalibrationDataSet(const fs::path& directory) {
    return WriteDataSet(directory / "Dataset_Calibration.root", {{"charge", CalibrationCharges()}});
}

// ---------------------------------------------------------------------------
// TRestDataSetTMVA
// ---------------------------------------------------------------------------

/// Two gaussian observables x and y (sigma 1) centered at (center, center)
std::vector<Column> GaussianSample(double center, int entries, unsigned seed) {
    TRandom3 random(seed);
    std::vector<double> x, y;
    for (int i = 0; i < entries; i++) {
        x.push_back(random.Gaus(center, 1));
        y.push_back(random.Gaus(center, 1));
    }
    return {{"x", x}, {"y", y}};
}

}  // namespace

// ---------------------------------------------------------------------------
// TRestDataSetCalibration
// ---------------------------------------------------------------------------

TEST(DataSetCalibration, FitsThePeaksAndAddsTheEnergy) {
    const std::string path = WriteCalibrationDataSet(TestDirectory());
    TRestDataSet dataSet(path);

    TRestDataSetCalibration calibration("calib", YAML::Load(kCalibrationConfig));
    // Without referenceFileName the reference is the dataset processed
    calibration.ObtainParameters(dataSet, nullptr);
    EXPECT_TRUE(SameFile(calibration.fReferenceUsed, path)) << calibration.fReferenceUsed;
    EXPECT_TRUE(SameFile(calibration.fParametersFileName, path)) << calibration.fParametersFileName;

    ASSERT_EQ(calibration.fPeaks.size(), 2u);
    const auto& first = calibration.fPeaks[0];
    const auto& second = calibration.fPeaks[1];
    EXPECT_NEAR(first.fPosition, 5900, 10);
    EXPECT_NEAR(second.fPosition, 8000, 15);
    EXPECT_EQ(first.fFitStatus, 0);
    EXPECT_EQ(second.fFitStatus, 0);
    // FWHM of the gaussian relative to the mean, in %
    EXPECT_NEAR(first.fFWHM, 2.35482 * 200 / 5900 * 100, 0.5);
    EXPECT_NEAR(second.fFWHM, 2.35482 * 250 / 8000 * 100, 0.5);
    // The fit window contains the peak
    EXPECT_LT(first.fFitRange.first, 5900);
    EXPECT_GT(first.fFitRange.second, 5900);
    ASSERT_TRUE(first.fFitParameters["mean"].IsDefined());
    EXPECT_NEAR(first.fFitParameters["mean"][0].as<double>(), first.fPosition, 1e-6);

    // energy = slope * charge + intercept: 1 keV per 1000
    EXPECT_NEAR(calibration.fSlope, 1e-3, 1e-5);
    EXPECT_NEAR(calibration.fIntercept, 0, 0.05);

    // Objects: the spectrum with the peak fits and the linear fit (2 peaks and the zero point)
    ASSERT_NE(calibration.GetSpectrum(), nullptr);
    EXPECT_NEAR(calibration.GetSpectrum()->Integral(), 30000, 1);
    EXPECT_NEAR(TRestPeakFit::GetPeakMean(*calibration.GetSpectrum(), "g0"), first.fPosition, 1e-9);
    EXPECT_NEAR(TRestPeakFit::GetPeakMean(*calibration.GetSpectrum(), "g1"), second.fPosition, 1e-9);
    ASSERT_NE(calibration.GetLinearFit(), nullptr);
    EXPECT_EQ(calibration.GetLinearFit()->GetN(), 3);

    // The calibrated energy column
    calibration.Apply(dataSet);
    EXPECT_EQ(calibration.GetEnergyColumn(), "calib_Energy");
    EXPECT_TRUE(HasColumn(dataSet, "calib_Energy"));
    const double mean = *dataSet.GetDataFrame().Mean("calib_Energy");
    EXPECT_NEAR(mean, (2 * 5.9 + 8.0) / 3, 0.02);
}

TEST(DataSetCalibration, ReferenceFileName) {
    const fs::path directory = TestDirectory();
    const std::string calibrationPath = WriteCalibrationDataSet(directory);
    // A background without peaks, calibrated with the parameters of the calibration
    TRandom3 random(99);
    std::vector<double> charges;
    for (int i = 0; i < 1000; i++) charges.push_back(random.Uniform(0, 12000));
    const std::string backgroundPath =
        WriteDataSet(directory / "Dataset_Background.root", {{"charge", charges}});
    TRestDataSet background(backgroundPath);

    YAML::Node config = YAML::Load(kCalibrationConfig);
    config["referenceFileName"] = calibrationPath;
    config["calibratedObservable"] = "Charge";
    TRestDataSetCalibration calibration("calib", config);
    calibration.ObtainParameters(background, nullptr);
    EXPECT_TRUE(SameFile(calibration.fReferenceUsed, calibrationPath)) << calibration.fReferenceUsed;
    EXPECT_NEAR(calibration.fSlope, 1e-3, 1e-5);
    EXPECT_NEAR(calibration.fPeaks[0].fPosition, 5900, 10);

    calibration.Apply(background);
    const auto charge = Values(background, "charge");
    const auto energy = Values(background, "calib_Charge");
    ASSERT_EQ(energy.size(), charges.size());
    for (size_t i = 0; i < energy.size(); i++)
        EXPECT_NEAR(energy[i], calibration.fSlope * charge[i] + calibration.fIntercept, 1e-9);
}

TEST(DataSetCalibration, StoredParametersAreRead) {
    const std::string path = WriteCalibrationDataSet(TestDirectory());
    TRestDataSetCalibration calibration("calib", YAML::Load(kCalibrationConfig));
    {
        TRestDataSet dataSet(path);
        calibration.ObtainParameters(dataSet, nullptr);
    }
    const double slope = calibration.fSlope, intercept = calibration.fIntercept;
    const double position = calibration.fPeaks[0].fPosition;
    // The dataset as processed by the calibration: its configuration, results and objects in the file
    calibration.UpdateFile(path);

    // The same process on the dataset processed by it: the parameters are read, not computed
    TRestDataSet dataSet(path);
    TRestDataSetCalibration again("calib", YAML::Load(kCalibrationConfig));
    again.ObtainParameters(dataSet, nullptr);
    EXPECT_NEAR(again.fSlope, slope, 1e-12);
    EXPECT_NEAR(again.fIntercept, intercept, 1e-9);
    ASSERT_EQ(again.fPeaks.size(), 2u);
    EXPECT_NEAR(again.fPeaks[0].fPosition, position, 1e-6);
    EXPECT_NE(again.GetSpectrum(), nullptr);
    EXPECT_NE(again.GetLinearFit(), nullptr);

    // Loaded from the file
    TRestDataSetCalibration loaded;
    loaded.ReadFromFile(path, "calib");
    EXPECT_EQ(loaded.GetName(), "calib");
    EXPECT_NEAR(loaded.fSlope, slope, 1e-12);
    EXPECT_EQ(loaded.fObservable, "charge");
    EXPECT_NE(loaded.GetSpectrum(), nullptr);
    EXPECT_THROW(loaded.ReadFromFile(path, "missing"), std::runtime_error);
}

TEST(DataSetCalibration, InvalidConfiguration) {
    auto config = [](const std::string& key, const std::string& value) {
        YAML::Node node = YAML::Load(kCalibrationConfig);
        node[key] = YAML::Load(value);
        return node;
    };
    EXPECT_THROW(TRestDataSetCalibration("calib", config("observable", "\"\"")), std::runtime_error);
    EXPECT_THROW(TRestDataSetCalibration("calib", config("peaks", "[]")), std::runtime_error);
    EXPECT_THROW(TRestDataSetCalibration("calib", config("nBins", "0")), std::runtime_error);
    EXPECT_THROW(TRestDataSetCalibration("calib", config("window", "1")), std::runtime_error);
    // Unknown options (TRestTools::NormalizeOption: std::invalid_argument)
    EXPECT_THROW(TRestDataSetCalibration("calib", config("peakModel", "lorentzian")), std::invalid_argument);
    EXPECT_THROW(TRestDataSetCalibration("calib", config("background", "quadratic")), std::invalid_argument);
    EXPECT_THROW(TRestDataSetCalibration("calib", config("peaks", "[{energy: 0}]")), std::runtime_error);
}

TEST(DataSetCalibration, MissingCutThrows) {
    const std::string path = WriteCalibrationDataSet(TestDirectory());
    TRestDataSet dataSet(path);
    YAML::Node config = YAML::Load(kCalibrationConfig);
    config["cuts"] = YAML::Load("[missing]");
    TRestDataSetCalibration calibration("calib", config);
    // No manager with the TRestCut
    EXPECT_THROW(calibration.ObtainParameters(dataSet, nullptr), std::runtime_error);
}

// ---------------------------------------------------------------------------
// TRestDataSetLogOdds
// ---------------------------------------------------------------------------

namespace {
const std::string kLogOddsConfig = R"(
class: TRestDataSetLogOdds
verbose: warning
observables:
  - {observable: x, range: [0, 4], nBins: 4}
  - {observable: y, range: [0, 1], nBins: 1}
)";
}  // namespace

TEST(DataSetLogOdds, ProbabilitiesAndLogOdds) {
    // x: 50 entries in the first bin, 30 in the second, 20 in the third, 1 out of the range; y: one bin
    std::vector<double> x, y;
    for (int i = 0; i < 50; i++) x.push_back(0.5);
    for (int i = 0; i < 30; i++) x.push_back(1.5);
    for (int i = 0; i < 20; i++) x.push_back(2.5);
    x.push_back(10);
    y.assign(x.size(), 0.5);
    const std::string path = WriteDataSet(TestDirectory() / "Dataset.root", {{"x", x}, {"y", y}});
    TRestDataSet dataSet(path);

    TRestDataSetLogOdds logOdds("logOdds", YAML::Load(kLogOddsConfig));
    logOdds.ObtainParameters(dataSet, nullptr);

    // PDFs normalized in their range (the entry out of the range is not counted)
    ASSERT_EQ(logOdds.fObservables.size(), 2u);
    const auto& pdfX = logOdds.fObservables[0];
    ASSERT_EQ(pdfX.fPDF.size(), 4u);
    EXPECT_NEAR(pdfX.fPDF[0], 0.5, 1e-12);
    EXPECT_NEAR(pdfX.fPDF[1], 0.3, 1e-12);
    EXPECT_NEAR(pdfX.fPDF[2], 0.2, 1e-12);
    EXPECT_EQ(pdfX.fPDF[3], 0);
    EXPECT_NEAR(pdfX.GetProbability(1.2), 0.3, 1e-12);
    EXPECT_EQ(pdfX.GetProbability(-1), 0);
    EXPECT_EQ(pdfX.GetProbability(4), 0);  // [min, max)
    const auto histogram = pdfX.GetHistogram("pdf_x");
    ASSERT_TRUE(histogram != nullptr);
    EXPECT_EQ(histogram->GetNbinsX(), 4);
    EXPECT_NEAR(histogram->GetBinContent(2), 0.3, 1e-12);

    // log(1 - p) - log(p), 1000 for p = 0 and -1000 for p = 1, and the sum
    logOdds.Apply(dataSet);
    EXPECT_EQ(logOdds.GetColumn("x"), "logOdds_x");
    EXPECT_EQ(logOdds.GetTotalColumn(), "logOdds_total");
    EXPECT_TRUE(HasColumn(dataSet, "logOdds_x"));
    EXPECT_TRUE(HasColumn(dataSet, "logOdds_total"));
    const auto values = Values(dataSet, "x");
    const auto oddsX = Values(dataSet, "logOdds_x");
    const auto oddsY = Values(dataSet, "logOdds_y");
    const auto total = Values(dataSet, "logOdds_total");
    ASSERT_EQ(oddsX.size(), x.size());
    for (size_t i = 0; i < oddsX.size(); i++) {
        double expected = 1000;
        if (values[i] == 0.5) expected = 0;
        if (values[i] == 1.5) expected = std::log(0.7) - std::log(0.3);
        if (values[i] == 2.5) expected = std::log(0.8) - std::log(0.2);
        EXPECT_NEAR(oddsX[i], expected, 1e-9) << "x = " << values[i];
        EXPECT_EQ(oddsY[i], -1000);
        EXPECT_NEAR(total[i], oddsX[i] + oddsY[i], 1e-9);
    }
}

TEST(DataSetLogOdds, Errors) {
    EXPECT_THROW(TRestDataSetLogOdds("logOdds", YAML::Load("{observables: []}")), std::runtime_error);
    EXPECT_THROW(
        TRestDataSetLogOdds("logOdds", YAML::Load("{observables: [{observable: x, range: [1, 1]}]}")),
        std::runtime_error);
    EXPECT_THROW(TRestDataSetLogOdds("logOdds", YAML::Load("{observables: [{range: [0, 1]}]}")),
                 std::runtime_error);

    // No entries of an observable in its range
    const std::string path = WriteDataSet(TestDirectory() / "Dataset.root", {{"x", {1, 2, 3}}});
    TRestDataSet dataSet(path);
    TRestDataSetLogOdds logOdds("logOdds", YAML::Load("{observables: [{observable: x, range: [100, 200]}]}"));
    EXPECT_THROW(logOdds.ObtainParameters(dataSet, nullptr), std::runtime_error);
}

// ---------------------------------------------------------------------------
// TRestDataSetSignalSelection
// ---------------------------------------------------------------------------

TEST(DataSetSignalSelection, UpperQuantile) {
    // x = 1..100: the 75% lowest values are those up to 75
    std::vector<double> x;
    for (int i = 1; i <= 100; i++) x.push_back(i);
    const std::string path = WriteDataSet(TestDirectory() / "Dataset.root", {{"x", x}});
    TRestDataSet dataSet(path);

    TRestDataSetSignalSelection selection(
        "selection", YAML::Load("{class: TRestDataSetSignalSelection, verbose: warning, observable: x, "
                                "quantiles: [0, 0.75]}"));
    selection.ObtainParameters(dataSet, nullptr);
    EXPECT_TRUE(std::isinf(selection.fRange.first));
    EXPECT_LT(selection.fRange.first, 0);
    EXPECT_DOUBLE_EQ(selection.fRange.second, 75);
    EXPECT_DOUBLE_EQ(selection.fEfficiency, 0.75);

    // The entries in the range are kept
    selection.Apply(dataSet);
    EXPECT_EQ(*dataSet.GetDataFrame().Count(), 75u);
    for (double value : Values(dataSet, "x")) EXPECT_LE(value, 75);
}

TEST(DataSetSignalSelection, QuantileIntervalOfAnIntegerColumn) {
    // eventID (an int column) = 0..99: from the value at 25% (25) to the value at 75% (74)
    std::vector<double> x(100, 1);
    const std::string path = WriteDataSet(TestDirectory() / "Dataset.root", {{"x", x}});
    TRestDataSet dataSet(path);

    TRestDataSetSignalSelection selection(
        "selection", YAML::Load("{verbose: warning, observable: eventID, quantiles: [0.25, 0.75]}"));
    selection.ObtainParameters(dataSet, nullptr);
    EXPECT_DOUBLE_EQ(selection.fRange.first, 25);
    EXPECT_DOUBLE_EQ(selection.fRange.second, 74);
    EXPECT_DOUBLE_EQ(selection.fEfficiency, 0.5);

    selection.Apply(dataSet);
    EXPECT_EQ(*dataSet.GetDataFrame().Count(), 50u);
}

TEST(DataSetSignalSelection, InvalidConfiguration) {
    EXPECT_THROW(TRestDataSetSignalSelection("selection", YAML::Load("{quantiles: [0, 0.5]}")),
                 std::runtime_error);
    EXPECT_THROW(
        TRestDataSetSignalSelection("selection", YAML::Load("{observable: x, quantiles: [0.5, 0.5]}")),
        std::runtime_error);
    EXPECT_THROW(TRestDataSetSignalSelection("selection", YAML::Load("{observable: x, quantiles: [0, 1.5]}")),
                 std::runtime_error);
    EXPECT_THROW(
        TRestDataSetSignalSelection("selection", YAML::Load("{observable: x, quantiles: [-0.1, 1]}")),
        std::runtime_error);
}

// ---------------------------------------------------------------------------
// TRestDataSetTMVA
// ---------------------------------------------------------------------------

TEST(DataSetTMVA, BoostedDecisionTree) {
    const fs::path directory = TestDirectory();
    const std::string signalPath =
        WriteDataSet(directory / "Dataset_Signal.root", GaussianSample(1, 400, 11));
    const std::string backgroundPath =
        WriteDataSet(directory / "Dataset_Background.root", GaussianSample(-1, 400, 12));

    YAML::Node config = YAML::Load(R"(
class: TRestDataSetTMVA
verbose: warning
observables: [x, y]
method: BDT
options: "!H:!V:NTrees=20:MaxDepth=2"
)");
    config["backgroundFileName"] = backgroundPath;
    TRestDataSetTMVA tmva("tmva", config);

    TRestDataSet signal(signalPath);
    tmva.ObtainParameters(signal, nullptr);

    // Gaussians 2 sigma apart along the diagonal: area under the ROC curve about 0.98
    EXPECT_GT(tmva.fROCIntegral, 0.9);
    EXPECT_LE(tmva.fROCIntegral, 1);
    EXPECT_GT(tmva.fSeparation, 0.3);
    EXPECT_FALSE(tmva.fWeights.empty());
    ASSERT_EQ(tmva.fVariableSeparation.size(), 2u);
    for (double separation : tmva.fVariableSeparation) EXPECT_GT(separation, 0.1);
    EXPECT_EQ(tmva.fVariableImportance.size(), 2u);
    EXPECT_GE(tmva.fKSSignal, 0);
    EXPECT_GE(tmva.fKSBackground, 0);

    // The score column, higher for the signal
    EXPECT_EQ(tmva.GetColumn(), "tmva_score");
    tmva.Apply(signal);
    EXPECT_TRUE(HasColumn(signal, "tmva_score"));
    TRestDataSet background(backgroundPath);
    tmva.Apply(background);
    const auto signalScores = Values(signal, "tmva_score");
    const auto backgroundScores = Values(background, "tmva_score");
    ASSERT_EQ(signalScores.size(), 400u);
    ASSERT_EQ(backgroundScores.size(), 400u);
    double signalMean = 0, backgroundMean = 0;
    for (double score : signalScores) signalMean += score / signalScores.size();
    for (double score : backgroundScores) backgroundMean += score / backgroundScores.size();
    EXPECT_GT(signalMean, backgroundMean);
}

TEST(DataSetTMVA, InvalidConfiguration) {
    EXPECT_THROW(TRestDataSetTMVA("tmva", YAML::Load("{backgroundFileName: b.root}")), std::runtime_error);
    EXPECT_THROW(TRestDataSetTMVA("tmva", YAML::Load("{observables: [x]}")), std::runtime_error);

    // Apply without training
    TRestDataSetTMVA tmva("tmva", YAML::Load("{observables: [x], backgroundFileName: b.root}"));
    const std::string path = WriteDataSet(TestDirectory() / "Dataset.root", {{"x", {1, 2}}});
    TRestDataSet dataSet(path);
    EXPECT_THROW(tmva.Apply(dataSet), std::runtime_error);
}
