#include "TRestGeant4Track.h"
#include "TRestGeant4Event.h"
#include "TRestGeant4Metadata.h"
#include <iostream>
#include <algorithm>

using namespace std;

TRestGeant4Track::TRestGeant4Track() = default;

TRestGeant4Track::~TRestGeant4Track() = default;

Int_t TRestGeant4Track::GetProcessID(const std::string& processName) const {
    const TRestGeant4Metadata* metadata = GetGeant4Metadata();
    if (metadata != nullptr) {
        const auto processID = metadata->GetGeant4PhysicsInfo().GetProcessID(processName);
        if (processID != 0) {
            return processID;
        }
    }

    cout << "WARNING : The process " << processName << " was not found" << endl;
    return -1;
}

std::string TRestGeant4Track::GetProcessName(Int_t processID) const {
    const TRestGeant4Metadata* metadata = GetGeant4Metadata();
    if (metadata != nullptr) {
        const auto& processName = metadata->GetGeant4PhysicsInfo().GetProcessName(processID);
        if (!processName.empty()) {
            return processName;
        }
    }

    cout << "WARNING : The process " << processID << " was not found" << endl;
    return "";
}

EColor TRestGeant4Track::GetParticleColor() const {
    EColor color = kGray;

    if (GetParticleName() == "e-")
        color = kRed;
    else if (GetParticleName() == "e+")
        color = kBlue;
    else if (GetParticleName() == "alpha")
        color = kOrange;
    else if (GetParticleName() == "mu-")
        color = kViolet;
    else if (GetParticleName() == "gamma")
        color = kGreen;
    else
        cout << "TRestGeant4Track::GetParticleColor. Particle NOT found! Returning gray color." << endl;

    return color;
}

size_t TRestGeant4Track::GetNumberOfHits(Int_t volID) const {
    size_t numberOfHits = 0;
    for (unsigned int n = 0; n < fHits.GetNumberOfHits(); n++) {
        if (volID != -1 && fHits.GetVolumeId(n) != volID) {
            continue;
        }
        numberOfHits++;
    }
    return numberOfHits;
}

size_t TRestGeant4Track::GetNumberOfPhysicalHits(Int_t volID) const {
    size_t numberOfHits = 0;
    for (unsigned int n = 0; n < fHits.GetNumberOfHits(); n++) {
        if (volID != -1 && fHits.GetVolumeId(n) != volID) {
            continue;
        }
        if (fHits.GetEnergy(n) <= 0) {
            continue;
        }
        numberOfHits++;
    }
    return numberOfHits;
}

void TRestGeant4Track::PrintTrack(size_t maxHits) const {
    cout << " * TrackID: " << fTrackID << " - Particle: " << fParticleName << " - ParentID: " << fParentID;
    if (GetParentTrack() != nullptr) {
        cout << " - Parent particle: " << GetParentTrack()->GetParticleName();
    }
    cout << " - Created by '" << fCreatorProcess << "' in volume '" << GetInitialVolume()
         << "' with initial KE of " << (fInitialKineticEnergy) << " - Initial position "
         << (fInitialPosition) << " mm at time " << (fGlobalTimestamp)
         << " - Time length of " << (fTimeLength) << " and spatial length of "
         << (fLength) << endl;

    cout << "   Initial position " << (fInitialPosition) << " mm at time "
         << (fGlobalTimestamp) << " - Time offset " << (fTimeOffset)
         << " - Time length of " << (fTimeLength) << " and spatial length of "
         << (fLength) << endl;

    size_t nHits = GetNumberOfHits();
    if (maxHits > 0 && maxHits < nHits) {
        nHits = min(maxHits, nHits);
        cout << "Printing only the first " << nHits << " hits of the track" << endl;
    }

    const TRestGeant4Metadata* metadata = GetGeant4Metadata();
    for (unsigned int i = 0; i < nHits; i++) {
        std::string processName = GetProcessName(fHits.GetHitProcess(i));
        if (processName.empty()) {
            processName = std::to_string(fHits.GetHitProcess(i));
        }

        std::string volumeName = "";
        if (metadata != nullptr) {
            volumeName = metadata->GetGeant4GeometryInfo().GetVolumeFromID(fHits.GetHitVolume(i));
        }
        if (volumeName.empty()) {
            volumeName = std::to_string(fHits.GetHitVolume(i));
        }
        cout << "      - Hit " << i << " - Energy: " << (fHits.GetEnergy(i))
             << " - Process: " << processName << " - Volume: " << volumeName
             << " - Position: " << (ROOT::Math::XYZVector(fHits.GetX(i), fHits.GetY(i), fHits.GetZ(i)))
             << " mm - Time: " << (fHits.GetTime(i))
             << " - KE: " << (fHits.GetKineticEnergy(i)) << endl;
    }
}

