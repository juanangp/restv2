#include "TRestGeant4Track.h"

#include <algorithm>
#include <iostream>

#include <TMath.h>

#include "TRestGeant4Event.h"
#include "TRestGeant4Metadata.h"

using namespace std;

TRestGeant4Track::TRestGeant4Track(TRestGeant4Event* event, std::size_t index)
    : fEvent(event), fTrackIndex(index) {}

bool TRestGeant4Track::IsValid() const {
    if (fEvent == nullptr) return false;

    const auto& data = fEvent->fEventData;
    const std::size_t n = data.trackIDs.size();

    return fTrackIndex < n &&
           data.parentIDs.size() == n &&
           data.trackParticleNames.size() == n &&
           data.trackCreatorProcesses.size() == n &&
           data.trackDepositedEnergy.size() == n &&
           data.trackInitialEnergies.size() == n &&
           data.trackStartIndices.size() == n &&
           data.trackNHits.size() == n &&
           data.trackGlobalTimestamps.size() == n &&
           data.trackTimeOffsets.size() == n &&
           data.trackTimeLengths.size() == n &&
           data.trackLengths.size() == n &&
           data.trackWeights.size() == n &&
           data.trackInitialPositions.size() == n &&
           data.trackSecondariesIndices.size() == n &&
           data.trackSecondariesOffsets.size() == n;
}

std::size_t TRestGeant4Track::GetHitStart() const {
    if (!IsValid()) return 0;
    return static_cast<std::size_t>(fEvent->fEventData.trackStartIndices.at(fTrackIndex));
}

std::size_t TRestGeant4Track::GetHitCount() const {
    if (!IsValid()) return 0;
    return static_cast<std::size_t>(fEvent->fEventData.trackNHits.at(fTrackIndex));
}

TRestHits TRestGeant4Track::GetHits() const {
    if (!IsValid()) return TRestHits();

    const auto& data = fEvent->fEventData;
    return TRestHits(
        const_cast<TRestHitsData*>(&data.hitsStorage),
        static_cast<int>(GetHitStart()),
        static_cast<int>(GetHitCount()));
}

Int_t TRestGeant4Track::GetTrackID() const {
    return fEvent->fEventData.trackIDs.at(fTrackIndex);
}

Int_t TRestGeant4Track::GetParentID() const {
    return fEvent->fEventData.parentIDs.at(fTrackIndex);
}

const std::string& TRestGeant4Track::GetParticleName() const {
    return fEvent->fEventData.trackParticleNames.at(fTrackIndex);
}

const std::string& TRestGeant4Track::GetCreatorProcess() const {
    return fEvent->fEventData.trackCreatorProcesses.at(fTrackIndex);
}

Double_t TRestGeant4Track::GetGlobalTime() const {
    return fEvent->fEventData.trackGlobalTimestamps.at(fTrackIndex);
}

Double_t TRestGeant4Track::GetTimeOffset() const {
    return fEvent->fEventData.trackTimeOffsets.at(fTrackIndex);
}

Double_t TRestGeant4Track::GetTimeLength() const {
    return fEvent->fEventData.trackTimeLengths.at(fTrackIndex);
}

Double_t TRestGeant4Track::GetInitialKineticEnergy() const {
    return fEvent->fEventData.trackInitialEnergies.at(fTrackIndex);
}

ROOT::Math::XYZVector TRestGeant4Track::GetInitialPosition() const {
    return fEvent->fEventData.trackInitialPositions.at(fTrackIndex);
}

Double_t TRestGeant4Track::GetWeight() const {
    return fEvent->fEventData.trackWeights.at(fTrackIndex);
}

Double_t TRestGeant4Track::GetTotalEnergy() const {
    return GetHits().GetTotalEnergy();
}

Double_t TRestGeant4Track::GetDepositedEnergy() const {
    return fEvent->fEventData.trackDepositedEnergy.at(fTrackIndex);
}

Double_t TRestGeant4Track::GetLength() const {
    return fEvent->fEventData.trackLengths.at(fTrackIndex);
}

const TRestGeant4Metadata* TRestGeant4Track::GetGeant4Metadata() const {
    if (fEvent == nullptr) return nullptr;
    return fEvent->GetGeant4Metadata();
}

void TRestGeant4Track::SetEvent(TRestGeant4Event* event) {
    fEvent = event;
}

Int_t TRestGeant4Track::GetHitProcess(size_t hit) const {
    return fEvent->fEventData.hitProcessID.at(GetHitStart() + hit);
}

Int_t TRestGeant4Track::GetHitVolumeID(size_t hit) const {
    return fEvent->fEventData.hitVolumeID.at(GetHitStart() + hit);
}


std::string TRestGeant4Track::GetHitVolumeName(size_t hit) const {
  const auto* metadata = GetGeant4Metadata();
  std::string volumeName = metadata->GetGeant4GeometryInfo().GetVolumeFromID(GetHitVolumeID(hit));
  return volumeName;
}

