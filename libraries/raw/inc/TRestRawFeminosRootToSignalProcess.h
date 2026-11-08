#ifndef TRESTRAWFEMINOSROOTTOSIGNALPROCESS_H
#define TRESTRAWFEMINOSROOTTOSIGNALPROCESS_H

class TFile;
class TTree;

#include <limits>
#include <vector>

#include "TRestEventProcess.h"

/// \class TRestRawFeminosRootToSignalProcess
/// \brief Converts the ROOT files of feminos-daq (https://github.com/rest-for-physics/feminos-daq) into
/// TRestRawSignalEvent.
///
/// The input file is the `inputFileName` of the TRestRun, with the trees `events` (timestamp in ms,
/// signal_ids, signal_values: the samples of all the signals one after the other) and `run`. The event ID
/// is the entry of the `events` tree.
///
/// With `useFeminosDaqRunInfo` (true) the run takes from the `run` tree its number, tag, description
/// (comments) and start time (the ones not empty, they replace the configuration and the input file
/// name), and adds to `runInfo` the detector, driftField (V/cm/bar), meshVoltage (V) and pressure (bar),
/// unless the configuration already gives them. Otherwise, or if the `run` tree has no `timestamp`, the
/// start time is the one of the first event.
/// The end time is always the one of the last event. No observables.
///
/// Example (see libraries/raw/examples/FeminosRootToRaw.yaml):
/// \code
/// feminosRootToSignal:
///   class: "TRestRawFeminosRootToSignalProcess"
///   useFeminosDaqRunInfo: true
/// \endcode
class TRestRawFeminosRootToSignalProcess : public TRestEventProcess {
   private:
    long long fInputTreeEntry = 0;

    TFile* fInputFile = nullptr;
    TTree* fInputTree = nullptr;

    unsigned long long fTimestamp = 0;
    std::vector<unsigned short>* fSignalIds = nullptr;
    std::vector<unsigned short>* fSignalValues = nullptr;
    unsigned long long fStartTimestamp = std::numeric_limits<unsigned long long>::max();
    unsigned long long fEndTimestamp = 0;
    /// True when the run start time was taken from the `run` tree
    bool fStartFromRunTree = false;

    /// \brief Copies the run information of the `run` tree to the TRestRun. Returns true if it set the
    /// run start time (the tree has a `timestamp` branch).
    bool ReadRunInfo(TTree* runTree);

   public:
    /// Take the run number, tag, description, start time and runInfo from the `run` tree (YAML
    /// `useFeminosDaqRunInfo`)
    bool fUseFeminosDaqRunInfo = true;

    /// \brief Default constructor (no configuration).
    TRestRawFeminosRootToSignalProcess();
    /// \brief Constructor from the YAML node of the process section.
    TRestRawFeminosRootToSignalProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructor from a YAML file and the name of the process section.
    TRestRawFeminosRootToSignalProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads `useFeminosDaqRunInfo`.
    void LoadConfig() override;
    /// \brief Opens the input file, reads the run information and connects the event branches.
    void InitProcess() override;
    /// \brief Converts one entry of the `events` tree into a TRestRawSignalEvent.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Sets the run times and closes the input file.
    void EndProcess() override;

    /// \brief Number of entries of the `events` tree (-1 before InitProcess).
    long long GetInputEventCount() const override { return fInputTree ? fInputTree->GetEntries() : -1; }
    /// \brief True once all the entries of the `events` tree have been read.
    bool InputFinished() const override { return fInputTree && fInputTreeEntry >= fInputTree->GetEntries(); }

    std::string GetInputEvent() const override { return "None"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }

    std::string GetClassName() const override { return "TRestRawFeminosRootToSignalProcess"; }
};

#endif
