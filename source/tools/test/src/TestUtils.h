#ifndef REST_TOOLS_TEST_UTILS_H
#define REST_TOOLS_TEST_UTILS_H

/// Helpers shared by the unit tests of source/tools: a clean work directory per test (under TESTS_WORK_DIR)
/// and small file utilities.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

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

/// Content of a text file
inline std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

/// Number of non-overlapping occurrences of `what` in `text`
inline int Count(const std::string& text, const std::string& what) {
    int count = 0;
    for (size_t position = text.find(what); position != std::string::npos;
         position = text.find(what, position + what.size()))
        count++;
    return count;
}

}  // namespace TestUtils

#endif
