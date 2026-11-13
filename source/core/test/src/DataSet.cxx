/// Unit tests of TRestDataSet: generation from small run files written with TRestRun (selection of the runs
/// with runConditions, TRestCut on the entries, observables, new columns with run parameters), writing and
/// opening the dataset file, the file name patterns of the runs ([runRange], [runTag|Mixed]...), merging
/// datasets, and the errors (repeated runs, missing cuts or files). The files are written in a work
/// directory per test.

#include <TRestCut.h>
#include <TRestDataSet.h>
#include <TRestRunInfo.h>
#include <TRestTools.h>
#include <TRestYAMLUtils.h>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <ROOT/RDataFrame.hxx>
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "TestUtils.h"

namespace fs = std::filesystem;

namespace {
/// Three runs in dir/runs: R00001_Calib (300 V), R00002_Calib and R00003_Calib (400 V), each with the
/// entries ana_energy = 10, 20, 30, 40, 50 and one hour apart
void WriteRuns(const fs::path& dir) {
    for (int number = 1; number <= 3; number++) {
        TestUtils::RunFile run;
        run.runNumber = number;
        run.runTag = "Calib";
        run.meshVoltage = number == 1 ? "300 V" : "400 V";
        run.startTime = 1.7e9 + 3600 * number;
        TestUtils::WriteRunFile(dir / "runs", run);
    }
}

/// Configuration of a dataset of the runs of dir/runs
YAML::Node DataSetConfig(const fs::path& dir, const std::string& extra = "") {
    YAML::Node node = YAML::Load("class: TRestDataSet\n" + extra);
    node["filePattern"] = (dir / "runs" / "R*_Calib.root").string();
    return node;
}

/// Values of a column of the dataset
template <typename T>
std::vector<T> Column(const TRestDataSet& dataSet, const std::string& name) {
    auto values = dataSet.GetDataFrame().Take<T>(name);
    std::vector<T> result(values->begin(), values->end());
    std::sort(result.begin(), result.end());
    return result;
}
}  // namespace

TEST(DataSet, GenerateAllRuns) {
    const fs::path dir = TestUtils::WorkDir();
    WriteRuns(dir);
    TRestDataSet dataSet("dataSet", DataSetConfig(dir));
    EXPECT_FALSE(dataSet.HasDataFrame());
    dataSet.Generate();
    ASSERT_TRUE(dataSet.HasDataFrame());

    const auto& runs = dataSet.GetRuns();
    ASSERT_EQ(runs.size(), 3u);
    // In the order of the files
    EXPECT_EQ(runs[0].run->GetRunNumber(), 1);
    EXPECT_EQ(runs[2].run->GetRunNumber(), 3);
    EXPECT_EQ(runs[1].entries, 5);
    EXPECT_EQ(fs::path(runs[1].fileName).filename(), "R00002_Calib.root");
    EXPECT_EQ(*dataSet.GetDataFrame().Count(), 15u);

    // Always written: the event info and runIndex; all the observables without `observables`
    const auto& columns = dataSet.GetColumns();
    for (const std::string column :
         {"eventID", "subEventID", "timeSeconds", "timeNanoSecs", "runIndex", "ana_energy"}) {
        EXPECT_NE(std::find(columns.begin(), columns.end(), column), columns.end()) << column;
    }
    EXPECT_EQ(Column<int>(dataSet, "runIndex"),
              (std::vector<int>{0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2}));

    // The runs: parameters, times and file name patterns
    const auto voltages = dataSet.GetRunParameter("runInfo.meshVoltage");
    ASSERT_EQ(voltages.size(), 3u);
    EXPECT_EQ(voltages[0], "300 V");
    EXPECT_EQ(voltages[2], "400 V");
    EXPECT_FALSE(dataSet.GetRunParameter("runInfo.missing")[0].has_value());
    EXPECT_DOUBLE_EQ(dataSet.GetTotalTime(), 3 * 1800.);
    EXPECT_DOUBLE_EQ(dataSet.GetStartTime(), 1.7e9 + 3600);
    EXPECT_DOUBLE_EQ(dataSet.GetEndTime(), 1.7e9 + 3 * 3600 + 1800);

    EXPECT_EQ(dataSet.ResolveFilePattern("D_[runStart]_[runEnd]_[runTag].root"), "D_00001_00003_Calib.root");
    EXPECT_EQ(dataSet.ResolveFilePattern("D_[runRange].root"), "D_00001_00003.root");
    EXPECT_EQ(dataSet.ResolveFilePattern("D_[runInfo.meshVoltage|Mixed].root"), "D_Mixed.root");
    EXPECT_THROW(dataSet.ResolveFilePattern("D_[runInfo.meshVoltage].root"), std::runtime_error);
}

