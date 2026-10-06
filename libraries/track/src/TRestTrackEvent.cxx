#include "TRestTrackEvent.h"
#include "TRestTrack.h" 
#include "TRestLogManager.h"

#include <algorithm>
#include <limits>
#include <numeric>

#include "TRandom.h"

namespace {
const bool kRegistered = []() {
    EventRegistry::Instance().Register("TRestTrackEvent",
                                       []() { return std::make_unique<TRestTrackEvent>(); });
    return true;
}();

}  // namespace


int TRestTrackEvent::GetTrackStartIndex(int n) const {
    int start = 0;
    for (int i = 0; i < n; i++) start += fData.trackNHits[i];
    return start;
}

int TRestTrackEvent::FindTrackIndexById(Int_t id) const {
    for (size_t i = 0; i < fData.trackID.size(); i++)
        if (fData.trackID[i] == id) return (int)i;
    return -1;
}

Int_t TRestTrackEvent::AddTrack(Int_t trackID, Int_t parentID) {
    fData.trackID.push_back(trackID);
    fData.parentID.push_back(parentID);
    fData.trackNHits.push_back(0);
    RebuildTrackViews();
    SetLevels();
    return (Int_t)fData.trackID.size() - 1;
}

Int_t TRestTrackEvent::AddTrack(Int_t trackID, Int_t parentID, const TRestHits& hits,
                               const std::vector<XYZVector>& sigmas) {
    Int_t index = AddTrack(trackID, parentID);

    const size_t n = hits.GetNumberOfHits();
    for (size_t i = 0; i < n; i++) {
        XYZVector sigma = (i < sigmas.size()) ? sigmas[i] : XYZVector(0,0,0);
        AddHitToTrack(hits.GetPosition((int)i), hits.GetEnergy((int)i), hits.GetTime((int)i),
                      hits.GetType((int)i), sigma);
    }
    return index;
}

void TRestTrackEvent::AddHitToTrack(const XYZVector& position, Double_t energy, Double_t time,
                                    REST_HitType type, const XYZVector& sigma) {
    fData.hits.x.push_back((float)position.X());
    fData.hits.y.push_back((float)position.Y());
    fData.hits.z.push_back((float)position.Z());
    fData.hits.time.push_back((float)time);
    fData.hits.energy.push_back((float)energy);
    fData.hits.type.push_back((int)type);
    
    fData.sigmaX.push_back((float)sigma.X());
    fData.sigmaY.push_back((float)sigma.Y());
    fData.sigmaZ.push_back((float)sigma.Z());
    
    fData.trackNHits.back()++;
    RebuildTrackViews();
}

void TRestTrackEvent::RemoveTrack(Int_t n) {
    int start = GetTrackStartIndex(n);
    int count = fData.trackNHits[n];

    fData.hits.x.erase(fData.hits.x.begin() + start, fData.hits.x.begin() + start + count);
    fData.hits.y.erase(fData.hits.y.begin() + start, fData.hits.y.begin() + start + count);
    fData.hits.z.erase(fData.hits.z.begin() + start, fData.hits.z.begin() + start + count);
    fData.hits.time.erase(fData.hits.time.begin() + start, fData.hits.time.begin() + start + count);
    fData.hits.energy.erase(fData.hits.energy.begin() + start, fData.hits.energy.begin() + start + count);
    fData.hits.type.erase(fData.hits.type.begin() + start, fData.hits.type.begin() + start + count);
    fData.sigmaX.erase(fData.sigmaX.begin() + start, fData.sigmaX.begin() + start + count);
    fData.sigmaY.erase(fData.sigmaY.begin() + start, fData.sigmaY.begin() + start + count);
    fData.sigmaZ.erase(fData.sigmaZ.begin() + start, fData.sigmaZ.begin() + start + count);

    fData.trackID.erase(fData.trackID.begin() + n);
    fData.parentID.erase(fData.parentID.begin() + n);
    fData.trackNHits.erase(fData.trackNHits.begin() + n);

    RebuildTrackViews();
    SetLevels();
}

