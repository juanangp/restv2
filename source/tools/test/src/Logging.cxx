/// Unit tests of the logging (TRestLogManager.h): the verbose levels and their names, the context of the
/// messages (TRestLogScope), the filtering of the RESTError/RESTWarning/RESTInfo/RESTDebug messages by the
/// verbose level, the suppression of repeated messages and the log file. The console output is captured
/// with GoogleTest (CaptureStdout).

#include <TRestLogManager.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <stdexcept>
#include <string>

#include "TestUtils.h"

using Level = TRestLogManager::REST_Verbose_Level;

namespace {
/// Restores the global settings of the logging changed by a test
struct RestoreLogSettings {
    Level level = TRestLogManager::globalVerboseLevel;
    int maxRepeated = TRestLogManager::maxRepeated;
    double interval = TRestLogManager::repeatedInterval;
    ~RestoreLogSettings() {
        TRestLogManager::setGlobalVerboseLevel(level);
        TRestLogManager::setMaxRepeated(maxRepeated);
        TRestLogManager::setRepeatedInterval(interval);
    }
};
}  // namespace

TEST(Logging, VerboseNames) {
    EXPECT_EQ(TRestLogManager::GetVerboseLevelFromString("silent"), Level::REST_Silent);
    EXPECT_EQ(TRestLogManager::GetVerboseLevelFromString("error"), Level::REST_Error);
    EXPECT_EQ(TRestLogManager::GetVerboseLevelFromString("warning"), Level::REST_Warning);
    EXPECT_EQ(TRestLogManager::GetVerboseLevelFromString("info"), Level::REST_Info);
    EXPECT_EQ(TRestLogManager::GetVerboseLevelFromString("debug"), Level::REST_Debug);
    EXPECT_THROW(TRestLogManager::GetVerboseLevelFromString("loud"), std::invalid_argument);
    EXPECT_THROW(TRestLogManager::GetVerboseLevelFromString("INFO"), std::invalid_argument);

    for (const std::string name : {"silent", "error", "warning", "info", "debug"}) {
        EXPECT_EQ(TRestLogManager::GetStringFromVerbose(TRestLogManager::GetVerboseLevelFromString(name)),
                  name);
    }
    EXPECT_THROW(TRestLogManager::GetStringFromVerbose(static_cast<Level>(42)), std::invalid_argument);
}

TEST(Logging, ScopeSetsAndRestoresContext) {
    RestoreLogSettings restore;
    TRestLogManager::setGlobalVerboseLevel(Level::REST_Info);
    EXPECT_EQ(GetLogContext().level, Level::REST_Info);
    EXPECT_EQ(GetLogContext().name, nullptr);

    const std::string name = "myProcess";
    {
        TRestLogScope outer({Level::REST_Debug, &name});
        EXPECT_EQ(GetLogContext().level, Level::REST_Debug);
        ASSERT_NE(GetLogContext().name, nullptr);
        EXPECT_EQ(*GetLogContext().name, "myProcess");
        {
            TRestLogScope inner({Level::REST_Error, nullptr});
            EXPECT_EQ(GetLogContext().level, Level::REST_Error);
        }
        EXPECT_EQ(GetLogContext().level, Level::REST_Debug);
    }
    EXPECT_EQ(GetLogContext().level, Level::REST_Info);
}