Double_t TRestGeant4Track::GetHitKineticEnergy(size_t hit) const {
    return fEvent->fEventData.hitKineticEnergy.at(GetHitStart() + hit);
}

ROOT::Math::XYZVector TRestGeant4Track::GetHitMomentumDirection(size_t hit) const {
    return fEvent->fEventData.hitMomentumDirection.at(GetHitStart() + hit);
}

ROOT::Math::XYZVector TRestGeant4Track::GetHitPosition(size_t hit) const {
    double x = fEvent->fEventData.hitsStorage.x.at(GetHitStart() + hit);
    double y = fEvent->fEventData.hitsStorage.y.at(GetHitStart() + hit);
    double z = fEvent->fEventData.hitsStorage.z.at(GetHitStart() + hit);
    
    return ROOT::Math::XYZVector(x,y,z);
}

std::string TRestGeant4Track::GetHitHadronicTargetIsotopeName(size_t hit) const {
    return fEvent->fEventData.hitHadronicTargetIsotopeName.at(GetHitStart() + hit);
}

Int_t TRestGeant4Track::GetHitHadronicTargetIsotopeA(size_t hit) const {
    return fEvent->fEventData.hitHadronicTargetIsotopeA.at(GetHitStart() + hit);
}

Int_t TRestGeant4Track::GetHitHadronicTargetIsotopeZ(size_t hit) const {
    return fEvent->fEventData.hitHadronicTargetIsotopeZ.at(GetHitStart() + hit);
}

Int_t TRestGeant4Track::GetProcessID(const std::string& processName) const {
    const auto* metadata = GetGeant4Metadata();
    if (metadata != nullptr) {
        const auto processID = metadata->GetGeant4PhysicsInfo().GetProcessID(processName);
        if (processID != 0) return processID;
    }

    cout << "WARNING : The process " << processName << " was not found" << endl;
    return -1;
}

std::string TRestGeant4Track::GetProcessName(Int_t processID) const {
    const auto* metadata = GetGeant4Metadata();
    if (metadata != nullptr) {
        const auto& processName = metadata->GetGeant4PhysicsInfo().GetProcessName(processID);
        if (!processName.empty()) return processName;
    }

    cout << "WARNING : The process " << processID << " was not found" << endl;
    return "";
}

EColor TRestGeant4Track::GetParticleColor() const {
    EColor color = kGray;

    if (GetParticleName() == "e-") color = kRed;
    else if (GetParticleName() == "e+") color = kBlue;
    else if (GetParticleName() == "alpha") color = kOrange;
    else if (GetParticleName() == "mu-") color = kViolet;
    else if (GetParticleName() == "gamma") color = kGreen;
    else
        cout << "TRestGeant4Track::GetParticleColor. Particle NOT found! Returning gray color."
             << endl;

    return color;
}

size_t TRestGeant4Track::GetNumberOfHits(Int_t volID) const {
    if (!IsValid()) return 0;
    if (volID < 0) return GetHitCount();

    size_t numberOfHits = 0;
    for (size_t i = 0; i < GetHitCount(); ++i) {
        if (GetHitVolumeID(i) == volID) ++numberOfHits;
    }
    return numberOfHits;
}

size_t TRestGeant4Track::GetNumberOfPhysicalHits(Int_t volID) const {
    if (!IsValid()) return 0;

    const auto hits = GetHits();
    size_t numberOfHits = 0;
    for (size_t i = 0; i < GetHitCount(); ++i) {
        if (volID >= 0 && GetHitVolumeID(i) != volID) continue;
        if (hits.GetEnergy(static_cast<int>(i)) <= 0) continue;
        ++numberOfHits;
    }
    return numberOfHits;
}

Double_t TRestGeant4Track::GetEnergyInVolume(Int_t volID) const {
    if (!IsValid()) return 0;

    const auto hits = GetHits();
    Double_t energy = 0;
    for (size_t i = 0; i < GetHitCount(); ++i) {
        if (GetHitVolumeID(i) == volID)
            energy += hits.GetEnergy(static_cast<int>(i));
    }
    return energy;
}

ROOT::Math::XYZVector TRestGeant4Track::GetMeanPositionInVolume(Int_t volID) const {
    if (!IsValid()) return {};

    const auto hits = GetHits();
    ROOT::Math::XYZVector position;
    Double_t totalEnergy = 0;

    for (size_t i = 0; i < GetHitCount(); ++i) {
        if (GetHitVolumeID(i) != volID) continue;

        const Double_t energy = hits.GetEnergy(static_cast<int>(i));
        position += energy * ROOT::Math::XYZVector(
            hits.GetX(static_cast<int>(i)),
            hits.GetY(static_cast<int>(i)),
            hits.GetZ(static_cast<int>(i)));
        totalEnergy += energy;
    }

    if (totalEnergy == 0) {
        const double nan = TMath::QuietNaN();
        return {nan, nan, nan};
    }
    return position / totalEnergy;
}

