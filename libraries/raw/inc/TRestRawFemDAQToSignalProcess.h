#pragma once

#include <TFile.h>
#include <TTree.h>

#include <limits>
#include <vector>

#include "TRestEventProcess.h"
#include "TRestRawSignalEvent.h"
#include "TRestRun.h"

/// \class TRestRawFemDAQToSignalProcess
/// \brief Converts FemDAQ ROOT tree entries into REST raw signal events.
///
class TRestRawFemDAQToSignalProcess : public TRestEventProcess {
   private:
    Long64_t fInputTreeEntry = 0;

    TFile* fInputFile = nullptr;
    TTree* fInputTree = nullptr;

    Double_t fTimestamp = 0;
    Int_t fEventID = 0;
    std::vector<int>* fSignalIds = nullptr;
    std::vector<short>* fSignalValues = nullptr;
    Double_t fStartTimestamp = std::numeric_limits<double>::max();
    Double_t fEndTimestamp = 0;

   public:
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
    Long64_t GetInputEventCount() const override { return fInputTree ? fInputTree->GetEntries() : -1; }

    std::string GetInputEvent() const override { return "None"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }

    std::string GetClassName() const override { return "TRestRawFemDAQToSignalProcess"; }
};
