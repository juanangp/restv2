#ifndef TRESTEVENTPROCESS_H
#define TRESTEVENTPROCESS_H

#include "TRestEvent.h"
#include "TRestMetadata.h"

class TRestCut;
class TRestManager;
class TRestRun;

/// \class TRestEventProcess
/// \brief Abstract base for event-processing pipeline stages.
///
/// A process consumes an input event and writes into an output event object.
/// Returning `false` from `ProcessEvent` indicates that the event is rejected. The event is also rejected
/// after the process if it does not pass the TRestCut of `cuts` (names of TRestCut in the manager).
class TRestEventProcess : public TRestMetadata {
   private:
    /// The TRestCut of fCutNames, resolved in InitCuts
    std::vector<TRestCut*> fCuts;  //!

   public:
    std::vector<std::string> fObservables = {"none"};
    /// Names of the TRestCut (in the manager) applied to the event after the process
    std::vector<std::string> fCutNames = {};

    TRestEventProcess();
    TRestEventProcess(const std::string& instanceName, const YAML::Node& node);
    TRestEventProcess(const std::string& fileName, const std::string& sectionName);
    ~TRestEventProcess() override = default;

    virtual std::string GetInputEvent() const = 0;
    virtual std::string GetOutputEvent() const = 0;

    /// Pointer to the owning manager, giving access to sibling metadata
    /// (readouts, calibrations, etc.) loaded alongside this process's pipeline.
    /// May be nullptr if the process runs outside a TRestManager context.
    TRestManager* fManager = nullptr;

    /// \brief Initializes process resources before event loop.
    virtual void InitProcess() = 0;

    /// \brief Processes one event.
    /// \param input Input event view.
    /// \param output Output event storage.
    /// \return `true` to keep event, `false` to discard it.
    virtual bool ProcessEvent(const TRestEvent& input, TRestEvent& output) = 0;

    /// \brief Finalization hook after event loop.
    virtual void EndProcess() {}

    /// \brief Returns expected input event count when known.
    /// \return Number of input events or `-1` if unknown.
    virtual Long64_t GetInputEventCount() const;

    /// \brief No-op configuration loader for process instances.
    virtual void LoadConfig() override;

    /// \brief Initializes this process by delegating to `InitProcess`.
    void Initialize() override { InitProcess(); }

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
    /// The `Run` parameter defers the TRestRun lookup to the point of instantiation,
    /// where TRestRun.h must be included.
    template <typename T, typename Run = TRestRun>
    void RegisterObservable(const std::string& name, T& variable) {
        if (!ShouldRegisterObservable(name)) return;
        Run* run = fRunInfo;
        run->SetObservable(GetName() + "_" + name, variable);
    }

   private:
    bool ShouldRegisterObservable(const std::string& name) const;
};

#endif
