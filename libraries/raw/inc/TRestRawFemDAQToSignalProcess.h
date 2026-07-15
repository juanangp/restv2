#pragma once

#include "TRestEventProcess.h"
#include "TRestRawSignalEvent.h"
#include "TRestRun.h"
#include <vector>
#include <TFile.h>
#include <TTree.h>
#include <limits>

///
/// Read data from the root file output of femdaq into a TRestRawSignalEvent
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

    virtual void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override;
    Long64_t GetInputEventCount() const override {
        return fInputTree ? fInputTree->GetEntries() : -1;
    }

    std::string GetInputEvent() const override { return "None";}
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent";}

    std::string GetClassName() const override { return "TRestRawFemDAQToSignalProcess"; }
};
