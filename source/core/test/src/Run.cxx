/// Unit tests of TRestRun: the configuration (output file name with [parameter] patterns and mainDataPath,
/// "/dev/null" output, run parameters taken from the name of the input file with inputFormat), writing a
/// small run file (analysis tree with observables, metadata) and reading it back (run parameters,
/// entries, observables, metadata of the file), and a run configured with that file as input
/// (inheritInputRun). The files are written in a work directory per test.

#include <TFile.h>
#include <TRestCut.h>
#include <TRestRun.h>
#include <TRestTools.h>
#include <TTree.h>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <stdexcept>
#include <string>

#include "TestUtils.h"

namespace fs = std::filesystem;

TEST(Run, OutputFileName) {
    const fs::path dir = TestUtils::WorkDir();
    YAML::Node node = YAML::Load(
        "class: TRestRun\n"
        "runNumber: 7\n"
        "runTag: My Tag\n"
        "runInfo: {meshVoltage: 300 V}\n"
        "outputFileName: \"R[runNumber]_[runTag]_[runInfo.meshVoltage].root\"\n");
    node["mainDataPath"] = dir.string();
    TRestRun run("run", node);
    EXPECT_EQ(run.GetClassName(), "TRestRun");
    EXPECT_EQ(run.GetRunNumber(), 7);
    // Relative to mainDataPath, the texts without spaces
    EXPECT_EQ(fs::path(run.GetOutputFileName()), dir / "R00007_MyTag_300V.root");
    EXPECT_FALSE(run.HasNullOutput());
    EXPECT_FALSE(run.HasOutputFileOpen());  // opened by OpenOutputFile
    EXPECT_FALSE(run.IsOpen());             // no input
    // The resolved values are written to the node
    EXPECT_EQ(run.GetYAMLNode()["outputFileName"].as<std::string>(), run.GetOutputFileName());

    EXPECT_EQ(run.PrefixMainDataPath("/abs/file.root"), "/abs/file.root");
    EXPECT_EQ(run.PrefixMainDataPath("/dev/null"), "/dev/null");

    // Without mainDataPath, the output file name stays relative: to the launch directory, not to the one
    // where the configuration was read (restG4 reads it from the directory of the configuration file)
    TRestRun relative("run",
                      YAML::Load("class: TRestRun\nrunNumber: 7\noutputFileName: \"R[runNumber].root\"\n"));
    EXPECT_EQ(relative.GetOutputFileName(), "R00007.root");
}

TEST(Run, NullOutputAndErrors) {
    TRestRun null("run", YAML::Load("class: TRestRun\nrunNumber: 1\noutputFileName: Null\n"));
    EXPECT_TRUE(null.HasNullOutput());
    EXPECT_EQ(null.GetOutputFileName(), "/dev/null");
    TRestRun defaults("run", YAML::Load("class: TRestRun\nrunNumber: 1\n"));
    EXPECT_TRUE(defaults.HasNullOutput());

    EXPECT_THROW(TRestRun("run", YAML::Load("class: TRestRun\nrunTag: preserve\n")), std::runtime_error);
    EXPECT_THROW(TRestRun("run", YAML::Load("class: TRestRun\noutputFileName: \"R[unknown].root\"\n")),
                 std::runtime_error);
    // A configuration key that does not exist (as restManager creates it, by class)
    EXPECT_THROW(MetadataClassRegistry::Instance().Create(
                     "run", YAML::Load("class: TRestRun\nrunNumber: 1\noutputFile: x.root\n")),
                 std::runtime_error);
    // Not a REST run file
    EXPECT_THROW(TRestRun("/nonexistent/file.root"), std::runtime_error);
}