TEST(DataSet, SelectionCutsColumnsAndWrite) {
    const fs::path dir = TestUtils::WorkDir();
    WriteRuns(dir);
    TRestCut highEnergy("highEnergy", YAML::Load("class: TRestCut\nconditions: {e: \"ana_energy > 25\"}\n"));
    TRestDataSet dataSet("dataSet", DataSetConfig(dir,
                                                  "observables: [\"ana_*\"]\n"
                                                  "runConditions:\n"
                                                  "  mesh: \"runInfo.meshVoltage > 0.35 kV\"\n"
                                                  "  tag: 'runTag == \"Calib\"'\n"
                                                  "cuts: [highEnergy]\n"
                                                  "columns:\n"
                                                  "  energyPerVolt: \"ana_energy / runInfo.meshVoltage\"\n"
                                                  "  doubled: \"2 * energyPerVolt\"\n"));
    dataSet.Generate({&highEnergy});

    // Runs 2 and 3 (400 V), the entries with ana_energy > 25
    ASSERT_EQ(dataSet.GetRuns().size(), 2u);
    EXPECT_EQ(dataSet.GetRuns()[0].run->GetRunNumber(), 2);
    EXPECT_EQ(dataSet.GetRuns()[1].run->GetRunNumber(), 3);
    EXPECT_EQ(*dataSet.GetDataFrame().Count(), 6u);
    EXPECT_EQ(Column<double>(dataSet, "ana_energy"), (std::vector<double>{30, 30, 40, 40, 50, 50}));
    // The run parameter of the run of each entry, in REST units
    const auto perVolt = Column<double>(dataSet, "energyPerVolt");
    ASSERT_EQ(perVolt.size(), 6u);
    EXPECT_NEAR(perVolt.front(), 30. / 400, 1e-12);
    EXPECT_NEAR(perVolt.back(), 50. / 400, 1e-12);
    EXPECT_NEAR(Column<double>(dataSet, "doubled").back(), 2 * 50. / 400, 1e-12);
    EXPECT_EQ(Column<int>(dataSet, "runIndex"), (std::vector<int>{0, 0, 0, 1, 1, 1}));

    // A column added from restRoot
    dataSet.Define("energyKeV", "ana_energy * 1000");
    const auto& columns = dataSet.GetColumns();
    EXPECT_NE(std::find(columns.begin(), columns.end(), "energyKeV"), columns.end());

    // Written: the name from the runs, the selected entries of each run
    dataSet.Write((dir / "D_[runRange]_[runTag].root").string());
    const fs::path fileName = dir / "D_00002_00003_Calib.root";
    ASSERT_TRUE(fs::exists(fileName));
    EXPECT_EQ(dataSet.GetRuns()[0].selectedEntries, 3);
    EXPECT_EQ(dataSet.GetRuns()[1].selectedEntries, 3);
    EXPECT_EQ(fs::path(dataSet.fOutputFileName), fileName);
    EXPECT_TRUE(TRestTools::isDataSet(fileName.string()));
    EXPECT_FALSE(TRestTools::isValidTRestRun(fileName.string()));

    // Its metadata: the dataset (with its input and output) and the cut applied
    const YAML::Node store = TRestYAMLUtils::ReadMetadataStore(fileName.string());
    EXPECT_EQ(store["dataSet"]["class"].as<std::string>(), "TRestDataSet");
    EXPECT_EQ(store["highEnergy"]["class"].as<std::string>(), "TRestCut");
    EXPECT_FALSE(store["run"].IsDefined());  // the runs are in RESTDataSetInfo

    // Opened as it is
    TRestDataSet opened(fileName.string());
    ASSERT_TRUE(opened.HasDataFrame());
    ASSERT_EQ(opened.GetRuns().size(), 2u);
    EXPECT_EQ(opened.GetRuns()[1].run->GetRunNumber(), 3);
    EXPECT_EQ(opened.GetRuns()[1].run->GetParameter("runInfo.meshVoltage"), "400 V");
    EXPECT_EQ(opened.GetRuns()[0].selectedEntries, 3);
    EXPECT_EQ(opened.GetRuns()[0].entries, 5);
    EXPECT_EQ(*opened.GetDataFrame().Count(), 6u);
    const auto openedColumns = opened.GetColumns();
    for (const std::string column :
         {"runIndex", "timeSeconds", "timeNanoSecs", "energyPerVolt", "doubled", "energyKeV", "ana_energy"}) {
        EXPECT_NE(std::find(openedColumns.begin(), openedColumns.end(), column), openedColumns.end())
            << column;
    }
    EXPECT_EQ(opened.GetConfig()["class"].as<std::string>(), "TRestDataSet");
    EXPECT_THROW(TRestDataSet((dir / "runs" / "R00001_Calib.root").string()), std::runtime_error);
}

