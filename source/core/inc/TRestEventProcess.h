#pragma once

#include "TRestEvent.h"
#include "TRestMetadata.h"

class TRestRun;

/// \class TRestEventProcess
/// \brief Abstract base for event-processing pipeline stages.
///
/// A process consumes an input event and writes into an output event object.
/// Returning `false` from `ProcessEvent` indicates that the event is rejected.
class TRestEventProcess : public TRestMetadata {
    DECLARE_LOG_CLASS(TRestEventProcess)

   public:
    TRestEventProcess();
    TRestEventProcess(const std::string& instanceName, const YAML::Node& node);
    TRestEventProcess(const std::string& fileName, const std::string& sectionName);
    ~TRestEventProcess() override = default;

    virtual std::string GetInputEvent() const = 0;
    virtual std::string GetOutputEvent() const = 0;

    /// Pointer to run context shared with this process.
    TRestRun* fRunInfo = nullptr;

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
    virtual Long64_t GetInputEventCount() const { return -1; }

    /// \brief No-op configuration loader for process instances.
    virtual void LoadConfig() override;

    /// \brief Initializes this process by delegating to `InitProcess`.
    void Initialize() override { InitProcess(); }

    /// \brief Sets run context pointer.
    /// \param r Run context.
    inline void SetRunInfo(TRestRun* r) { fRunInfo = r; }
};
