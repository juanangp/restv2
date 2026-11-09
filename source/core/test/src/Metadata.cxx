/// Unit tests of the metadata framework (TRestMetadata.h, TRestMetadataStore.h, TRestManager.h): the
/// registered fields of a class (only registered keys are accepted, values with units, written back with
/// UpdateYAMLFromParams), the creation of the metadata by `class` (MetadataClassRegistry), the verbose level,
/// the configuration stored in a ROOT file (WriteMetadata/ReadMetadata, TRestMetadataStore), a metadata read
/// from a section of a YAML file with its own parameters, and a TRestManager creating its metadata.
/// A small metadata class defined here (TestMetadata) is registered as any REST class.

#include <TFile.h>
#include <TRestCut.h>
#include <TRestLogManager.h>
#include <TRestManager.h>
#include <TRestMetadata.h>
#include <TRestMetadataStore.h>
#include <TRestSystemOfUnits.h>
#include <TRestYAMLUtils.h>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "TestUtils.h"

namespace fs = std::filesystem;
using Level = TRestLogManager::REST_Verbose_Level;

namespace {

/// A metadata with a field of each kind
class TestMetadata : public TRestMetadata {
   public:
    int fCount = 1;
    TRestWithUnits fEnergy = 2.;
    std::string fLabel = "default";
    std::vector<std::string> fNames;
    std::map<std::string, double> fWeights;

    TestMetadata() = default;
    TestMetadata(const std::string& instanceName, const YAML::Node& node)
        : TRestMetadata(instanceName, node) {
        LoadConfig();
    }

    std::string GetClassName() const override { return "TestMetadata"; }
    void LoadConfig() override {
        UpdateParamsFromYAML<TestMetadata>(fNode);
        UpdateYAMLFromParams<TestMetadata>(fNode);
    }
    void Initialize() override {}
};

const bool kTestMetadataRegistered = [] {
    auto& fields = TRestMetadataFieldRegistry::Instance();
    fields.RegisterField<TestMetadata>("count", &TestMetadata::fCount);
    fields.RegisterField<TestMetadata>("energy", &TestMetadata::fEnergy);
    fields.RegisterField<TestMetadata>("label", &TestMetadata::fLabel);
    fields.RegisterField<TestMetadata>("names", &TestMetadata::fNames);
    fields.RegisterField<TestMetadata>("weights", &TestMetadata::fWeights);
    MetadataClassRegistry::Instance().Register(
        "TestMetadata", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TestMetadata>(instanceName, params);
        });
    return true;
}();

/// The message of the exception thrown by f, empty if none
template <typename Function>
std::string ErrorOf(Function f) {
    try {
        f();
    } catch (const std::exception& error) {
        return error.what();
    }
    return "";
}

const char* kTestConfig =
    "class: TestMetadata\n"
    "count: 3\n"
    "energy: 5.9 MeV\n"
    "label: myLabel\n"
    "names: [a, b]\n"
    "weights: {x: 0.5, y: 2}\n";

}  // namespace

TEST(Metadata, RegistryCreatesByClass) {
    ASSERT_TRUE(kTestMetadataRegistered);
    auto& registry = MetadataClassRegistry::Instance();
    EXPECT_TRUE(registry.Contains("TestMetadata"));
    EXPECT_TRUE(registry.Contains("TRestCut"));
    EXPECT_TRUE(registry.Contains("TRestRun"));
    EXPECT_TRUE(registry.Contains("TRestManager"));
    EXPECT_FALSE(registry.Contains("TRestNotAClass"));

    auto metadata = registry.Create("test", YAML::Load(kTestConfig));
    ASSERT_NE(metadata, nullptr);
    EXPECT_EQ(metadata->GetClassName(), "TestMetadata");
    EXPECT_EQ(metadata->GetName(), "test");
    auto* test = dynamic_cast<TestMetadata*>(metadata.get());
    ASSERT_NE(test, nullptr);
    EXPECT_EQ(test->fCount, 3);

    EXPECT_THROW(registry.Create("x", YAML::Load("count: 3")), std::runtime_error);  // no class
    EXPECT_THROW(registry.Create("x", YAML::Load("class: TRestNotAClass")), std::runtime_error);
}

