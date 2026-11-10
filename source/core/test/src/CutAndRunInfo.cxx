/// Unit tests of TRestCut (conditions on the observables: expression, evaluation on the current values of
/// the branches of an analysis tree) and TRestRunInfo (parameters of a run: GetParameter,
/// GetParameterValue with units, file name patterns, FromConfig). The trees are built in memory.

#include <TRestCut.h>
#include <TRestRunInfo.h>
#include <TTree.h>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
/// A cut from the YAML of its conditions
TRestCut MakeCut(const std::string& conditions) {
    return TRestCut("cut", YAML::Load("class: TRestCut\nconditions: " + conditions));
}

const char* kRunConfig =
    "class: TRestRun\n"
    "runNumber: 123\n"
    "subRunNumber: 2\n"
    "runType: calibration\n"
    "runTag: AlphaCAMM\n"
    "runUser: rest\n"
    "experimentName: IAXO\n"
    "runInfo:\n"
    "  meshVoltage: 300 V\n"
    "  gain: 50\n"
    "  source: Am-241\n"
    "startTime: 1700000000\n"
    "endTime: 1700003600\n"
    "entriesSaved: 1000\n";
}  // namespace

TEST(Cut, Expression) {
    TRestCut cut = MakeCut("{a: \"x > 1\", b: \"y < 2 || z == 0\"}");
    EXPECT_EQ(cut.GetClassName(), "TRestCut");
    EXPECT_EQ(cut.fConditions.size(), 2u);
    // In the order of the names
    EXPECT_EQ(cut.GetExpression(), "(x > 1) && (y < 2 || z == 0)");

    TRestCut empty;
    EXPECT_EQ(empty.GetExpression(), "");
    EXPECT_THROW(MakeCut("{a: \"\"}"), std::runtime_error);  // empty condition
}

TEST(Cut, EvaluateOnTree) {
    TTree tree("AnalysisTree", "test");
    tree.SetDirectory(nullptr);
    double energy = 0;
    int nSignals = 0;
    bool ok = true;
    tree.Branch("ana_energy", &energy);
    tree.Branch("ana_nSignals", &nSignals);
    tree.Branch("ok", &ok);

    TRestCut cut =
        MakeCut("{energy: \"ana_energy > 25 && ana_energy < 1e2\", signals: \"ana_nSignals >= 2\", ok: ok}");
    EXPECT_THROW(cut.Evaluate(), std::runtime_error);  // Compile first
    cut.Compile(&tree);

    // The current values of the branches
    energy = 30;
    nSignals = 2;
    EXPECT_TRUE(cut.Evaluate());
    energy = 20;
    EXPECT_FALSE(cut.Evaluate());
    energy = 150;
    EXPECT_FALSE(cut.Evaluate());
    energy = 99;
    nSignals = 1;
    EXPECT_FALSE(cut.Evaluate());
    nSignals = 5;
    ok = false;
    EXPECT_FALSE(cut.Evaluate());
    ok = true;
    EXPECT_TRUE(cut.Evaluate());

    // Without conditions: always true
    TRestCut empty;
    empty.Compile(&tree);
    EXPECT_TRUE(empty.Evaluate());
}

TEST(Cut, CompileErrors) {
    TTree tree("AnalysisTree", "test");
    tree.SetDirectory(nullptr);
    double energy = 0;
    std::vector<double> values;
    tree.Branch("ana_energy", &energy);
    tree.Branch("ana_values", &values);

    TRestCut cut = MakeCut("{a: \"ana_energy > 1\"}");
    EXPECT_THROW(cut.Compile(nullptr), std::runtime_error);
    // Not a scalar
    TRestCut vectorCut = MakeCut("{a: \"ana_values > 1\"}");
    EXPECT_THROW(vectorCut.Compile(&tree), std::runtime_error);
    // Not an observable of the tree
    TRestCut unknown = MakeCut("{a: \"ana_missing > 1\"}");
    EXPECT_THROW(unknown.Compile(&tree), std::runtime_error);
}