Int_t TRestTrackEvent::GetNumberOfTracks(const TString& option) const {
    if (option == "") return (Int_t)fTrackHits.size();

    Int_t nT = 0;
    for (int n = 0; n < (Int_t)fTrackHits.size(); n++) {
        if (!isTopLevel(n)) continue;

        if (option == "X" && isXZ(n))
            nT++;
        else if (option == "Y" && isYZ(n))
            nT++;
        else if (option == "XYZ" && isXYZ(n))
            nT++;
    }
    return nT;
}

Int_t TRestTrackEvent::GetOriginTrackID(Int_t tck) const {
    Int_t originTrackID = fData.trackID[tck];
    Int_t pID = fData.parentID[tck];

    while (pID != 0) {
        originTrackID = pID;
        int idx = FindTrackIndexById(originTrackID);
        if (idx < 0) break;
        pID = fData.parentID[idx];
    }

    return originTrackID;
}

TRestTrack TRestTrackEvent::GetTrack(std::size_t n) { 
    return TRestTrack(this, n); 
}

TRestTrack TRestTrackEvent::GetTrack(std::size_t n) const { 
    return TRestTrack(const_cast<TRestTrackEvent*>(this), n); 
}

TRestTrack TRestTrackEvent::GetTrackById(Int_t id) {
    int idx = FindTrackIndexById(id);
    if (idx < 0) return TRestTrack();
    return TRestTrack(this, idx);
}

TRestTrack TRestTrackEvent::GetTrackById(Int_t id) const {
    int idx = FindTrackIndexById(id);
    if (idx < 0) return TRestTrack();
    return TRestTrack(const_cast<TRestTrackEvent*>(this), idx);
}

TRestTrack TRestTrackEvent::GetMaxEnergyTrackInX() {
    Int_t track = -1;
    Double_t maxEnergy = 0;
    for (int tck = 0; tck < (Int_t)fTrackHits.size(); tck++) {
        if (!isTopLevel(tck) || !isXZ(tck)) continue;
        if (GetTrackEnergy(tck) > maxEnergy) {
            maxEnergy = GetTrackEnergy(tck);
            track = tck;
        }
    }
    return track == -1 ? TRestTrack() : TRestTrack(this, track);
}

TRestTrack TRestTrackEvent::GetMaxEnergyTrackInY() {
    Int_t track = -1;
    Double_t maxEnergy = 0;
    for (int tck = 0; tck < (Int_t)fTrackHits.size(); tck++) {
        if (!isTopLevel(tck) || !isYZ(tck)) continue;
        if (GetTrackEnergy(tck) > maxEnergy) {
            maxEnergy = GetTrackEnergy(tck);
            track = tck;
        }
    }
    return track == -1 ? TRestTrack() : TRestTrack(this, track);
}

TRestTrack TRestTrackEvent::GetMaxEnergyTrack(const TString& option) {
    if (option == "X") return GetMaxEnergyTrackInX();
    if (option == "Y") return GetMaxEnergyTrackInY();

    Int_t track = -1;
    Double_t maxEnergy = 0;
    for (int tck = 0; tck < (Int_t)fTrackHits.size(); tck++) {
        if (!isTopLevel(tck) || !isXYZ(tck)) continue;
        if (GetTrackEnergy(tck) > maxEnergy) {
            maxEnergy = GetTrackEnergy(tck);
            track = tck;
        }
    }
    return track == -1 ? TRestTrack() : TRestTrack(this, track);
}

TRestTrack TRestTrackEvent::GetSecondMaxEnergyTrack(const TString& option) {
    TRestTrack first = GetMaxEnergyTrack(option);
    if (first.GetEvent() == nullptr) return TRestTrack(); // Reemplazo de !first

    Int_t firstTck = (Int_t)first.GetIndex();
    Int_t id = GetTrackID(firstTck);

    Int_t track = -1;
    Double_t maxEnergy = 0;
    for (int tck = 0; tck < (Int_t)fTrackHits.size(); tck++) {
        if (!isTopLevel(tck) || GetTrackID(tck) == id) continue;

        Double_t en = GetTrackEnergy(tck);
        bool matches = false;
        if (option == "X" && (isXZ(tck) || isXYZ(tck)))
            matches = true;
        else if (option == "Y" && (isYZ(tck) || isXYZ(tck)))
            matches = true;
        else if (option != "X" && option != "Y" && isXYZ(tck))
            matches = true;

        if (matches && en > maxEnergy) {
            maxEnergy = en;
            track = tck;
        }
    }
    return track == -1 ? TRestTrack() : TRestTrack(this, track);
}