TEST(Metadata, RegisteredFieldsAreRead) {
    TestMetadata test("test", YAML::Load(kTestConfig));
    EXPECT_EQ(test.fCount, 3);
    EXPECT_DOUBLE_EQ(test.fEnergy, 5900.);  // in REST units (keV)
    EXPECT_EQ(test.fLabel, "myLabel");
    EXPECT_EQ(test.fNames, (std::vector<std::string>{"a", "b"}));
    EXPECT_DOUBLE_EQ(test.fWeights.at("y"), 2.);

    // Not given: the default
    TestMetadata defaults("defaults", YAML::Load("class: TestMetadata"));
    EXPECT_EQ(defaults.fCount, 1);
    EXPECT_DOUBLE_EQ(defaults.fEnergy, 2.);
    EXPECT_EQ(defaults.fLabel, "default");
    EXPECT_TRUE(defaults.fNames.empty());

    const auto keys = TRestMetadataFieldRegistry::Instance().GetFieldKeys(&test);
    EXPECT_EQ(keys, (std::set<std::string>{"count", "energy", "label", "names", "weights"}));
}

TEST(Metadata, UnknownKeysThrow) {
    const YAML::Node node = YAML::Load("class: TestMetadata\ncount: 3\nbogus: 1\ntypo: 2\n");
    const std::string message = ErrorOf([&] { MetadataClassRegistry::Instance().Create("test", node); });
    EXPECT_NE(message.find("'bogus'"), std::string::npos) << message;
    EXPECT_NE(message.find("'typo'"), std::string::npos) << message;
    EXPECT_NE(message.find("count"), std::string::npos) << message;  // the valid keys listed

    // Created directly it is not checked until CheckConfig
    TestMetadata test("test", node);
    EXPECT_EQ(test.fCount, 3);
    EXPECT_THROW(test.CheckConfig(), std::runtime_error);

    // class, name and verbose are always accepted; not checked when created from a file (check false)
    EXPECT_NO_THROW(MetadataClassRegistry::Instance().Create(
        "test", YAML::Load("class: TestMetadata\nname: other\nverbose: warning\n")));
    EXPECT_NO_THROW(MetadataClassRegistry::Instance().Create("TestMetadata", "test", node, false));
}

TEST(Metadata, InvalidValuesThrow) {
    EXPECT_THROW(TestMetadata("test", YAML::Load("count: many")), std::runtime_error);
    EXPECT_THROW(TestMetadata("test", YAML::Load("energy: 5 apples")), std::runtime_error);
    EXPECT_THROW(TestMetadata("test", YAML::Load("names: {a: 1}")), std::runtime_error);
    const std::string message = ErrorOf([] { TestMetadata("myTest", YAML::Load("count: many")); });
    EXPECT_NE(message.find("myTest"), std::string::npos) << message;
    EXPECT_NE(message.find("count"), std::string::npos) << message;
}

TEST(Metadata, NameAndVerbose) {
    TestMetadata named("instance", YAML::Load("name: custom\nverbose: debug\n"));
    EXPECT_EQ(named.GetName(), "custom");
    EXPECT_EQ(named.GetVerboseLevel(), Level::REST_Debug);
    EXPECT_EQ(named.GetLogContext().level, Level::REST_Debug);

    // Without verbose: the global one, written in the node
    const Level global = TRestLogManager::globalVerboseLevel;
    TestMetadata plain("instance", YAML::Load("count: 2"));
    EXPECT_EQ(plain.GetName(), "instance");
    EXPECT_EQ(plain.GetVerboseLevel(), global);
    EXPECT_EQ(plain.GetYAMLNode()["verbose"].as<std::string>(),
              TRestLogManager::GetStringFromVerbose(global));

    EXPECT_THROW(TestMetadata("instance", YAML::Load("verbose: loud")), std::runtime_error);
}

