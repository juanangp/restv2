#ifndef TRESTRAWDAQSIGNALEVENT_H
#define TRESTRAWDAQSIGNALEVENT_H

#include <vector>

#include "TRestRawSignalEvent.h"

/// \class TRestRawDAQSignalEvent
/// \brief TRestRawSignalEvent acquired by restDAQ, with the information of each FEM that sent signals.
///
/// It is written in the tree `TRestRawSignalEvent` (TRestRun::RegisterEvent with that name), so the files
/// are read as a plain TRestRawSignalEvent, which ignores the branches of the FEMs: the pipeline, the
/// browser and the readout work as with any raw signal event. The signals are grouped by FEM, in the order
/// of the FEM entries (AddFEM after its signals). The branches, one entry per FEM:
///
/// * **fFEMIDs**: FEM id (`id` of the FEM in TRestRawDAQMetadata).
/// * **fFEMEventCounts**: event counter of the FEM (0xFFFFFFFF: event without header).
/// * **fFEMTimestamps**: timestamp of the FEM, in ticks of 10 ns.
/// * **fFEMNSignals**: number of signals of the FEM.
///
/// A process resynchronizing the FEMs must check that the branches exist (HasFEMInfo after
/// SetBranchAddresses).
class TRestRawDAQSignalEvent : public TRestRawSignalEvent {
   protected:
    std::vector<int> fFEMIDs;
    std::vector<unsigned int> fFEMEventCounts;
    std::vector<unsigned long long> fFEMTimestamps;
    std::vector<int> fFEMNSignals;

    // Persisted pointers for ROOT branch addresses
    std::vector<int>* fPtrFEMIDs = nullptr;
    std::vector<unsigned int>* fPtrFEMEventCounts = nullptr;
    std::vector<unsigned long long>* fPtrFEMTimestamps = nullptr;
    std::vector<int>* fPtrFEMNSignals = nullptr;

    /// Whether the tree read has the branches of the FEMs
    bool fHasFEMInfo = false;

   public:
    std::string GetClassName() const override { return "TRestRawDAQSignalEvent"; }

    void Initialize() override {
        TRestRawSignalEvent::Initialize();
        fFEMIDs.clear();
        fFEMEventCounts.clear();
        fFEMTimestamps.clear();
        fFEMNSignals.clear();
    }

    void CreateBranches(TTree* tree) override;
    /// \brief Binds the branches of the FEMs only if the tree has them (HasFEMInfo)
    void SetBranchAddresses(TTree* tree) override;
    void CopyFrom(const TRestEvent* other) override;
    void MoveFrom(TRestEvent* other) override;

    /// \brief Adds a FEM, after its nSignals signals
    void AddFEM(int id, unsigned int eventCount, unsigned long long timestamp, int nSignals) {
        fFEMIDs.push_back(id);
        fFEMEventCounts.push_back(eventCount);
        fFEMTimestamps.push_back(timestamp);
        fFEMNSignals.push_back(nSignals);
    }

    /// \brief Whether the event has the information of the FEMs (written by restDAQ, or set with AddFEM)
    bool HasFEMInfo() const { return fHasFEMInfo || !fFEMIDs.empty(); }

    int GetNumberOfFEMs() const { return static_cast<int>(fFEMIDs.size()); }
    int GetFEMID(int n) const { return fFEMIDs.at(n); }
    unsigned int GetFEMEventCount(int n) const { return fFEMEventCounts.at(n); }
    unsigned long long GetFEMTimestamp(int n) const { return fFEMTimestamps.at(n); }
    int GetFEMNSignals(int n) const { return fFEMNSignals.at(n); }

    void PrintEvent() const override;

    TRestRawDAQSignalEvent() = default;
    ~TRestRawDAQSignalEvent() = default;
};

#endif
