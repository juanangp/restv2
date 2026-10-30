#include "TRestTrackEvent.h"

#include <algorithm>
#include <limits>
#include <numeric>

#include "TGraph.h"
#include "TH1F.h"
#include "TH3F.h"
#include "TMarker.h"
#include "TPolyLine3D.h"
#include "TPolyMarker3D.h"
#include "TRestHitsAnalysis.h"
#include "TRestLogManager.h"
#include "TRestTools.h"
#include "TRestTrack.h"

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
    return (Int_t)fData.trackID.size() - 1;
}

Int_t TRestTrackEvent::AddTrack(Int_t trackID, Int_t parentID, const TRestHits& hits,
                                const std::vector<XYZVector>& sigmas) {
    Int_t index = AddTrack(trackID, parentID);

    const size_t n = hits.GetNumberOfHits();
    for (size_t i = 0; i < n; i++) {
        XYZVector sigma = (i < sigmas.size()) ? sigmas[i] : XYZVector(0, 0, 0);
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
    // The hit goes to the last track: only its view changes
    if (fTrackHits.size() == fData.trackNHits.size()) {
        const int nHits = fData.trackNHits.back();
        fTrackHits.back() = TRestHits(&fData.hits, (int)fData.hits.x.size() - nHits, nHits);
    } else {
        RebuildTrackViews();
    }
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
}

Int_t TRestTrackEvent::GetNumberOfTracks(const TString& option) const {
    if (option == "") return (Int_t)fTrackHits.size();

    Int_t nT = 0;
    for (int n = 0; n < (Int_t)fTrackHits.size(); n++) {
        if (option == "X" && isXZ(n))
            nT++;
        else if (option == "Y" && isYZ(n))
            nT++;
        else if (option == "XYZ" && isXYZ(n))
            nT++;
    }
    return nT;
}

TRestTrack TRestTrackEvent::GetTrack(std::size_t n) { return TRestTrack(this, n); }

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
        if (!isXZ(tck)) continue;
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
        if (!isYZ(tck)) continue;
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
        if (!isXYZ(tck)) continue;
        if (GetTrackEnergy(tck) > maxEnergy) {
            maxEnergy = GetTrackEnergy(tck);
            track = tck;
        }
    }
    return track == -1 ? TRestTrack() : TRestTrack(this, track);
}

