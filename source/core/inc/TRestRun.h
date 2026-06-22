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
// ============================================================
class TRestRun : public TRestMetadata {
    DECLARE_LOG_CLASS(TRestRun)

   protected:
    int fRunNumber = 0;
    int fParentRunNumber = 0;
    std::string fRunType = "Null";
    std::string fRunUser = "Null";
    std::string fRunTag = "Null";
    std::string fRunDescription = "Null";
    std::string fExperimentName = "Null";

    std::string fInputFileName = "Null";
    std::string fOutputFileName = "rest_default.root";
    double fStartTime = 0;
    double fEndTime = 0;
    int fEntriesSaved = 0;

    std::unique_ptr<TFile> fInputFile;
    std::unique_ptr<TFile> fOutputFile;

    std::map<std::string, TTree*> fInputEventTrees;
    std::map<std::string, TTree*> fOutputEventTrees;
    TTree* fAnalysisTree = nullptr;

    std::map<std::string, TRestEvent*> fInputEvents;
    std::map<std::string, TRestEvent*> fOutputEvents;

   public:
    TRestRun();
    explicit TRestRun(const std::string& inputFileName);
    TRestRun(const std::string& instanceName, const std::string& sectionName, const YAML::Node& node);
    TRestRun(const std::string& fileName, const std::string& sectionName);

    ~TRestRun() override;

    std::string GetClassName() const override { return "TRestRun"; }

    // Run info getters
    int GetRunNumber() const { return fRunNumber; }
    int GetParentRunNumber() const { return fParentRunNumber; }
    std::string GetRunType() const { return fRunType; }
    std::string GetRunUser() const { return fRunUser; }
    std::string GetRunTag() const { return fRunTag; }
    std::string GetExperimentName() const { return fExperimentName; }
    std::string GetInputFileName() const { return fInputFileName; }
    std::string GetOutputFileName() const { return fOutputFileName; }

    void OpenInputFile(const std::string& filename);
    void OpenOutputFile();
    void CloseFiles();

    Long64_t GetEntries() const {
        if (!fAnalysisTree) return 0;
        return fAnalysisTree->GetEntries();
    }
    bool GetEntry(Long64_t entry);
    bool HasEvent(const std::string& treeName) const;

    template <typename T = TRestEvent>
    T& GetEvent(const std::string& treeName) {
        auto it = fInputEvents.find(treeName);
        if (it == fInputEvents.end()) {
            throw std::runtime_error("TRestRun: Tree '" + treeName + "' does not exist.");
        }
        return dynamic_cast<T&>(*(it->second));
    }

    template <typename T>
    void RegisterEvent(const std::string& className, T& eventObject) {
        static_assert(std::is_base_of_v<TRestEvent, T>, "T must inherit from TRestEvent");

        if (!fOutputFile) {
            throw std::runtime_error("TRestRun: No output file added");
        }
        
        if (fOutputEventTrees.find(className) == fOutputEventTrees.end()) {
            fOutputFile->cd();
            auto* tree = new TTree(className.c_str(), (className + " AOD Event Tree").c_str());
            tree->SetAutoSave(0);
            fOutputEventTrees[className] = tree;
        }

        fOutputEvents[className] = &eventObject;

        eventObject.CreateBranches(fOutputEventTrees[className]);
        
        if (fOutputEvents.size() == 1 && fAnalysisTree) {
            eventObject.TRestEvent::CreateBranches(fAnalysisTree);
        }
    }

    template <typename T>
    void SetObservable(const std::string& name, T& variable) {
        if (!fAnalysisTree) throw std::runtime_error("TRestRun: Output file not open");
        fAnalysisTree->Branch(name.c_str(), &variable);
    }
    template <typename T>
    void GetObservable(const std::string& name, T& variable) {
        if (!fAnalysisTree) throw std::runtime_error("TRestRun: Input file missing");

        fAnalysisTree->SetBranchAddress(name.c_str(), &variable);
    }

    void AddMetadata(const std::string& instanceName, const std::string& className,
                     const YAML::Node& configNode);
    YAML::Node GetMetadata(const std::string& instanceName) const;

    TRestEvent& GetInputEvent(const std::string& treeName);
    void Fill();

    // TRestMetadata interface
    void LoadConfig() override;
    void Initialize() override {}
    void PrintMetadata() override;
};