TRestTrack TRestTrackEvent::GetOriginTrack(Int_t tck) {
    return GetTrackById(GetOriginTrackID(tck));
}

TRestTrack TRestTrackEvent::GetOriginTrackById(Int_t tckId) {
    int idx = FindTrackIndexById(tckId);
    if (idx < 0) return TRestTrack();
    return GetOriginTrack(idx);
}

Double_t TRestTrackEvent::GetMaxEnergyTrackVolume(const TString& option) {
    TRestTrack t = GetMaxEnergyTrack(option);
    return t.GetEvent() != nullptr ? t.GetVolume() : 0;
}

Double_t TRestTrackEvent::GetMaxEnergyTrackLength(const TString& option) {
    TRestTrack t = GetMaxEnergyTrack(option);
    return t.GetEvent() != nullptr ? t.GetLength() : 0;
}

Double_t TRestTrackEvent::GetEnergy(const TString& option) {
    Double_t en = 0;
    for (int tck = 0; tck < (Int_t)fTrackHits.size(); tck++) {
        if (!isTopLevel(tck)) continue;

        if (option == "")
            en += GetTrackEnergy(tck);
        else if (option == "X" && (isXZ(tck) || isXYZ(tck)))
            en += GetTrackEnergy(tck);
        else if (option == "Y" && (isYZ(tck) || isXYZ(tck)))
            en += GetTrackEnergy(tck);
        else if (option == "XYZ" && isXYZ(tck))
            en += GetTrackEnergy(tck);
    }
    return en;
}

Bool_t TRestTrackEvent::isXYZ() const {
    for (int tck = 0; tck < (Int_t)fTrackHits.size(); tck++)
        if (!isXYZ(tck)) return false;
    return true;
}

Int_t TRestTrackEvent::GetTotalHits() const {
    Int_t totHits = 0;
    for (const auto& track : fTrackHits) totHits += (Int_t)track.GetNumberOfHits();
    return totHits;
}

// .cxx
void TRestTrackEvent::SwapTrackHits(Int_t n, int i, int j) {
    const int start = GetTrackStartIndex(n);
    const int gi = start + i, gj = start + j;

    std::swap(fData.hits.x[gi], fData.hits.x[gj]);
    std::swap(fData.hits.y[gi], fData.hits.y[gj]);
    std::swap(fData.hits.z[gi], fData.hits.z[gj]);
    std::swap(fData.hits.time[gi], fData.hits.time[gj]);
    std::swap(fData.hits.energy[gi], fData.hits.energy[gj]);
    std::swap(fData.hits.type[gi], fData.hits.type[gj]);
    std::swap(fData.sigmaX[gi], fData.sigmaX[gj]);
    std::swap(fData.sigmaY[gi], fData.sigmaY[gj]);
    std::swap(fData.sigmaZ[gi], fData.sigmaZ[gj]);
}

void TRestTrackEvent::SortTrackHits(Int_t n, std::function<bool(int, int)> compareCondition) {
    const int start = GetTrackStartIndex(n);
    const int count = fData.trackNHits[n];
    if (count < 2) return;

    auto order = TRestHitsUtils::BuildSortOrder(
        count, compareCondition,
        [this, start](int a, int b) { return fData.hits.z[start + a] < fData.hits.z[start + b]; });

    TRestHitsUtils::ApplyPermutation(order, start, fData.hits.x, fData.hits.y, fData.hits.z,
                                     fData.hits.time, fData.hits.energy, fData.hits.type, fData.sigmaX,
                                     fData.sigmaY, fData.sigmaZ);
}

