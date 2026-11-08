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
///
/// Configuration (a section inside the TRestManager of restManager), with the keys of TRestRunInfo plus
/// the processing ones:
///
/// \code{.yaml}
/// run:
///   class: TRestRun
///   name: restRun
///   runNumber: 1              # "auto": a new run number ($REST_HOME/.rest/runNumber)
///   subRunNumber: 0
///   runType: Test
///   runTag: SignalPeaks
///   runDescription: SignalPeaks
///   experimentName: Test
///   runInfo: {meshVoltage: 300 V}
///   inputFileName: SignalPeaks_input.root
///   # [parameter] of the name of the input file, used for the run parameters not given
///   inputFormat: "R[runNumber]_[runTag]_V[runInfo.meshVoltage].root"
///   inheritInputRun: false    # true: the parameters not given are taken from the run of the input file
///   mainDataPath: /data/rest  # prefix of relative output file names
///   outputFileName: "R[runNumber]_[runTag].root"   # "/dev/null" (default): nothing written
///   outputBasketSize: 1048576 # bytes per branch basket of the output event trees, 0: ROOT default
///   # outputEvent is written by REST: the last saved event type
/// \endcode
///
/// The run parameters not given in the configuration are taken from the name of the input file
/// (inputFormat), otherwise from the run of the input file if inheritInputRun is true; startTime, endTime,
/// mergedSubRuns and runInfo always come from the input run (the keys of the configuration replace those
/// of runInfo). Input files that are not REST run files (e.g. DAQ files) are not opened: they are read
/// by a process. A pattern with wildcards (`*`, `?`) is not opened either (used by TRestAnalysisPlot).
///
/// A REST run file contains the `AnalysisTree` (event info and observables), one tree per saved event
/// type (named as the event class) and the `RESTMetadataStore` directory with the configurations.
class TRestRun : public TRestRunInfo {
   private:
    /// The metadata of the input file (without its TRestRun), with their objects
    TRestMetadataStore fMetadataStore;

    /// \brief Address of the data of the observable `name` of the output analysis tree, checked to be of
    /// type `type` (see GetObservable)
    const void* GetObservableAddress(const std::string& name, const std::type_info& type) const;

    /// Pointers to the objects bound to the observable branches (SetObservable of a non-fundamental type),
    /// owned here so they live as long as the output analysis tree
    std::vector<std::shared_ptr<void>> fObservablePointers;  //!

    /// Whether the output analysis tree copies the observables of the input analysis tree (see
    /// SetInputAnalysisStorage)
    bool fInputAnalysisStorage = true;  //!

    /// \brief Whether the output analysis tree is a copy of the input one (input analysis tree and
    /// fInputAnalysisStorage)
    bool CopiesInputAnalysisTree() const { return fAnalysisTree && fInputAnalysisStorage; }

   public:
    /// Input file (`inputFileName`), "Null" if none.
    std::string fInputFileName = "Null";
    /// Output file; "/dev/null" (default, also "Null" or empty) for no output: the pipeline runs but
    /// nothing is written
    std::string fOutputFileName = "/dev/null";
    /// Directory prefixed to a relative output file name (`mainDataPath`).
    std::string fMainDataPath = "";
    /// Pattern of the input file name (`inputFormat`) with [parameter] placeholders, used to build the
    /// input file name when inputFileName is not given and to take run parameters from it.
    std::string fInputFormat = "";
    /// Last saved event type: output event of the last process of the chain that wrote the file (e.g.
    /// TRestTrackEvent), preserved from the input file otherwise. Selected event (GetEvent) when the file
    /// is opened
    std::string fOutputEvent = "";
    /// Current entry of the input file (GetEntry).
    long long fEntry = 0;

    /// Entries of the input analysis tree.
    long long fInputEntries = 0;

    /// Configuration of the TRestRun stored in the input file.
    YAML::Node fInputFileNode;

    std::unique_ptr<TFile> fInputFile;   ///< Input ROOT file.
    std::unique_ptr<TFile> fOutputFile;  ///< Output ROOT file.

    /// Event trees of the input file, by event class name (owned by the input file).
    std::map<std::string, TTree*> fInputEventTrees;
    /// Event trees of the output file, by event class name (owned by the output file).
    std::map<std::string, TTree*> fOutputEventTrees;
    TTree* fAnalysisTree = nullptr;        ///< `AnalysisTree` of the input file.
    TTree* fOutputAnalysisTree = nullptr;  ///< `AnalysisTree` of the output file.

    /// Events bound to the input event trees, by class name (owned by the run).
    std::map<std::string, TRestEvent*> fInputEvents;
    /// Events registered for output (RegisterEvent), by class name (not owned).
    std::map<std::string, TRestEvent*> fOutputEvents;

