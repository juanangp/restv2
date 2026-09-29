#include "TRestDetectorSignalEvent.h"

#include <iostream>

// ---------------------------------------------------------------------------
// Self-registration in EventRegistry (no macro)
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    EventRegistry::Instance().Register("TRestRawSignalEvent",
                                       []() { return std::make_unique<TRestDetectorSignalEvent>(); });
    return true;
}();
}  // namespace



int TRestDetectorSignal::GetTimeIndex(double t) const {
    int start = fData->offsets[fSignalIdx];
    int nPoints = GetNPoints();

    for (int n = 0; n < nPoints; n++) {
        if (t == fData->allSignalsTime[start + n]) {
            return n;
        }
    }
    return -1;
}


void TRestDetectorSignal::AddPoint(double time, double amplitude) {
    int start = fData->offsets[fSignalIdx];
    int nPoints = GetNPoints();
    int insertPos = start + nPoints;

    fData->allSignalsTime.insert(fData->allSignalsTime.begin() + insertPos, time);
    fData->allSignalsCharge.insert(fData->allSignalsCharge.begin() + insertPos, amplitude);

    for (size_t i = fSignalIdx + 1; i < fData->offsets.size(); ++i) {
        fData->offsets[i] += 1;
    }
}

void TRestDetectorSignal::IncreaseAmplitude(double time, double amplitude) {

    int localIndex = GetTimeIndex(time);

    if (localIndex >= 0) {
        int globalIdx = fData->offsets[fSignalIdx] + localIndex;
        fData->allSignalsTime[globalIdx] = time;
        fData->allSignalsCharge[globalIdx] += amplitude;
    } else {
        AddPoint(time, amplitude);
    }
}

void TRestDetectorSignal::Sort() {
    int start = fData->offsets[fSignalIdx];
    int nPoints = GetNPoints();
    if (nPoints <= 1) return;

    std::vector<int> indices(nPoints);
    std::iota(indices.begin(), indices.end(), 0);

    std::sort(indices.begin(), indices.end(), [this, start](int a, int b) {
        return fData->allSignalsTime[start + a] < fData->allSignalsTime[start + b];
    });

    bool alreadySorted = true;
    for (int i = 0; i < nPoints; ++i) {
        if (indices[i] != i) {
            alreadySorted = false;
            break;
        }
    }
    if (alreadySorted) return;

    std::vector<double> sortedTime(nPoints);
    std::vector<double> sortedCharge(nPoints);

    for (int i = 0; i < nPoints; ++i) {
        sortedTime[i] = fData->allSignalsTime[start + indices[i]];
        sortedCharge[i] = fData->allSignalsCharge[start + indices[i]];
    }

    std::copy(sortedTime.begin(), sortedTime.end(), fData->allSignalsTime.begin() + start);
    std::copy(sortedCharge.begin(), sortedCharge.end(), fData->allSignalsCharge.begin() + start);
}

void TRestDetectorSignal::AddOffset(double offset) {
    int start = fData->offsets[fSignalIdx];
    int nPoints = GetNPoints();

    for (int i = 0; i < nPoints; i++) {
        fData->allSignalsCharge[start + i] += offset;
    }
}

void TRestDetectorSignal::MultiplySignalBy(double factor) {
    int start = fData->offsets[fSignalIdx];
    int nPoints = GetNPoints();

    for (int i = 0; i < nPoints; i++) {
        fData->allSignalsCharge[start + i] *= factor;
    }
}

void TRestDetectorSignal::ExponentialConvolution(double fromTime, double decayTime, double offset) {
    int start = fData->offsets[fSignalIdx];
    int nPoints = GetNPoints();

    for (int i = 0; i < nPoints; i++) {
        int globalIdx = start + i;
        if (fData->allSignalsTime[globalIdx] > fromTime) {
            fData->allSignalsCharge[globalIdx] =
                (fData->allSignalsCharge[globalIdx] - offset) * std::exp(-(fData->allSignalsTime[globalIdx] - fromTime) / decayTime) + offset;
        }
    }
}

void TRestDetectorSignal::SignalAddition(const TRestDetectorSignal* inSignal) {
    if (this->GetNPoints() != inSignal->GetNPoints()) {
        RESTError << "ERROR : I cannot add two signals with different number of points" << RESTendl;
        return;
    }

    int start = fData->offsets[fSignalIdx];
    int inStart = inSignal->fData->offsets[inSignal->fSignalIdx];
    int nPoints = GetNPoints();
    int badallSignalsTimes = 0;

    for (int i = 0; i < nPoints; i++) {
        if (fData->allSignalsTime[start + i] != inSignal->fData->allSignalsTime[inStart + i]) {
            std::cout << "Time : " << fData->allSignalsTime[start + i] 
                      << " != " << inSignal->fData->allSignalsTime[inStart + i] << std::endl;
            badallSignalsTimes++;
        }
    }

    if (badallSignalsTimes) {
        std::cout << "ERROR : The times of signal addition must be the same" << std::endl;
        return;
    }

    for (int i = 0; i < nPoints; i++) {
        fData->allSignalsCharge[start + i] += inSignal->fData->allSignalsCharge[inStart + i];
    }
}

TPad* TRestDetectorSignalEvent::DrawEvent(const TString& /*option*/) const {
    // Placeholder – actual drawing would create a TPad with TGraphs
    RESTWarning << "TRestDetectorSignalEvent::DrawEvent not yet implemented" << RESTendl;
    return nullptr;
}

void TRestDetectorSignalEvent::PrintEvent() const {
    TRestEvent::PrintEvent();
    RESTLog << "  Signals: " << GetNumberOfSignals() << RESTendl;

    for (int i = 0; i < GetNumberOfSignals(); ++i) {
        TRestDetectorSignal sig = GetSignal(i);

        RESTLog << " SignalID " << sig.GetSignalID() << " (nPoints=" << sig.GetNPoints() << "): ";

        auto points = sig.GetPointsVector();
        for (const auto &[t,a] : points) {
            RESTLog << t << " "<<a<<RESTendl;
        }
        RESTLog << RESTendl;
    }
}

