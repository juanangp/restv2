/// Unit tests of the generic functions of TRestTools (TRestTools.h): strings (Split, Trim, ReplaceAll,
/// CountString, Format, NormalizeOption, ReplaceDelimited, ReplaceIdentifiers...), numbers, times, files
/// and paths (GetFilesMatchingPattern, GetFullPath, SearchFileInPath, isRootFile...), the automatic run
/// number and the geometries stored in ROOT files (WriteGeometry, ReadGeometryHash). The files are created
/// in a work directory per test.

#include <TFile.h>
#include <TGeoManager.h>
#include <TGeoMaterial.h>
#include <TGeoMedium.h>
#include <TGeoVolume.h>
#include <TNamed.h>
#include <TRestTools.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "TestUtils.h"

namespace fs = std::filesystem;
using TestUtils::WorkDir;
using TestUtils::WriteFile;

TEST(Tools, SplitTrimCase) {
    EXPECT_EQ(TRestTools::Split("a,b,,c", ','), (std::vector<std::string>{"a", "b", "", "c"}));
    EXPECT_EQ(TRestTools::Split("a,b,", ','), (std::vector<std::string>{"a", "b"}));
    EXPECT_TRUE(TRestTools::Split("", ',').empty());

    EXPECT_EQ(TRestTools::Trim("  \t text \n"), "text");
    EXPECT_EQ(TRestTools::Trim("   "), "");
    EXPECT_EQ(TRestTools::Trim("xxtextxx", "x"), "text");

    EXPECT_EQ(TRestTools::ToLower("TripleMaxAVERAGE"), "triplemaxaverage");
    EXPECT_EQ(TRestTools::CleanString(" a b/c:d*e?\"<>|f\\.root"), "abcdef.root");
    EXPECT_EQ(TRestTools::CleanExpression(" a +\tb\n* c "), "a+b*c");
}

TEST(Tools, ReplaceAndCount) {
    std::string text = "aaa";
    TRestTools::ReplaceAll(text, "a", "aa");  // the replacements are not scanned again
    EXPECT_EQ(text, "aaaaaa");
    text = "x.y.z";
    TRestTools::ReplaceAll(text, ".", "_");
    EXPECT_EQ(text, "x_y_z");
    TRestTools::ReplaceAll(text, "", "nothing");
    EXPECT_EQ(text, "x_y_z");

    EXPECT_EQ(TRestTools::CountString("abcabcab", "ab"), 3);
    EXPECT_EQ(TRestTools::CountString("aaaa", "aa"), 2);  // non-overlapping
    EXPECT_EQ(TRestTools::CountString("abc", ""), 0);
    EXPECT_EQ(TRestTools::CountString("", "a"), 0);
}

TEST(Tools, Numbers) {
    EXPECT_TRUE(TRestTools::isANumber("1.5e3"));
    EXPECT_TRUE(TRestTools::isANumber(" 42 "));
    EXPECT_TRUE(TRestTools::isANumber("-0.5"));
    EXPECT_FALSE(TRestTools::isANumber("1.5x"));
    EXPECT_FALSE(TRestTools::isANumber(""));
    EXPECT_FALSE(TRestTools::isANumber("abc"));

    EXPECT_EQ(TRestTools::StringToInteger("12.7"), 12);
    EXPECT_EQ(TRestTools::StringToInteger("0x1F"), 31);
    EXPECT_EQ(TRestTools::StringToInteger(""), 0);
    EXPECT_EQ(TRestTools::StringToInteger("abc"), 0);  // with an error message

    EXPECT_TRUE(TRestTools::StringToBool("TRUE"));
    EXPECT_TRUE(TRestTools::StringToBool("on"));
    EXPECT_TRUE(TRestTools::StringToBool("1"));
    EXPECT_FALSE(TRestTools::StringToBool("yes"));
    EXPECT_FALSE(TRestTools::StringToBool("false"));

    EXPECT_GT(TRestTools::GetRandomSeed(), 0);
}

