#include "TRestRawSignalEvent.h"

#include <TPad.h>

#include <iostream>

// ---------------------------------------------------------------------------
// Self-registration in EventRegistry (no macro)
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    EventRegistry::Instance().Register("TRestRawSignalEvent",
                                       []() { return std::make_unique<TRestRawSignalEvent>(); });
    return true;
}();
}  // namespace

TPad* TRestRawSignalEvent::DrawEvent(const TString& /*option*/) const {
    // Placeholder – actual drawing would create a TPad with TGraphs
    RESTWarning << "TRestRawSignalEvent::DrawEvent not yet implemented" << RESTendl;
    return nullptr;
}

void TRestRawSignalEvent::PrintEvent() const {
    TRestEvent::PrintEvent();
    RESTLog << "  Signals: " << GetNumberOfSignals() << RESTendl;

    for (int i = 0; i < GetNumberOfSignals(); ++i) {
        TRestRawSignal sig = GetSignal(i);

        RESTLog << " SignalID " << sig.GetSignalID() << " (nPoints=" << sig.GetNPoints() << "): ";

        for (int j = 0; j < sig.GetNPoints(); ++j) {
            RESTLog << sig.GetPoint(j) << " ";
        }
        RESTLog << RESTendl;
    }
}
