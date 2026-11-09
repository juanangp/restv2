/// Unit tests of the GDML parser (TRestGDMLParser.h): the entities of a GDML (files and texts) expanded,
/// the configuration parameters applied to its constants and text entities (Resolve), and written back into
/// the files (UpdateParameters, restG4 --update-gdml). Small GDML files are written in a work directory per
/// test; no geometry is built.

#include <TRestGDMLParser.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

#include "TestUtils.h"

namespace fs = std::filesystem;
using TestUtils::ReadFile;
using TestUtils::WorkDir;
using TestUtils::WriteFile;

namespace {
/// A GDML with a file entity (materials.xml), a text entity (gasName) and a constant (thickness)
fs::path WriteGeometry(const fs::path& dir) {
    WriteFile(dir / "materials.xml", "<materials><element name=\"Argon\"/></materials>\n");
    WriteFile(dir / "setup.gdml",
              "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
              "<!DOCTYPE gdml [\n"
              "<!ENTITY materials SYSTEM \"materials.xml\">\n"
              "<!ENTITY gasName \"Argon\">\n"
              "]>\n"
              "<gdml>\n"
              "  <define>\n"
              "    <constant name=\"thickness\" value=\"1\"/>\n"
              "    <constant name=\"other\" value=\"7\"/>\n"
              "  </define>\n"
              "  &materials;\n"
              "  <material name=\"&gasName;\"/>\n"
              "</gdml>\n");
    return dir / "setup.gdml";
}
}  // namespace

TEST(GDML, ResolveEntitiesAndDefaults) {
    const fs::path gdml = WriteGeometry(WorkDir());
    std::set<std::string> applied;
    const std::string text = TRestGDMLParser::Resolve(gdml.string(), {}, &applied);

    EXPECT_EQ(text.find("DOCTYPE"), std::string::npos) << text;
    EXPECT_EQ(text.find("&materials;"), std::string::npos) << text;
    EXPECT_NE(text.find("<element name=\"Argon\"/>"), std::string::npos) << text;       // file included
    EXPECT_NE(text.find("<material name=\"Argon\"/>"), std::string::npos) << text;      // text entity
    EXPECT_NE(text.find("name=\"thickness\" value=\"1\""), std::string::npos) << text;  // default kept
    EXPECT_TRUE(applied.empty());
}

TEST(GDML, ResolveWithParameters) {
    const fs::path gdml = WriteGeometry(WorkDir());
    std::set<std::string> applied;
    const std::string text = TRestGDMLParser::Resolve(
        gdml.string(), {{"thickness", "5 um"}, {"gasName", "Xenon"}, {"notInGDML", "1"}}, &applied);

    // A constant: the number in REST units (mm)
    EXPECT_NE(text.find("name=\"thickness\" value=\"0.005\""), std::string::npos) << text;
    EXPECT_NE(text.find("name=\"other\" value=\"7\""), std::string::npos) << text;
    EXPECT_NE(text.find("<material name=\"Xenon\"/>"), std::string::npos) << text;
    EXPECT_EQ(applied, (std::set<std::string>{"thickness", "gasName"}));

    // A constant must be given a number
    EXPECT_THROW(TRestGDMLParser::Resolve(gdml.string(), {{"thickness", "thick"}}), std::runtime_error);
}

TEST(GDML, UpdateParameters) {
    const fs::path gdml = WriteGeometry(WorkDir());
    std::set<std::string> applied;
    TRestGDMLParser::UpdateParameters(gdml.string(), {{"thickness", "2 cm"}, {"gasName", "Neon"}}, &applied);
    EXPECT_EQ(applied, (std::set<std::string>{"thickness", "gasName"}));

    const std::string text = ReadFile(gdml);
    // Only the values change: the DOCTYPE and the references are kept
    EXPECT_NE(text.find("<!ENTITY gasName \"Neon\">"), std::string::npos) << text;
    EXPECT_NE(text.find("name=\"thickness\" value=\"20\""), std::string::npos) << text;
    EXPECT_NE(text.find("&materials;"), std::string::npos) << text;
    EXPECT_NE(text.find("<material name=\"&gasName;\"/>"), std::string::npos) << text;

    // Resolved again: the new defaults
    const std::string resolved = TRestGDMLParser::Resolve(gdml.string(), {});
    EXPECT_NE(resolved.find("<material name=\"Neon\"/>"), std::string::npos) << resolved;

    EXPECT_THROW(TRestGDMLParser::UpdateParameters("https://example.com/setup.gdml", {}), std::runtime_error);
}

TEST(GDML, MissingFile) {
    const fs::path dir = WorkDir();
    EXPECT_THROW(TRestGDMLParser::Resolve((dir / "missing.gdml").string(), {}), std::runtime_error);
    WriteFile(dir / "broken.gdml",
              "<!DOCTYPE gdml [\n<!ENTITY part SYSTEM \"missing.xml\">\n]>\n<gdml>&part;</gdml>\n");
    EXPECT_THROW(TRestGDMLParser::Resolve((dir / "broken.gdml").string(), {}), std::runtime_error);
}