    /// Selected event, see GetEvent/SetEvent
    TRestEvent* fEvent = nullptr;

    /// inheritInputRun: the run parameters not given in the configuration nor in the name of the input
    /// file (inputFormat) are taken from the run of the input file
    bool fInheritInputRun = false;
    /// Bytes per branch basket of the output event trees (`outputBasketSize`), 0 = ROOT default.
    int fOutputBasketSize = 1024 * 1024;  //!  bytes per branch basket of output event trees, 0 = ROOT default

    /// \brief Prefixes main data path to a relative filename (absolute names, "Null" and "/dev/null" are
    /// returned unchanged).
    /// \param fileName Input filename.
    /// \return Resolved full path.
    std::string PrefixMainDataPath(const std::string& fileName) const;

    /// \brief Resolves the pattern of outputFileName and the main data path; "Null" or empty: "/dev/null".
    /// Throws if it is the input file (it would be overwritten).
    void ResolveOutputFileName();

    /// \brief Resolves the output file name, writes the resolved parameters to the node and opens the
    /// output file (OpenOutputFile).
    void FormOutputFile();

    /// \brief Parameters in the name of the input file, from the [parameter] of inputFormat (its file name,
    /// without directories nor extension; each [parameter] matches a part without "_", the rest is
    /// literal). Throws if a [parameter] is not a run parameter nor runInfo.key; if the name of the input
    /// file does not match, a warning is printed and nothing is returned.
    /// \return Parameter name (e.g. runNumber, runInfo.meshVoltage) and value as written in the name.
    std::map<std::string, std::string> ParseInputFormat() const;

    /// \brief Default constructor.
    TRestRun();

    /// \brief Constructor from input file name.
    /// \param inputFileName Input ROOT file path.
    explicit TRestRun(const std::string& inputFileName);

    /// \brief Constructor from YAML node.
    /// \param instanceName Instance name.
    /// \param node YAML node with parameters (LoadConfig is called, opening the input file).
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

    /// \brief Sets output file name and mirrors value into YAML node when available.
    /// \param v Output file path.
    void SetOutputFileName(const std::string& v);

    /// \brief Sets the last saved event type of the output file and mirrors it into the YAML node.
    void SetOutputEvent(const std::string& v);

    /// \brief Opens input ROOT file and initializes input tree bindings. Throws if the file is not a REST
    /// run file (nothing is done for an empty name or if a file is already open).
    /// \param filename Input file path.
    void OpenInputFile(const std::string& filename);
    /// \brief The parameters of the run of the input file (runNumber, runTag, runType..., the times of the
    /// data taking, entriesSaved and runInfo) as those of this run; used by TRestRun(inputFileName), which
    /// has no configuration (a configured run takes them with inheritInputRun or inputFormat)
    void LoadInputRunParameters();

    /// \brief Whether an input file is open and valid.
    bool IsOpen() const { return fInputFile && !fInputFile->IsZombie(); }
    /// \brief Input ROOT file, or nullptr if none is open.
    TFile* GetInputFile() const { return fInputFile.get(); }

    /// \brief Whether the output analysis tree copies the observables of the input file (default true).
    /// When false, it only has the event info branches and the observables of the processes (as without
    /// input analysis tree). Must be set before OpenOutputFile.
    void SetInputAnalysisStorage(bool store) { fInputAnalysisStorage = store; }

    /// \brief Opens output ROOT file and creates the output analysis tree (a copy of the input one, see
    /// SetInputAnalysisStorage, or a new one). With "UPDATE" only the file is opened.
    /// \param fileName Output file, fOutputFileName if empty (and stored there).
    /// \param option TFile open option.
    void OpenOutputFile(std::string fileName = "", const std::string& option = "RECREATE");

    /// \brief Writes the output trees and closes open input/output files.
    void CloseFiles();

    /// \brief Returns number of entries of the input file (its analysis tree).
    /// \return Entry count or `0` when analysis tree is missing.
    long long GetEntries() const {
        if (fInputEntries > 0) return fInputEntries;
        if (!fAnalysisTree) return 0;
        return fAnalysisTree->GetEntries();
    }

    /// \brief Entries filled in the output analysis tree, 0 without output.
    long long GetSavedEntries() const {
        if (!fOutputAnalysisTree) return 0;
        return fOutputAnalysisTree->GetEntries();
    }

