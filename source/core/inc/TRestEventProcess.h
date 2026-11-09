#ifndef TRESTEVENTPROCESS_H
#define TRESTEVENTPROCESS_H

#include "TRestEvent.h"
#include "TRestMetadata.h"
// RegisterObservable uses TRestRun (TRestRun.h does not include this header): every process gets it
#include "TRestRun.h"

class TRestCut;
class TRestManager;

/// \class TRestEventProcess
/// \brief Abstract base for event-processing pipeline stages.
///
/// A process consumes an input event and writes into an output event object.
/// Returning `false` from `ProcessEvent` indicates that the event is rejected. The event is also rejected
/// after the process if it does not pass the TRestCut of `cuts` (names of TRestCut in the manager).
///
/// Common configuration keys of every process (besides those of the derived class):
///
/// \code{.yaml}
/// pipeline:
///   class: TRestEventProcessManager
///   myProcess:
///     class: TRestSomeProcess
///     verbose: info
///     observables: ["all"]         # names offered by the process, "all" or "none" (default)
///     cuts: ["fiducial"]           # TRestCut of the manager applied after the process
/// \endcode
///
/// The observables are written in the analysis tree as `<processName>_<observable>`. A name in
/// `observables` that the process does not offer throws at initialization.
///
/// A derived process implements GetInputEvent / GetOutputEvent (event class names; "None" as input for a
/// generator creating the events, empty for processes of any event type taking the event of the
/// previous one), InitProcess (registering its observables with RegisterObservable) and ProcessEvent.
class TRestEventProcess : public TRestMetadata {
   private:
    /// The TRestCut of fCutNames, resolved in InitCuts
    std::vector<TRestCut*> fCuts;  //!

    /// Names of all the observables the process offered in InitProcess (registered or not)
    mutable std::vector<std::string> fOfferedObservables;  //!

    /// \brief Throws if a name of `observables` (other than "all" and "none") was not offered by the
    /// process in InitProcess: a misspelled name, or one whose parameters are not enabled
    void CheckObservables() const;

   public:
    /// Observables to register in the analysis tree (`observables`): names, "all" or "none".
    std::vector<std::string> fObservables = {"none"};
    /// Names of the TRestCut (in the manager) applied to the event after the process
    std::vector<std::string> fCutNames = {};

    /// \brief Default constructor.
    TRestEventProcess();
    /// \brief Constructs from a YAML node and reads the common fields (LoadConfig).
    TRestEventProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructs from a section of a YAML file and reads the common fields (LoadConfig).
    TRestEventProcess(const std::string& fileName, const std::string& sectionName);
    ~TRestEventProcess() override = default;  ///< Destructor.

    /// \brief Class name of the input event; "None" for a generator, empty to take the output event of
    /// the previous process (processes of any event type).
    virtual std::string GetInputEvent() const = 0;
    /// \brief Class name of the output event (the same as the input for processes of any event type).
    virtual std::string GetOutputEvent() const = 0;

    /// Pointer to the owning manager, giving access to sibling metadata
    /// (readouts, calibrations, etc.) loaded alongside this process's pipeline.
    /// May be nullptr if the process runs outside a TRestManager context.
    TRestManager* fManager = nullptr;

    /// \brief Initializes process resources before event loop; registers the observables
    /// (RegisterObservable). fRunInfo and fManager are set when it is called.
    virtual void InitProcess() = 0;

    /// \brief Processes one event.
    /// \param input Input event view.
    /// \param output Output event storage.
    /// \return `true` to keep event, `false` to discard it.
    virtual bool ProcessEvent(const TRestEvent& input, TRestEvent& output) = 0;

    /// \brief Finalization hook after event loop.
    virtual void EndProcess() {}

    /// \brief Returns expected input event count when known.
    /// \return Number of input events (entries of the run by default) or `-1` if unknown.
    virtual long long GetInputEventCount() const;

    /// \brief For a process that reads its events from an external file (e.g. a DAQ binary file) whose
    /// number of events is unknown (GetInputEventCount -1): true once ProcessEvent returned false because
    /// the file has no more events. The manager then ends the event loop (it is not a rejected event).
    virtual bool InputFinished() const { return false; }

    /// \brief Fraction (0-1) of the external input file already read, for the progress when the number
    /// of events is unknown; -1 if the process does not read an external file.
    virtual double GetInputFraction() const { return -1; }

    /// \brief Reads the common fields (`observables`, `cuts`) from the node; derived classes call it
    /// and then read their own fields.
    virtual void LoadConfig() override;

    /// \brief Initializes this process by delegating to `InitProcess`, then checks the names of
    /// `observables` against the ones it offered.
    void Initialize() override {
        fOfferedObservables.clear();
        InitProcess();
        CheckObservables();
    }

    /// \brief Resolves the TRestCut of `cuts` in the manager and compiles them on the output analysis
    /// tree. Called after the initialization of all the processes (all the observables registered).
    void InitCuts();

    /// \brief True if the current observables pass all the cuts of the process (true without cuts).
    bool PassCuts() const;

    /// \brief Sets the owning manager, for access to sibling metadata.
    /// \param mgr Owning manager, or nullptr.
    inline void SetManager(TRestManager* mgr) { fManager = mgr; }

    /// \brief Registers a variable as analysis observable if enabled in the configuration.
    /// The `observables` list of the configuration uses `name`; the branch of the analysis tree is
    /// `processName_name` (as legacy), so the observables of different processes never collide.
    /// Every call offers the observable (CheckObservables); a name offered twice throws.
    /// \param name Observable name (without the process prefix).
    /// \param variable Variable bound to the branch; must live while the output is written.
    template <typename T>
    void RegisterObservable(const std::string& name, T& variable) {
        if (!ShouldRegisterObservable(name)) return;
        fRunInfo->SetObservable(GetName() + "_" + name, variable);
    }

   private:
    bool ShouldRegisterObservable(const std::string& name) const;
};

#endif
