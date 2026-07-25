#pragma once

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <vector>

#include "TGraph.h"
#include "TRestEvent.h"
#include "TRestLogManager.h"
#include "TRestPulseShapeAnalysis.h"

// ============================================================
//  TRestRawSignal
//  One digitised signal (time series) from a single readout
//  channel.  Stored as 16-bit ADC samples.
// ============================================================
/// \brief Shared contiguous storage used by TRestRawSignal lightweight views.
struct TRestRawSignalData {
    std::vector<short> allSamples;
    std::vector<int> signalIDs;
    std::vector<int> offsets;

    void clear() {
        allSamples.clear();
        signalIDs.clear();
        offsets.clear();
    }
};

/// \brief Non-owning view over one signal stored inside TRestRawSignalData.
class TRestRawSignal {
   protected:
    TRestRawSignalData* fData = nullptr;
    int fSignalIdx = -1;

   public:
    TRestRawSignal(TRestRawSignalData* data, int idx) : fData(data), fSignalIdx(idx) {}

    /// \brief Returns the detector channel identifier for this signal.
    int GetSignalID() const { return fData->signalIDs[fSignalIdx]; }

    /// \brief Returns the number of ADC samples belonging to this signal.
    int GetNPoints() const {
        int start = fData->offsets[fSignalIdx];
        int next = (fSignalIdx + 1 < (int)fData->offsets.size()) ? fData->offsets[fSignalIdx + 1]
                                                                 : (int)fData->allSamples.size();
        return next - start;
    }

    /// \brief Copies this signal waveform into a standalone sample vector.
    std::vector<short> GetData() const {
        int start = fData->offsets[fSignalIdx];
        int nPoints = GetNPoints();
        return std::vector<short>(fData->allSamples.begin() + start,
                                  fData->allSamples.begin() + start + nPoints);
    }

    /// \brief Returns one ADC sample by local signal index.
    short GetPoint(int i) const { return fData->allSamples[fData->offsets[fSignalIdx] + i]; }

    /// \brief Adds delta to one ADC sample bin in-place.
    void IncreaseBinBy(int bin, short delta) { fData->allSamples[fData->offsets[fSignalIdx] + bin] += delta; }

    /// \brief Builds a ROOT graph representation for quick waveform inspection.
    TGraph GetGraph() const {
        const std::string title = "Signal ID: " + std::to_string(GetSignalID());
        return TRestPulseShapeAnalysis::GetGraph(GetData(), title);
    }
};

// ============================================================
//  TRestRawSignalEvent
//  Collection of signals from one detector
//  readout event.
// ============================================================
/// \class TRestRawSignalEvent
/// \brief Event container holding all digitized waveforms of one acquisition trigger.
class TRestRawSignalEvent : public TRestEvent {
   protected:
    TRestRawSignalData fSignalData;
    mutable std::vector<TRestRawSignal> fSignalsViews;

    // Persisted pointers for ROOT branch addresses
    std::vector<short>* fPtrSamples = nullptr;
    std::vector<int>* fPtrIDs = nullptr;
    std::vector<int>* fPtrOffsets = nullptr;

   public:
    std::string GetClassName() const override { return "TRestRawSignalEvent"; }

    void Initialize() override {
        fSignalData.clear();
        fSignalsViews.clear();
    }

    void CreateBranches(TTree* tree) override {
        TRestEvent::CreateBranches(tree);
        tree->Branch("fSigSamples", &fSignalData.allSamples);
        tree->Branch("fSigIDs", &fSignalData.signalIDs);
        tree->Branch("fSigOffsets", &fSignalData.offsets);
    }

    void SetBranchAddresses(TTree* tree) override {
        TRestEvent::SetBranchAddresses(tree);
        fPtrSamples = &fSignalData.allSamples;
        fPtrIDs = &fSignalData.signalIDs;
        fPtrOffsets = &fSignalData.offsets;
        tree->SetBranchAddress("fSigSamples", &fPtrSamples);
        tree->SetBranchAddress("fSigIDs", &fPtrIDs);
        tree->SetBranchAddress("fSigOffsets", &fPtrOffsets);
    }

    void RefreshViews() const override {
        fSignalsViews.clear();
        for (int i = 0; i < (int)fSignalData.signalIDs.size(); ++i) {
            fSignalsViews.emplace_back(const_cast<TRestRawSignalData*>(&fSignalData), i);
        }
    }

    void CopyFrom(const TRestEvent* other) override {
        TRestEvent::CopyFrom(other);
        auto source = dynamic_cast<const TRestRawSignalEvent*>(other);
        if (source) {
            this->fSignalData.allSamples = source->fSignalData.allSamples;
            this->fSignalData.signalIDs = source->fSignalData.signalIDs;
            this->fSignalData.offsets = source->fSignalData.offsets;
            this->RefreshViews();
        }
    }

    /// \brief Appends a new signal waveform associated with a detector channel ID.
    void AddSignal(int sID, const std::vector<short>& samples) {
        fSignalData.offsets.push_back((int)fSignalData.allSamples.size());
        fSignalData.signalIDs.push_back(sID);
        fSignalData.allSamples.insert(fSignalData.allSamples.end(), samples.begin(), samples.end());
        fSignalsViews.clear();
    }

    /// \brief Removes one signal by index and compacts the backing sample storage.
    inline void RemoveSignal(int index) {
        if (index >= (int)fSignalData.signalIDs.size()) return;

        int start = fSignalData.offsets[index];
        int end = (index + 1 < (int)fSignalData.offsets.size()) ? fSignalData.offsets[index + 1]
                                                                : (int)fSignalData.allSamples.size();
        int sizeToRemove = end - start;

        fSignalData.allSamples.erase(fSignalData.allSamples.begin() + start,
                                     fSignalData.allSamples.begin() + end);
        fSignalData.signalIDs.erase(fSignalData.signalIDs.begin() + index);
        fSignalData.offsets.erase(fSignalData.offsets.begin() + index);

        for (size_t i = index; i < fSignalData.offsets.size(); ++i) {
            fSignalData.offsets[i] -= sizeToRemove;
        }
        fSignalsViews.clear();
    }

    /// \brief Removes the first signal matching the given channel ID.
    inline void RemoveSignalByID(int id) {
        auto it = std::find(fSignalData.signalIDs.begin(), fSignalData.signalIDs.end(), id);
        if (it == fSignalData.signalIDs.end()) return;
        RemoveSignal(std::distance(fSignalData.signalIDs.begin(), it));
    }

    // --- GETTERS ---
    /// \brief Returns the number of stored signal waveforms in this event.
    int GetNumberOfSignals() const { return static_cast<int>(fSignalData.signalIDs.size()); }

    inline TRestRawSignal& GetSignal(int n) {
        if (fSignalsViews.size() != fSignalData.signalIDs.size()) RefreshViews();
        return fSignalsViews.at(n);
    }

    inline const TRestRawSignal& GetSignal(int n) const {
        if (fSignalsViews.size() != fSignalData.signalIDs.size()) RefreshViews();
        return fSignalsViews.at(n);
    }

    inline TRestRawSignal& GetSignalByID(int id) {
        auto it = std::find(fSignalData.signalIDs.begin(), fSignalData.signalIDs.end(), id);
        if (it == fSignalData.signalIDs.end()) throw std::runtime_error("Signal ID not found");
        return GetSignal(std::distance(fSignalData.signalIDs.begin(), it));
    }

    void PrintEvent() const override;
    TPad* DrawEvent(const TString& option = "") const override;

    TRestRawSignalEvent() = default;
    ~TRestRawSignalEvent() = default;
};