ROOT::Math::XYZVector TRestGeant4Track::GetFirstPositionInVolume(Int_t volID) const {
    if (!IsValid()) return {};

    const auto hits = GetHits();
    for (size_t i = 0; i < GetHitCount(); ++i) {
        if (GetHitVolumeID(i) != volID) continue;
        return {hits.GetX(static_cast<int>(i)), hits.GetY(static_cast<int>(i)),
                hits.GetZ(static_cast<int>(i))};
    }
    const double nan = TMath::QuietNaN();
    return {nan, nan, nan};
}

ROOT::Math::XYZVector TRestGeant4Track::GetLastPositionInVolume(Int_t volID) const {
    if (!IsValid()) return {};

    const auto hits = GetHits();
    for (size_t i = GetHitCount(); i > 0; --i) {
        const size_t hit = i - 1;
        if (GetHitVolumeID(hit) != volID) continue;
        return {hits.GetX(static_cast<int>(hit)), hits.GetY(static_cast<int>(hit)),
                hits.GetZ(static_cast<int>(hit))};
    }
    const double nan = TMath::QuietNaN();
    return {nan, nan, nan};
}

std::vector<Int_t> TRestGeant4Track::GetSecondaryTrackIDs() const {
    const auto& data = fEvent->fEventData;
    const int start = data.trackSecondariesIndices.at(fTrackIndex);
    const int offset = data.trackSecondariesOffsets.at(fTrackIndex);

    std::vector<Int_t> secondaries;
    if (offset <= 0) return secondaries;

    secondaries.reserve(static_cast<std::size_t>(offset));
    for (int i = 0; i < offset; ++i) {
        const std::size_t pos = static_cast<std::size_t>(start + i);
        if (pos < data.trackSecondariesIDs.size()) secondaries.push_back(data.trackSecondariesIDs[pos]);
    }
    return secondaries;
}

std::optional<TRestGeant4Track> TRestGeant4Track::GetParentTrack() const {
    if (fEvent == nullptr || GetParentID() == 0) return std::nullopt;
    if (fEvent->fTrackIDToTrackIndex.size() != fEvent->fEventData.trackIDs.size())
        fEvent->RebuildTrackIndex();

    const auto it = fEvent->fTrackIDToTrackIndex.find(GetParentID());
    if (it == fEvent->fTrackIDToTrackIndex.end()) return std::nullopt;
    return TRestGeant4Track(fEvent, static_cast<std::size_t>(it->second));
}

std::vector<TRestGeant4Track> TRestGeant4Track::GetSecondaryTracks() const {
    std::vector<TRestGeant4Track> secondaryTracks;
    if (fEvent == nullptr) return secondaryTracks;

    if (fEvent->fTrackIDToTrackIndex.size() != fEvent->fEventData.trackIDs.size())
        fEvent->RebuildTrackIndex();

    for (const int secID : GetSecondaryTrackIDs()) {
        const auto it = fEvent->fTrackIDToTrackIndex.find(secID);
        if (it == fEvent->fTrackIDToTrackIndex.end()) continue;
        secondaryTracks.emplace_back(fEvent, static_cast<std::size_t>(it->second));
    }
    return secondaryTracks;
}

std::string TRestGeant4Track::GetInitialVolume() const {
    const auto* metadata = GetGeant4Metadata();
    if (metadata == nullptr || GetHitCount() == 0) return "";
    return metadata->GetGeant4GeometryInfo().GetVolumeFromID(GetHitVolumeID(0));
}

std::string TRestGeant4Track::GetFinalVolume() const {
    const auto* metadata = GetGeant4Metadata();
    if (metadata == nullptr || GetHitCount() == 0) return "";
    return metadata->GetGeant4GeometryInfo().GetVolumeFromID(GetHitVolumeID(GetHitCount() - 1));
}

Double_t TRestGeant4Track::GetEnergyInVolume(const std::string& volumeName, bool children) const {
    const auto* metadata = GetGeant4Metadata();
    if (metadata == nullptr) return 0;

    const auto volumeId = metadata->GetGeant4GeometryInfo().GetIDFromVolume(volumeName);
    if (!children) return GetEnergyInVolume(volumeId);

    Double_t energy = 0;
    std::vector<TRestGeant4Track> tracks{*this};

    while (!tracks.empty()) {
        TRestGeant4Track track = tracks.back();
        tracks.pop_back();
        energy += track.GetEnergyInVolume(volumeId);
        for (const auto& secondaryTrack : track.GetSecondaryTracks())
            tracks.push_back(secondaryTrack);
    }
    return energy;
}

