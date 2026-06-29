#pragma once

#include "TRestEvent.h"
#include "TRestMetadata.h"

class TRestRun;

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

    TRestRun* fRunInfo = nullptr;

    virtual void InitProcess() = 0;
    virtual bool ProcessEvent(const TRestEvent& input, TRestEvent& output) = 0;
    virtual void EndProcess() {}
    virtual Long64_t GetInputEventCount() const { return -1; }

    void LoadConfig() override {}
    void Initialize() override { InitProcess(); }

    inline void SetRunInfo(TRestRun* r) { fRunInfo = r; }

};
