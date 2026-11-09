/// Unit tests of TRestDataSetCombine (GoogleTest, BUILD_TESTING): two dataset files of the same runs written
/// with synthetic columns (as the Micromegas and the vetoes processed apart), combined by run, eventID and
/// subEventID; the entries without match, the filter, the order of the runs and the errors.

#include <TFile.h>
#include <TObjString.h>
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
#include "TRestDataSetCombine.h"

namespace fs = std::filesystem;

namespace {

/// An entry of a synthetic dataset: run index, eventID, a double and an int column
struct Row {
    int runIndex;
    int eventID;
    double value;
    int count;
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

/// Writes a dataset with runIndex, eventID, subEventID (0), the columns `<prefix>Value` (double) and
/// `<prefix>Count` (int), and the runs (runNumber of each runIndex) in RESTDataSetInfo
std::string WriteDataSet(const fs::path& path, const std::vector<int>& runNumbers,
                         const std::vector<Row>& rows, const std::string& prefix) {
    std::unique_ptr<TFile> file(TFile::Open(path.string().c_str(), "RECREATE"));
    if (!file || file->IsZombie()) throw std::runtime_error("cannot create " + path.string());
    auto* tree = new TTree("AnalysisTree", "AnalysisTree");  // owned by the file
    int runIndex = 0, eventID = 0, subEventID = 0, count = 0;
    double value = 0;
    tree->Branch("runIndex", &runIndex, "runIndex/I");
    tree->Branch("eventID", &eventID, "eventID/I");
    tree->Branch("subEventID", &subEventID, "subEventID/I");
    tree->Branch((prefix + "Value").c_str(), &value, (prefix + "Value/D").c_str());
    tree->Branch((prefix + "Count").c_str(), &count, (prefix + "Count/I").c_str());
    for (const auto& row : rows) {
        runIndex = row.runIndex;
        eventID = row.eventID;
        value = row.value;
        count = row.count;
        tree->Fill();
    }
    tree->Write();

    std::string runs;
    for (int number : runNumbers) {
        runs += "- run: {runNumber: " + std::to_string(number) +
                ", runTag: Synthetic}\n  fileName: synthetic.root\n  entries: 0\n  selectedEntries: 0\n"
                "  processing: 0\n";
    }
    TObjString text(runs.c_str());
    file->WriteTObject(&text, "RESTDataSetInfo");
    file->Close();
    return path.string();
}

/// The Micromegas: run 1 (index 0) with the events 0-9 and run 2 (index 1) with the events 0-4
std::vector<Row> MicromegasRows() {
    std::vector<Row> rows;
    for (int event = 0; event < 10; event++) rows.push_back({0, event, 10.0 * event, event});
    for (int event = 0; event < 5; event++) rows.push_back({1, event, 100.0 + event, event});
    return rows;
}

/// The vetoes, runs in the other order (run 2 is the index 0): the even events of run 1 up to 12 (10 and 12
/// are not in the Micromegas) and the event 3 of run 2
std::vector<Row> VetoRows() {
    std::vector<Row> rows;
    for (int event = 0; event <= 12; event += 2) rows.push_back({1, event, event + 0.5, 1});
    rows.push_back({0, 3, 7.5, 2});
    return rows;
}

TRestDataSetCombine MakeCombine(const std::string& combineFileName, const std::string& extra = "") {
    return TRestDataSetCombine("vetoCombine", YAML::Load("{verbose: warning, combineFileName: \"" +
                                                         combineFileName + "\"" + extra + "}"));
}

}  // namespace

TEST(DataSetCombine, KeepsTheUnmatchedEntries) {
    const auto directory = TestDirectory();
    TRestDataSet micromegas(WriteDataSet(directory / "Dataset_MM.root", {1, 2}, MicromegasRows(), "mm"));
    const std::string veto = WriteDataSet(directory / "Dataset_Veto.root", {2, 1}, VetoRows(), "veto");

    auto combine = MakeCombine(veto);
    combine.ObtainParameters(micromegas, nullptr);
    combine.Apply(micromegas);

    // All the entries of the Micromegas, with the columns of the vetoes where they match
    auto frame = micromegas.GetDataFrame();
    const auto runs = *frame.Take<int>("runIndex");
    const auto events = *frame.Take<int>("eventID");
    const auto matched = *frame.Take<int>("vetoCombine_matched");
    const auto values = *frame.Take<double>("vetoValue");
    const auto counts = *frame.Take<int>("vetoCount");
    ASSERT_EQ(runs.size(), 15u);
    for (size_t n = 0; n < runs.size(); n++) {
        const bool expected = (runs[n] == 0 && events[n] % 2 == 0) || (runs[n] == 1 && events[n] == 3);
        EXPECT_EQ(matched[n], expected ? 1 : 0) << "run " << runs[n] << " event " << events[n];
        if (!expected) {
            EXPECT_TRUE(std::isnan(values[n]));
            EXPECT_EQ(counts[n], 0);
        } else if (runs[n] == 0) {
            EXPECT_DOUBLE_EQ(values[n], events[n] + 0.5);
            EXPECT_EQ(counts[n], 1);
        } else {
            // The run is matched by its number, not by its index
            EXPECT_DOUBLE_EQ(values[n], 7.5);
            EXPECT_EQ(counts[n], 2);
        }
    }

    // The columns combined and matched are written, the entry index is not
    const auto& columns = micromegas.GetColumns();
    for (const std::string name : {"vetoValue", "vetoCount", "vetoCombine_matched"})
        EXPECT_NE(std::find(columns.begin(), columns.end(), name), columns.end()) << name;
    EXPECT_EQ(std::find(columns.begin(), columns.end(), "vetoCombine_entry"), columns.end());
}

TEST(DataSetCombine, OnlyTheMatchedEntries) {
    const auto directory = TestDirectory();
    TRestDataSet micromegas(WriteDataSet(directory / "Dataset_MM.root", {1, 2}, MicromegasRows(), "mm"));
    const std::string veto = WriteDataSet(directory / "Dataset_Veto.root", {2, 1}, VetoRows(), "veto");

    auto combine = MakeCombine(veto, ", keepUnmatched: false, observables: [\"vetoValue\"], prefix: \"j_\"");
    combine.ObtainParameters(micromegas, nullptr);
    combine.Apply(micromegas);

    // Events 0, 2, 4, 6, 8 of run 1 and 3 of run 2
    auto frame = micromegas.GetDataFrame();
    EXPECT_EQ(*frame.Count(), 6u);
    for (double value : *frame.Take<double>("j_vetoValue")) EXPECT_FALSE(std::isnan(value));
    const auto names = frame.GetColumnNames();
    EXPECT_EQ(std::find(names.begin(), names.end(), "j_vetoCount"), names.end());  // not selected
}

TEST(DataSetCombine, Errors) {
    const auto directory = TestDirectory();
    const std::string mm = WriteDataSet(directory / "Dataset_MM.root", {1, 2}, MicromegasRows(), "mm");

    // A column that already exists (the same names in both): a prefix is needed
    {
        TRestDataSet micromegas(mm);
        const std::string same = WriteDataSet(directory / "Dataset_Same.root", {1}, {{0, 1, 1, 1}}, "mm");
        auto combine = MakeCombine(same);
        EXPECT_THROW(combine.Apply(micromegas), std::runtime_error);
        auto prefixed = MakeCombine(same, ", prefix: \"other_\"");
        EXPECT_NO_THROW(prefixed.Apply(micromegas));
    }
    // An event twice
    {
        TRestDataSet micromegas(mm);
        const std::string twice =
            WriteDataSet(directory / "Dataset_Twice.root", {1}, {{0, 1, 1, 1}, {0, 1, 2, 2}}, "veto");
        auto combine = MakeCombine(twice);
        EXPECT_THROW(combine.Apply(micromegas), std::runtime_error);
    }
    // No column matching the observables
    {
        TRestDataSet micromegas(mm);
        const std::string veto = WriteDataSet(directory / "Dataset_Veto.root", {1}, VetoRows(), "veto");
        auto combine = MakeCombine(veto, ", observables: [\"none*\"]");
        EXPECT_THROW(combine.Apply(micromegas), std::runtime_error);
    }
    // Other runs: nothing matches
    {
        TRestDataSet micromegas(mm);
        const std::string other = WriteDataSet(directory / "Dataset_Other.root", {7}, {{0, 1, 1, 1}}, "veto");
        auto combine = MakeCombine(other);
        combine.Apply(micromegas);
        for (int matched : *micromegas.GetDataFrame().Take<int>("vetoCombine_matched")) EXPECT_EQ(matched, 0);
    }
    EXPECT_THROW(TRestDataSetCombine("combine", YAML::Load("{verbose: warning}")), std::runtime_error);
}
