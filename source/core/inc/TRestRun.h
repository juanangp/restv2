#pragma once

#include <TFile.h>
#include <TTree.h>

#include <memory>
#include <string>
#include <vector>

#include "TRestEvent.h"
#include "TRestMetadata.h"
#include "TRestTools.h"

/// \class TRestRun
/// \brief Run-level container for metadata and ROOT I/O orchestration.
///
/// `TRestRun` owns input/output ROOT files, event trees, event object bindings,
/// and run descriptors used during processing and AOD persistence.
class TRestRun : public TRestMetadata {
    DECLARE_LOG_CLASS(TRestRun)

   public:
    int fRunNumber = -1;
    int fSubRunNumber = 0;
    std::string fRunType = "Null";
    std::string fRunUser = "Null";
    std::string fRunTag = "Null";
    std::string fRunDescription = "Null";
    std::string fExperimentName = "Null";

    std::string fInputFileName = "Null";
    std::string fOutputFileName = "rest_default.root";
    std::string fMainDataPath = "";
    std::string fInputFormat = "";
    double fStartTime = 0;
    double fEndTime = 0;
    int fEntriesSaved = 0;

    YAML::Node fInputFileNode;

    std::unique_ptr<TFile> fInputFile;
    std::unique_ptr<TFile> fOutputFile;

    std::map<std::string, TTree*> fInputEventTrees;
    std::map<std::string, TTree*> fOutputEventTrees;
    TTree* fAnalysisTree = nullptr;

    std::map<std::string, TRestEvent*> fInputEvents;
    std::map<std::string, TRestEvent*> fOutputEvents;

    std::string fConfigRunNumber = "";
    std::string fConfigSubRunNumber = "";
    std::string fConfigRunType = "Null";
    std::string fConfigRunUser = "Null";
    std::string fConfigRunTag = "Null";
    std::string fConfigRunDescription = "Null";
    std::string fConfigExperimentName = "Null";
    bool fIsInitializedFromConfig = false;

    /// \brief Resolves placeholders in file patterns.
    /// \param pattern Input filename pattern.
    /// \return Resolved filename.
    std::string ResolveFilePattern(const std::string& pattern) const;

    /// \brief Prefixes main data path to a relative filename.
    /// \param fileName Input filename.
    /// \return Resolved full path.
    std::string PrefixMainDataPath(const std::string& fileName) const;

    /// \brief Resolves effective input filename from inputFileName/inputFormat.
    void ResolveInputFormat();

    /// \brief Default constructor.
    TRestRun();

    /// \brief Constructor from input file name.
    /// \param inputFileName Input ROOT file path.
    explicit TRestRun(const std::string& inputFileName);

    /// \brief Constructor from YAML node.
    /// \param instanceName Instance name.
    /// \param node YAML node with parameters.
    TRestRun(const std::string& instanceName, const YAML::Node& node);

    /// \brief Constructor from YAML file and section.
    /// \param fileName YAML file path.
    /// \param sectionName YAML section name.
    TRestRun(const std::string& fileName, const std::string& sectionName);

    /// \brief Destructor.
    ~TRestRun() override;

    /// \brief Returns class name for metadata introspection.
    /// \return Class name string.
    std::string GetClassName() const override { return "TRestRun"; }

    /// \brief Returns run number.
    /// \return Run number.
    int GetRunNumber() const { return fRunNumber; }

    /// \brief Returns parent run number.
    /// \return Parent run number.
    int GetSubRunNumber() const { return fSubRunNumber; }

    /// \brief Returns run type label.
    /// \return Run type.
    std::string GetRunType() const { return fRunType; }

    /// \brief Returns run user label.
    /// \return Run user.
    std::string GetRunUser() const { return fRunUser; }

    /// \brief Returns run tag.
    /// \return Run tag.
    std::string GetRunTag() const { return fRunTag; }

    /// \brief Returns experiment name.
    /// \return Experiment name.
    std::string GetExperimentName() const { return fExperimentName; }

    /// \brief Returns input file name.
    /// \return Input file path.
    std::string GetInputFileName() const { return fInputFileName; }

    /// \brief Returns output file name.
    /// \return Output file path.
    std::string GetOutputFileName() const { return fOutputFileName; }

    /// \brief Indicates whether output file is currently open.
    /// \return `true` when output file is open.
    bool HasOutputFileOpen() const { return fOutputFile != nullptr; }

    /// \brief Sets run number.
    /// \param v Run number.
    void SetRunNumber(int v) { fRunNumber = v; }

    /// \brief Sets parent run number.
    /// \param v Parent run number.
    void SetSubRunNumber(int v) { fSubRunNumber = v; }

