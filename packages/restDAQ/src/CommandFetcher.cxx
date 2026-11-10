#include "CommandFetcher.h"

// readline.h uses FILE without including stdio.h (needed on macOS)
#include <readline/history.h>
#include <readline/readline.h>

#include <charconv>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#include "TRestTools.h"

std::atomic<bool> CommandFetcher::g_shutdown(false);

CommandFetcher::CommandFetcher(RunConfig& rC) : runConfig(rC) {
    daq = FEMDAQ::Create(runConfig);

    // History of the commands, in the directory of REST of the user (created if it does not exist)
    histFile = TRestTools::GetRESTUserDirectory() + "/femclient_history";
    read_history(histFile.c_str());
    if (runConfig.verboseLevel >= RunConfig::Verbosity::REST_Info)
        std::cout << "History file " << histFile << std::endl;
}

void CommandFetcher::handleCommand(const std::string& line) {
    if (line.empty()) return;

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    std::vector<std::string> args;
    std::string arg;
    while (iss >> arg) args.push_back(arg);

    if (cmd == "exit" || cmd == "quit") {
        requestShutdown();
        return;
    } else if (cmd == "exec" && !args.empty()) {
        execFile(args[0]);
        return;
    } else if (cmd == "fem" && !args.empty()) {
        daq->setActiveFEM(args[0]);
        return;
    } else if (cmd == "addMetadata") {
        // Write Metadata
        if (!daq->isReadOnly()) daq->UpdateRunConfigInfo();
    } else if (cmd == "fopen") {
        // Open log and root files
        if (!daq->isReadOnly()) daq->OpenFiles(args);
        // Flag options, empty/all --> Logs and root files, log --> only logs,
        // root --> only root
    } else if (cmd == "fclose") {
        // Close log and root files
        if (!daq->isReadOnly()) daq->CloseFiles();
    } else if (cmd == "credits") {
        handleCreditsCommand(args);
    } else if (cmd == "startDAQ") {
        // Start acquisiton loop
        daq->startDAQ(args);
    } else if (cmd == "stopDAQ") {
        // Stop acquisiton loop
        daq->stopDAQ();
    } else if (cmd == "Pedestals") {
        // Start Pedestal loop (DCC)
        daq->Pedestals(args);
    } else if (cmd == "verbose" && !args.empty()) {
        // Setting verbose (override run config)
        runConfig.SetVerboseLevel(args[0]);
    } else if (cmd == "sleep" && !args.empty()) {
        int sleepTime = 1;
        auto result = std::from_chars(args[0].data(), args[0].data() + args[0].size(), sleepTime);
        if (result.ec == std::errc()) {
            std::this_thread::sleep_for(std::chrono::seconds(sleepTime));
        } else {
            std::cout << "Cannot decode sleep time " << args[0] << std::endl;
        }
    } else {
        // Send cmd to to DAQ
        daq->SendCommand(line.c_str());
    }
}

void CommandFetcher::handleCreditsCommand(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cout << "Usage: credits show | credits restore [frameThr reqFrames]" << std::endl;
        return;
    }

    const std::string& subCmd = args[0];

    if (subCmd == "show") {
        daq->CreditsShow();
    } else if (subCmd == "restore") {
        if (args.size() == 1) {
            daq->CreditsRestore();
        } else if (args.size() >= 3) {
            uint32_t frameThr = 0;
            uint32_t reqFrames = 0;

            auto res1 = std::from_chars(args[1].data(), args[1].data() + args[1].size(), frameThr);
            auto res2 = std::from_chars(args[2].data(), args[2].data() + args[2].size(), reqFrames);

            if (res1.ec != std::errc() || res2.ec != std::errc()) {
                std::cout << "Error: Invalid integer arguments for credits restore." << std::endl;
            } else if (frameThr == 0 || frameThr > reqFrames) {
                // frameThr 0 would send requests without end, above reqFrames none
                std::cout << "Error: credits restore needs 0 < frameThr <= reqFrames." << std::endl;
            } else {
                daq->CreditsRestore(reqFrames, frameThr);
            }
        } else {
            std::cout << "Usage: credits restore (to use defaults) OR credits "
                         "restore <frameThr> <reqFrames>"
                      << std::endl;
        }
    } else {
        std::cout << "Unknown credits sub-command: " << subCmd << std::endl;
    }
}

void CommandFetcher::runInteractive() {
    std::cout << "?? Interactive CLI ready. Type 'quit' to exit.\n";

    while (!shutdownRequested()) {
        char* input = readline("> ");

        if (shutdownRequested()) {
            if (input) free(input);
            break;
        }

        if (!input) break;

        std::string line(input);
        free(input);
        input = nullptr;

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (!line.empty()) {
            add_history(line.c_str());
            append_history(1, histFile.c_str());
            runCommand(line);
        }
    }

    // Save full history on exit
    write_history(histFile.c_str());
    std::cout << "?? Exiting CLI.\n";
}

void CommandFetcher::execFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open command file: " << filename << std::endl;
        return;
    }

    daq->SetExecFile(filename);

    std::string line;
    while (std::getline(file, line)) {
        // if (shutdownRequested()) break;
        if (line.empty() || line[0] == '#') continue;
        if (runConfig.verboseLevel >= RunConfig::Verbosity::REST_Info) std::cout << "> " << line << std::endl;
        runCommand(line);
    }
    daq->SetExecFile("");
}

void CommandFetcher::runCommand(const std::string& line) {
    // A failed command (wrong arguments, FEM timeout...) does not end the session
    try {
        handleCommand(line);
    } catch (const std::exception& e) {
        // CTRL-C during a command (e.g. addMetadata) ends the program, as in FEMDAQ
        if (shutdownRequested()) throw;
        std::cout << "[!] Command '" << line << "' failed: " << e.what() << std::endl;
    }
}

void CommandFetcher::requestShutdown() {
    g_shutdown = true;
    FEMDAQ::abrt = true;
}

bool CommandFetcher::shutdownRequested() { return g_shutdown.load(); }
