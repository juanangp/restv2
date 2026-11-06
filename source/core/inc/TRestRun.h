#ifndef TRESTRUN_H
#define TRESTRUN_H

#include <TFile.h>
#include <TTree.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <typeinfo>
#include <vector>

#include "TRestEvent.h"
#include "TRestMetadata.h"
#include "TRestMetadataStore.h"
#include "TRestRunInfo.h"

/// \class TRestRun
/// \brief Run-level container for metadata and ROOT I/O orchestration.
///
/// `TRestRun` owns input/output ROOT files, event trees, event object bindings,
/// and run descriptors used during processing and AOD persistence. The description of the run (numbers,
/// tags, runInfo, times) is its base TRestRunInfo.
class TRestRun : public TRestRunInfo {
   private:
    /// The metadata of the input file (without its TRestRun), with their objects
    TRestMetadataStore fMetadataStore;

    /// \brief Address of the data of the observable `name` of the output analysis tree, checked to be of
    /// type `type` (see GetObservable)
    const void* GetObservableAddress(const std::string& name, const std::type_info& type) const;

   public:
    std::string fInputFileName = "Null";
    /// Output file; "/dev/null" (default, also "Null" or empty) for no output: the pipeline runs but
    /// nothing is written
    std::string fOutputFileName = "/dev/null";
    std::string fMainDataPath = "";
    std::string fInputFormat = "";
    /// Last saved event type: output event of the last process of the chain that wrote the file (e.g.
    /// TRestTrackEvent), preserved from the input file otherwise. Selected event (GetEvent) when the file
    /// is opened
    std::string fOutputEvent = "";
    long long fEntry = 0;

    long long fInputEntries = 0;

    YAML::Node fInputFileNode;

    std::unique_ptr<TFile> fInputFile;
    std::unique_ptr<TFile> fOutputFile;

    std::map<std::string, TTree*> fInputEventTrees;
    std::map<std::string, TTree*> fOutputEventTrees;
    TTree* fAnalysisTree = nullptr;
    TTree* fOutputAnalysisTree = nullptr;

    std::map<std::string, TRestEvent*> fInputEvents;
    std::map<std::string, TRestEvent*> fOutputEvents;

    /// Selected event, see GetEvent/SetEvent
    TRestEvent* fEvent = nullptr;

    /// inheritInputRun: the run parameters not given in the configuration nor in the name of the input
    /// file (inputFormat) are taken from the run of the input file
    bool fInheritInputRun = false;
    int fOutputBasketSize = 1024 * 1024;  //!  bytes per branch basket of output event trees, 0 = ROOT default

    /// \brief Prefixes main data path to a relative filename.
    /// \param fileName Input filename.
    /// \return Resolved full path.
    std::string PrefixMainDataPath(const std::string& fileName) const;

    /// \brief Resolves the pattern of outputFileName and the main data path; "Null" or empty: "/dev/null".
    /// Throws if it is the input file (it would be overwritten).
    void ResolveOutputFileName();

    void FormOutputFile();

    /// \brief Parameters in the name of the input file, from the [parameter] of inputFormat.
    /// \return Parameter name (e.g. runNumber, runInfo.meshVoltage) and value as written in the name.
    std::map<std::string, std::string> ParseInputFormat() const;

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

    /// \brief Returns input file name.
    /// \return Input file path.
    std::string GetInputFileName() const { return fInputFileName; }

    /// \brief Returns output file name.
    /// \return Output file path.
    std::string GetOutputFileName() const { return fOutputFileName; }

    /// \brief Indicates whether output file is currently open.
    /// \return `true` when output file is open.
    bool HasOutputFileOpen() const { return fOutputFile != nullptr; }

    /// \brief Whether there is no output (outputFileName "/dev/null"): nothing is written.
    bool HasNullOutput() const { return fOutputFileName == "/dev/null"; }

    /// \brief Returns the last saved event type (see fOutputEvent), empty if unknown.
    std::string GetOutputEvent() const { return fOutputEvent; }

    /// \brief Sets input file name and mirrors value into YAML node when available.
    /// \param v Input file path.
    void SetInputFileName(const std::string& v);

    void SetOutputFileName(const std::string& v);

    /// \brief Sets the last saved event type of the output file and mirrors it into the YAML node.
    void SetOutputEvent(const std::string& v);

    /// \brief Opens input ROOT file and initializes input tree bindings.
    /// \param filename Input file path.
    void OpenInputFile(const std::string& filename);

    bool IsOpen() const { return fInputFile && !fInputFile->IsZombie(); }
    /// \brief Input ROOT file, or nullptr if none is open.
    TFile* GetInputFile() const { return fInputFile.get(); }

    /// \brief Opens output ROOT file and creates output trees.
    void OpenOutputFile(std::string fileName = "", const std::string& option = "RECREATE");

    /// \brief Closes open input/output files.
    void CloseFiles();

