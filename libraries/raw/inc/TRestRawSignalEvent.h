#ifndef TRESTRAWSIGNALEVENT_H
#define TRESTRAWSIGNALEVENT_H

#include <algorithm>
#include <climits>
#include <stdexcept>
#include <vector>

#include "TGraph.h"
#include "TRestEvent.h"
#include "TRestPulseShapeAnalysis.h"

// ============================================================
//  TRestRawSignal
//  One digitised signal (time series) from a single readout
//  channel.  Stored as 16-bit ADC samples.
// ============================================================
/// \brief Shared contiguous storage used by TRestRawSignal lightweight views.
///
/// The samples of all the signals one after the other; signal i starts at offsets[i] and ends at
/// offsets[i + 1] (or at the end of allSamples for the last one).
struct TRestRawSignalData {
    std::vector<short> allSamples;  ///< ADC samples of all the signals, concatenated
    std::vector<int> signalIDs;     ///< Signal (DAQ channel) ID of each signal
    std::vector<int> offsets;       ///< Index in allSamples of the first sample of each signal

    /// \brief Removes all the signals.
    void clear() {
        allSamples.clear();
        signalIDs.clear();
        offsets.clear();
    }
};

/// \class TRestRawSignal
/// \brief Non-owning view over one signal stored inside TRestRawSignalData.
///
/// Obtained with TRestRawSignalEvent::GetSignal or GetSignalByID. The view is invalidated when signals are
/// added to or removed from the event. Bin indices are not checked.
class TRestRawSignal {
   protected:
    TRestRawSignalData* fData = nullptr;  ///< Storage of the event the signal belongs to
    int fSignalIdx = -1;                  ///< Index of the signal inside fData

   public:
    /// \brief View of the signal number `idx` of `data`.
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

    /// \brief Returns one ADC sample by local signal index (bin, from 0 to GetNPoints() - 1).
    short GetPoint(int i) const { return fData->allSamples[fData->offsets[fSignalIdx] + i]; }

    /// \brief Adds delta to one ADC sample bin in-place.
    void IncreaseBinBy(int bin, short delta) { fData->allSamples[fData->offsets[fSignalIdx] + bin] += delta; }

    /// \brief Adds delta to one ADC sample bin in-place, clamping the result to [SHRT_MIN, SHRT_MAX].
    void IncreaseBinByClamped(int bin, int delta) {
        short& sample = fData->allSamples[fData->offsets[fSignalIdx] + bin];
        sample = static_cast<short>(std::clamp((int)sample + delta, (int)SHRT_MIN, (int)SHRT_MAX));
    }

    /// \brief Returns true if the raw waveform shows an ADC saturation flat top.
    ///
    /// Legacy criterion (TRestRawSignal::IsADCSaturation): the first bin holding the maximum raw value
    /// and the following bins, nFlat in total, all have that same maximum value. It does not depend on
    /// the ADC range, so it works for 12-bit data and for simulated signals with any maximum value.
    bool IsADCSaturation(int nFlat = 3) const {
        if (nFlat <= 0) return false;
        const int nPoints = GetNPoints();
        if (nPoints <= 0) return false;
        const short* samples = fData->allSamples.data() + fData->offsets[fSignalIdx];
        const int maxBin = std::distance(samples, std::max_element(samples, samples + nPoints));
        if (maxBin + nFlat > nPoints) return false;
        for (int i = maxBin + 1; i < maxBin + nFlat; i++) {
            if (samples[i] != samples[maxBin]) return false;
        }
        return true;
    }

    /// \brief Builds a ROOT graph representation for quick waveform inspection (titled with the signal ID).
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
///
/// Each signal is a vector of 16-bit ADC samples identified by its signal ID (the DAQ channel, decoded into
/// a readout channel by the detector library). The samples are stored contiguously (TRestRawSignalData)
/// and accessed through TRestRawSignal views. In the event tree (named `TRestRawSignalEvent`) the signals
/// are written in the branches `fSigSamples`, `fSigIDs` and `fSigOffsets`, besides the ones of TRestEvent.
///
/// Signal IDs are unique in an event: AddSignal ignores, with a warning, an ID that already exists.
/// Adding or removing signals invalidates the references returned by GetSignal.
class TRestRawSignalEvent : public TRestEvent {
   protected:
    TRestRawSignalData fSignalData;                     ///< Samples, IDs and offsets of all the signals
    mutable std::vector<TRestRawSignal> fSignalsViews;  ///< Views over fSignalData, rebuilt on demand

    // Persisted pointers for ROOT branch addresses
    std::vector<short>* fPtrSamples = nullptr;
    std::vector<int>* fPtrIDs = nullptr;
    std::vector<int>* fPtrOffsets = nullptr;

   public:
    std::string GetClassName() const override { return "TRestRawSignalEvent"; }

    /// \brief Resets the event header and removes all the signals.
    void Initialize() override {
        TRestEvent::Initialize();
        fSignalData.clear();
        fSignalsViews.clear();
    }

    /// \brief Creates the TRestEvent branches plus fSigSamples, fSigIDs and fSigOffsets.
    void CreateBranches(TTree* tree) override {
        TRestEvent::CreateBranches(tree);
        tree->Branch("fSigSamples", &fSignalData.allSamples);
        tree->Branch("fSigIDs", &fSignalData.signalIDs);
        tree->Branch("fSigOffsets", &fSignalData.offsets);
    }