    /// \brief Finds and loads (GetEntry) the first entry of the input analysis tree with the given IDs.
    /// \param eventID Event ID.
    /// \param subEventID Subevent ID, -1 for any.
    /// \param tag Subevent tag, empty for any.
    /// \return Entry index, -1 if not found.
    long long GetEntryWithID(int eventID, int subEventID = -1, const std::string& tag = "");

    /// \brief Prints the observables of the input analysis tree for the current entry (TTree::Show).
    void PrintObservables();

    /// \brief Loads one entry from input trees (event trees and, if any was read, the analysis tree). A
    /// file without event trees (only the analysis tree) is read from the analysis tree alone.
    /// \param entry Entry index.
    /// \return `true` if entry was loaded (from an event tree, or from the analysis tree without them).
    bool GetEntry(long long entry);
    /// \brief Loads the entry following the current one.
    /// \return `true` if entry was loaded.
    bool GetNextEntry();

    /// \brief Index of the current entry.
    long long GetCurrentEntry() const { return fEntry; }

    /// \brief Checks whether an input event tree exists.
    /// \param treeName Event tree/class name.
    /// \return `true` when event is available.
    bool HasEvent(const std::string& treeName) const;

    /// \brief Returns typed input event by tree name.
    /// \tparam T Event type.
    /// \param treeName Event tree/class name.
    /// \return Reference to typed input event.
    /// \throws std::runtime_error if the tree does not exist; std::bad_cast if it is not a T.
    template <typename T = TRestEvent>
    T& GetEvent(const std::string& treeName) {
        auto it = fInputEvents.find(treeName);
        if (it == fInputEvents.end()) {
            throw std::runtime_error("TRestRun: Tree '" + treeName + "' does not exist.");
        }
        return dynamic_cast<T&>(*(it->second));
    }

    /// \brief Registers an output event object and creates its output tree if needed.
    ///
    /// Creates the event branches, writes the format version in the tree UserInfo and, for the first
    /// registered event, the event info branches of the output analysis tree. The output file must be open.
    /// \tparam T Event type.
    /// \param className Output tree/class name.
    /// \param eventObject Event bound to the branches (not owned, must outlive the run output).
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
        // following ones copy its info); with an input analysis tree copied they are cloned from it
        if (fOutputAnalysisTree && !CopiesInputAnalysisTree() && !fOutputAnalysisTree->GetBranch("eventID")) {
            eventObject.TRestEvent::CreateBranches(fOutputAnalysisTree);
        }
    }

    /// \brief Registers an observable branch in the output analysis tree (or binds the existing one).
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
            auto holder = std::make_shared<T*>(&variable);
            fObservablePointers.push_back(holder);
            T** ptr = holder.get();
            if (!branch) {
                fOutputAnalysisTree->Branch(name.c_str(), ptr);
            } else {
                fOutputAnalysisTree->SetBranchAddress(name.c_str(), ptr);
            }
        }
    }
    /// \brief Current value of an observable of the analysis tree. While processing (output analysis tree),
    /// of the input file (not with inputAnalysisStorage: false, see SetInputAnalysisStorage) or registered by
    /// a previous process (e.g. `g4Ana_gas_FirstHitTime`); reading a
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

    /// \brief Binds the branches of the output analysis tree without address to the addresses of the input
    /// analysis tree branches of the same name.
    void SyncAnalysisTreeBranches();

    /// \brief Writes a metadata (its configuration) to the RESTMetadataStore of the output file.
    /// \param metadata Metadata to write; throws if null.
    void AddMetadata(TRestMetadata* metadata);
    /// \brief Writes the metadata of the input file (GetMetadataStore) to the output file.
    void AddHistoricMetadata();

    /// \brief Retrieves metadata YAML node by instance name from the input file.
    /// \param instanceName Metadata instance name.
    /// \return YAML node (null node if not found).
    YAML::Node GetMetadata(const std::string& instanceName) const;

    /// \brief The first metadata of the input file of a class, nullptr if none.
    /// \param className Class name (GetClassName).
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

    /// \brief The input events by class name.
    std::map<std::string, TRestEvent*> GetEventMap() const { return fInputEvents; }

    /// \brief Fills all registered output trees and the output analysis tree for the current event, and
    /// increases the saved entries.
    void Fill();

    /// \brief Loads run configuration from YAML node: reads the fields (runNumber "auto" gives a new run
    /// number), opens the input file if it is a REST run file, completes the run parameters from the
    /// input file (inputFormat, inheritInputRun), resolves outputFileName and writes the result to the node.
    void LoadConfig() override;

    /// \brief No-op initialize hook.
    void Initialize() override {}

    /// \brief Prints run metadata summary.
    void PrintMetadata() const override;

    /// \brief Prints the metadata of the input file.
    void PrintAllMetadata() const;
};

#endif
