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
    DECLARE_LOG_CLASS(TRestEventProcess)

   public:
    using TRestMetadata::TRestMetadata;
    ~TRestEventProcess() override = default;

    virtual void InitProcess() = 0;
    virtual void ProcessEvent(const TRestEvent& input, TRestEvent& output) = 0;
    virtual void EndProcess() {}

    void LoadConfig() override {}
    void Initialize() override { InitProcess(); }
};
