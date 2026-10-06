#include "TRestTrack.h"
#include "TRestTrackEvent.h"
#include <algorithm>
#include <limits>
#include <numeric>
#include <iostream>

TRestTrack::TRestTrack(TRestTrackEvent* event, std::size_t index) 
    : fEvent(event), fTrackIndex(index) {}

bool TRestTrack::IsValid() const {
    return fEvent != nullptr && fTrackIndex < fEvent->fTrackHits.size();
}

std::size_t TRestTrack::GetHitStart() const {
    if (!IsValid()) return 0;
    return fEvent->GetTrackStartIndex(fTrackIndex);
}

std::size_t TRestTrack::GetHitCount() const {
    if (!IsValid()) return 0;
    return fEvent->fData.trackNHits[fTrackIndex];
}

const TRestHits TRestTrack::GetHits() const {
    if (!IsValid()) return TRestHits();
    return fEvent->fTrackHits[fTrackIndex];
}

Int_t TRestTrack::GetTrackID() const {
    return IsValid() ? fEvent->fData.trackID[fTrackIndex] : -1;
}

Int_t TRestTrack::GetParentID() const {
    return IsValid() ? fEvent->fData.parentID[fTrackIndex] : -1;
}

Int_t TRestTrack::GetNumberOfHits() const {
    return IsValid() ? fEvent->fData.trackNHits[fTrackIndex] : 0;
}

Double_t TRestTrack::GetEnergy() const {
    if (!IsValid()) return 0.0;
    return fEvent->GetTrackEnergy(fTrackIndex);
}

Double_t TRestTrack::GetLength() const {
    if (!IsValid()) return 0.0;
    return fEvent->fTrackHits[fTrackIndex].GetTotalDistance();
}

Double_t TRestTrack::GetVolume() const {
    if (!IsValid()) return 0.0;
    return fEvent->fTrackHits[fTrackIndex].GetMaximumHitDistance2();
}

void TRestTrack::ComputeBoundaries(const TRestHits& hits, XYZVector& orig, XYZVector& end) {
    const int nHits = (int)hits.GetNumberOfHits();
    if (nHits == 0) return;
    int maxBin = 0;
    double maxEn = 0;
    for (int i = 0; i < nHits; i++) {
        double en = hits.GetEnergy(i);
        if (en > maxEn) { maxEn = en; maxBin = i; }
    }
    auto maxPos = hits.GetPosition(maxBin);
    auto pos0 = hits.GetPosition(0);
    auto posE = hits.GetPosition(nHits - 1);
    if ((pos0 - maxPos).R() < (posE - maxPos).R()) {
        end = pos0; orig = posE;
    } else {
        orig = pos0; end = posE;
    }
}

void TRestTrack::GetBoundaries(XYZVector& orig, XYZVector& end) const {
    if (!IsValid()) return;
    ComputeBoundaries(fEvent->fTrackHits[fTrackIndex], orig, end);
}

std::vector<float> TRestTrack::GetSigmaX() const {
    std::vector<float> vec;
    if (!IsValid()) return vec;
    std::size_t start = GetHitStart();
    std::size_t count = GetHitCount();
    vec.assign(fEvent->fData.sigmaX.begin() + start, fEvent->fData.sigmaX.begin() + start + count);
    return vec;
}

std::vector<float> TRestTrack::GetSigmaY() const {
    std::vector<float> vec;
    if (!IsValid()) return vec;
    std::size_t start = GetHitStart();
    std::size_t count = GetHitCount();
    vec.assign(fEvent->fData.sigmaY.begin() + start, fEvent->fData.sigmaY.begin() + start + count);
    return vec;
}

std::vector<float> TRestTrack::GetSigmaZ() const {
    std::vector<float> vec;
    if (!IsValid()) return vec;
    std::size_t start = GetHitStart();
    std::size_t count = GetHitCount();
    vec.assign(fEvent->fData.sigmaZ.begin() + start, fEvent->fData.sigmaZ.begin() + start + count);
    return vec;
}

float TRestTrack::GetSigmaX(std::size_t localHitIdx) const {
    return fEvent->fData.sigmaX[GetHitStart() + localHitIdx];
}

float TRestTrack::GetSigmaY(std::size_t localHitIdx) const {
    return fEvent->fData.sigmaY[GetHitStart() + localHitIdx];
}

float TRestTrack::GetSigmaZ(std::size_t localHitIdx) const {
    return fEvent->fData.sigmaZ[GetHitStart() + localHitIdx];
}
