#pragma once

#include "TRestEvent.h"
#include "TRestMetadata.h"

// ============================================================
//  TRestEventProcess
//  Abstract base for all event-processing steps.
//  A process receives a raw pointer to an input event and
//  returns a raw pointer to the (possibly same) output event.
//  Returning nullptr signals that the event should be dropped.
// ============================================================
class TRestEventProcess : public TRestMetadata {
   public:
    std::string GetClassName() const override { return "TRestEventProcess"; }

    /// Input event accessor – must be implemented by derived class.
    virtual TRestEvent* GetInputEvent()  const = 0;
    /// Output event accessor – must be implemented by derived class.
    virtual TRestEvent* GetOutputEvent() const = 0;

    /// Called once before the processing loop starts.
    virtual void BeginOfEventProcess(TRestEvent* inputEvent = nullptr) = 0;
    /// Process one event; return nullptr to drop it.
    virtual TRestEvent* ProcessEvent(TRestEvent* inputEvent) = 0;
    /// Called once after the processing loop ends.
    virtual void EndOfEventProcess(TRestEvent* inputEvent = nullptr) = 0;

    // Default no-op implementations of TRestMetadata virtuals
    void LoadConfig()    override {}
    void Initialize()    override {}
    void PrintMetadata() override {}
};