TRestTrack TRestTrackEvent::GetSecondMaxEnergyTrack(const TString& option) {
    TRestTrack first = GetMaxEnergyTrack(option);
    if (first.GetEvent() == nullptr) return TRestTrack();  // Reemplazo de !first

    Int_t firstTck = (Int_t)first.GetIndex();
    Int_t id = GetTrackID(firstTck);

    Int_t track = -1;
    Double_t maxEnergy = 0;
    for (int tck = 0; tck < (Int_t)fTrackHits.size(); tck++) {
        if (GetTrackID(tck) == id) continue;

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

    auto order = TRestHitsUtils::BuildSortOrder(count, compareCondition, [this, start](int a, int b) {
        return fData.hits.z[start + a] < fData.hits.z[start + b];
    });

    TRestHitsUtils::ApplyPermutation(order, start, fData.hits.x, fData.hits.y, fData.hits.z, fData.hits.time,
                                     fData.hits.energy, fData.hits.type, fData.sigmaX, fData.sigmaY,
                                     fData.sigmaZ);
}

void TRestTrackEvent::ShuffleTrackHits(Int_t n, int NLoop) {
    const int start = GetTrackStartIndex(n);
    const int count = fData.trackNHits[n];
    if (count < 2) return;

    auto order = TRestHitsUtils::BuildShuffleOrder(count, NLoop);

    TRestHitsUtils::ApplyPermutation(order, start, fData.hits.x, fData.hits.y, fData.hits.z, fData.hits.time,
                                     fData.hits.energy, fData.hits.type, fData.sigmaX, fData.sigmaY,
                                     fData.sigmaZ);
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
    tckX.GetBoundaries(origX, endX);
    tckY.GetBoundaries(origY, endY);

    double originZ = (origX.Z() + origY.Z()) / 2.;
    double endZ = (endX.Z() + endY.Z()) / 2.;

    orig = XYZVector(origX.X(), origY.Y(), originZ);
    end = XYZVector(endX.X(), endY.Y(), endZ);
}

///////////////////////////////////////////////
/// \brief Reduces the hits of a track into `nodes` nodes (TRestHitsAnalysis::KMeansClustering), starting
/// equally spaced along the hits order, and adds them to `output` as a track whose parent is this one.
///
Int_t TRestTrackEvent::KMeansClustering(Int_t trackIndex, Int_t nodes, TRestTrackEvent& output, Int_t maxIt,
                                        Bool_t fixBoundaries) const {
    const TRestHits& hits = fTrackHits[trackIndex];
    const auto points = hits.GetPoints();
    if (nodes < 2 || points.empty()) return -1;

    std::vector<XYZVector> centroids(nodes);
    for (int n = 0; n < nodes; n++) centroids[n] = points[(n * (points.size() - 1)) / (nodes - 1)];
    const auto result =
        TRestHitsAnalysis::KMeansClustering(points, hits.GetEnergies(), centroids, maxIt, fixBoundaries);
    if (result.empty()) return -1;

    const REST_HitType type = hits.GetType(0);
    Int_t newTrackIndex = output.AddTrack(output.GetNumberOfTracks() + 1, GetTrackID(trackIndex));
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (const auto& node : result) {
        // Undefined coordinates back to NaN, as in the hits (GetPoints sets them to 0)
        const XYZVector position(TRestHitsData::IsDefined(type, 0) ? node.position.X() : nan,
                                 TRestHitsData::IsDefined(type, 1) ? node.position.Y() : nan,
                                 TRestHitsData::IsDefined(type, 2) ? node.position.Z() : nan);
        output.AddHitToTrack(position, node.energy, 0, type, node.sigma);
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
}

void TRestTrackEvent::PrintTrackEvent(Bool_t fullInfo) const {
    TRestEvent::PrintEvent();

    std::cout << "Number of tracks : "
              << GetNumberOfTracks("XYZ") + GetNumberOfTracks("X") + GetNumberOfTracks("Y") << std::endl;
    std::cout << "Number of tracks XZ " << GetNumberOfTracks("X") << std::endl;
    std::cout << "Number of tracks YZ " << GetNumberOfTracks("Y") << std::endl;
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

namespace {
/// Marker size of a hit, growing with its energy (as legacy TRestTrackEvent::DrawEvent for top level tracks)
double GetHitMarkerSize(const TRestHits& hits, int n) {
    const double minSize = 0.4, maxSize = 2.;
    if (hits.GetNumberOfHits() == 1) return 2;
    if (hits.GetNumberOfHits() > 100) return 0.8;

    const double maxEnergy = hits.GetMaximumHitEnergy(), meanEnergy = hits.GetMeanHitEnergy();
    if (maxEnergy <= meanEnergy) return maxSize;
    const double slope = maxSize / (maxEnergy - meanEnergy);
    return std::max(0.1, slope * (hits.GetEnergy(n) - meanEnergy) + maxSize - minSize);
}

/// Tracks with less hits are drawn with a line joining them (as legacy)
const size_t kMaxHitsForLines = 100;

}  // namespace

TPad* TRestTrackEvent::DrawEvent(const TString& option) const {
    std::vector<std::string> options;
    for (const auto& opt : TRestTools::Split(option.Data(), ':')) {
        if (opt == "print") {
            PrintEvent();
        } else if (!opt.empty()) {
            options.push_back(opt);
        }
    }
    if (options.empty()) options.push_back("tracks");

    if (GetNumberOfTracks() == 0) {
        std::cout << "Empty event " << std::endl;
        return nullptr;
    }

    TVirtualPad* safePad = gPad;

    delete fPad;
    fPad = new TPad(Form("pad_track_event_%d", GetID()), "REST Track Viewer", 0, 0, 1, 1);
    fPad->Divide(1, options.size());
    fPad->Draw();

    for (size_t i = 0; i < options.size(); i++) {
        // name(rootOption)[pitch]
        const auto [name, histOption, pitch] = ParseDrawOption(options[i]);

        TVirtualPad* optionPad = fPad->cd(i + 1);
        if (name == "tracks") {
            DrawTracks(optionPad);
        } else if (name == "hits") {
            // View over the hits of all the tracks; DrawHistograms does not modify them
            TRestHits(const_cast<TRestHitsData*>(&fData.hits), 0, (int)fData.hits.x.size())
                .DrawHistograms(optionPad, histOption, pitch);
        } else {
            std::cout << "TRestTrackEvent::DrawEvent: unknown option '" << options[i] << "'" << std::endl;
        }
    }

    fPad->Modified();
    if (safePad) safePad->cd();
    return fPad;
}

void TRestTrackEvent::DrawTracks(TVirtualPad* pad) const {
    std::vector<int> tracks(GetNumberOfTracks());
    std::iota(tracks.begin(), tracks.end(), 0);

    std::vector<double> values[3];
    double maxEnergy = 0;
    bool has3D = false;
    for (int t : tracks) {
        const auto& hits = fTrackHits[t];
        for (size_t n = 0; n < hits.GetNumberOfHits(); n++) {
            for (int a = 0; a < 3; a++) {
                if (TRestHitsData::IsDefined(hits.GetType(n), a))
                    values[a].push_back(hits.GetCoordinate(n, a));
            }
            maxEnergy = std::max(maxEnergy, hits.GetEnergy(n));
            if (hits.GetType(n) % REST_HitType::XYZ == 0) has3D = true;
        }
    }

    // Ranges as TRestDetectorHitsEvent (margin around the hits)
    double min[3], max[3];
    int nBins[3];
    for (int a = 0; a < 3; a++) {
        TRestHitsAnalysis::GetBoundaries(values[a], TRestHitsAnalysis::GetMinimumDistance(values[a]), max[a],
                                         min[a], nBins[a]);
    }

    // {u, v} coordinates of each view: the projections with hits defining both or, if there are none
    // (1D hits), each coordinate against the energy (v = -1)
    auto definesBoth = [](int type, int u, int v) {
        return TRestHitsData::IsDefined(type, u) && (v < 0 || TRestHitsData::IsDefined(type, v));
    };
    std::vector<std::pair<int, int>> views;
    for (const auto& plane : TRestHitsData::kPlanes) {
        bool hasHits = false;
        for (int t : tracks) {
            for (size_t n = 0; n < fTrackHits[t].GetNumberOfHits() && !hasHits; n++) {
                hasHits = definesBoth(fTrackHits[t].GetType(n), plane[0], plane[1]);
            }
        }
        if (hasHits) views.emplace_back(plane[0], plane[1]);
    }
    if (views.empty()) {
        for (int a = 0; a < 3; a++) {
            if (!values[a].empty()) views.emplace_back(a, -1);
        }
    }

    const int nPads = views.size() + (has3D ? 1 : 0);
    if (nPads == 0) return;
    pad->Divide(nPads, 1);

    // A colour per track: ROOT colours 2 (red, the first one in legacy) to 9
    std::vector<Color_t> colors;
    for (int t : tracks) colors.push_back(2 + t % 8);

    const std::string title = Form("Event ID %d", GetID());
    for (size_t i = 0; i < views.size(); i++) {
        const auto [u, v] = views[i];
        pad->cd(i + 1);
        SetPadMargins(gPad);

        auto* frame = new TH1F("frame", title.c_str(), 1, min[u], max[u]);
        frame->SetMinimum(v >= 0 ? min[v] : 0);
        frame->SetMaximum(v >= 0 ? max[v] : 1.1 * maxEnergy);
        DrawOwned(frame, "AXIS", TRestHitsData::kAxisTitles[u],
                  v >= 0 ? TRestHitsData::kAxisTitles[v] : "Energy");

        // First the hits of every track, then the track lines over them (as legacy)
        std::vector<TGraph*> lines;
        for (size_t k = 0; k < tracks.size(); k++) {
            const int t = tracks[k];
            const auto& hits = fTrackHits[t];

            std::vector<double> us, vs;
            for (size_t n = 0; n < hits.GetNumberOfHits(); n++) {
                if (!definesBoth(hits.GetType(n), u, v)) continue;
                us.push_back(hits.GetCoordinate(n, u));
                vs.push_back(v >= 0 ? hits.GetCoordinate(n, v) : hits.GetEnergy(n));

                auto* marker = new TMarker(us.back(), vs.back(), 20);
                marker->SetMarkerColor(colors[k]);
                marker->SetMarkerSize(GetHitMarkerSize(hits, n));
                marker->SetBit(TObject::kCanDelete);
                marker->Draw();
            }

            if (us.size() > 1 && us.size() < kMaxHitsForLines)
                lines.push_back(new TGraph(us.size(), us.data(), vs.data()));
        }
        for (auto* line : lines) {
            line->SetLineWidth(2);
            line->SetBit(TObject::kCanDelete);
            line->Draw("L");
        }
    }

    if (!has3D) return;

    pad->cd(nPads);
    auto* frame =
        new TH3F("XYZframe", title.c_str(), 1, min[0], max[0], 1, min[1], max[1], 1, min[2], max[2]);
    DrawOwned(frame, "", TRestHitsData::kAxisTitles[0], TRestHitsData::kAxisTitles[1],
              TRestHitsData::kAxisTitles[2]);

    std::vector<TPolyLine3D*> lines;
    for (size_t k = 0; k < tracks.size(); k++) {
        const int t = tracks[k];
        const auto& hits = fTrackHits[t];

        auto* line = new TPolyLine3D();
        for (size_t n = 0; n < hits.GetNumberOfHits(); n++) {
            if (hits.GetType(n) % REST_HitType::XYZ != 0) continue;
            line->SetNextPoint(hits.GetX(n), hits.GetY(n), hits.GetZ(n));

            auto* marker = new TPolyMarker3D(1, 20);
            marker->SetPoint(0, hits.GetX(n), hits.GetY(n), hits.GetZ(n));
            marker->SetMarkerColor(colors[k]);
            marker->SetMarkerSize(GetHitMarkerSize(hits, n));
            marker->SetBit(TObject::kCanDelete);
            marker->Draw();
        }

        // Size: points set (GetN is the allocated size)
        if (line->Size() > 1 && line->Size() < (int)kMaxHitsForLines) {
            lines.push_back(line);
        } else {
            delete line;
        }
    }
    // Over the hits of every track
    for (auto* line : lines) {
        line->SetLineWidth(2);
        line->SetBit(TObject::kCanDelete);
        line->Draw();
    }
}
