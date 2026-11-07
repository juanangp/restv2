#include "TRestRawDAQSignalEvent.h"

#include "TRestLogManager.h"

// ---------------------------------------------------------------------------
// Self-registration in EventRegistry (no macro)
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    EventRegistry::Instance().Register("TRestRawDAQSignalEvent",
                                       []() { return std::make_unique<TRestRawDAQSignalEvent>(); });
    return true;
}();
}  // namespace

void TRestRawDAQSignalEvent::CreateBranches(TTree* tree) {
    TRestRawSignalEvent::CreateBranches(tree);
    tree->Branch("fFEMIDs", &fFEMIDs);
    tree->Branch("fFEMEventCounts", &fFEMEventCounts);
    tree->Branch("fFEMTimestamps", &fFEMTimestamps);
    tree->Branch("fFEMNSignals", &fFEMNSignals);
}

void TRestRawDAQSignalEvent::SetBranchAddresses(TTree* tree) {
    TRestRawSignalEvent::SetBranchAddresses(tree);
    fHasFEMInfo = tree->GetBranch("fFEMIDs") != nullptr;
    if (!fHasFEMInfo) return;
    fPtrFEMIDs = &fFEMIDs;
    fPtrFEMEventCounts = &fFEMEventCounts;
    fPtrFEMTimestamps = &fFEMTimestamps;
    fPtrFEMNSignals = &fFEMNSignals;
    BindBranch(tree, "fFEMIDs", &fPtrFEMIDs);
    BindBranch(tree, "fFEMEventCounts", &fPtrFEMEventCounts);
    BindBranch(tree, "fFEMTimestamps", &fPtrFEMTimestamps);
    BindBranch(tree, "fFEMNSignals", &fPtrFEMNSignals);
}

void TRestRawDAQSignalEvent::CopyFrom(const TRestEvent* other) {
    TRestRawSignalEvent::CopyFrom(other);
    auto source = dynamic_cast<const TRestRawDAQSignalEvent*>(other);
    if (!source) return;
    fFEMIDs = source->fFEMIDs;
    fFEMEventCounts = source->fFEMEventCounts;
    fFEMTimestamps = source->fFEMTimestamps;
    fFEMNSignals = source->fFEMNSignals;
}

void TRestRawDAQSignalEvent::MoveFrom(TRestEvent* other) {
    TRestRawSignalEvent::MoveFrom(other);
    auto source = dynamic_cast<TRestRawDAQSignalEvent*>(other);
    if (!source) return;
    fFEMIDs = std::move(source->fFEMIDs);
    fFEMEventCounts = std::move(source->fFEMEventCounts);
    fFEMTimestamps = std::move(source->fFEMTimestamps);
    fFEMNSignals = std::move(source->fFEMNSignals);
    source->fFEMIDs.clear();
    source->fFEMEventCounts.clear();
    source->fFEMTimestamps.clear();
    source->fFEMNSignals.clear();
}

void TRestRawDAQSignalEvent::PrintEvent() const {
    TRestRawSignalEvent::PrintEvent();
    for (int n = 0; n < GetNumberOfFEMs(); ++n) {
        RESTLog << "  FEM " << fFEMIDs[n] << ": event count " << fFEMEventCounts[n] << ", timestamp "
                << fFEMTimestamps[n] << ", " << fFEMNSignals[n] << " signals" << RESTendl;
    }
}
