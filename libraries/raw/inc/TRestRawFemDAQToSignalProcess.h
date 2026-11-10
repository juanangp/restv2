#ifndef TRESTRAWFEMDAQTOSIGNALPROCESS_H
#define TRESTRAWFEMDAQTOSIGNALPROCESS_H

class TFile;
class TTree;

#include <limits>
#include <vector>

#include "TRestEventProcess.h"
class TRestRawSignalEvent;

/// \class TRestRawFemDAQToSignalProcess
/// \brief Converts the ROOT files of the old FEMDAQ (tree `SignalEvent`, before restDAQ) into REST raw
/// signal events. The files of restDAQ are REST files, read directly.
///
/// The input is `inputFileName` of the process or, if empty, the one of the TRestRun (a .root file). From
/// the branches `eventID`, `timestamp`, `signalsID` and `pulses` (512 samples per signal, one signal after
/// the other) of each entry it builds one event. The run start and end times are taken from the
/// `startTime` and `endTime` strings of the file (if present); with `useFeminosDaqRunInfo` the run tag and
/// type are taken from the `run` section of the `RunConfigYAML` string of the file; with
/// `setRunStartEndFromEvents` the start and end times are the ones of the first and last events. No
/// observables.
///
/// Example:
/// \code
/// femDAQToSignal:
///   class: "TRestRawFemDAQToSignalProcess"
///   inputFileName: ""                  # empty: inputFileName of the run
///   useFeminosDaqRunInfo: true
///   setRunStartEndFromEvents: false
/// \endcode
///
/// \author JuanAn García <juanangp@unizar.es>
class TRestRawFemDAQToSignalProcess : public TRestEventProcess {
   private:
    long long fInputTreeEntry = 0;

    TFile* fInputFile = nullptr;
    TTree* fInputTree = nullptr;

    double fTimestamp = 0;
    int fEventID = 0;
    std::vector<int>* fSignalIds = nullptr;
    std::vector<short>* fSignalValues = nullptr;
    double fStartTimestamp = std::numeric_limits<double>::max();
    double fEndTimestamp = 0;

   public:
    /// FEMDAQ file to read, the input file of the run if not given
    std::string fInputFileName;
    /// Take the run tag and type from the RunConfigYAML string of the file (YAML `useFeminosDaqRunInfo`)
    bool fUseFeminosDaqRunInfo = true;
    /// Set the run start and end times from the first and last events (YAML `setRunStartEndFromEvents`)
    bool fSetRunStartEndFromEvents = false;

    /// \brief Default constructor (no configuration).
    TRestRawFemDAQToSignalProcess();
    /// \brief Constructor from the YAML node of the process section.
    TRestRawFemDAQToSignalProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructor from a YAML file and the name of the process section.
    TRestRawFemDAQToSignalProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads input file names and conversion options from YAML.
    virtual void LoadConfig() override;
    /// \brief Opens input ROOT resources and connects branch addresses.
    void InitProcess() override;
    /// \brief Converts one FemDAQ entry into a TRestRawSignalEvent payload.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Closes input resources and updates run-level timestamps.
    void EndProcess() override;
    /// \brief Returns the number of available entries in the input tree.
    long long GetInputEventCount() const override { return fInputTree ? fInputTree->GetEntries() : -1; }
    /// \brief True once all the entries of the input tree have been read.
    bool InputFinished() const override { return fInputTree && fInputTreeEntry >= fInputTree->GetEntries(); }

    std::string GetInputEvent() const override { return "None"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }

    std::string GetClassName() const override { return "TRestRawFemDAQToSignalProcess"; }
};

#endif