TEST(Tools, FormatAndOptions) {
    EXPECT_EQ(TRestTools::Format("%05d_%s", 12, "x"), "00012_x");
    EXPECT_EQ(TRestTools::Format("%.2f", 1.234), "1.23");
    EXPECT_EQ(TRestTools::Format("%s", ""), "");
    // Longer than any internal buffer
    EXPECT_EQ(TRestTools::Format("%s", std::string(1000, 'a').c_str()).size(), 1000u);

    const std::vector<std::string> options = {"max", "tripleMaxAverage"};
    EXPECT_EQ(TRestTools::NormalizeOption("method", "TRIPLEMAXAVERAGE", options), "tripleMaxAverage");
    EXPECT_EQ(TRestTools::NormalizeOption("method", "Max", options), "max");
    try {
        TRestTools::NormalizeOption("method", "min", options);
        FAIL() << "an invalid option must throw";
    } catch (const std::invalid_argument& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("method"), std::string::npos) << message;
        EXPECT_NE(message.find("tripleMaxAverage"), std::string::npos) << message;
    }
}

TEST(Tools, ReplaceDelimited) {
    auto replacement = [](const std::string& content) -> std::optional<std::string> {
        if (content == "runNumber") return "123";
        if (content == "loop") return "[runNumber]";  // not scanned again
        return std::nullopt;
    };
    EXPECT_EQ(TRestTools::ReplaceDelimited("R[runNumber]_[tag].root", '[', ']', replacement),
              "R123_[tag].root");
    EXPECT_EQ(TRestTools::ReplaceDelimited("[loop]", '[', ']', replacement), "[runNumber]");
    EXPECT_EQ(TRestTools::ReplaceDelimited("no delimiters", '[', ']', replacement), "no delimiters");
    EXPECT_EQ(TRestTools::ReplaceDelimited("open [runNumber", '[', ']', replacement), "open [runNumber");
    EXPECT_EQ(TRestTools::ReplaceDelimited("{a}{runNumber}", '{', '}', replacement), "{a}123");
}

TEST(Tools, ReplaceIdentifiers) {
    auto replacement = [](const std::string& name) -> std::optional<std::string> {
        if (name == "x") return "[0]";
        if (name == "y") return "[1]";
        return std::nullopt;
    };
    const std::string identifier = "[A-Za-z_][A-Za-z0-9_]*";
    // e5 of 1e5 is not an identifier, sqrt is kept (nullopt)
    EXPECT_EQ(TRestTools::ReplaceIdentifiers("x + 1e5 * y", identifier, replacement), "[0] + 1e5 * [1]");
    EXPECT_EQ(TRestTools::ReplaceIdentifiers("sqrt(x*x+y*y)", identifier, replacement),
              "sqrt([0]*[0]+[1]*[1])");
    EXPECT_EQ(TRestTools::ReplaceIdentifiers("xy > 2.x", identifier, replacement), "xy > 2.x");
}

TEST(Tools, PatternToRegex) {
    // `*` is the only wildcard, punctuation is escaped
    EXPECT_EQ(TRestTools::PatternToRegex("R*.root"), "R.*\\.root");
    EXPECT_EQ(TRestTools::PatternToRegex("a?b"), "a\\?b");
    EXPECT_EQ(TRestTools::PatternToRegex("abc"), "abc");
}