    /// \brief Returns number of entries in analysis tree.
    /// \return Entry count or `0` when analysis tree is missing.
    long long GetEntries() const {
        if (fInputEntries > 0) return fInputEntries;
        if (!fAnalysisTree) return 0;
        return fAnalysisTree->GetEntries();
    }

    long long GetSavedEntries() const {
        if (!fOutputAnalysisTree) return 0;
        return fOutputAnalysisTree->GetEntries();
    }

    long long GetEntryWithID(int eventID, int subEventID = -1, const std::string& tag = "");

    void PrintObservables();

    /// \brief Loads one entry from input trees.
    /// \param entry Entry index.
    /// \return `true` if entry was loaded.
    bool GetEntry(long long entry);
    bool GetNextEntry();

    long long GetCurrentEntry() const { return fEntry; }

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
        eventObject.WriteFormatVersion(fOutputEventTrees[className]);
        // Larger baskets mean fewer, bigger writes, which helps a lot on network filesystems
        if (fOutputBasketSize > 0) fOutputEventTrees[className]->SetBasketSize("*", fOutputBasketSize);

        // Event info branches of the analysis tree only once, bound to the first registered event (the
        // following ones copy its info); with an input analysis tree they are cloned from it
        if (fOutputAnalysisTree && !fAnalysisTree && !fOutputAnalysisTree->GetBranch("eventID")) {
            eventObject.TRestEvent::CreateBranches(fOutputAnalysisTree);
        }
    }

    /// \brief Registers an observable branch in analysis tree.
    /// \tparam T Observable value type.
    /// \param name Observable branch name.
    /// \param variable Variable bound to branch.
    template <typename T>
    void SetObservable(const std::string& name, T& variable) {
        if (!fOutputAnalysisTree) throw std::runtime_error("TRestRun: Output file not open");
        TBranch* branch = fOutputAnalysisTree->GetBranch(name.c_str());

        if constexpr (std::is_fundamental_v<T>) {
            if (!branch)
                fOutputAnalysisTree->Branch(name.c_str(), &variable);
            else
                fOutputAnalysisTree->SetBranchAddress(name.c_str(), &variable);
        } else {
            T** ptr = new T*(&variable);
            if (!branch) {
                fOutputAnalysisTree->Branch(name.c_str(), ptr);
            } else {
                fOutputAnalysisTree->SetBranchAddress(name.c_str(), ptr);
            }
        }
    }
    /// \brief Current value of an observable of the analysis tree. While processing (output analysis tree),
    /// of the input file or registered by a previous process (e.g. `g4Ana_gas_FirstHitTime`); reading a
    /// file without output (macros, browser), of the input file, with the entry of GetEntry. The reference
    /// is bound to the data of the branch (the input tree buffer or the variable of the process), so it can
    /// be taken once (e.g. in InitProcess, after the previous processes are initialized) and gives the value
    /// of each entry, without binding the branch again. Any type of observable (int, double, bool,
    /// std::string, std::vector<double>...); T must be the type of the branch. Throws if the observable is
    /// missing or has another type.
    template <typename T>
    const T& GetObservable(const std::string& name) const {
        return *static_cast<const T*>(GetObservableAddress(name, typeid(T)));
    }

    void SyncAnalysisTreeBranches();

    /// \brief Adds metadata object description into run store.
    /// \param instanceName Metadata instance name.
    /// \param className Metadata class name.
    /// \param configNode Metadata YAML node.
    void AddMetadata(TRestMetadata* metadata);
    void AddHistoricMetadata();

    /// \brief Retrieves metadata YAML node by instance name.
    /// \param instanceName Metadata instance name.
    /// \return YAML node (null node if not found).
    YAML::Node GetMetadata(const std::string& instanceName) const;

    TRestMetadata* GetMetadataClass(const std::string& className) const;
    /// \brief The metadata of the input file of class T or derived from it
    template <class T>
    std::vector<T*> GetMetadataOfType() const {
        return fMetadataStore.GetOfType<T>();
    }
    /// \brief The metadata of the input file (objects, with their objects loaded from it)
    const TRestMetadataStore& GetMetadataStore() const { return fMetadataStore; }

    /// \brief Selects the event of a tree and returns it.
    /// \param treeName Event tree/class name.
    /// \return Event, nullptr if the tree does not exist.
    TRestEvent* GetEvent(const std::string& treeName);

    /// \brief Returns the selected event: the last saved event type (GetOutputEvent) when the file is
    /// opened, or the one set with SetEvent.
    TRestEvent* GetEvent() { return fEvent; }

    /// \brief Selects the event of a tree.
    /// \param treeName Event tree/class name.
    void SetEvent(const std::string& treeName);

    std::map<std::string, TRestEvent*> GetEventMap() const { return fInputEvents; }

    /// \brief Fills all registered output trees for current event.
    void Fill();

    /// \brief Loads run configuration from YAML node.
    void LoadConfig() override;

    /// \brief No-op initialize hook.
    void Initialize() override {}

    /// \brief Prints run metadata summary.
    void PrintMetadata() const override;

    void PrintAllMetadata() const;
};

#endif