TEST(Metadata, UpdateYAMLRoundTrip) {
    TestMetadata test("test", YAML::Load(kTestConfig));
    // The node has the values used, units converted
    EXPECT_DOUBLE_EQ(test.GetYAMLNode()["energy"].as<double>(), 5900.);

    test.fCount = 7;
    test.fEnergy = 1.5;
    test.fNames.push_back("c");
    test.fWeights["z"] = 3;
    YAML::Node written(YAML::NodeType::Map);
    test.UpdateYAMLFromParams<TestMetadata>(written);
    EXPECT_EQ(written["count"].as<int>(), 7);
    EXPECT_DOUBLE_EQ(written["energy"].as<double>(), 1.5);
    EXPECT_EQ(written["names"].size(), 3u);

    // Read back from the YAML written (also after a dump)
    TestMetadata copy("copy", YAML::Load(YAML::Dump(written)));
    EXPECT_EQ(copy.fCount, 7);
    EXPECT_DOUBLE_EQ(copy.fEnergy, 1.5);
    EXPECT_EQ(copy.fLabel, "myLabel");
    EXPECT_EQ(copy.fNames, test.fNames);
    EXPECT_EQ(copy.fWeights, test.fWeights);
}

TEST(Metadata, WriteAndReadFromFile) {
    const fs::path dir = TestUtils::WorkDir();
    const std::string fileName = (dir / "metadata.root").string();

    TestMetadata test("test", YAML::Load(kTestConfig));
    test.fCount = 11;  // written with the current values
    TRestCut cut("fiducial", YAML::Load("class: TRestCut\nconditions: {e: \"ana_energy > 25\"}\n"));
    {
        TFile file(fileName.c_str(), "RECREATE");
        test.WriteMetadata(&file);
        cut.WriteMetadata(&file);
        TRestMetadata::WriteMetadata(&file, "empty", YAML::Node());  // nothing for an empty node
        file.Close();
    }
    {
        TFile file(fileName.c_str(), "READ");
        const YAML::Node node = TRestMetadata::ReadMetadata(&file, "test");
        ASSERT_TRUE(node.IsMap());
        EXPECT_EQ(node["class"].as<std::string>(), "TestMetadata");
        EXPECT_EQ(node["count"].as<int>(), 11);
        EXPECT_DOUBLE_EQ(node["energy"].as<double>(), 5900.);
        EXPECT_TRUE(TRestMetadata::ReadMetadata(&file, "missing").IsNull());
        EXPECT_TRUE(TRestMetadata::ReadMetadata(&file, "empty").IsNull());
        // Read-only file
        EXPECT_THROW(test.WriteMetadata(&file), std::runtime_error);

        // The metadata of the file as objects
        TRestMetadataStore store;
        store.Load(&file);
        EXPECT_EQ(store.GetAll().size(), 2u);
        auto* stored = dynamic_cast<TestMetadata*>(store.Get("test"));
        ASSERT_NE(stored, nullptr);
        EXPECT_EQ(stored->fCount, 11);
        EXPECT_DOUBLE_EQ(stored->fEnergy, 5900.);
        auto* storedCut = dynamic_cast<TRestCut*>(store.GetClass("TRestCut"));
        ASSERT_NE(storedCut, nullptr);
        EXPECT_EQ(storedCut->GetName(), "fiducial");
        EXPECT_EQ(storedCut->GetExpression(), "(ana_energy > 25)");
        EXPECT_EQ(store.GetOfType<TestMetadata>().size(), 1u);
        EXPECT_EQ(store.Get("missing"), nullptr);

        TRestMetadataStore skipped;
        skipped.Load(&file, nullptr, {"TRestCut"});
        EXPECT_EQ(skipped.GetAll().size(), 1u);
        EXPECT_EQ(skipped.GetClass("TRestCut"), nullptr);
        skipped.Clear();
        EXPECT_TRUE(skipped.IsEmpty());
        file.Close();
    }
    EXPECT_THROW(TRestMetadata::WriteMetadata(nullptr, "x", YAML::Load("a: 1")), std::runtime_error);
}