TEST(Tools, Times) {
    EXPECT_EQ(TRestTools::ToTimeStringLong(30), "30.00 seconds");
    EXPECT_EQ(TRestTools::ToTimeStringLong(90), "1.50 minutes");
    EXPECT_EQ(TRestTools::ToTimeStringLong(9000), "2.50 hours");
    EXPECT_EQ(TRestTools::ToTimeStringLong(2 * 86400), "2.00 days");
    EXPECT_EQ(TRestTools::ToTimeStringLong(14 * 86400), "2.00 weeks");

    // Local time: StringToTimeStamp and GetTimeStampFromUnixTime are inverse
    const double stamp = TRestTools::StringToTimeStamp("2022/04/28 13:20:05");
    EXPECT_EQ(TRestTools::GetTimeStampFromUnixTime(stamp), "2022-04-28 13:20:05");
    EXPECT_DOUBLE_EQ(TRestTools::StringToTimeStamp("2022-04-28 13:20"), stamp - 5);
    EXPECT_DOUBLE_EQ(TRestTools::StringToTimeStamp("2022-04-28"),
                     TRestTools::StringToTimeStamp("2022/04/28 00:00"));
    EXPECT_THROW(TRestTools::StringToTimeStamp("hello"), std::runtime_error);
    EXPECT_THROW(TRestTools::StringToTimeStamp("2022/04"), std::runtime_error);

    EXPECT_GT(TRestTools::GetCurrentTime(), 1.6e9);
}

TEST(Tools, Paths) {
    const fs::path current = fs::current_path();
    // The current directory (possibly written with a trailing separator)
    EXPECT_TRUE(fs::equivalent(TRestTools::GetFullPath(""), current));
    EXPECT_EQ(fs::path(TRestTools::GetFullPath("a/../b.root")), (current / "b.root").lexically_normal());
    EXPECT_EQ(TRestTools::GetFullPath("/data/./x/../run.root"), "/data/run.root");

    const auto [path, name] = TRestTools::SeparatePathAndName("/data/run.root");
    EXPECT_EQ(path, "/data");
    EXPECT_EQ(name, "run.root");
    EXPECT_EQ(TRestTools::SeparatePathAndName("run.root").first, "");
}

TEST(Tools, URLs) {
    EXPECT_TRUE(TRestTools::IsURL("https://example.org/materials.xml"));
    EXPECT_TRUE(TRestTools::IsURL("http://example.org"));
    EXPECT_FALSE(TRestTools::IsURL("geometry/materials.xml"));
    EXPECT_FALSE(TRestTools::IsURL("/data/https://x"));
    EXPECT_EQ(TRestTools::GetLocalFile("geometry/materials.xml"), "geometry/materials.xml");

    // A URL already in the cache ($REST_HOME/.rest/download) is not downloaded again (no network needed)
    const fs::path dir = WorkDir();
    const char* previous = std::getenv("REST_HOME");
    const std::string saved = previous ? previous : "";
    setenv("REST_HOME", dir.c_str(), 1);
    const fs::path cached = dir / ".rest" / "download" / "example.org_rest-common_v1.0_materials.xml";
    WriteFile(cached, "<materials/>");
    const std::string url = "https://example.org/rest-common/v1.0/materials.xml";
    EXPECT_EQ(fs::path(TRestTools::GetLocalFile(url)), cached);
    if (previous) {
        setenv("REST_HOME", saved.c_str(), 1);
    } else {
        unsetenv("REST_HOME");
    }
}

TEST(Tools, FilesMatchingPattern) {
    const fs::path dir = WorkDir();
    WriteFile(dir / "R001_ANA.root", "x");
    WriteFile(dir / "R002_ana.root", "x");  // the match ignores case
    WriteFile(dir / "R003_RAW.root", "x");
    WriteFile(dir / "sub" / "R004_ANA.root", "x");  // subdirectories are not searched
    fs::create_directories(dir / "R005_ANA.root");  // a directory is not a file

    auto files = TRestTools::GetFilesMatchingPattern((dir / "R*_ANA.root").string());
    std::sort(files.begin(), files.end());
    ASSERT_EQ(files.size(), 2u);
    EXPECT_EQ(fs::path(files[0]).filename(), "R001_ANA.root");
    EXPECT_EQ(fs::path(files[1]).filename(), "R002_ana.root");

    EXPECT_EQ(TRestTools::GetFilesMatchingPattern((dir / "*.root").string()).size(), 3u);
    EXPECT_TRUE(TRestTools::GetFilesMatchingPattern((dir / "R?01_ANA.root").string()).empty());
    EXPECT_TRUE(TRestTools::GetFilesMatchingPattern("").empty());
}