void TRestTrackEvent::ShuffleTrackHits(Int_t n, int NLoop) {
    const int start = GetTrackStartIndex(n);
    const int count = fData.trackNHits[n];
    if (count < 2) return;

    auto order = TRestHitsUtils::BuildShuffleOrder(count, NLoop);

    TRestHitsUtils::ApplyPermutation(order, start, fData.hits.x, fData.hits.y, fData.hits.z,
                                     fData.hits.time, fData.hits.energy, fData.hits.type, fData.sigmaX,
                                     fData.sigmaY, fData.sigmaZ);
}

void TRestTrackEvent::RemoveTrackHit(Int_t n, int hitIdx) {
    const int idx = GetTrackStartIndex(n) + hitIdx;

    fData.hits.x.erase(fData.hits.x.begin() + idx);
    fData.hits.y.erase(fData.hits.y.begin() + idx);
    fData.hits.z.erase(fData.hits.z.begin() + idx);
    fData.hits.time.erase(fData.hits.time.begin() + idx);
    fData.hits.energy.erase(fData.hits.energy.begin() + idx);
    fData.hits.type.erase(fData.hits.type.begin() + idx);
    fData.sigmaX.erase(fData.sigmaX.begin() + idx);
    fData.sigmaY.erase(fData.sigmaY.begin() + idx);
    fData.sigmaZ.erase(fData.sigmaZ.begin() + idx);

    fData.trackNHits[n]--;
    RebuildTrackViews();
}


void TRestTrackEvent::MergeTrackHits(Int_t n, int i, int j) {
    const int start = GetTrackStartIndex(n);
    const int gi = start + i, gj = start + j;

    const double enI = fData.hits.energy[gi];
    const double enJ = fData.hits.energy[gj];
    const double totalEnergy = enI + enJ;

    if (totalEnergy > 0) {
        fData.hits.x[gi] = (fData.hits.x[gi] * enI + fData.hits.x[gj] * enJ) / totalEnergy;
        fData.hits.y[gi] = (fData.hits.y[gi] * enI + fData.hits.y[gj] * enJ) / totalEnergy;
        fData.hits.z[gi] = (fData.hits.z[gi] * enI + fData.hits.z[gj] * enJ) / totalEnergy;
        fData.hits.time[gi] = (fData.hits.time[gi] * enI + fData.hits.time[gj] * enJ) / totalEnergy;

        fData.sigmaX[gi] = (fData.sigmaX[gi] * enI + fData.sigmaX[gj] * enJ) / totalEnergy;
        fData.sigmaY[gi] = (fData.sigmaY[gi] * enI + fData.sigmaY[gj] * enJ) / totalEnergy;
        fData.sigmaZ[gi] = (fData.sigmaZ[gi] * enI + fData.sigmaZ[gj] * enJ) / totalEnergy;

        fData.hits.energy[gi] = (float)totalEnergy;
    }

    RemoveTrackHit(n, j);
}

Int_t TRestTrackEvent::GetLevel(Int_t tck) const {
    Int_t lvl = 1;
    Int_t parentTrackId = fData.parentID[tck];

    while (parentTrackId > 0) {
        lvl++;
        int idx = FindTrackIndexById(parentTrackId);
        if (idx < 0) break;
        parentTrackId = fData.parentID[idx];
    }
    return lvl;
}

void TRestTrackEvent::SetLevels() {
    Int_t maxLevel = 0;
    for (int tck = 0; tck < (Int_t)fData.trackID.size(); tck++) {
        Int_t lvl = GetLevel(tck);
        if (maxLevel < lvl) maxLevel = lvl;
    }
    fLevels = maxLevel;
}

Bool_t TRestTrackEvent::isTopLevel(Int_t tck) const { return GetLevels() == GetLevel(tck); }

