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
    bool fUseFeminosDaqRunInfo = true;
    bool fSetRunStartEndFromEvents = false;

    TRestRawFemDAQToSignalProcess();
    TRestRawFemDAQToSignalProcess(const std::string& instanceName, const YAML::Node& node);
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