TEST(Tools, SearchFileInPath) {
    const fs::path dir = WorkDir();
    WriteFile(dir / "a" / "b" / "target.txt", "x");
    EXPECT_EQ(fs::path(TRestTools::SearchFileInPath({(dir / "none").string(), dir.string()}, "target.txt")),
              dir / "a" / "b" / "target.txt");
    EXPECT_EQ(TRestTools::SearchFileInPath({dir.string()}, "missing.txt"), "");
    // A file that exists is returned as it is
    const std::string existing = (dir / "a" / "b" / "target.txt").string();
    EXPECT_EQ(TRestTools::SearchFileInPath({}, existing), existing);
}

TEST(Tools, RootFiles) {
    const fs::path dir = WorkDir();
    const std::string text = (dir / "text.root").string();
    WriteFile(text, "not a ROOT file");
    EXPECT_TRUE(TRestTools::fileExists(text));
    EXPECT_FALSE(TRestTools::isRootFile(text));
    EXPECT_FALSE(TRestTools::isRootFile((dir / "missing.root").string()));
    EXPECT_FALSE(TRestTools::fileExists((dir / "missing.root").string()));

    const std::string root = (dir / "plain.root").string();
    {
        TFile file(root.c_str(), "RECREATE");
        TNamed object("object", "title");
        object.Write();
        file.Close();
    }
    EXPECT_TRUE(TRestTools::isRootFile(root));
    // No AnalysisTree nor RESTMetadataStore
    EXPECT_FALSE(TRestTools::isValidTRestRun(root));
    EXPECT_FALSE(TRestTools::isDataSet(root));
    EXPECT_FALSE(TRestTools::isValidTRestRun(text));
}

TEST(Tools, RunNumberAuto) {
    const fs::path dir = WorkDir();
    // Its own REST_HOME: the first number is 0, then incremented
    const char* previous = std::getenv("REST_HOME");
    const std::string saved = previous ? previous : "";
    setenv("REST_HOME", dir.c_str(), 1);

    EXPECT_EQ(fs::path(TRestTools::GetRESTUserDirectory()), dir / ".rest");
    EXPECT_TRUE(fs::is_directory(dir / ".rest"));
    const int first = TRestTools::GetRunNumberAuto();
    const int second = TRestTools::GetRunNumberAuto();
    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, 1);
    EXPECT_TRUE(fs::exists(dir / ".rest" / "runNumber"));

    if (previous) {
        setenv("REST_HOME", saved.c_str(), 1);
    } else {
        unsetenv("REST_HOME");
    }
}

TEST(Tools, GeometryHash) {
    const fs::path dir = WorkDir();
    // A world box with a daughter
    auto* geometry = new TGeoManager("TestGeometry", "test");
    auto* material = new TGeoMaterial("Vacuum", 0, 0, 0);
    auto* medium = new TGeoMedium("Vacuum", 1, material);
    TGeoVolume* world = geometry->MakeBox("World", medium, 100, 100, 100);
    TGeoVolume* box = geometry->MakeBox("Box", medium, 10, 10, 10);
    world->AddNode(box, 1);
    geometry->SetTopVolume(world);
    geometry->CloseGeometry();

    const std::string description = TRestTools::DescribeGeometry(geometry);
    EXPECT_NE(description.find("World"), std::string::npos);
    EXPECT_NE(description.find("Box"), std::string::npos);
    const std::string hash = TRestTools::GetGeometryHash(geometry);
    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(TRestTools::GetGeometryHash(geometry), hash);

    const std::string fileName = (dir / "geometry.root").string();
    {
        TFile file(fileName.c_str(), "RECREATE");
        TRestTools::WriteGeometry(&file, geometry, "Geometry");
        file.Close();
    }
    {
        TFile file(fileName.c_str(), "READ");
        EXPECT_EQ(TRestTools::ReadGeometryHash(&file, "Geometry"), hash);
        EXPECT_EQ(TRestTools::ReadGeometryHash(&file, "Other"), "");
        file.Close();
    }
    TRestTools::DeleteGeometry(geometry);
}