TEST(Run, ParametersFromInputFileName) {
    const fs::path dir = TestUtils::WorkDir();
    // Not a REST run file (e.g. a DAQ file): not opened, its name gives the run parameters
    const fs::path input = dir / "R00042_Calib_V350.dat";
    TestUtils::WriteFile(input, "raw data");
    YAML::Node node = YAML::Load(
        "class: TRestRun\n"
        "runType: given\n"
        "inputFormat: \"R[runNumber]_[runTag]_V[runInfo.meshVoltage].dat\"\n");
    node["inputFileName"] = input.string();
    TRestRun run("run", node);
    EXPECT_FALSE(run.IsOpen());
    EXPECT_EQ(run.GetRunNumber(), 42);
    EXPECT_EQ(run.GetRunTag(), "Calib");
    EXPECT_EQ(run.GetRunType(), "given");  // the configuration wins
    EXPECT_EQ(run.GetRunInformation().at("meshVoltage"), "350");

    const auto parameters = run.ParseInputFormat();
    EXPECT_EQ(parameters.at("runNumber"), "00042");
    EXPECT_EQ(parameters.at("runInfo.meshVoltage"), "350");

    // A [parameter] that is not of the run
    YAML::Node wrong = YAML::Load("class: TRestRun\ninputFormat: \"R[runNumber]_[detector].dat\"\n");
    wrong["inputFileName"] = input.string();
    EXPECT_THROW(TRestRun("run", wrong), std::runtime_error);
}

TEST(Run, WriteAndReadRunFile) {
    const fs::path dir = TestUtils::WorkDir();
    TRestCut cut("fiducial", YAML::Load("class: TRestCut\nconditions: {e: \"ana_energy > 25\"}\n"));
    TestUtils::RunFile description;
    description.runNumber = 12;
    const std::string fileName = TestUtils::WriteRunFile(dir, description, {&cut});
    EXPECT_EQ(fs::path(fileName), dir / "R00012_Test.root");
    EXPECT_TRUE(TRestTools::isRootFile(fileName));
    EXPECT_TRUE(TRestTools::isValidTRestRun(fileName));
    EXPECT_FALSE(TRestTools::isDataSet(fileName));

    TRestRun run(fileName);
    ASSERT_TRUE(run.IsOpen());
    EXPECT_EQ(run.GetName(), "run");
    // The parameters of the run of the file
    EXPECT_EQ(run.GetRunNumber(), 12);
    EXPECT_EQ(run.GetRunTag(), "Test");
    EXPECT_EQ(run.GetRunType(), "Test");
    EXPECT_EQ(run.GetRunInformation().at("meshVoltage"), "300 V");
    EXPECT_DOUBLE_EQ(run.GetStartTime(), description.startTime);
    EXPECT_DOUBLE_EQ(run.GetEndTime(), description.startTime + 1800);
    EXPECT_EQ(run.GetEntriesSaved(), 5);
    EXPECT_EQ(run.GetEntries(), 5);

    // Observables of the input analysis tree, entry by entry
    ASSERT_TRUE(run.GetEntry(2));
    EXPECT_EQ(run.GetCurrentEntry(), 2);
    EXPECT_DOUBLE_EQ(run.GetObservable<double>("ana_energy"), 30.);
    EXPECT_EQ(run.GetObservable<int>("eventID"), 2);
    EXPECT_THROW(run.GetObservable<int>("ana_energy"), std::runtime_error);  // another type
    EXPECT_THROW(run.GetObservable<double>("ana_missing"), std::runtime_error);
    ASSERT_TRUE(run.GetNextEntry());
    EXPECT_DOUBLE_EQ(run.GetObservable<double>("ana_energy"), 40.);
    EXPECT_FALSE(run.GetEntry(10));

    EXPECT_EQ(run.GetEntryWithID(4), 4);
    EXPECT_DOUBLE_EQ(run.GetObservable<double>("ana_energy"), 50.);
    EXPECT_EQ(run.GetEntryWithID(99), -1);

    // The metadata of the file
    EXPECT_EQ(run.GetMetadata("run")["runNumber"].as<int>(), 12);
    EXPECT_EQ(run.GetMetadata("fiducial")["class"].as<std::string>(), "TRestCut");
    auto* storedCut = dynamic_cast<TRestCut*>(run.GetMetadataClass("TRestCut"));
    ASSERT_NE(storedCut, nullptr);
    EXPECT_EQ(storedCut->GetName(), "fiducial");
    EXPECT_EQ(storedCut->GetRunInfo(), &run);
    EXPECT_EQ(run.GetMetadataOfType<TRestCut>().size(), 1u);
    // Its own TRestRun is not in the store
    EXPECT_EQ(run.GetMetadataClass("TRestRun"), nullptr);
    EXPECT_FALSE(run.HasEvent("TRestRawSignalEvent"));
    EXPECT_EQ(run.GetEvent(), nullptr);

    run.CloseFiles();
    EXPECT_FALSE(run.IsOpen());
}

