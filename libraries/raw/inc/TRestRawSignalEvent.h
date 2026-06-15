#pragma once

#include <TRestEvent.h>
#include <TRestLogManager.h>

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <vector>

// ============================================================
//  TRestRawSignal
//  One digitised signal (time series) from a single readout
//  channel.  Stored as 16-bit ADC samples.
// ============================================================
struct TRestRawSignalData {
    std::vector<short> allSamples;
    std::vector<int>   signalIDs;
    std::vector<int>   offsets;
    
    void clear() {
        allSamples.clear(); signalIDs.clear(); offsets.clear();
    }
};

class TRestRawSignal {
   protected:
    TRestRawSignalData* fData = nullptr;
    int fSignalIdx = -1;

   public:
    TRestRawSignal(TRestRawSignalData* data, int idx) 
        : fData(data), fSignalIdx(idx) {}

    int GetSignalID() const { return fData->signalIDs[fSignalIdx]; }
    
    int GetNPoints() const {
        int start = fData->offsets[fSignalIdx];
        int next = (fSignalIdx + 1 < (int)fData->offsets.size()) 
                    ? fData->offsets[fSignalIdx+1] : (int)fData->allSamples.size();
        return next - start;
    }

    std::vector<short> GetData() const {
      int start = fData->offsets[fSignalIdx];
      int nPoints = GetNPoints();
      return std::vector<short>(fData->allSamples.begin() + start, 
                                fData->allSamples.begin() + start + nPoints);
    }

    short GetPoint(int i) const {
        return fData->allSamples[fData->offsets[fSignalIdx] + i];
    }

    void IncreaseBinBy(int bin, short delta) {
        fData->allSamples[fData->offsets[fSignalIdx] + bin] += delta;
    }
};

// ============================================================
//  TRestRawSignalEvent
//  Collection of signals from one detector
//  readout event.
// ============================================================
class TRestRawSignalEvent : public TRestEvent {
   protected:
    TRestRawSignalData fSignalData;
    mutable std::vector<TRestRawSignal> fSignalsViews;

  public:
    std::string GetClassName() const override { return "TRestRawSignalEvent"; }
    
    void Initialize() override {
        fSignalData.clear();
        fSignalsViews.clear();
    }

    void CreateBranches(TTree* tree) override {
        TRestEvent::CreateBranches(tree);
        tree->Branch("fSigSamples", &fSignalData.allSamples);
        tree->Branch("fSigIDs",     &fSignalData.signalIDs);
        tree->Branch("fSigOffsets", &fSignalData.offsets);
    }

    void SetBranchAddresses(TTree* tree) override {
        TRestEvent::SetBranchAddresses(tree);
        tree->SetBranchAddress("fSigSamples", &fSignalData.allSamples);
        tree->SetBranchAddress("fSigIDs",     &fSignalData.signalIDs);
        tree->SetBranchAddress("fSigOffsets", &fSignalData.offsets);
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
            this->fSignalData.signalIDs  = source->fSignalData.signalIDs;
            this->fSignalData.offsets    = source->fSignalData.offsets;
            this->fSignalsViews.clear();
        }
    }

    void AddSignal(int sID, const std::vector<short>& samples) {
        fSignalData.offsets.push_back((int)fSignalData.allSamples.size());
        fSignalData.signalIDs.push_back(sID);
        fSignalData.allSamples.insert(fSignalData.allSamples.end(), samples.begin(), samples.end());
        fSignalsViews.clear(); 
    }

    inline void RemoveSignal(int index) {
        if (index >= (int)fSignalData.signalIDs.size()) return;

        int start = fSignalData.offsets[index];
        int end = (index + 1 < (int)fSignalData.offsets.size()) 
                  ? fSignalData.offsets[index+1] : (int)fSignalData.allSamples.size();
        int sizeToRemove = end - start;

        fSignalData.allSamples.erase(fSignalData.allSamples.begin() + start, fSignalData.allSamples.begin() + end);
        fSignalData.signalIDs.erase(fSignalData.signalIDs.begin() + index);
        fSignalData.offsets.erase(fSignalData.offsets.begin() + index);

        for (size_t i = index; i < fSignalData.offsets.size(); ++i) {
            fSignalData.offsets[i] -= sizeToRemove;
        }
        fSignalsViews.clear(); 
    }

    inline void RemoveSignalByID(int id) {
        auto it = std::find(fSignalData.signalIDs.begin(), fSignalData.signalIDs.end(), id);
        if (it == fSignalData.signalIDs.end()) return;
        RemoveSignal(std::distance(fSignalData.signalIDs.begin(), it));
    }

    // --- GETTERS ---
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

    TRestRawSignalEvent()  = default;
    ~TRestRawSignalEvent() = default;
};