    /// \brief Binds the branches written by CreateBranches for reading.
    void SetBranchAddresses(TTree* tree) override {
        TRestEvent::SetBranchAddresses(tree);
        fPtrSamples = &fSignalData.allSamples;
        fPtrIDs = &fSignalData.signalIDs;
        fPtrOffsets = &fSignalData.offsets;
        BindBranch(tree, "fSigSamples", &fPtrSamples);
        BindBranch(tree, "fSigIDs", &fPtrIDs);
        BindBranch(tree, "fSigOffsets", &fPtrOffsets);
    }

    /// \brief Rebuilds the TRestRawSignal views (one per signal) over the current storage.
    void RefreshViews() const override {
        fSignalsViews.clear();
        for (int i = 0; i < (int)fSignalData.signalIDs.size(); ++i) {
            fSignalsViews.emplace_back(const_cast<TRestRawSignalData*>(&fSignalData), i);
        }
    }

    /// \brief Copies the header and, if `other` is a TRestRawSignalEvent, its signals.
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

    /// \brief Moves the header and, if `other` is a TRestRawSignalEvent, its signals (left empty in it).
    void MoveFrom(TRestEvent* other) override {
        TRestEvent::MoveFrom(other);
        auto source = dynamic_cast<TRestRawSignalEvent*>(other);
        if (source) {
            this->fSignalData.allSamples = std::move(source->fSignalData.allSamples);
            this->fSignalData.signalIDs = std::move(source->fSignalData.signalIDs);
            this->fSignalData.offsets = std::move(source->fSignalData.offsets);

            source->fSignalData.clear();
            source->fSignalsViews.clear();

            this->RefreshViews();
        }
    }

    /// \brief True if the event has a signal with ID `id`.
    bool SignalExists(int id) const {
        return std::find(fSignalData.signalIDs.begin(), fSignalData.signalIDs.end(), id) !=
               fSignalData.signalIDs.end();
    }

    /// \brief Appends a new signal waveform associated with a detector channel ID.
    /// \param sID Signal ID; if it already exists the signal is not added (warning printed).
    /// \param samples ADC samples of the signal.
    void AddSignal(int sID, const std::vector<short>& samples) {
        if (SignalExists(sID)) {
            std::cout << "Warning. Signal ID : " << sID
                      << " already exists. Signal will not be added to signal event" << std::endl;
            return;
        }

        fSignalData.offsets.push_back((int)fSignalData.allSamples.size());
        fSignalData.signalIDs.push_back(sID);
        fSignalData.allSamples.insert(fSignalData.allSamples.end(), samples.begin(), samples.end());
        fSignalsViews.clear();
    }

    /// \brief Removes one signal by index and compacts the backing sample storage (nothing if the index is
    /// beyond the last signal).
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

    /// \brief Removes the first signal matching the given channel ID (nothing if not found).
    inline void RemoveSignalByID(int id) {
        auto it = std::find(fSignalData.signalIDs.begin(), fSignalData.signalIDs.end(), id);
        if (it == fSignalData.signalIDs.end()) return;
        RemoveSignal(std::distance(fSignalData.signalIDs.begin(), it));
    }

    // --- GETTERS ---
    /// \brief Returns the number of stored signal waveforms in this event.
    int GetNumberOfSignals() const { return static_cast<int>(fSignalData.signalIDs.size()); }

    /// \brief View of the signal number `n` (0 to GetNumberOfSignals() - 1; throws std::out_of_range).
    inline TRestRawSignal& GetSignal(int n) {
        if (fSignalsViews.size() != fSignalData.signalIDs.size()) RefreshViews();
        return fSignalsViews.at(n);
    }

    /// \brief Const view of the signal number `n` (throws std::out_of_range).
    inline const TRestRawSignal& GetSignal(int n) const {
        if (fSignalsViews.size() != fSignalData.signalIDs.size()) RefreshViews();
        return fSignalsViews.at(n);
    }

    /// \brief View of the signal with ID `id` (throws std::runtime_error if not found).
    inline TRestRawSignal& GetSignalByID(int id) {
        auto it = std::find(fSignalData.signalIDs.begin(), fSignalData.signalIDs.end(), id);
        if (it == fSignalData.signalIDs.end()) throw std::runtime_error("Signal ID not found");
        return GetSignal(std::distance(fSignalData.signalIDs.begin(), it));
    }

    /// \brief Const view of the signal with ID `id` (throws std::runtime_error if not found).
    inline const TRestRawSignal& GetSignalByID(int id) const {
        auto it = std::find(fSignalData.signalIDs.begin(), fSignalData.signalIDs.end(), id);
        if (it == fSignalData.signalIDs.end()) throw std::runtime_error("Signal ID not found");
        return GetSignal(std::distance(fSignalData.signalIDs.begin(), it));
    }

    /// \brief Prints the header and the samples of every signal.
    void PrintEvent() const override;
    /// \brief Draws the signals in a TMultiGraph (time bin vs ADC), the color by channel (ID % 72).
    /// \param option Selection of the signals (case insensitive): empty for all, a signal ID ("4400"), a
    /// range of IDs ("4320-4391", "ids[4320-4391]" or "ids[4320,4391]"); "printids" also prints the IDs
    /// drawn.
    /// \return The pad of the event, or nullptr for an event without signals.
    TPad* DrawEvent(const std::string& option = "") const override;

    TRestRawSignalEvent() = default;
    ~TRestRawSignalEvent() = default;
};

#endif
