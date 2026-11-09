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
//  One signal (time series) from a single readout channel,
//  stored as (time, amplitude) points in double precision.
// ============================================================
/// \brief Shared contiguous storage used by TRestDetectorSignal lightweight views: the points of all the
/// signals of an event, concatenated.
struct TRestDetectorSignalData {
    std::vector<double> allSignalsCharge;  ///< Amplitude (charge) of the points of all the signals
    std::vector<double> allSignalsTime;    ///< Time of the points of all the signals
    std::vector<int> signalIDs;            ///< Channel (DAQ) ID of each signal
    std::vector<int> offsets;              ///< Index of the first point of each signal in the point vectors

    /// \brief Removes all the signals.
    void clear() {
        allSignalsCharge.clear();
        allSignalsTime.clear();
        signalIDs.clear();
        offsets.clear();
    }
};

/// \class TRestDetectorSignal
/// \brief Non-owning view over one signal stored inside TRestDetectorSignalData: the (time, amplitude)
/// points of one readout channel. Obtained from TRestDetectorSignalEvent::GetSignal; it is invalidated when
/// signals are added to or removed from the event.
class TRestDetectorSignal {
   protected:
    TRestDetectorSignalData* fData = nullptr;  ///< Storage of the event the signal belongs to
    int fSignalIdx = -1;                       ///< Index of the signal in fData

   public:
    /// \brief Builds a view over the signal idx of data.
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

    /// \brief Copies the times of the points of this signal into a standalone vector.
    std::vector<double> GetTime() const {
        int start = fData->offsets[fSignalIdx];
        int nPoints = GetNPoints();
        return std::vector<double>(fData->allSignalsTime.begin() + start,
                                   fData->allSignalsTime.begin() + start + nPoints);
    }

    /// \brief Returns one sample by local signal index.
    std::pair<double, double> GetPoint(int i) const {
        int globalIdx = fData->offsets[fSignalIdx] + i;
        return {fData->allSignalsTime[globalIdx], fData->allSignalsCharge[globalIdx]};
    }

    /// \brief Returns the (time, amplitude) points of this signal.
    std::vector<std::pair<double, double>> GetPointsVector() const {
        int nPoints = GetNPoints();
        std::vector<std::pair<double, double>> signal;
        signal.reserve(nPoints);

        for (int i = 0; i < nPoints; ++i) {
            signal.push_back(GetPoint(i));
        }
        return signal;
    }

    /// \brief Adds amplitude to the point at exactly this time, or appends a new point if there is none.
    void IncreaseAmplitude(double time, double amplitude);
    /// \brief Appends a point at the end of the signal (the points are not sorted, see Sort).
    void AddPoint(double time, double amplitude);
    /// \brief Local index of the point with exactly time t, -1 if there is none.
    int GetTimeIndex(double t) const;
    /// \brief Sorts the points of the signal by time.
    void Sort();
    /// \brief Adds a constant to all the amplitudes.
    void AddOffset(double offset);
    /// \brief Multiplies all the amplitudes by a factor.
    void MultiplySignalBy(double factor);
    /// \brief Adds the amplitudes of another signal point by point. Both must have the same number of
    /// points and the same times; otherwise an error is printed and nothing is added.
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
/// \brief Event container holding the signals (time, amplitude points) of the readout channels of one
/// event, in physical units and double precision (unlike the digitized TRestRawSignalEvent).
///
/// Each signal is identified by its DAQ channel ID (one signal per ID) and accessed through a
/// TRestDetectorSignal view (GetSignal, GetSignalByID). Branches of the event tree: fSigTime,
/// fSigAmplitude, fSigIDs and fSigOffsets (plus those of TRestEvent). Produced by
/// TRestDetectorHitsToSignalProcess (simulations) or from the raw signals (TRestRawToDetectorSignalProcess,
/// connectors library), and used by the signal processes of this library (shaping, recovery, peaks finder,
/// signal to hits) and by TRestDetectorSignalToRawSignalProcess (connectors).
class TRestDetectorSignalEvent : public TRestEvent {
   protected:
    TRestDetectorSignalData fSignalData;                     ///< Points of all the signals
    mutable std::vector<TRestDetectorSignal> fSignalsViews;  ///< One view per signal (RefreshViews)

    // Branch addresses: views into the vectors of fSignalData, transient (as in TRestDetectorHitsEvent)
    std::vector<double>* fPtrTime = nullptr;       //!< Branch address of fSigTime
    std::vector<double>* fPtrAmplitude = nullptr;  //!< Branch address of fSigAmplitude
    std::vector<int>* fPtrIDs = nullptr;           //!< Branch address of fSigIDs
    std::vector<int>* fPtrOffsets = nullptr;       //!< Branch address of fSigOffsets

   public:
    /// \brief Returns the class name, "TRestDetectorSignalEvent".
    std::string GetClassName() const override { return "TRestDetectorSignalEvent"; }

    /// \brief Resets the event (TRestEvent::Initialize) and removes all the signals.
    void Initialize() override {
        TRestEvent::Initialize();
        fSignalData.clear();
        fSignalsViews.clear();
    }

