#ifndef TRESTDETECTORSIGNALEVENT_H
#define TRESTDETECTORSIGNALEVENT_H

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <vector>

#include "TGraph.h"
#include "TRestEvent.h"
#include "TRestLogManager.h"
#include "TRestPulseShapeAnalysis.h"

// ============================================================
//  TRestDetectorSignal
//  One digitised signal (time series) from a single readout
//  channel.  Stored as 16-bit ADC samples.
// ============================================================
/// \brief Shared contiguous storage used by TRestDetectorSignal lightweight views.
struct TRestDetectorSignalData {
    std::vector<double> allSignalsCharge;
    std::vector<double> allSignalsTime;
    std::vector<int> signalIDs;
    std::vector<int> offsets;

    void clear() {
        allSignalsCharge.clear();
        allSignalsTime.clear();
        signalIDs.clear();
        offsets.clear();
    }
};

/// \brief Non-owning view over one signal stored inside TRestDetectorSignalData.
class TRestDetectorSignal {
   protected:
    TRestDetectorSignalData* fData = nullptr;
    int fSignalIdx = -1;

   public:
    TRestDetectorSignal(TRestDetectorSignalData* data, int idx) : fData(data), fSignalIdx(idx) {}

    /// \brief Returns the detector channel identifier for this signal.
    int GetSignalID() const { return fData->signalIDs[fSignalIdx]; }

    /// \brief Returns the number of samples belonging to this signal.
    int GetNPoints() const {
        int start = fData->offsets[fSignalIdx];
        int next = (fSignalIdx + 1 < (int)fData->offsets.size()) ? fData->offsets[fSignalIdx + 1]
                                                                 : (int)fData->allSignalsTime.size();
        return next - start;
    }

    /// \brief Copies this signal waveform into a standalone sample vector.
    std::vector<double> GetCharge() const {
        int start = fData->offsets[fSignalIdx];
        int nPoints = GetNPoints();
        return std::vector<double>(fData->allSignalsCharge.begin() + start,
                                  fData->allSignalsCharge.begin() + start + nPoints);
    }

    std::vector<double> GetTime() const {
        int start = fData->offsets[fSignalIdx];
        int nPoints = GetNPoints();
        return std::vector<double>(fData->allSignalsTime.begin() + start,
                                  fData->allSignalsTime.begin() + start + nPoints);
    }

    /// \brief Returns one sample by local signal index.
    std::pair<double, double> GetPoint(int i) const {
        int globalIdx = fData->offsets[fSignalIdx] + i;
        return { fData->allSignalsTime[globalIdx], fData->allSignalsCharge[globalIdx] };
    }

    std::vector<std::pair<double, double>> GetPointsVector() const {
        int nPoints = GetNPoints();
        std::vector<std::pair<double, double>> signal;
        signal.reserve(nPoints);

        for (int i = 0; i < nPoints; ++i) {
            signal.push_back(GetPoint(i));
        }
        return signal;
    }

    /// \brief Adds delta to one charge sample bin in-place.
    void IncreaseAmplitude(double time, double amplitude);
    void AddPoint(double time, double amplitude);
    int GetTimeIndex(double t) const;
    void Sort();
    void AddOffset(double offset);
    void MultiplySignalBy(double factor);
    void ExponentialConvolution(double fromTime, double decayTime, double offset = 0);
    void SignalAddition(const TRestDetectorSignal* inSgnl);

    /// \brief Builds a ROOT graph representation for quick waveform inspection.
    TGraph GetGraph() const {
        const std::string title = "Signal ID: " + std::to_string(GetSignalID());
        std::vector<std::pair<double, double>> signal = GetPointsVector();
        return TRestPulseShapeAnalysis::GetGraphPair<double, double>(signal, title);
    }
};

// ============================================================
//  TRestDetectorSignalEvent
//  Collection of signals from one detector
//  readout event.
// ============================================================
/// \class TRestDetectorSignalEvent
/// \brief Event container holding all digitized waveforms of one acquisition trigger.
class TRestDetectorSignalEvent : public TRestEvent {
   protected:
    TRestDetectorSignalData fSignalData;
    mutable std::vector<TRestDetectorSignal> fSignalsViews;

    // Persisted pointers for ROOT branch addresses
    std::vector<double>* fPtrTime = nullptr;
    std::vector<double>* fPtrAmplitude = nullptr;
    std::vector<int>* fPtrIDs = nullptr;
    std::vector<int>* fPtrOffsets = nullptr;

   public:
    std::string GetClassName() const override { return "TRestDetectorSignalEvent"; }

    void Initialize() override {
        fSignalData.clear();
        fSignalsViews.clear();
    }

    void CreateBranches(TTree* tree) override {
        TRestEvent::CreateBranches(tree);
        tree->Branch("fSigTime", &fSignalData.allSignalsTime);
        tree->Branch("fSigAmplitude", &fSignalData.allSignalsCharge);
        tree->Branch("fSigIDs", &fSignalData.signalIDs);
        tree->Branch("fSigOffsets", &fSignalData.offsets);
    }