TEST(Metadata, SectionOfFileWithItsParameters) {
    const fs::path dir = TestUtils::WorkDir();
    TestUtils::WriteFile(dir / "cuts.yaml",
                         "parameters:\n"
                         "  threshold: 25\n"
                         "fiducial:\n"
                         "  class: TRestCut\n"
                         "  conditions:\n"
                         "    energy: \"ana_energy > ${PARAM:threshold}\"\n");

    // The parameters of the main configuration are kept
    TRestYAMLUtils::ResolveRefs(YAML::Load("parameters: {main: 1}\nx: \"${PARAM:main}\"\n"));
    TRestCut cut((dir / "cuts.yaml").string(), "fiducial");
    EXPECT_EQ(cut.GetName(), "fiducial");
    EXPECT_EQ(cut.fConditions.at("energy"), "ana_energy > 25");
    const auto parameters = TRestYAMLUtils::GetParameters();
    EXPECT_EQ(parameters.count("main"), 1u);
    EXPECT_EQ(parameters.count("threshold"), 0u);
}

TEST(Metadata, ManagerCreatesItsMetadata) {
    const YAML::Node node = YAML::Load(
        "class: TRestManager\n"
        "verbose: warning\n"
        "cutA:\n"
        "  class: TRestCut\n"
        "  conditions: {a: \"x > 1\"}\n"
        "cutB:\n"
        "  class: TRestCut\n"
        "  verbose: debug\n"
        "  conditions: {b: \"y < 2\"}\n"
        "test:\n"
        "  class: TestMetadata\n"
        "  count: 5\n");
    TRestManager manager("manager", node);

    const auto cuts = manager.GetMetadataOfType<TRestCut>();
    ASSERT_EQ(cuts.size(), 2u);
    EXPECT_EQ(cuts[0]->GetName(), "cutA");  // in declaration order
    EXPECT_EQ(cuts[1]->GetName(), "cutB");
    // Without its own verbose, the one of the manager
    EXPECT_EQ(cuts[0]->GetVerboseLevel(), Level::REST_Warning);
    EXPECT_EQ(cuts[1]->GetVerboseLevel(), Level::REST_Debug);

    auto* test = dynamic_cast<TestMetadata*>(manager.GetMetadataClass("TestMetadata"));
    ASSERT_NE(test, nullptr);
    EXPECT_EQ(test->fCount, 5);
    EXPECT_EQ(manager.GetMetadataClass("TRestRun"), nullptr);

    EXPECT_EQ(manager.FindMetadata<TRestCut>("cutB", "test", "cut")->GetName(), "cutB");
    EXPECT_EQ(manager.FindMetadata<TestMetadata>("", "test", "metadata"), test);          // the only one
    EXPECT_THROW(manager.FindMetadata<TRestCut>("", "test", "cut"), std::runtime_error);  // several
    EXPECT_THROW(manager.FindMetadata<TRestCut>("cutC", "test", "cut"), std::runtime_error);
}

TEST(Metadata, ManagerChecksKeys) {
    // A metadata of the manager with an unknown key
    EXPECT_THROW(TRestManager("manager", YAML::Load("cut:\n  class: TRestCut\n  bogus: 1\n")),
                 std::runtime_error);
    // An unknown key of the manager itself (created by class, as restManager does)
    const std::string message = ErrorOf([] {
        MetadataClassRegistry::Instance().Create(
            "manager", YAML::Load("class: TRestManager\nbogus: 1\ncut: {class: TRestCut}\n"));
    });
    EXPECT_NE(message.find("'bogus'"), std::string::npos) << message;
}