std::string TRestGeant4Track::GetLastProcessName() const {
    const auto* metadata = GetGeant4Metadata();
    if (metadata == nullptr || GetHitCount() == 0) return "";
    return metadata->GetGeant4PhysicsInfo().GetProcessName(GetHitProcess(GetHitCount() - 1));
}

Bool_t TRestGeant4Track::ContainsProcessInVolume(Int_t processID, Int_t volumeID) const {
    for (size_t i = 0; i < GetHitCount(); ++i) {
        if (GetHitProcess(i) != processID) continue;
        if (volumeID < 0 || GetHitVolumeID(i) == volumeID) return true;
    }
    return false;
}

double TRestGeant4Track::GetHitEnergy(size_t hit) const {
  return fEvent->fEventData.hitsStorage.energy.at(GetHitStart() + hit);
}


Bool_t TRestGeant4Track::GetHadronicOk() const {
    return !fEvent->fEventData.hitHadronicTargetIsotopeName.empty();
}

void TRestGeant4Track::SetHitEnergy(
    size_t hit,
    Double_t energy) {

    if (!IsValid()) return;

    const auto absoluteHit =
        GetHitStart() + hit;

    fEvent->fEventData.hitsStorage.energy.at(absoluteHit) =
        energy;
}


Bool_t TRestGeant4Track::ContainsProcessInVolume(const std::string& processName,
                                                  Int_t volumeID) const {
    const auto* metadata = GetGeant4Metadata();
    if (metadata == nullptr) return false;

    const auto processID = metadata->GetGeant4PhysicsInfo().GetProcessID(processName);
    return ContainsProcessInVolume(processID, volumeID);
}

void TRestGeant4Track::PrintTrack(int maxHits) const {
    std::cout << " * TrackID: " << GetTrackID()
              << " - Particle: " << GetParticleName()
              << " - ParentID: " << GetParentID();

    const auto parent = GetParentTrack();
    if (parent) {
        std::cout << " - Parent particle: '" << parent->GetParticleName() << "'";
    } else {
        std::cout << " - Parent particle: 'PrimaryGenerator'";
    }

    std::cout << " - Created by '" << GetCreatorProcess()
              << "' in volume '" << GetInitialVolume()
              << "' with initial KE of " << REST_Units::FormatAs(GetInitialEnergy(), REST_Units::Energy)
              << " - Initial position " << REST_Units::FormatAs(GetInitialPosition(), REST_Units::Length)
              << " - Time length of " << REST_Units::FormatAs(GetTimeLength(), REST_Units::Time)
              << " and spatial length of " << REST_Units::FormatAs(GetLength(), REST_Units::Length) 
              << std::endl;

    std::cout << "   Initial position " << REST_Units::FormatAs(GetInitialPosition(), REST_Units::Length)
              << " at time " << REST_Units::FormatAs(GetGlobalTime(), REST_Units::Time)
              << " - Time offset " << REST_Units::FormatAs(GetTimeOffset(), REST_Units::Time)
              << " - Time length of " << REST_Units::FormatAs(GetTimeLength(), REST_Units::Time)
              << " and spatial length of " << REST_Units::FormatAs(GetLength(), REST_Units::Length) 
              << std::endl;

    const auto hits = GetHits();
    const int nHits = static_cast<int>(hits.GetNumberOfHits());
    if (maxHits < 0) maxHits = nHits;
    const int nToPrint = std::min(nHits, maxHits);

    std::cout << "- Number of hits to print: " << nToPrint << "/" << nHits << std::endl;

    for (int i = 0; i < nToPrint; ++i) {
        ROOT::Math::XYZVector hitPos(hits.GetX(i), hits.GetY(i), hits.GetZ(i));

        std::cout << "      - Hit " << i 
                  << " - Energy: " << REST_Units::FormatAs(hits.GetEnergy(i), REST_Units::Energy)
                  << " - Process: " << GetProcessName(GetHitProcess(i))
                  << " - Volume: " << GetHitVolumeName(i) 
                  << " - Position: " << REST_Units::FormatAs(hitPos, REST_Units::Length)
                  << " - Time: " << REST_Units::FormatAs(hits.GetTime(i), REST_Units::Time)
                  << std::endl;
    }
}

void TRestGeant4Track::PrintTrackFilterVolumes(const std::set<std::string>& volumeNames) const {
    const auto* metadata = GetGeant4Metadata();
    if (metadata == nullptr) return;

    bool printTrack = false;
    for (size_t i = 0; i < GetHitCount(); ++i) {
        std::string volumeName = metadata->GetGeant4GeometryInfo().GetVolumeFromID(GetHitVolumeID(i));
        if (volumeNames.find(volumeName) != volumeNames.end()) {
            printTrack = true;
            break;
        }
    }

    if (printTrack) PrintTrack();
}