///////////////////////////////////////////////
/// \brief Retrieves the relative Z position of the most energetic track's
/// half-energy integral, to crosscheck if the track is upwards or downwards.
///
Double_t TRestTrackEvent::GetMaxTrackRelativeZ() {
    TRestTrack tckX = GetMaxEnergyTrackInX();
    TRestTrack tckY = GetMaxEnergyTrackInY();

    if (tckX.GetEvent() == nullptr || tckY.GetEvent() == nullptr) {
        RESTWarning << "Track is empty, skipping" << RESTendl;
        return -1;
    }

    TRestHits hitsX = tckX.GetHits();
    TRestHits hitsY = tckY.GetHits();

    std::vector<std::pair<double, double>> zEn;
    double totEn = 0;

    for (unsigned int i = 0; i < hitsX.GetNumberOfHits(); i++) {
        zEn.emplace_back(hitsX.GetZ(i), hitsX.GetEnergy(i));
        totEn += hitsX.GetEnergy(i);
    }
    for (unsigned int i = 0; i < hitsY.GetNumberOfHits(); i++) {
        zEn.emplace_back(hitsY.GetZ(i), hitsY.GetEnergy(i));
        totEn += hitsY.GetEnergy(i);
    }

    std::sort(zEn.begin(), zEn.end());

    double integ = 0;
    size_t pos = 0;
    for (pos = 0; pos < zEn.size(); pos++) {
        integ += zEn[pos].second;
        if (integ >= totEn / 2.) break;
    }

    double length = zEn.front().first - zEn.back().first;
    double diff = zEn.front().first - zEn[pos].first;

    return length == 0 ? 0 : diff / length;
}

///////////////////////////////////////////////
/// \brief Retrieves the origin and end of the track based on the most
/// energetic hit, combining the XZ and YZ projections.
///
void TRestTrackEvent::GetMaxTrackBoundaries(XYZVector& orig, XYZVector& end) {
    TRestTrack tckX = GetMaxEnergyTrackInX();
    TRestTrack tckY = GetMaxEnergyTrackInY();

    if (tckX.GetEvent() == nullptr || tckY.GetEvent() == nullptr) {
        RESTWarning << "Track is empty, skipping" << RESTendl;
        return;
    }

    XYZVector origX, endX, origY, endY;
    TRestTrack::ComputeBoundaries(tckX.GetHits(), origX, endX);
    TRestTrack::ComputeBoundaries(tckY.GetHits(), origY, endY);

    double originZ = (origX.Z() + origY.Z()) / 2.;
    double endZ = (endX.Z() + endY.Z()) / 2.;

    orig = XYZVector(origX.X(), origY.Y(), originZ);
    end = XYZVector(endX.X(), endY.Y(), endZ);
}

