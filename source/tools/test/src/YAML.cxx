/// Unit tests of the YAML utilities (TRestYAMLUtils.h): the `include` key of the configuration files
/// (LoadConfigFile), the references ${ENV:VAR}, ${PARAM:name} and ${a.b} with the `parameters` section and
/// its overrides (ResolveRefs, OpenConfigFile, --set), the checks of the parameters used, the access to the
/// nodes (GetParam, GetList, GetNodeAtPath, GetMetadataClass...), editing a file in place (SetValuesInFile)
/// and the metadata of a REST file (ReadMetadataStore). The files are written in a work directory per test.

#include <TDirectory.h>
#include <TFile.h>
#include <TObjString.h>
#include <TRestYAMLUtils.h>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "TestUtils.h"

namespace fs = std::filesystem;
using TestUtils::ReadFile;
using TestUtils::WorkDir;
using TestUtils::WriteFile;

namespace {
/// Whether calling f throws an exception whose message contains `text`
template <typename Function>
bool ThrowsWith(Function f, const std::string& text) {
    try {
        f();
    } catch (const std::exception& error) {
        return std::string(error.what()).find(text) != std::string::npos;
    }
    return false;
}
}  // namespace

TEST(YAML, IncludeMergesSections) {
    const fs::path dir = WorkDir();
    WriteFile(dir / "common" / "run.yaml",
              "runTag: common\n"
              "runType: calibration\n"
              "runInfo:\n"
              "  meshVoltage: 300 V\n"
              "  gain: 50\n");
    WriteFile(dir / "main.yaml",
              "run:\n"
              "  include: \"common/run.yaml\"\n"
              "  runTag: local\n"
              "  runInfo:\n"
              "    gain: 80\n");

    const YAML::Node config = TRestYAMLUtils::LoadConfigFile((dir / "main.yaml").string());
    const YAML::Node run = config["run"];
    EXPECT_FALSE(run["include"]);
    EXPECT_EQ(run["runTag"].as<std::string>(), "local");                  // its own keys override
    EXPECT_EQ(run["runType"].as<std::string>(), "calibration");           // taken from the included file
    EXPECT_EQ(run["runInfo"]["meshVoltage"].as<std::string>(), "300 V");  // maps merged recursively
    EXPECT_EQ(run["runInfo"]["gain"].as<int>(), 80);
}

TEST(YAML, IncludeSectionAndNested) {
    const fs::path dir = WorkDir();
    WriteFile(dir / "library.yaml",
              "cutA:\n"
              "  conditions: {a: \"x > 1\"}\n"
              "cutB:\n"
              "  include: \"base.yaml\"\n"
              "  conditions: {b: \"y < 2\"}\n");
    WriteFile(dir / "base.yaml", "class: TRestCut\n");
    WriteFile(dir / "main.yaml",
              "first:\n"
              "  include: \"library.yaml:cutA\"\n"
              "  class: TRestCut\n"
              "second:\n"
              "  include: \"library.yaml:cutB\"\n");

    const YAML::Node config = TRestYAMLUtils::LoadConfigFile((dir / "main.yaml").string());
    EXPECT_EQ(config["first"]["conditions"]["a"].as<std::string>(), "x > 1");
    EXPECT_EQ(config["first"]["class"].as<std::string>(), "TRestCut");
    // The included section includes another file itself
    EXPECT_EQ(config["second"]["class"].as<std::string>(), "TRestCut");
    EXPECT_EQ(config["second"]["conditions"]["b"].as<std::string>(), "y < 2");
}

TEST(YAML, IncludeErrors) {
    const fs::path dir = WorkDir();
    WriteFile(dir / "a.yaml", "section:\n  include: \"b.yaml\"\n");
    WriteFile(dir / "b.yaml", "include: \"a.yaml\"\n");
    EXPECT_TRUE(ThrowsWith([&] { TRestYAMLUtils::LoadConfigFile((dir / "a.yaml").string()); }, "itself"));

    WriteFile(dir / "missing.yaml", "section:\n  include: \"nothere.yaml\"\n");
    EXPECT_TRUE(
        ThrowsWith([&] { TRestYAMLUtils::LoadConfigFile((dir / "missing.yaml").string()); }, "not found"));

    WriteFile(dir / "lib.yaml", "x: {a: 1}\n");
    WriteFile(dir / "nokey.yaml", "section:\n  include: \"lib.yaml:y\"\n");
    EXPECT_TRUE(ThrowsWith([&] { TRestYAMLUtils::LoadConfigFile((dir / "nokey.yaml").string()); }, "no key"));

    WriteFile(dir / "list.yaml", "- 1\n- 2\n");
    WriteFile(dir / "notmap.yaml", "section:\n  include: \"list.yaml\"\n");
    EXPECT_TRUE(
        ThrowsWith([&] { TRestYAMLUtils::LoadConfigFile((dir / "notmap.yaml").string()); }, "must be a map"));
}

