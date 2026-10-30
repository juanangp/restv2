#include "TRestTrack.h"

#include "TRestHitsAnalysis.h"
#include "TRestTrackEvent.h"

TRestTrack::TRestTrack(TRestTrackEvent* event, std::size_t index) : fEvent(event), fTrackIndex(index) {}

bool TRestTrack::IsValid() const { return fEvent != nullptr && fTrackIndex < fEvent->fTrackHits.size(); }

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

Int_t TRestTrack::GetTrackID() const { return IsValid() ? fEvent->fData.trackID[fTrackIndex] : -1; }

Int_t TRestTrack::GetParentID() const { return IsValid() ? fEvent->fData.parentID[fTrackIndex] : -1; }

Int_t TRestTrack::GetNumberOfHits() const { return IsValid() ? fEvent->fData.trackNHits[fTrackIndex] : 0; }

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

void TRestTrack::GetBoundaries(XYZVector& orig, XYZVector& end) const {
    if (!IsValid()) return;
    const auto& hits = fEvent->fTrackHits[fTrackIndex];
    TRestHitsAnalysis::GetTrackEnds(hits.GetPoints(), hits.GetEnergies(), orig, end);
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
