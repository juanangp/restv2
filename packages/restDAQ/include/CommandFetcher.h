#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "FEMDAQ.h"

/// \file CommandFetcher.h
/// \brief Command console of restDAQ (femclient).

/// \class CommandFetcher
/// \brief Reads the commands of femclient (interactive console or command file) and executes them on the
/// FEMDAQ of the configuration.
///
/// Commands of femclient: `fopen [all|root|log]`, `fclose`, `addMetadata`, `startDAQ [flags]`, `stopDAQ`,
/// `Pedestals [flags]`, `credits show|restore [frameThr reqFrames]`, `fem <id>|*`, `verbose <level>`,
/// `exec <file>`, `sleep <s>`, `quit` or `exit`. Any other line is sent to the active FEMs
/// (FEMDAQ::SendCommand). A failed command writes its error and the session continues.
class CommandFetcher {
   public:
    /// Function that handles the arguments of a command
    using CommandHandler = std::function<void(const std::vector<std::string>&)>;

    /// \brief Creates the FEMDAQ of the electronics of the configuration (FEMDAQ::Create) and reads the
    /// command history ($REST_HOME/.rest/femclient_history, or ~/.rest)
    CommandFetcher(RunConfig& rC);
    ~CommandFetcher() = default;

    /// \brief Interactive console (readline with history) until `quit`, end of input or CTRL-C
    void runInteractive();  // Interactive shell
    /// \brief Executes the commands of a file, one per line (empty lines and lines starting with # are
    /// skipped). The file is copied in the log files opened while it runs
    void execFile(const std::string& filename);  // Execute from script

    // Graceful interruption control (set externally)
    /// \brief Ends the session and aborts the acquisition (FEMDAQ::abrt); async-signal-safe (atomics only)
    static void requestShutdown();
    /// \brief Whether the end of the session was requested
    static bool shutdownRequested();

    std::unique_ptr<FEMDAQ> daq;  ///< The DAQ of the electronics
    RunConfig& runConfig;         ///< Configuration (not owned)

    static std::atomic<bool> g_shutdown;  ///< End of the session requested (quit, CTRL-C, SIGTERM)

   private:
    void handleCommand(const std::string& line);
    /// \brief handleCommand, writing the error of a failed command instead of ending the session
    void runCommand(const std::string& line);
    void handleCreditsCommand(const std::vector<std::string>& args);
    std::string histFile;
};