TEST(YAML, Parameters) {
    const YAML::Node root = YAML::Load(
        "parameters:\n"
        "  isotope: U238\n"
        "  thickness: 5 um\n"
        "  label: \"${PARAM:isotope}_source\"\n"
        "generator:\n"
        "  isotope: \"${PARAM:isotope}\"\n"
        "  thickness: \"${PARAM:thickness}\"\n"
        "  name: \"${PARAM:label}\"\n");

    const YAML::Node config = TRestYAMLUtils::ResolveRefs(root);
    EXPECT_FALSE(config["parameters"]);  // removed from the configuration
    EXPECT_EQ(config["generator"]["isotope"].as<std::string>(), "U238");
    EXPECT_EQ(config["generator"]["thickness"].as<std::string>(), "5 um");
    // A parameter with references, resolved again
    EXPECT_EQ(config["generator"]["name"].as<std::string>(), "U238_source");

    const auto parameters = TRestYAMLUtils::GetParameters();
    EXPECT_EQ(parameters.size(), 3u);
    EXPECT_EQ(parameters.at("thickness"), "5 um");
    EXPECT_NO_THROW(TRestYAMLUtils::CheckParametersUsed());

    // --set isotope=Pb210
    std::map<std::string, std::string> overrides;
    TRestYAMLUtils::AddParameterOverride(overrides, "isotope=Pb210");
    const YAML::Node overridden = TRestYAMLUtils::ResolveRefs(root, overrides);
    EXPECT_EQ(overridden["generator"]["isotope"].as<std::string>(), "Pb210");
    EXPECT_EQ(overridden["generator"]["name"].as<std::string>(), "Pb210_source");
    EXPECT_EQ(TRestYAMLUtils::GetParameters().at("isotope"), "Pb210");
}

TEST(YAML, SelectBlocks) {
    const YAML::Node root = YAML::Load(
        "parameters:\n"
        "  generator: Gammas\n"
        "  energy: 5.5\n"
        "generator:\n"
        "  select: \"${PARAM:generator}\"\n"
        "  source: {particle: gamma, angular: {type: flux}}\n"
        "  cases:\n"
        "    Gammas:\n"
        "      type: point\n"
        "      source: {energy: \"${PARAM:energy} MeV\"}\n"
        "    Muons:\n"
        "      type: cosmic\n"
        "      source: {particle: mu-}\n"
        "list:\n"
        "  - {select: \"${PARAM:generator}\", cases: {Gammas: {a: 1}, default: {a: 2}}}\n"
        "scalar:\n"
        "  select: \"${PARAM:generator}\"\n"
        "  cases: {Muons: 1 GeV, default: 10 MeV}\n");

    const YAML::Node config = TRestYAMLUtils::ResolveRefs(root);
    // Only the case chosen, merged over the common keys
    const YAML::Node generator = config["generator"];
    EXPECT_FALSE(generator["select"]);
    EXPECT_FALSE(generator["cases"]);
    EXPECT_EQ(generator["type"].as<std::string>(), "point");
    EXPECT_EQ(generator["source"]["particle"].as<std::string>(), "gamma");
    EXPECT_EQ(generator["source"]["angular"]["type"].as<std::string>(), "flux");
    EXPECT_EQ(generator["source"]["energy"].as<std::string>(), "5.5 MeV");
    EXPECT_EQ(config["list"][0]["a"].as<int>(), 1);
    EXPECT_EQ(config["scalar"].as<std::string>(), "10 MeV");  // default
    EXPECT_NO_THROW(TRestYAMLUtils::CheckParametersUsed());

    // --set generator=Muons: the case wins over the common keys
    const YAML::Node muons = TRestYAMLUtils::ResolveRefs(root, {{"generator", "Muons"}});
    EXPECT_EQ(muons["generator"]["type"].as<std::string>(), "cosmic");
    EXPECT_EQ(muons["generator"]["source"]["particle"].as<std::string>(), "mu-");
    EXPECT_FALSE(muons["generator"]["source"]["energy"]);
    EXPECT_EQ(muons["list"][0]["a"].as<int>(), 2);
    EXPECT_EQ(muons["scalar"].as<std::string>(), "1 GeV");
    // The cases are not modified by the choice
    EXPECT_FALSE(root["scalar"]["cases"]["Gammas"]);

    // No case nor default; no cases; a scalar case with other keys
    EXPECT_TRUE(ThrowsWith([] { TRestYAMLUtils::ResolveRefs(YAML::Load("a: {select: x, cases: {y: 1}}")); },
                           "no case 'x'"));
    EXPECT_THROW(TRestYAMLUtils::ResolveRefs(YAML::Load("a: {select: x}")), std::runtime_error);
    EXPECT_THROW(TRestYAMLUtils::ResolveRefs(YAML::Load("a: {select: x, b: 1, cases: {x: 2}}")),
                 std::runtime_error);
}