void TRestGeant4Track::PrintTrackFilterVolumes(const std::set<std::string>& volumeNames) const {
    const TRestGeant4Metadata* metadata = GetGeant4Metadata();
    if (metadata == nullptr) {
        return;
    }
    bool skip = true;
    for (unsigned int i = 0; i < GetNumberOfHits(); i++) {
        std::string volumeName = metadata->GetGeant4GeometryInfo().GetVolumeFromID(fHits.GetHitVolume(i));
        if (volumeName.empty()) {
            volumeName = std::to_string(fHits.GetHitVolume(i));
        }
        if (volumeNames.find(volumeName) != volumeNames.end()) {
            skip = false;
            break;
        }
    }

    if (skip) {
        return;
    }

    cout << " * TrackID: " << fTrackID << " - Particle: " << fParticleName << " - ParentID: " << fParentID;
    if (GetParentTrack() != nullptr) {
        cout << " - Parent particle: " << GetParentTrack()->GetParticleName();
    }
    cout << " - Created by '" << fCreatorProcess << "' in volume '" << GetInitialVolume()
         << "' with initial KE of " << (fInitialKineticEnergy) << " - Initial position "
         << (fInitialPosition) << " mm at time " << (fGlobalTimestamp)
         << " - Time length of " << (fTimeLength) << " and spatial length of "
         << (fLength) << endl;

    size_t nHits = GetNumberOfHits();
    for (unsigned int i = 0; i < nHits; i++) {
        std::string processName = GetProcessName(fHits.GetHitProcess(i));
        if (processName.empty()) {
            processName = std::to_string(fHits.GetHitProcess(i));
        }

        std::string volumeName = metadata->GetGeant4GeometryInfo().GetVolumeFromID(fHits.GetHitVolume(i));
        if (volumeName.empty()) {
            volumeName = std::to_string(fHits.GetHitVolume(i));
        }
        if (volumeNames.find(volumeName) == volumeNames.end()) {
            continue;
        }
        cout << "      - Hit " << i << " - Energy: " << (fHits.GetEnergy(i))
             << " - Process: " << processName << " - Volume: " << volumeName
             << " - Position: " << (ROOT::Math::XYZVector(fHits.GetX(i), fHits.GetY(i), fHits.GetZ(i)))
             << " mm - Time: " << (fHits.GetTime(i))
             << " - KE: " << (fHits.GetKineticEnergy(i)) << endl;
    }
}

Bool_t TRestGeant4Track::ContainsProcessInVolume(Int_t processID, Int_t volumeID) const {
    for (unsigned int i = 0; i < GetNumberOfHits(); i++) {
        if (fHits.GetHitProcess(i) != processID) continue;
        if (volumeID == -1 || fHits.GetVolumeId(i) == volumeID) return true;
    }
    return false;
}

Bool_t TRestGeant4Track::ContainsProcessInVolume(const std::string& processName, Int_t volumeID) const {
    const TRestGeant4Metadata* metadata = GetGeant4Metadata();
    if (metadata == nullptr) {
        return false;
    }
    const auto processID = metadata->GetGeant4PhysicsInfo().GetProcessID(processName);
    for (unsigned int i = 0; i < GetNumberOfHits(); i++) {
        if (fHits.GetHitProcess(i) != processID) continue;
        if (volumeID == -1 || fHits.GetVolumeId(i) == volumeID) return true;
    }
    return false;
}

const TRestGeant4Metadata* TRestGeant4Track::GetGeant4Metadata() const {
    if (GetEvent() == nullptr) {
        return nullptr;
    }
    return GetEvent()->GetGeant4Metadata();
}

TRestGeant4Track* TRestGeant4Track::GetParentTrack() const {
    // TODO: Migrate to use TRestGeant4TrackView after architecture unification
    if (fEvent == nullptr) {
        return nullptr;
    }
    return nullptr;  // GetTrackByID now returns TRestGeant4TrackView, not TRestGeant4Track*
}

vector<const TRestGeant4Track*> TRestGeant4Track::GetSecondaryTracks() const {
    // TODO: Migrate to use TRestGeant4TrackView after architecture unification
    vector<const TRestGeant4Track*> secondaryTracks = {};
    // GetTrackByID now returns TRestGeant4TrackView, not TRestGeant4Track*
    return secondaryTracks;
}

std::string TRestGeant4Track::GetInitialVolume() const {
    const auto metadata = GetGeant4Metadata();
    if (metadata == nullptr) {
        return "";
    }
    const auto& hits = GetHits();
    return GetGeant4Metadata()->GetGeant4GeometryInfo().GetVolumeFromID(hits.GetVolumeId(0));
}

std::string TRestGeant4Track::GetFinalVolume() const {
    const auto metadata = GetGeant4Metadata();
    if (metadata == nullptr) {
        return "";
    }
    const auto& hits = GetHits();
    return GetGeant4Metadata()->GetGeant4GeometryInfo().GetVolumeFromID(
        hits.GetVolumeId(hits.GetNumberOfHits() - 1));
}

Double_t TRestGeant4Track::GetEnergyInVolume(const std::string& volumeName, bool children) const {
    const auto metadata = GetGeant4Metadata();
    if (metadata == nullptr) {
        return 0;
    }

    const auto volumeId = metadata->GetGeant4GeometryInfo().GetIDFromVolume(volumeName);

    if (!children) {
        return GetEnergyInVolume(volumeId);
    }

    Double_t energy = 0;
    vector<const TRestGeant4Track*> tracks = {this};
    while (!tracks.empty()) {
        const TRestGeant4Track* track = tracks.back();
        tracks.pop_back();
        if (track == nullptr) {
            continue;
        }
        energy += track->GetEnergyInVolume(volumeId);
        for (const TRestGeant4Track* secondaryTrack : track->GetSecondaryTracks()) {
            tracks.push_back(secondaryTrack);
        }
    }
    return energy;
}

std::string TRestGeant4Track::GetLastProcessName() const {
    const auto metadata = GetGeant4Metadata();
    if (metadata == nullptr) {
        return "";
    }

    const auto& hits = GetHits();
    return GetGeant4Metadata()->GetGeant4PhysicsInfo().GetProcessName(
        hits.GetProcess(hits.GetNumberOfHits() - 1));
}