TEST(Run, ObservablesWhileProcessing) {
    const fs::path dir = TestUtils::WorkDir();
    YAML::Node node = YAML::Load("class: TRestRun\nrunNumber: 3\nrunTag: Obs\n");
    node["outputFileName"] = (dir / "R[runNumber]_[runTag].root").string();
    TRestRun run("run", node);
    double unbound = 0;
    EXPECT_THROW(run.SetObservable("x", unbound), std::runtime_error);  // no output file yet

    run.OpenOutputFile();
    EXPECT_TRUE(run.HasOutputFileOpen());
    double energy = 0;
    std::string label = "none";
    run.SetObservable("proc_energy", energy);
    run.SetObservable("proc_label", label);
    // The reference is bound to the variable of the process
    const double& bound = run.GetObservable<double>("proc_energy");
    energy = 12.5;
    EXPECT_DOUBLE_EQ(bound, 12.5);
    label = "text";
    EXPECT_EQ(run.GetObservable<std::string>("proc_label"), "text");
    run.Fill();
    energy = 7;
    run.Fill();
    EXPECT_EQ(run.GetSavedEntries(), 2);
    EXPECT_EQ(run.GetEntriesSaved(), 2);
    const std::string fileName = run.GetOutputFileName();
    run.CloseFiles();

    TFile file(fileName.c_str(), "READ");
    auto* tree = file.Get<TTree>("AnalysisTree");
    ASSERT_NE(tree, nullptr);
    EXPECT_EQ(tree->GetEntries(), 2);
    EXPECT_NE(tree->GetBranch("proc_label"), nullptr);
}

TEST(Run, InputRunInherited) {
    const fs::path dir = TestUtils::WorkDir();
    TestUtils::RunFile description;
    description.runNumber = 21;
    description.runTag = "Background";
    const std::string input = TestUtils::WriteRunFile(dir, description);

    // inheritInputRun: the parameters not given from the run of the input file
    YAML::Node inherit = YAML::Load("class: TRestRun\ninheritInputRun: true\nrunTag: Processed\n");
    inherit["inputFileName"] = input;
    TRestRun inherited("run", inherit);
    EXPECT_TRUE(inherited.IsOpen());
    EXPECT_EQ(inherited.GetRunNumber(), 21);
    EXPECT_EQ(inherited.GetRunTag(), "Processed");
    EXPECT_EQ(inherited.GetEntries(), 5);
    EXPECT_EQ(inherited.GetRunInformation().at("meshVoltage"), "300 V");
    EXPECT_DOUBLE_EQ(inherited.GetStartTime(), description.startTime);  // always from the input run

    // Without it the number is not taken, the times and runInfo are
    YAML::Node plain = YAML::Load("class: TRestRun\n");
    plain["inputFileName"] = input;
    TRestRun notInherited("run", plain);
    EXPECT_EQ(notInherited.GetRunNumber(), -1);
    EXPECT_EQ(notInherited.GetRunInformation().at("meshVoltage"), "300 V");
    EXPECT_DOUBLE_EQ(notInherited.GetEndTime(), description.startTime + 1800);

    // The output cannot be the input
    YAML::Node same = YAML::Load("class: TRestRun\n");
    same["inputFileName"] = input;
    same["outputFileName"] = input;
    EXPECT_THROW(TRestRun("run", same), std::runtime_error);
}