TEST(YAML, ParameterErrors) {
    // Not defined
    EXPECT_TRUE(ThrowsWith([] { TRestYAMLUtils::ResolveRefs(YAML::Load("a: \"${PARAM:none}\"")); }, "none"));
    // Override of a parameter that does not exist
    const YAML::Node root = YAML::Load("parameters: {a: 1}\nx: \"${PARAM:a}\"\n");
    EXPECT_TRUE(ThrowsWith([&] { TRestYAMLUtils::ResolveRefs(root, {{"b", "2"}}); }, "no parameter 'b'"));
    // A parameter must be a scalar
    EXPECT_THROW(TRestYAMLUtils::ResolveRefs(YAML::Load("parameters: {a: [1, 2]}")), std::runtime_error);
    EXPECT_THROW(TRestYAMLUtils::ResolveRefs(YAML::Load("parameters: [1, 2]")), std::runtime_error);

    // --set needs name=value
    std::map<std::string, std::string> overrides;
    EXPECT_THROW(TRestYAMLUtils::AddParameterOverride(overrides, "noequal"), std::runtime_error);
    EXPECT_THROW(TRestYAMLUtils::AddParameterOverride(overrides, "=value"), std::runtime_error);
    TRestYAMLUtils::AddParameterOverride(overrides, "expr=a=b");
    EXPECT_EQ(overrides.at("expr"), "a=b");
}

TEST(YAML, ParametersUsed) {
    TRestYAMLUtils::ResolveRefs(YAML::Load("parameters: {used: 1, unused: 2}\nx: \"${PARAM:used}\"\n"));
    EXPECT_TRUE(ThrowsWith([] { TRestYAMLUtils::CheckParametersUsed(); }, "unused"));
    // e.g. used by the GDML
    TRestYAMLUtils::MarkParameterUsed("unused");
    EXPECT_NO_THROW(TRestYAMLUtils::CheckParametersUsed());

    // Another file read without replacing the current parameters
    TRestYAMLUtils::ResolveRefs(YAML::Load("parameters: {other: 3}\ny: \"${PARAM:other}\"\n"), {}, false);
    const auto parameters = TRestYAMLUtils::GetParameters();
    EXPECT_EQ(parameters.count("used"), 1u);
    EXPECT_EQ(parameters.count("other"), 0u);
    EXPECT_NO_THROW(TRestYAMLUtils::CheckParametersUsed());
}

TEST(YAML, EnvironmentAndPathReferences) {
    setenv("REST_TEST_YAML_VARIABLE", "fromEnv", 1);
    unsetenv("REST_TEST_YAML_UNDEFINED");
    const YAML::Node config =
        TRestYAMLUtils::ResolveRefs(YAML::Load("env: \"${ENV:REST_TEST_YAML_VARIABLE}/data\"\n"
                                               "undefined: \"[${ENV:REST_TEST_YAML_UNDEFINED}]\"\n"
                                               "detector:\n"
                                               "  pitch: 5\n"
                                               "  name: \"${run.tag}\"\n"
                                               "size: \"${detector.pitch} mm\"\n"
                                               "run:\n"
                                               "  tag: Calibration\n"
                                               "list: [\"${detector.pitch}\", 2]\n"));
    EXPECT_EQ(config["env"].as<std::string>(), "fromEnv/data");
    EXPECT_EQ(config["undefined"].as<std::string>(), "[]");
    EXPECT_EQ(config["size"].as<std::string>(), "5 mm");
    EXPECT_EQ(config["detector"]["name"].as<std::string>(), "Calibration");
    EXPECT_EQ(config["list"][0].as<int>(), 5);

    EXPECT_TRUE(
        ThrowsWith([] { TRestYAMLUtils::ResolveRefs(YAML::Load("a: \"${b.c}\"")); }, "key not found"));
    EXPECT_TRUE(
        ThrowsWith([] { TRestYAMLUtils::ResolveRefs(YAML::Load("a: \"${b}\"\nb: {c: 1}")); }, "non-scalar"));
    // A reference to itself never ends
    EXPECT_THROW(TRestYAMLUtils::ResolveRefs(YAML::Load("a: \"x${a}\"")), std::runtime_error);
}