TEST(RunInfo, Parameters) {
    TRestRunInfo info("run", YAML::Load(kRunConfig));
    info.LoadConfig();
    EXPECT_EQ(info.GetRunNumber(), 123);
    EXPECT_EQ(info.GetSubRunNumber(), 2);
    EXPECT_EQ(info.GetRunType(), "calibration");
    EXPECT_EQ(info.GetRunTag(), "AlphaCAMM");
    EXPECT_EQ(info.GetRunUser(), "rest");
    EXPECT_EQ(info.GetExperimentName(), "IAXO");
    EXPECT_EQ(info.GetRunDescription(), "Null");  // not given
    EXPECT_DOUBLE_EQ(info.GetStartTime(), 1700000000.);
    EXPECT_DOUBLE_EQ(info.GetEndTime(), 1700003600.);
    EXPECT_EQ(info.GetEntriesSaved(), 1000);
    EXPECT_EQ(info.GetRunInformation().at("meshVoltage"), "300 V");

    EXPECT_EQ(info.GetParameter("runTag"), "AlphaCAMM");
    EXPECT_EQ(info.GetParameter("runNumber"), "123");
    EXPECT_EQ(info.GetParameter("runInfo.meshVoltage"), "300 V");  // as written
    EXPECT_FALSE(info.GetParameter("runInfo.missing").has_value());
    EXPECT_FALSE(info.GetParameter("runInfo").has_value());  // not a scalar

    EXPECT_DOUBLE_EQ(info.GetParameterValue("runInfo.meshVoltage").value(), 300.);
    EXPECT_DOUBLE_EQ(info.GetParameterValue("runInfo.gain").value(), 50.);
    EXPECT_FALSE(info.GetParameterValue("runInfo.source").has_value());  // not a number
    EXPECT_FALSE(info.GetParameterValue("runInfo.missing").has_value());
}

TEST(RunInfo, SettersAndParameters) {
    TRestRunInfo info;
    EXPECT_EQ(info.GetRunNumber(), -1);
    info.SetRunNumber(7);
    info.SetRunTag("Background");
    info.SetStartTimeStamp(100);
    info.SetEndTimeStamp(200);
    info.SetEntriesSaved(42);
    // GetParameters reads the members, not the node
    const YAML::Node parameters = info.GetParameters();
    EXPECT_EQ(parameters["runNumber"].as<int>(), 7);
    EXPECT_EQ(parameters["runTag"].as<std::string>(), "Background");
    EXPECT_DOUBLE_EQ(parameters["endTime"].as<double>(), 200.);
    EXPECT_EQ(parameters["entriesSaved"].as<long long>(), 42);
    EXPECT_EQ(info.GetParameter("runTag"), "Background");
}

TEST(RunInfo, ResolveFilePattern) {
    TRestRunInfo info("run", YAML::Load(kRunConfig));
    info.LoadConfig();
    // runNumber with 5 digits, subRunNumber with 3, texts without spaces nor special characters
    EXPECT_EQ(info.ResolveFilePattern("R[runNumber]_[subRunNumber]_[runTag].root"),
              "R00123_002_AlphaCAMM.root");
    EXPECT_EQ(info.ResolveFilePattern("[runType]_V[runInfo.meshVoltage].root"), "calibration_V300V.root");
    EXPECT_EQ(info.ResolveFilePattern("plain.root"), "plain.root");
    EXPECT_FALSE(info.ResolveFilePattern("v[version].root").empty());
    EXPECT_THROW(info.ResolveFilePattern("R[unknown].root"), std::runtime_error);
    EXPECT_THROW(info.ResolveFilePattern("R[runInfo.missing].root"), std::runtime_error);
}

TEST(RunInfo, FromConfigKeepsOnlyRunParameters) {
    YAML::Node config = YAML::Load(kRunConfig);
    config["inputFileName"] = "input.root";
    config["outputFileName"] = "output.root";
    const auto info = TRestRunInfo::FromConfig("myRun", config);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->GetName(), "myRun");
    EXPECT_EQ(info->GetRunNumber(), 123);
    EXPECT_EQ(info->GetRunInformation().at("gain"), "50");
    // The processing keys of TRestRun are not kept
    const YAML::Node node = info->GetYAMLNode();
    EXPECT_FALSE(node["inputFileName"].IsDefined());
    EXPECT_FALSE(node["outputFileName"].IsDefined());
    EXPECT_EQ(node["runTag"].as<std::string>(), "AlphaCAMM");
}