TEST(Logging, LevelFiltering) {
    RestoreLogSettings restore;
    TRestLogManager::setMaxRepeated(0);  // no suppression

    std::string output;
    {
        TRestLogScope scope({Level::REST_Warning, nullptr});
        ::testing::internal::CaptureStdout();
        RESTError << "error-message" << RESTendl;
        RESTWarning << "warning-message" << RESTendl;
        RESTInfo << "info-message" << RESTendl;
        RESTDebug << "debug-message" << RESTendl;
        output = ::testing::internal::GetCapturedStdout();
    }
    EXPECT_NE(output.find("error-message"), std::string::npos) << output;
    EXPECT_NE(output.find("warning-message"), std::string::npos) << output;
    EXPECT_EQ(output.find("info-message"), std::string::npos) << output;
    EXPECT_EQ(output.find("debug-message"), std::string::npos) << output;

    {
        TRestLogScope scope({Level::REST_Debug, nullptr});
        ::testing::internal::CaptureStdout();
        RESTInfo << "info-message" << RESTendl;
        RESTDebug << "debug-message " << 42 << RESTendl;
        output = ::testing::internal::GetCapturedStdout();
    }
    EXPECT_NE(output.find("info-message"), std::string::npos) << output;
    EXPECT_NE(output.find("debug-message 42"), std::string::npos) << output;

    {
        // Errors are always written
        TRestLogScope scope({Level::REST_Silent, nullptr});
        ::testing::internal::CaptureStdout();
        RESTError << "silent-error" << RESTendl;
        RESTWarning << "silent-warning" << RESTendl;
        output = ::testing::internal::GetCapturedStdout();
    }
    EXPECT_NE(output.find("silent-error"), std::string::npos) << output;
    EXPECT_EQ(output.find("silent-warning"), std::string::npos) << output;
}

TEST(Logging, RepeatedMessagesSuppressed) {
    RestoreLogSettings restore;
    TRestLogManager::setMaxRepeated(3);
    TRestLogManager::setRepeatedInterval(0);  // a single window

    TRestLogScope scope({Level::REST_Info, nullptr});
    ::testing::internal::CaptureStdout();
    // The same call site: similar messages (a few different characters)
    for (int i = 0; i < 10; i++) RESTWarning << "channel " << i << " not found" << RESTendl;
    const std::string output = ::testing::internal::GetCapturedStdout();

    EXPECT_EQ(TestUtils::Count(output, "not found"), 3) << output;
    EXPECT_EQ(TestUtils::Count(output, "further similar messages suppressed"), 1) << output;
    EXPECT_NE(output.find("channel 2"), std::string::npos) << output;
    EXPECT_EQ(output.find("channel 3"), std::string::npos) << output;
}

TEST(Logging, DebugContextNeverSuppressed) {
    RestoreLogSettings restore;
    TRestLogManager::setMaxRepeated(2);
    TRestLogManager::setRepeatedInterval(0);

    TRestLogScope scope({Level::REST_Debug, nullptr});
    ::testing::internal::CaptureStdout();
    for (int i = 0; i < 5; i++) RESTWarning << "debug context " << i << RESTendl;
    const std::string output = ::testing::internal::GetCapturedStdout();
    EXPECT_EQ(TestUtils::Count(output, "debug context"), 5) << output;
}

TEST(Logging, LogFile) {
    RestoreLogSettings restore;
    TRestLogManager::setMaxRepeated(0);
    const std::filesystem::path dir = TestUtils::WorkDir();
    const std::string fileName = (dir / "rest.log").string();

    TRestLogManager::instance().SetLogFile(fileName);
    const std::string name = "myMetadata";
    {
        TRestLogScope scope({Level::REST_Info, &name});
        ::testing::internal::CaptureStdout();
        RESTInfo << "message-to-file" << RESTendl;
        ::testing::internal::GetCapturedStdout();
    }
    RESTError << "global-error" << RESTendl;
    // Another file: the previous one is closed (flushed)
    TRestLogManager::instance().SetLogFile((dir / "other.log").string());

    const std::string text = TestUtils::ReadFile(fileName);
    EXPECT_NE(text.find("[myMetadata]"), std::string::npos) << text;
    EXPECT_NE(text.find("message-to-file"), std::string::npos) << text;
    EXPECT_NE(text.find("[Global]"), std::string::npos) << text;
    EXPECT_NE(text.find("global-error"), std::string::npos) << text;
    EXPECT_EQ(text.find("\033["), std::string::npos) << "no colors in the file";
}

TEST(Logging, ProgressBar) {
    TRestProgressBar bar;
    ::testing::internal::CaptureStdout();
    bar.Update(5);  // not started: nothing
    bar.Reset(10, 20, "Files");
    bar.Update(5);
    bar.Finish();
    const std::string output = ::testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("5 Files"), std::string::npos) << output;
    EXPECT_NE(output.find("10 Files"), std::string::npos) << output;
    EXPECT_NE(output.find("100.0%"), std::string::npos) << output;
}
