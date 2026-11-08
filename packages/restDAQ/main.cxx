/// femclient (package restDAQ): data taking of AFTER/AGET electronics (ARC, FEMINOS, DCC and TCM), as in
/// FEMDAQ, based on the `mclient` program of Denis Calvet.
///
///    femclient config.yaml [-e|--exec commands.txt] [--read-only] [--tcm] [--set name=value ...]
///
/// The configuration has a TRestRun (run numbers, tags, runInfo, mainDataPath and outputFileName) and a
/// TRestRawDAQMetadata (electronics, FEMs and limits of the run), see packages/restDAQ/README.md. The
/// commands are typed in an interactive console (readline, history in $REST_HOME/.rest/femclient_history) or
/// read from a file (--exec). Options:
///
/// * **-e, --exec**: file of commands to execute instead of the interactive console.
/// * **--read-only**: the commands fopen, fclose and addMetadata do nothing (no files written).
/// * **--tcm**: the commands go to the TCM (`TCM` of the TRestRawDAQMetadata).
/// * **--set name=value**: the parameters of the configuration for this execution.

#include <readline/readline.h>
#include <unistd.h>

#include <csignal>
#include <iostream>
#include <map>
#include <string>

#include "CommandFetcher.h"
#include "TRestYAMLUtils.h"

namespace {
const char* kUsage =
    "Usage: femclient <config.yaml> [-e|--exec <commands file>] [--read-only] [--tcm] [--set name=value ...]";

void signalHandler(int) {
    // Only async-signal-safe calls (write, atomics) in a signal handler
    const char message[] = "\n[Signal] Caught signal. Shutting down...\n";
    [[maybe_unused]] const auto written = write(STDOUT_FILENO, message, sizeof(message) - 1);
    CommandFetcher::requestShutdown();
}
}  // namespace

int main(int argc, char** argv) {
    std::string configFile = "";
    std::string execFile = "";
    bool readOnly = false;
    bool tcm = false;
    std::map<std::string, std::string> parameters;

    try {
        for (int i = 1; i < argc; i++) {
            const std::string arg = argv[i];
            if ((arg == "-e" || arg == "--exec") && i + 1 < argc) {
                execFile = argv[++i];
            } else if ((arg == "-c" || arg == "--config-file") && i + 1 < argc) {
                configFile = argv[++i];
            } else if (arg == "--read-only") {
                readOnly = true;
            } else if (arg == "--tcm") {
                tcm = true;
            } else if (arg == "--set" && i + 1 < argc) {
                TRestYAMLUtils::AddParameterOverride(parameters, argv[++i]);
            } else if (arg == "-h" || arg == "--help") {
                std::cout << kUsage << std::endl;
                return 0;
            } else if (arg.rfind("-", 0) != 0 && configFile.empty()) {
                configFile = arg;
            } else {
                throw std::runtime_error("unknown argument '" + arg + "'");
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "femclient: " << error.what() << "\n" << kUsage << std::endl;
        return 1;
    }

    if (configFile.empty()) {
        std::cerr << "femclient: no configuration file\n" << kUsage << std::endl;
        return 1;
    }

    std::signal(SIGINT, signalHandler);   // Ctrl+C
    std::signal(SIGTERM, signalHandler);  // kill <pid>

    rl_event_hook = []() -> int {
        if (CommandFetcher::g_shutdown.load()) {
            rl_done = 1;
        }
        return 0;
    };

    try {
        RunConfig runConfig(configFile, parameters, tcm);
        if (readOnly) runConfig.readOnly = true;

        CommandFetcher cmdFetcher(runConfig);
        if (!execFile.empty())
            cmdFetcher.execFile(execFile);
        else
            cmdFetcher.runInteractive();

    } catch (const std::exception& e) {
        std::cout << "\n[!] Execution aborted: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "\n[!] An unexpected error occurred." << std::endl;
    }

    rl_event_hook = nullptr;

    return 0;
}
