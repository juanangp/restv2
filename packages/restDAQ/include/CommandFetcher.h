#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "FEMDAQ.h"

class CommandFetcher {
   public:
    using CommandHandler = std::function<void(const std::vector<std::string>&)>;

    CommandFetcher(RunConfig& rC);
    ~CommandFetcher() = default;

    void runInteractive();                       // Interactive shell
    void execFile(const std::string& filename);  // Execute from script

    // Graceful interruption control (set externally)
    static void requestShutdown();
    static bool shutdownRequested();

    std::unique_ptr<FEMDAQ> daq;
    RunConfig& runConfig;

    static std::atomic<bool> g_shutdown;

   private:
    void handleCommand(const std::string& line);
    /// \brief handleCommand, writing the error of a failed command instead of ending the session
    void runCommand(const std::string& line);
    void handleCreditsCommand(const std::vector<std::string>& args);
    std::string histFile;
};
