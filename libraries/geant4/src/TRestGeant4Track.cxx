#include "TRestGeant4Track.h"

#include <algorithm>
#include <iostream>

#include "TRestGeant4Event.h"
#include "TRestGeant4Metadata.h"

using namespace std;

TRestGeant4Track::TRestGeant4Track() = default;

TRestGeant4Track::~TRestGeant4Track() = default;

/// \brief Resolves a Geant4 process name into its numeric process identifier.
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

/// \brief Resolves a Geant4 process identifier into its process name.
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

/// \brief Returns a display color associated with the track particle species.
EColor TRestGeant4Track::GetParticleColor() const {
    EColor color = kGray;

    // COMPATIBILITY: Implicit conversion from std::string returned by GetParticleName() works natively
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

/// \brief Prints a detailed textual summary of the track and its hits.
void TRestGeant4Track::PrintTrack(size_t maxHits) const {
    cout << " * TrackID: " << fTrackID << " - Particle: " << fParticleName << " - ParentID: " << fParentID;
    if (GetParentTrack() != nullptr) {
        cout << " - Parent particle: " << GetParentTrack()->GetParticleName();
    }
    cout << " - Created by '" << fCreatorProcess << "' in volume '" << GetInitialVolume()
         << "' with initial KE of " << (fInitialKineticEnergy) << " - Initial position " << (fInitialPosition)
         << " mm at time " << (fGlobalTimestamp) << " - Time length of " << (fTimeLength)
         << " and spatial length of " << (fLength) << endl;

    cout << "   Initial position " << (fInitialPosition) << " mm at time " << (fGlobalTimestamp)
         << " - Time offset " << (fTimeOffset) << " - Time length of " << (fTimeLength)
         << " and spatial length of " << (fLength) << endl;

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
        cout << "      - Hit " << i << " - Energy: " << (fHits.GetEnergy(i)) << " - Process: " << processName
             << " - Volume: " << volumeName
             << " - Position: " << (ROOT::Math::XYZVector(fHits.GetX(i), fHits.GetY(i), fHits.GetZ(i)))
             << " mm - Time: " << (fHits.GetTime(i)) << " - KE: " << (fHits.GetKineticEnergy(i)) << endl;
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
         << "' with initial KE of " << (fInitialKineticEnergy) << " - Initial position " << (fInitialPosition)
         << " mm at time " << (fGlobalTimestamp) << " - Time length of " << (fTimeLength)
         << " and spatial length of " << (fLength) << endl;

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
        cout << "      - Hit " << i << " - Energy: " << (fHits.GetEnergy(i)) << " - Process: " << processName
             << " - Volume: " << volumeName
             << " - Position: " << (ROOT::Math::XYZVector(fHits.GetX(i), fHits.GetY(i), fHits.GetZ(i)))
             << " mm - Time: " << (fHits.GetTime(i)) << " - KE: " << (fHits.GetKineticEnergy(i)) << endl;
    }
}

/// \brief Checks whether any hit in the track matches a process and optional volume filter.
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

/// \brief Returns the Geant4 metadata associated with the owning event.
const TRestGeant4Metadata* TRestGeant4Track::GetGeant4Metadata() const {
    if (GetEvent() == nullptr) {
        return nullptr;
    }
    return GetEvent()->GetGeant4Metadata();
}
// FIXED: Securely resolve parent references through the active event view framework layer
TRestGeant4Track* TRestGeant4Track::GetParentTrack() const {
    if (GetEvent() == nullptr || fParentID == 0) {
        return nullptr;
    }
    
    // GetTrackByID returns a TRestGeant4TrackView. Since we migrated fTracks to pointers,
    // we fetch the raw pointer matching the parent's index from the fast indexing map.
    auto& event = const_cast<TRestGeant4Event&>(*GetEvent());
    auto it = event.fTrackIDToTrackIndex.find(fParentID);
    if (it != event.fTrackIDToTrackIndex.end()) {
        size_t idx = it->second;
        if (idx < event.GetTracks().size()) {
            return event.GetTracks()[idx]; // Returns the exact parent TRestGeant4Track* pointer
        }
    }
    return nullptr;
}

// FIXED: Resolve secondary track pointers using the event tracking matrix mappings
vector<const TRestGeant4Track*> TRestGeant4Track::GetSecondaryTracks() const {
    vector<const TRestGeant4Track*> secondaryTracks;
    if (GetEvent() == nullptr) return secondaryTracks;

    auto& event = const_cast<TRestGeant4Event&>(*GetEvent());
    const auto& allTracks = event.GetTracks();

    // Iterate through registered secondary IDs and resolve their active pointers
    for (int secID : fSecondaryTrackIDs) {
        auto it = event.fTrackIDToTrackIndex.find(secID);
        if (it != event.fTrackIDToTrackIndex.end()) {
            size_t idx = it->second;
            if (idx < allTracks.size() && allTracks[idx] != nullptr) {
                secondaryTracks.push_back(allTracks[idx]);
            }
        }
    }
    return secondaryTracks;
}

std::string TRestGeant4Track::GetInitialVolume() const {
    const auto metadata = GetGeant4Metadata();
    if (metadata == nullptr || fHits.GetNumberOfHits() == 0) {
        return "";
    }
    // FIXED: TRestGeant4Hits utilizes GetHitVolume() to lookup indices safely
    return metadata->GetGeant4GeometryInfo().GetVolumeFromID(fHits.GetHitVolume(0));
}

std::string TRestGeant4Track::GetFinalVolume() const {
    const auto metadata = GetGeant4Metadata();
    if (metadata == nullptr || fHits.GetNumberOfHits() == 0) {
        return "";
    }
    // FIXED: Swapped out legacy GetVolumeId for the real GetHitVolume API method call
    size_t lastIdx = fHits.GetNumberOfHits() - 1;
    return metadata->GetGeant4GeometryInfo().GetVolumeFromID(fHits.GetHitVolume(lastIdx));
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
    if (metadata == nullptr || fHits.GetNumberOfHits() == 0) {
        return "";
    }
    // FIXED: Swapped out legacy hits.GetProcess for the real GetHitProcess API method call
    size_t lastIdx = fHits.GetNumberOfHits() - 1;
    return metadata->GetGeant4PhysicsInfo().GetProcessName(fHits.GetHitProcess(lastIdx));
}