///////////////////////////////////////////////
/// \brief Reduces the hits of a track into `nodes` centroids via k-means
/// clustering, and stores the result as a new child track.
///
Int_t TRestTrackEvent::KMeansClustering(Int_t trackIndex, Int_t nodes, Int_t maxIt, Bool_t fixBoundaries) {
    TRestHits& src = fTrackHits[trackIndex];
    const int nHits = (int)src.GetNumberOfHits();
    if (nodes < 2 || nHits == 0) return -1;

    std::vector<XYZVector> centroid(nodes), centroidOld(nodes);
    for (int n = 0; n < nodes; n++) {
        int idx = (nodes > 1) ? (n * (nHits - 1)) / (nodes - 1) : 0;
        centroid[n] = src.GetPosition(idx);
    }

    std::vector<std::vector<int>> clusterHits(nodes);

    for (int it = 0; it < maxIt; it++) {
        for (auto& v : clusterHits) v.clear();

        for (int i = 0; i < nHits; i++) {
            double minDist = std::numeric_limits<double>::max();
            int clIndex = -1;
            XYZVector hitPos = src.GetPosition(i);
            for (int n = 0; n < nodes; n++) {
                if (fixBoundaries && (n == 0 || n == nodes - 1)) continue;
                double dist = (centroid[n] - hitPos).R();
                if (dist < minDist) { minDist = dist; clIndex = n; }
            }
            if (clIndex >= 0) clusterHits[clIndex].push_back(i);
        }

        bool converge = true;
        for (int n = 0; n < nodes; n++) {
            if (fixBoundaries && (n == 0 || n == nodes - 1)) continue;
            if (!clusterHits[n].empty()) {
                double meanX = 0, meanY = 0, meanZ = 0, totalEnergy = 0;
                for (int i : clusterHits[n]) {
                    double en = src.GetEnergy(i);
                    XYZVector pos = src.GetPosition(i);
                    meanX += pos.X() * en;
                    meanY += pos.Y() * en;
                    meanZ += pos.Z() * en;
                    totalEnergy += en;
                }
                if (totalEnergy > 0)
                    centroid[n] = XYZVector(meanX / totalEnergy, meanY / totalEnergy, meanZ / totalEnergy);
            }
            converge &= ((centroid[n] - centroidOld[n]).R() < 1e-9);
            centroidOld[n] = centroid[n];
        }
        if (converge) break;
    }

    const REST_HitType type = src.GetType(0);
    std::vector<double> clusterEnergy(nodes, 0.0);
    for (int n = 0; n < nodes; n++) {
        for (int i : clusterHits[n]) clusterEnergy[n] += src.GetEnergy(i);
    }

    Int_t newTrackIndex = AddTrack(GetTrackID(trackIndex), GetTrackID(trackIndex));
    for (int n = 0; n < nodes; n++) {
        if (fixBoundaries && (n == 0 || n == nodes - 1)) {
            AddHitToTrack(centroid[n], 0, 0, type);
        } else if (!clusterHits[n].empty()) {
            AddHitToTrack(centroid[n], clusterEnergy[n], 0, type);
        }
    }

    return newTrackIndex;
}

void TRestTrackEvent::PrintOnlyTracks() const {
    std::cout << "TrackEvent " << GetID() << std::endl;
    std::cout << "-----------------------" << std::endl;
    for (int i = 0; i < (Int_t)fTrackHits.size(); i++) {
        std::cout << "Track " << i << " id : " << GetTrackID(i) << " parent : " << GetParentID(i)
                 << std::endl;
    }
    std::cout << "-----------------------" << std::endl;
    std::cout << "Track levels : " << GetLevels() << std::endl;
}

void TRestTrackEvent::PrintTrackEvent(Bool_t fullInfo) const {
    TRestEvent::PrintEvent();

    std::cout << "Number of tracks : "
             << GetNumberOfTracks("XYZ") + GetNumberOfTracks("X") + GetNumberOfTracks("Y") << std::endl;
    std::cout << "Number of tracks XZ " << GetNumberOfTracks("X") << std::endl;
    std::cout << "Number of tracks YZ " << GetNumberOfTracks("Y") << std::endl;
    std::cout << "Track levels : " << GetLevels() << std::endl;
    std::cout << "+++++++++++++++++++++++++++++++++++" << std::endl;
    for (int i = 0; i < (Int_t)fTrackHits.size(); i++) PrintTrack(i, fullInfo);
}

void TRestTrackEvent::PrintTrack(Int_t n, Bool_t fullInfo) const {
    XYZVector mean = fTrackHits[n].GetMeanPosition();

    std::cout << "Track ID : " << GetTrackID(n) << " Parent ID : " << GetParentID(n);

    if (isXY(n)) std::cout << " is XY " << std::endl;
    if (isXZ(n)) std::cout << " is XZ " << std::endl;
    if (isYZ(n)) std::cout << " is YZ " << std::endl;
    if (isXYZ(n)) std::cout << " is XYZ " << std::endl;
    std::cout << "Energy : " << GetTrackEnergy(n) << std::endl;
    std::cout << "Length : " << GetTrackLength(n) << std::endl;
    std::cout << "Mean position : ( " << mean.X() << " , " << mean.Y() << " , " << mean.Z() << " ) "
             << std::endl;
    std::cout << "Number of track hits : " << GetNumberOfHits(n) << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    if (fullInfo) {
        fTrackHits[n].PrintHits();
        std::cout << "----------------------------------------" << std::endl;
    }
}