    void SetBranchAddresses(TTree* tree) override {
        TRestEvent::SetBranchAddresses(tree);
        fPtrAmplitude = &fSignalData.allSignalsCharge;
        fPtrTime = &fSignalData.allSignalsTime;
        fPtrIDs = &fSignalData.signalIDs;
        fPtrOffsets = &fSignalData.offsets;
        
        tree->SetBranchAddress("fSigAmplitude", &fPtrAmplitude);
        tree->SetBranchAddress("fSigTime", &fPtrTime);
        tree->SetBranchAddress("fSigIDs", &fPtrIDs);
        tree->SetBranchAddress("fSigOffsets", &fPtrOffsets);
    }

    void RefreshViews() const override {
        fSignalsViews.clear();
        for (int i = 0; i < (int)fSignalData.signalIDs.size(); ++i) {
            fSignalsViews.emplace_back(const_cast<TRestDetectorSignalData*>(&fSignalData), i);
        }
    }

    void CopyFrom(const TRestEvent* other) override {
        TRestEvent::CopyFrom(other);
        auto source = dynamic_cast<const TRestDetectorSignalEvent*>(other);
        if (source) {
            this->fSignalData.allSignalsCharge = source->fSignalData.allSignalsCharge;
            this->fSignalData.allSignalsTime = source->fSignalData.allSignalsTime;
            this->fSignalData.signalIDs = source->fSignalData.signalIDs;
            this->fSignalData.offsets = source->fSignalData.offsets;
            this->RefreshViews();
        }
    }

    bool HasSignalID(int id) const {
        return std::find(fSignalData.signalIDs.begin(), fSignalData.signalIDs.end(), id) 
               != fSignalData.signalIDs.end();
    }

    /// \brief Appends a new signal waveform associated with a detector channel ID.
    void AddSignal(int sID, const std::vector<std::pair<double, double>>& points) {
      if (HasSignalID(sID)) {
        std::cout << "Warning. Signal ID : " << sID
                  << " already exists. Signal will not be added to signal event" << std::endl;
        return;
      }

      fSignalData.offsets.push_back((int)fSignalData.allSignalsCharge.size());
      fSignalData.signalIDs.push_back(sID);

      fSignalData.allSignalsTime.reserve(fSignalData.allSignalsTime.size() + points.size());
      fSignalData.allSignalsCharge.reserve(fSignalData.allSignalsCharge.size() + points.size());

      for (const auto& point : points) {
        fSignalData.allSignalsTime.push_back(point.first);
        fSignalData.allSignalsCharge.push_back(point.second);
      }

      fSignalsViews.clear();
    }

  void AddSignal(int sID, const std::vector<double>& times, const std::vector<double>& amplitudes) {
      if (times.size() != amplitudes.size()) {
        std::cout << "Warning. Times and Amplitudes vectors size mismatch for Signal ID: " << sID << std::endl;
        return;
      }

      std::vector<std::pair<double, double>> points;
      points.reserve(times.size());
    
      for (size_t i = 0; i < times.size(); ++i) {
        points.emplace_back(times[i], amplitudes[i]);
      }

    AddSignal(sID, points);
  }

    /// \brief Removes one signal by index and compacts the backing sample storage.
    inline void RemoveSignal(int index) {
        if (index >= (int)fSignalData.signalIDs.size()) return;

        int start = fSignalData.offsets[index];
        int end = (index + 1 < (int)fSignalData.offsets.size()) ? fSignalData.offsets[index + 1]
                                                                : (int)fSignalData.allSignalsCharge.size();
        int sizeToRemove = end - start;

        fSignalData.allSignalsCharge.erase(fSignalData.allSignalsCharge.begin() + start,
                                       fSignalData.allSignalsCharge.begin() + end);
        fSignalData.allSignalsTime.erase(fSignalData.allSignalsTime.begin() + start,
                                     fSignalData.allSignalsTime.begin() + end);
                                     
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

    inline TRestDetectorSignal& GetSignal(int n) {
        if (fSignalsViews.size() != fSignalData.signalIDs.size()) RefreshViews();
        return fSignalsViews.at(n);
    }

    inline const TRestDetectorSignal& GetSignal(int n) const {
        if (fSignalsViews.size() != fSignalData.signalIDs.size()) RefreshViews();
        return fSignalsViews.at(n);
    }

    inline TRestDetectorSignal& GetSignalByID(int id) {
        auto it = std::find(fSignalData.signalIDs.begin(), fSignalData.signalIDs.end(), id);
        if (it == fSignalData.signalIDs.end()) throw std::runtime_error("Signal ID not found");
        return GetSignal(std::distance(fSignalData.signalIDs.begin(), it));
    }

    inline const TRestDetectorSignal& GetSignalByID(int id) const {
        auto it = std::find(fSignalData.signalIDs.begin(), fSignalData.signalIDs.end(), id);
        if (it == fSignalData.signalIDs.end()) throw std::runtime_error("Signal ID not found");
        return GetSignal(std::distance(fSignalData.signalIDs.begin(), it));
    }

    void PrintEvent() const override;
    TPad* DrawEvent(const TString& option = "") const override;

    TRestDetectorSignalEvent() = default;
    ~TRestDetectorSignalEvent() = default;
};

#endif
