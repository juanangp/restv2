
#ifndef REST_APPLICATION_H
#define REST_APPLICATION_H

#include <cstdint>
#include <map>
#include <string>

#include "SimulationManager.h"

class G4VisManager;
class G4UIExecutive;
class G4RunManager;

class TGeoManager;

/// \file Application.h
/// \brief The restG4 application: command line options and the simulation run.

/// \brief Command line of restG4
namespace CommandLineOptions {
/// \brief Options of the restG4 command line; the ones not given (0, empty) keep the values of the
/// configuration file
struct Options {
    /// Configuration file (YAML), the positional argument or --config (-c)
    std::string configFile{};
    std::string outputFile{};    ///< --output (-o): output file, the outputFileName of the TRestRun
    std::string geometryFile{};  ///< --geometry (-g): GDML file, the gdmlFilename of the TRestGeant4Metadata

    /// --interactive (-i): not implemented, the option exits with an error
    bool interactive = false;

    /// --threads (-t, -j): number of worker threads, 0 for the serial run manager
    int nThreads = 0;

    int nEvents = 0;  ///< --events (-n): number of events (primaries) to simulate, nEvents of the metadata
    long seed = 0;    ///< --seed (-s): random seed (positive), the seed of the TRestGeant4Metadata

    /// --entries (-e): the simulation stops when this number of events has been stored
    int nRequestedEntries = 0;
    /// --time: time limit of the simulation in seconds (given as 1h20m30s, 10m, 30s...)
    int timeLimitSeconds = 0;

    int runNumber = -1;  ///< --runNumber: run number of the TRestRun, -1 to keep the configuration one

    /// --set name=value: the parameters of the configuration for this execution
    std::map<std::string, std::string> parameters;
    /// --update-gdml: the parameters written in the GDML files, without simulating
    bool updateGdml = false;

    // reference to original argc and argv necessary to pass to G4UIExecutive
    int argc;     ///< argc of main, for G4UIExecutive
    char** argv;  ///< argv of main, for G4UIExecutive
};

/// \brief Parses the command line. Without arguments, with --help or with a wrong option it writes the
/// usage and exits (exit code 0 for --help or no arguments, 1 for an error)
/// \return the options; the configuration file is mandatory
Options ProcessCommandLineOptions(int argc, char* const argv[]);
/// \brief Writes the options given in the command line
void PrintOptions(const Options& options);
/// \brief Writes the usage of restG4 (the command line options)
void ShowUsage();

}  // namespace CommandLineOptions

/// \class Application
/// \brief The restG4 application: reads the configuration, runs the Geant4 simulation and writes the
/// output file.
///
/// The configuration file must have a section of class TRestGeant4Manager, with a TRestRun, a
/// TRestGeant4Metadata and a TRestGeant4PhysicsLists. The `parameters` of the file and the --set of the
/// command line are resolved by TRestYAMLUtils, and the GDML file is searched relative to the configuration
/// file (then in the geometryPath of the metadata).
class Application {
   public:
    /// \brief Runs the simulation with the options of the command line.
    ///
    /// Applies the options to the metadata, creates the output file of the TRestRun, the run manager
    /// (serial or multithreading) and the user initializations, simulates nEvents (/run/beamOn), writes the
    /// geometry and the metadata (TRestRun, TRestGeant4Metadata, TRestGeant4PhysicsLists) and checks the
    /// output file. With `biasingVolumes` (nested, `nEvents` required) it runs the biasing stages
    /// (RunBiasingStages). With --update-gdml it only writes the parameters in the GDML files and exits.
    /// Fatal configuration errors end the program (exit(1)).
    void Run(const CommandLineOptions::Options& options);

    ~Application() = default;

   private:
    SimulationManager fSimulationManager;

    /// \brief Volume biasing: a Geant4 run (/run/beamOn) per stage. Stage 0 simulates nEvents of the
    /// generator; after stage k (k < number of volumes) the particles stored entering volume k are replayed
    /// biasingFactor times each by stage k + 1; only the last stage writes its events. It stops, with an
    /// error message and no output events, if a stage before the last one stores no particle, is stopped
    /// (time limit, CTRL-C) or would exceed the Geant4 limit of events of a run
    void RunBiasingStages(int64_t nEvents);

    static void ValidateOutputFile(const std::string& outputFile);
};

#endif  // REST_APPLICATION_H