TEST(DataSet, MergeDataSets) {
    const fs::path dir = TestUtils::WorkDir();
    WriteRuns(dir);
    fs::create_directories(dir / "datasets");
    TRestDataSet first("first", DataSetConfig(dir, "runConditions: {number: \"runNumber == 1\"}\n"));
    first.Generate();
    first.Write((dir / "datasets" / "D_[runRange].root").string());
    TRestDataSet second("second", DataSetConfig(dir, "runConditions: {number: \"runNumber > 1\"}\n"));
    second.Generate();
    second.Write((dir / "datasets" / "D_[runRange].root").string());
    EXPECT_TRUE(fs::exists(dir / "datasets" / "D_00001.root"));
    EXPECT_TRUE(fs::exists(dir / "datasets" / "D_00002_00003.root"));

    YAML::Node node = YAML::Load("class: TRestDataSet\n");
    node["filePattern"] = (dir / "datasets" / "D_*.root").string();
    TRestDataSet merged("merged", node);
    merged.Generate();
    // The runs joined, runIndex renumbered
    ASSERT_EQ(merged.GetRuns().size(), 3u);
    EXPECT_EQ(merged.GetRuns()[0].run->GetRunNumber(), 1);
    EXPECT_EQ(merged.GetRuns()[2].run->GetRunNumber(), 3);
    EXPECT_EQ(*merged.GetDataFrame().Count(), 15u);
    EXPECT_EQ(Column<int>(merged, "runIndex").back(), 2);
    // The file of each run is the run file
    EXPECT_EQ(fs::path(merged.GetRuns()[2].fileName).filename(), "R00003_Calib.root");

    // The same run twice (a dataset and its runs) cannot be merged
    TestUtils::WriteRunFile(dir / "datasets", TestUtils::RunFile());  // R00001_Test.root, not a dataset
    YAML::Node mixed = YAML::Load("class: TRestDataSet\n");
    mixed["filePattern"] = (dir / "datasets" / "*.root").string();
    TRestDataSet mixedDataSet("mixed", mixed);
    EXPECT_THROW(mixedDataSet.Generate(), std::runtime_error);  // run files and datasets
}

TEST(DataSet, Errors) {
    const fs::path dir = TestUtils::WorkDir();
    WriteRuns(dir);

    // A run twice: the same runNumber and subRunNumber in two files
    TestUtils::RunFile repeated;
    repeated.runNumber = 2;
    repeated.runTag = "Calib2";
    TestUtils::WriteRunFile(dir / "runs", repeated);  // R00002_Calib2.root
    YAML::Node all = YAML::Load("class: TRestDataSet\n");
    all["filePattern"] = (dir / "runs" / "R*.root").string();
    TRestDataSet twice("twice", all);
    try {
        twice.Generate();
        FAIL() << "a repeated run must throw";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("twice"), std::string::npos) << error.what();
    }

    // A cut that is not given
    TRestDataSet withCut("withCut", DataSetConfig(dir, "cuts: [missingCut]\n"));
    EXPECT_THROW(withCut.Generate(), std::runtime_error);

    // No run selected, no files, no pattern
    TRestDataSet none("none", DataSetConfig(dir, "runConditions: {number: \"runNumber > 100\"}\n"));
    EXPECT_THROW(none.Generate(), std::runtime_error);
    YAML::Node noFiles = YAML::Load("class: TRestDataSet\n");
    noFiles["filePattern"] = (dir / "runs" / "X*.root").string();
    TRestDataSet empty("empty", noFiles);
    EXPECT_THROW(empty.Generate(), std::runtime_error);
    TRestDataSet noPattern("noPattern", YAML::Load("class: TRestDataSet\n"));
    EXPECT_THROW(noPattern.Generate(), std::runtime_error);

    // Not generated
    EXPECT_THROW(noPattern.Write((dir / "out.root").string()), std::runtime_error);
    EXPECT_THROW(noPattern.Define("x", "1"), std::runtime_error);
    // An empty run condition
    EXPECT_THROW(TRestDataSet("bad", DataSetConfig(dir, "runConditions: {a: \"\"}\n")), std::runtime_error);
}