    /// \brief Creates the event branches in a tree: those of TRestEvent and fSigTime, fSigAmplitude,
    /// fSigIDs and fSigOffsets.
    void CreateBranches(TTree* tree) override {
        TRestEvent::CreateBranches(tree);
        tree->Branch("fSigTime", &fSignalData.allSignalsTime);
        tree->Branch("fSigAmplitude", &fSignalData.allSignalsCharge);
        tree->Branch("fSigIDs", &fSignalData.signalIDs);
        tree->Branch("fSigOffsets", &fSignalData.offsets);
    }

    /// \brief Binds the signal branches of an input tree (written by CreateBranches) to the storage.
    void SetBranchAddresses(TTree* tree) override {
        TRestEvent::SetBranchAddresses(tree);
        fPtrAmplitude = &fSignalData.allSignalsCharge;
        fPtrTime = &fSignalData.allSignalsTime;
        fPtrIDs = &fSignalData.signalIDs;
        fPtrOffsets = &fSignalData.offsets;

        BindBranch(tree, "fSigAmplitude", &fPtrAmplitude);
        BindBranch(tree, "fSigTime", &fPtrTime);
        BindBranch(tree, "fSigIDs", &fPtrIDs);
        BindBranch(tree, "fSigOffsets", &fPtrOffsets);
    }

    /// \brief Rebuilds one TRestDetectorSignal view per stored signal (after reading an entry).
    void RefreshViews() const override {
        fSignalsViews.clear();
        for (int i = 0; i < (int)fSignalData.signalIDs.size(); ++i) {
            fSignalsViews.emplace_back(const_cast<TRestDetectorSignalData*>(&fSignalData), i);
        }
    }

    /// \brief Copies the event header (TRestEvent::CopyFrom) and, if other is a TRestDetectorSignalEvent,
    /// its signals.
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

    /// \brief Whether the event has a signal of the channel id.
    bool HasSignalID(int id) const {
        return std::find(fSignalData.signalIDs.begin(), fSignalData.signalIDs.end(), id) !=
               fSignalData.signalIDs.end();
    }

    /// \brief Appends a new signal waveform associated with a detector channel ID.
    /// \param sID Channel (DAQ) ID; if the event already has it, a warning is printed and nothing is added.
    /// \param points (time, amplitude) points of the signal.
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

    /// \brief Appends a new signal from separate time and amplitude vectors (same size, otherwise a warning
    /// is printed and nothing is added).
    void AddSignal(int sID, const std::vector<double>& times, const std::vector<double>& amplitudes) {
        if (times.size() != amplitudes.size()) {
            std::cout << "Warning. Times and Amplitudes vectors size mismatch for Signal ID: " << sID
                      << std::endl;
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

    /// \brief Signal n (0 to GetNumberOfSignals() - 1); throws std::out_of_range otherwise.
    inline TRestDetectorSignal& GetSignal(int n) {
        if (fSignalsViews.size() != fSignalData.signalIDs.size()) RefreshViews();
        return fSignalsViews.at(n);
    }

    /// \brief Signal n (const).
    inline const TRestDetectorSignal& GetSignal(int n) const {
        if (fSignalsViews.size() != fSignalData.signalIDs.size()) RefreshViews();
        return fSignalsViews.at(n);
    }

    /// \brief Signal of the channel id; throws std::runtime_error if the event does not have it.
    inline TRestDetectorSignal& GetSignalByID(int id) {
        auto it = std::find(fSignalData.signalIDs.begin(), fSignalData.signalIDs.end(), id);
        if (it == fSignalData.signalIDs.end()) throw std::runtime_error("Signal ID not found");
        return GetSignal(std::distance(fSignalData.signalIDs.begin(), it));
    }

    /// \brief Signal of the channel id (const); throws std::runtime_error if the event does not have it.
    inline const TRestDetectorSignal& GetSignalByID(int id) const {
        auto it = std::find(fSignalData.signalIDs.begin(), fSignalData.signalIDs.end(), id);
        if (it == fSignalData.signalIDs.end()) throw std::runtime_error("Signal ID not found");
        return GetSignal(std::distance(fSignalData.signalIDs.begin(), it));
    }

    /// \brief Prints the event header and the points of each signal.
    void PrintEvent() const override;
    /// \brief Draws all the signals as lines in one TMultiGraph (color from the signal ID); the option is
    /// not used. \return The pad, nullptr for an empty event.
    TPad* DrawEvent(const std::string& option = "") const override;

    /// \brief Builds an empty event.
    TRestDetectorSignalEvent() = default;
    /// \brief Copies the header and the signals; the views are rebuilt over the storage of the copy when
    /// used (the implicit copy would leave them over the storage of `other`).
    TRestDetectorSignalEvent(const TRestDetectorSignalEvent& other)
        : TRestEvent(other), fSignalData(other.fSignalData) {}
    /// \brief Copies the header and the signals (see the copy constructor).
    TRestDetectorSignalEvent& operator=(const TRestDetectorSignalEvent& other) {
        if (this != &other) {
            TRestEvent::operator=(other);
            fSignalData = other.fSignalData;
            fSignalsViews.clear();
        }
        return *this;
    }
    ~TRestDetectorSignalEvent() = default;
};

#endif
