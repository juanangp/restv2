#pragma once

#include <TFile.h>
#include <TTree.h>

#include <memory>
#include <string>

#include "TRestEvent.h"
#include "TRestMetadata.h"

// ============================================================
//  TRestRun
//
//  Holds run-level metadata and owns the ROOT file I/O.
//  Data is stored using the AOD pattern:
//    - One TTree ("EventTree") with one branch per event type.
//    - Branches hold plain C++ objects – no schema evolution,
//      no RNTuple experimental API.
// ============================================================
class TRestRun : public TRestMetadata {
    DECLARE_LOG_CLASS(TRestRun)

   protected:
    int         fRunNumber       = 0;
    int         fParentRunNumber = 0;
    std::string fRunType         = "Null";
    std::string fRunUser         = "Null";
    std::string fRunTag          = "Null";
    std::string fRunDescription  = "Null";
    std::string fExperimentName  = "Null";

    std::string fInputFileName  = "Null";
    std::string fOutputFileName = "rest_default.root";
    double      fStartTime      = 0;
    double      fEndTime        = 0;
    int         fEntriesSaved   = 0;

    // --- AOD I/O ---
    std::unique_ptr<TFile> fOutputFile;
    TTree*                 fEventTree = nullptr;  // owned by fOutputFile

   public:
    TRestRun(const std::string& sectionName, const YAML::Node& node);
    TRestRun(const std::string& fileName,    const std::string& sectionName);

    std::string GetClassName() const override { return "TRestRun"; }

    // Run info getters
    int         GetRunNumber()       const { return fRunNumber; }
    int         GetParentRunNumber() const { return fParentRunNumber; }
    std::string GetRunType()         const { return fRunType; }
    std::string GetRunUser()         const { return fRunUser; }
    std::string GetRunTag()          const { return fRunTag; }
    std::string GetExperimentName()  const { return fExperimentName; }
    std::string GetInputFileName()   const { return fInputFileName; }
    std::string GetOutputFileName()  const { return fOutputFileName; }

    // --- AOD file management ---

    /// Open the output ROOT file and create the EventTree.
    void OpenOutputFile();

    /// Register a branch for a concrete event type T.
    /// Must be called before the first FillEvent().
    template <typename T>
    void RegisterEventBranch(const std::string& branchName) {
        static_assert(std::is_base_of_v<TRestEvent, T>,
                      "T must derive from TRestEvent");
        if (!fEventTree)
            throw std::runtime_error("TRestRun: call OpenOutputFile() first");
        auto* obj = new T();
        fEventTree->Branch(branchName.c_str(), &obj);
    }

    /// Fill one entry into the EventTree.
    void FillEvent();

    /// Write and close the output file.
    void CloseOutputFile();

    // TRestMetadata interface
    void LoadConfig()    override;
    void Initialize()    override {}
    void PrintMetadata() override;
};
