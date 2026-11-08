/// \file main.cxx
/// \brief restG4: Geant4 simulation with a REST configuration (YAML), output as a REST run file with
/// TRestGeant4Event events.
///
///    restG4 config.yaml [options]
///
/// The configuration has a section of class TRestGeant4Manager with a TRestRun, a TRestGeant4Metadata and a
/// TRestGeant4PhysicsLists (see packages/restG4/README.md and the examples). Options (Application.cxx,
/// CommandLineOptions::ProcessCommandLineOptions), which override the values of the configuration:
///
/// * **-h, --help**: usage.
/// * **-c, --config** file: configuration file (same as the positional argument).
/// * **-o, --output** file: output file.
/// * **-n, --events** N: number of events to simulate (1E5 notation accepted).
/// * **-e, --entries** N: stop when N events have been stored (1E5 notation accepted).
/// * **--time** 1h20m30s: time limit of the simulation.
/// * **-g, --geometry** file: GDML geometry file.
/// * **-s, --seed** N: random seed (positive).
/// * **-t, -j, --threads** N: number of worker threads (multithreading), 0 (default) serial.
/// * **--runNumber** N: run number.
/// * **--set** name=value: a parameter of the `parameters` section, for this execution (repeatable).
/// * **--update-gdml**: writes the parameters in the constants and entities of the GDML files and exits.
/// * **-i, --interactive**: not implemented (exits with an error).
///
/// The exit code is 1 on an error.

#include <TRestSystemOfUnits.h>

#include "Application.h"

using namespace std;

constexpr auto banner =
    "\n"
    "                         888     .d8888b.      d8888 \n"
    "                         888    d88P  Y88b    d8P888 \n"
    "                         888    888    888   d8P 888 \n"
    "888d888 .d88b.  .d8888b  888888 888         d8P  888 \n"
    "888P\"  d8P  Y8b 88K      888    888  88888 d88   888 \n"
    "888    88888888 \"Y8888b. 888    888    888 8888888888\n"
    "888    Y8b.          X88 Y88b.  Y88b  d88P       888 \n"
    "888     \"Y8888   88888P'  \"Y888  \"Y8888P88       888 \n"
    "\n"
    "🔍 Source code: https://github.com/rest-for-physics/restG4\n"
    "🐞 Bug reports: https://github.com/rest-for-physics/restG4/issues\n";

int main(int argc, char* argv[]) {
    REST_Units::SetGeoManagerDefaultUnits();

    cout << banner << endl;

    Application app;

    CommandLineOptions::Options options = CommandLineOptions::ProcessCommandLineOptions(argc, argv);

    try {
        app.Run(options);
    } catch (const std::exception& error) {
        cerr << "restG4: " << error.what() << endl;
        return 1;
    }
}