TEST(YAML, OpenConfigFile) {
    const fs::path dir = WorkDir();
    WriteFile(dir / "common.yaml", "runTag: \"${PARAM:tag}\"\n");
    WriteFile(dir / "main.yaml",
              "parameters:\n"
              "  tag: Background\n"
              "run:\n"
              "  include: common.yaml\n"
              "  runNumber: 3\n");
    // The parameters of the main file are used in the included one
    const YAML::Node config = TRestYAMLUtils::OpenConfigFile((dir / "main.yaml").string());
    EXPECT_EQ(config["run"]["runTag"].as<std::string>(), "Background");
    EXPECT_EQ(config["run"]["runNumber"].as<int>(), 3);
    const YAML::Node overridden =
        TRestYAMLUtils::OpenConfigFile((dir / "main.yaml").string(), {{"tag", "Calibration"}});
    EXPECT_EQ(overridden["run"]["runTag"].as<std::string>(), "Calibration");
}

TEST(YAML, NodeAccess) {
    YAML::Node node = YAML::Load(
        "run:\n"
        "  class: TRestRun\n"
        "  runInfo: {meshVoltage: 300 V}\n"
        "cut:\n"
        "  class: TRestCut\n"
        "  threshold: 5\n"
        "  names: [a, b]\n"
        "  single: c\n"
        "scalar: 1\n");

    const auto [key, section] = TRestYAMLUtils::GetMetadataClass(node, "TRestCut");
    EXPECT_EQ(key, "cut");
    EXPECT_EQ(section["threshold"].as<int>(), 5);
    EXPECT_TRUE(TRestYAMLUtils::GetMetadataClass(node, "TRestDataSet").first.empty());

    EXPECT_EQ(TRestYAMLUtils::GetNodeAtPath(node, "run.runInfo.meshVoltage").as<std::string>(), "300 V");
    EXPECT_FALSE(TRestYAMLUtils::GetNodeAtPath(node, "run.runInfo.missing"));
    EXPECT_FALSE(TRestYAMLUtils::GetNodeAtPath(node, "scalar.below"));
    EXPECT_FALSE(node["run"]["runInfo"]["missing"].IsDefined());  // not added by the search

    const YAML::Node cut = node["cut"];
    EXPECT_EQ(TRestYAMLUtils::GetParam<int>(cut, "threshold"), 5);
    EXPECT_EQ(TRestYAMLUtils::GetParam<int>(cut, "missing", 7), 7);
    EXPECT_EQ(TRestYAMLUtils::GetList(cut, "names"), (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(TRestYAMLUtils::GetList(cut, "single"), (std::vector<std::string>{"c"}));
    EXPECT_TRUE(TRestYAMLUtils::GetList(cut, "missing").empty());

    YAML::Node writable = node["cut"];
    EXPECT_EQ(TRestYAMLUtils::ReadParamOrDefault<int>(writable, "threshold", 1), 5);
    EXPECT_EQ(TRestYAMLUtils::ReadParamOrDefault<int>(writable, "newKey", 9), 9);
    EXPECT_EQ(node["cut"]["newKey"].as<int>(), 9);  // the default is written in the node
    TRestYAMLUtils::SetParam(writable, "label", std::string("text"));
    EXPECT_EQ(node["cut"]["label"].as<std::string>(), "text");
}

TEST(YAML, OverrideDetachCheckKeys) {
    YAML::Node node = YAML::Load("a: {gain: 1, sub: {gain: 2}}\nlist: [{gain: 3}]\ngain: 4\n");
    TRestYAMLUtils::OverrideParam(node, "gain", "10");
    EXPECT_EQ(node["gain"].as<int>(), 10);
    EXPECT_EQ(node["a"]["gain"].as<int>(), 10);
    EXPECT_EQ(node["a"]["sub"]["gain"].as<int>(), 10);
    EXPECT_EQ(node["list"][0]["gain"].as<int>(), 10);

    // A node shared by two keys becomes two independent copies
    YAML::Node shared = YAML::Load("{x: 1}");
    YAML::Node root;
    root["a"] = shared;
    root["b"] = shared;
    YAML::Node detached = TRestYAMLUtils::Detach(root);
    detached["a"]["x"] = 5;
    EXPECT_EQ(detached["b"]["x"].as<int>(), 1);
    EXPECT_EQ(root["a"]["x"].as<int>(), 1);
    EXPECT_EQ(YAML::Dump(detached).find('&'), std::string::npos);  // no anchors

    const YAML::Node keys = YAML::Load("{a: 1, b: 2, typo: 3}");
    EXPECT_NO_THROW(TRestYAMLUtils::CheckKeys(keys, {"a", "b", "typo"}, "plot"));
    EXPECT_TRUE(ThrowsWith([&] { TRestYAMLUtils::CheckKeys(keys, {"a", "b"}, "plot"); }, "'typo'"));
}

TEST(YAML, SetValuesInFile) {
    const fs::path dir = WorkDir();
    const std::string fileName = (dir / "config.yaml").string();
    WriteFile(fileName,
              "# Configuration of the run\n"
              "run:\n"
              "  class: TRestRun   # the run\n"
              "  runTag: old\n"
              "  runInfo:\n"
              "    operator: Juan  # who\n");

    TRestYAMLUtils::SetValuesInFile(fileName, {{"run.runTag", "new tag"},
                                               {"run.runInfo.operator", "Ana"},
                                               {"run.runInfo.gain", "50"},
                                               {"run.runDescription", "a: colon"}});

    const std::string text = ReadFile(fileName);
    // Comments kept
    EXPECT_NE(text.find("# Configuration of the run"), std::string::npos) << text;
    EXPECT_NE(text.find("# the run"), std::string::npos) << text;
    EXPECT_NE(text.find("# who"), std::string::npos) << text;

    const YAML::Node config = YAML::LoadFile(fileName);
    EXPECT_EQ(config["run"]["class"].as<std::string>(), "TRestRun");
    EXPECT_EQ(config["run"]["runTag"].as<std::string>(), "new tag");
    EXPECT_EQ(config["run"]["runInfo"]["operator"].as<std::string>(), "Ana");
    EXPECT_EQ(config["run"]["runInfo"]["gain"].as<std::string>(), "50");
    EXPECT_EQ(config["run"]["runDescription"].as<std::string>(), "a: colon");  // quoted

    // The parent is not a map
    EXPECT_THROW(TRestYAMLUtils::SetValuesInFile(fileName, {{"run.runTag.below", "x"}}), std::runtime_error);
    EXPECT_THROW(TRestYAMLUtils::SetValuesInFile((dir / "missing.yaml").string(), {{"a", "b"}}),
                 std::runtime_error);
}

TEST(YAML, ReadMetadataStore) {
    const fs::path dir = WorkDir();
    const std::string fileName = (dir / "store.root").string();
    {
        TFile file(fileName.c_str(), "RECREATE");
        TDirectory* store = file.mkdir("RESTMetadataStore");
        store->cd();
        TObjString run("class: TRestRun\nrunNumber: 5\n");
        run.Write("myRun");
        TObjString cut("class: TRestCut\nconditions: {a: \"x > 1\"}\n");
        cut.Write("fiducial");
        file.Close();
    }
    const YAML::Node store = TRestYAMLUtils::ReadMetadataStore(fileName);
    ASSERT_TRUE(store.IsMap());
    EXPECT_EQ(store.size(), 2u);
    EXPECT_EQ(store["myRun"]["runNumber"].as<int>(), 5);
    EXPECT_EQ(store["fiducial"]["class"].as<std::string>(), "TRestCut");
    EXPECT_EQ(TRestYAMLUtils::GetMetadataClass(store, "TRestRun").first, "myRun");

    // A ROOT file without metadata: empty
    const std::string empty = (dir / "empty.root").string();
    {
        TFile file(empty.c_str(), "RECREATE");
        file.Close();
    }
    EXPECT_EQ(TRestYAMLUtils::ReadMetadataStore(empty).size(), 0u);
    EXPECT_THROW(TRestYAMLUtils::ReadMetadataStore((dir / "missing.root").string()), std::runtime_error);
}