    /// \brief Sets run type.
    /// \param v Run type string.
    void SetRunType(const std::string& v) { fRunType = v; }

    /// \brief Sets run user.
    /// \param v Run user string.
    void SetRunUser(const std::string& v) { fRunUser = v; }

    /// \brief Sets run tag.
    /// \param v Run tag string.
    void SetRunTag(const std::string& v) { fRunTag = v; }

    /// \brief Sets run description.
    /// \param v Run description string.
    void SetRunDescription(const std::string& v) { fRunDescription = v; }

    /// \brief Sets input file name and mirrors value into YAML node when available.
    /// \param v Input file path.
    void SetInputFileName(const std::string& v) {
        fInputFileName = v;
        if (fNode) TRestTools::SetNodeParameter(fNode, "inputFileName", fInputFileName);
    }

    /// \brief Sets experiment name.
    /// \param v Experiment name.
    void SetExperimentName(const std::string& v) { fExperimentName = v; }

    /// \brief Sets start timestamp.
    /// \param v Start time.
    void SetStartTimeStamp(double v) { fStartTime = v; }

    /// \brief Sets end timestamp.
    /// \param v End time.
    void SetEndTimeStamp(double v) { fEndTime = v; }

    /// \brief Opens input ROOT file and initializes input tree bindings.
    /// \param filename Input file path.
    void OpenInputFile(const std::string& filename);

    /// \brief Opens output ROOT file and creates output trees.
    void OpenOutputFile();

    /// \brief Closes open input/output files.
    void CloseFiles();

    /// \brief Returns number of entries in analysis tree.
    /// \return Entry count or `0` when analysis tree is missing.
    Long64_t GetEntries() const {
        if (!fAnalysisTree) return 0;
        return fAnalysisTree->GetEntries();
    }

    /// \brief Loads one entry from input trees.
    /// \param entry Entry index.
    /// \return `true` if entry was loaded.
    bool GetEntry(Long64_t entry);

    /// \brief Checks whether an input event tree exists.
    /// \param treeName Event tree/class name.
    /// \return `true` when event is available.
    bool HasEvent(const std::string& treeName) const;

    /// \brief Returns typed input event by tree name.
    /// \tparam T Event type.
    /// \param treeName Event tree/class name.
    /// \return Reference to typed input event.
    template <typename T = TRestEvent>
    T& GetEvent(const std::string& treeName) {
        auto it = fInputEvents.find(treeName);
        if (it == fInputEvents.end()) {
            throw std::runtime_error("TRestRun: Tree '" + treeName + "' does not exist.");
        }
        return dynamic_cast<T&>(*(it->second));
    }

    /// \brief Registers an output event object and creates its output tree if needed.
    /// \tparam T Event type.
    /// \param className Output tree/class name.
    /// \param eventObject Event instance to bind.
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

        if (fOutputEvents.size() > 0 && fAnalysisTree) {
            eventObject.TRestEvent::CreateBranches(fAnalysisTree);
        }
    }

    /// \brief Registers an observable branch in analysis tree.
    /// \tparam T Observable value type.
    /// \param name Observable branch name.
    /// \param variable Variable bound to branch.
    template <typename T>
    void SetObservable(const std::string& name, T& variable) {
        if (!fAnalysisTree) throw std::runtime_error("TRestRun: Output file not open");
        fAnalysisTree->Branch(name.c_str(), &variable);
    }

    /// \brief Binds an observable branch for reading.
    /// \tparam T Observable value type.
    /// \param name Observable branch name.
    /// \param variable Variable receiving branch values.
    template <typename T>
    void GetObservable(const std::string& name, T& variable) {
        if (!fAnalysisTree) throw std::runtime_error("TRestRun: Input file missing");

        fAnalysisTree->SetBranchAddress(name.c_str(), &variable);
    }

    /// \brief Adds metadata object description into run store.
    /// \param instanceName Metadata instance name.
    /// \param className Metadata class name.
    /// \param configNode Metadata YAML node.
    void AddMetadata(const std::string& instanceName,
                     const YAML::Node& configNode);

    /// \brief Retrieves metadata YAML node by instance name.
    /// \param instanceName Metadata instance name.
    /// \return YAML node (null node if not found).
    YAML::Node GetMetadata(const std::string& instanceName) const;

    /// \brief Returns non-const reference to an input event by tree name.
    /// \param treeName Event tree/class name.
    /// \return Input event reference.
    TRestEvent& GetInputEvent(const std::string& treeName);

    /// \brief Fills all registered output trees for current event.
    void Fill();

    /// \brief Loads run configuration from YAML node.
    void LoadConfig() override;

    /// \brief No-op initialize hook.
    void Initialize() override {}

    /// \brief Prints run metadata summary.
    void PrintMetadata() override;
};
