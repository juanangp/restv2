#ifndef REST_CORE_TEST_UTILS_H
#define REST_CORE_TEST_UTILS_H

/// Helpers shared by the unit tests of source/core: a clean work directory per test (under TESTS_WORK_DIR),
/// text files, and small REST run files written with TRestRun (the input of the TRestRun and TRestDataSet
/// tests).

#include <TRestRun.h>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace TestUtils {

/// Empty directory of the current test, TESTS_WORK_DIR/<suite>_<test>, created again (cleared) on each call
inline std::filesystem::path WorkDir() {
    const auto* test = ::testing::UnitTest::GetInstance()->current_test_info();
    const std::filesystem::path directory =
        std::filesystem::path(TESTS_WORK_DIR) / (std::string(test->test_suite_name()) + "_" + test->name());
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    return directory;
}

/// Writes a text file (its directory created if needed)
inline void WriteFile(const std::filesystem::path& path, const std::string& text) {
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out << text;
}

/// Description of a run file written by WriteRunFile
struct RunFile {
    int runNumber = 1;
    std::string runTag = "Test";
    std::string meshVoltage = "300 V";
    /// Value of the observable ana_energy of each entry (one entry each)
    std::vector<double> energies = {10, 20, 30, 40, 50};
    /// Start time (unix seconds); the run lasts 1800 s
    double startTime = 1.7e9;
};

/// Writes a REST run file R<runNumber>_<runTag>.root in `dir` with TRestRun, as restManager does: an
/// AnalysisTree with the event info observables (eventID = entry, subEventID = 0, timestamp) and the
/// observable ana_energy (double), and the TRestRun in RESTMetadataStore (instance "run"). Extra metadata
/// are written with it. Returns the file name.
inline std::string WriteRunFile(const std::filesystem::path& dir, const RunFile& description,
                                const std::vector<TRestMetadata*>& metadata = {}) {
    std::filesystem::create_directories(dir);
    YAML::Node node(YAML::NodeType::Map);
    node["class"] = "TRestRun";
    node["runNumber"] = description.runNumber;
    node["runTag"] = description.runTag;
    node["runType"] = "Test";
    node["runInfo"]["meshVoltage"] = description.meshVoltage;
    node["startTime"] = description.startTime;
    node["endTime"] = description.startTime + 1800;
    node["outputFileName"] = (dir / "R[runNumber]_[runTag].root").string();

    TRestRun run("run", node);
    run.OpenOutputFile();
    int eventID = 0, subEventID = 0, timeNanoSecs = 0;
    long long timeSeconds = 0;
    double energy = 0;
    run.SetObservable("eventID", eventID);
    run.SetObservable("subEventID", subEventID);
    run.SetObservable("timeSeconds", timeSeconds);
    run.SetObservable("timeNanoSecs", timeNanoSecs);
    run.SetObservable("ana_energy", energy);
    for (size_t i = 0; i < description.energies.size(); i++) {
        eventID = static_cast<int>(i);
        timeSeconds = static_cast<long long>(description.startTime) + static_cast<long long>(i);
        energy = description.energies[i];
        run.Fill();
    }
    for (auto* meta : metadata) run.AddMetadata(meta);
    run.AddMetadata(&run);
    const std::string fileName = run.GetOutputFileName();
    run.CloseFiles();
    return fileName;
}

}  // namespace TestUtils

#endif
