#include "TRestGeant4Event.h"
#include "TRestRun.h"
#include "TRestGeant4Track.h"

#include <iostream>
#include <memory>

namespace {
const bool kRegistered = []() {
    EventRegistry::Instance().Register(
        "TRestGeant4Event", []() { return std::make_unique<TRestGeant4Event>(); });
    return true;
}();
}  // namespace

const TRestGeant4Metadata* TRestGeant4Event::GetGeant4Metadata() const {
    if (fMetadata) return fMetadata;

    if (fRestRun) {
        TRestMetadata* baseMeta = fRestRun->GetMetadataClass("TRestGeant4Metadata");
        if (baseMeta) {
            auto* nonConstThis = const_cast<TRestGeant4Event*>(this);
            nonConstThis->fMetadata =
                dynamic_cast<const TRestGeant4Metadata*>(baseMeta);
        }
    }

    return fMetadata;
}

void TRestGeant4Event::RebuildTrackIndex() const {
    fTrackIDToTrackIndex.clear();

    for (std::size_t i = 0; i < fEventData.trackIDs.size(); ++i) {
        fTrackIDToTrackIndex[fEventData.trackIDs[i]] = static_cast<int>(i);
    }
}

void TRestGeant4Event::RebuildVolumeIndex() {
    fVolumeIndexMap.clear();

    const std::size_t n =
        std::min(fEventData.volumeStoredNames.size(),
                 fEventData.volumeDepositedEnergy.size());

    for (std::size_t i = 0; i < n; ++i) {
        fVolumeIndexMap[fEventData.volumeStoredNames[i]] = i;
    }
}

void TRestGeant4Event::RebuildCrossIndex() {
    fCrossIndexMap.clear();

    const std::size_t n = std::min(
        {fEventData.crossVolumeNames.size(),
         fEventData.crossParticleNames.size(),
         fEventData.crossProcessNames.size(),
         fEventData.crossDepositedEnergies.size()});

    for (std::size_t i = 0; i < n; ++i) {
        fCrossIndexMap[std::make_tuple(fEventData.crossVolumeNames[i],
                                       fEventData.crossParticleNames[i],
                                       fEventData.crossProcessNames[i])] = i;
    }
}

void TRestGeant4Event::CreateBranches(TTree* tree) {
    TRestEvent::CreateBranches(tree);

    tree->Branch("fSubEventEnergy", &fEventData.subEventEnergy);
    tree->Branch("fTotalDepositedEnergy", &fEventData.totalDepositedEnergy);
    tree->Branch("fSensitiveVolumeEnergy", &fEventData.sensitiveVolumeEnergy);
    tree->Branch("fEventTimeWall", &fEventData.eventTimeWall);
    tree->Branch("fEventTimeWallPrimaryGeneration", &fEventData.eventTimeWallPrimaryGeneration);
    tree->Branch("fNVolumes", &fEventData.nVolumes);

    tree->Branch("fPrimaryPosition", &fEventData.primaryPosition);
    tree->Branch("fSubEventPosition", &fEventData.subEventPosition);
    tree->Branch("fSubEventDirection", &fEventData.subEventDirection);
    tree->Branch("fSubEventParticleName", &fEventData.subEventParticleName);
    tree->Branch("fPrimaryParticleNames", &fEventData.primaryParticleNames);
    tree->Branch("fPrimaryEnergies", &fEventData.primaryEnergies);
    tree->Branch("fPrimaryDirections", &fEventData.primaryDirections);
    tree->Branch("fVolumeStored", &fEventData.volumeStored);
    tree->Branch("fVolumeStoredNames", &fEventData.volumeStoredNames);
    tree->Branch("fVolumeDepositedEnergy", &fEventData.volumeDepositedEnergy);
    tree->Branch("fTrackIDs", &fEventData.trackIDs);
    tree->Branch("fTrackParentIDs", &fEventData.parentIDs);
    tree->Branch("fTrackParticleNames", &fEventData.trackParticleNames);
    tree->Branch("fTrackCreatorProcesses", &fEventData.trackCreatorProcesses);
    tree->Branch("fTrackDepositedEnergy", &fEventData.trackDepositedEnergy);
    tree->Branch("fTrackInitialEnergies", &fEventData.trackInitialEnergies);
    tree->Branch("fTrackStartIndices", &fEventData.trackStartIndices);
    tree->Branch("fTrackNHits", &fEventData.trackNHits);
    tree->Branch("fTrackGlobalTimestamp", &fEventData.trackGlobalTimestamps);
    tree->Branch("fTrackTimeOffset", &fEventData.trackTimeOffsets);
    tree->Branch("fTrackTimeLength", &fEventData.trackTimeLengths);
    tree->Branch("fTrackLength", &fEventData.trackLengths);
    tree->Branch("fTrackWeight", &fEventData.trackWeights);
    tree->Branch("fTrackSecondariesIDs", &fEventData.trackSecondariesIDs);
    tree->Branch("fTrackSecondariesIndex", &fEventData.trackSecondariesIndices);
    tree->Branch("fTrackSecondariesOffsets", &fEventData.trackSecondariesOffsets);
    tree->Branch("fTrackInitialPosition", &fEventData.trackInitialPositions);
    tree->Branch("fHitX", &fEventData.hitsStorage.x);
    tree->Branch("fHitY", &fEventData.hitsStorage.y);
    tree->Branch("fHitZ", &fEventData.hitsStorage.z);
    tree->Branch("fHitEnergy", &fEventData.hitsStorage.energy);
    tree->Branch("fHitTime", &fEventData.hitsStorage.time);
    tree->Branch("fHitType", &fEventData.hitsStorage.type);
    tree->Branch("fHitProcessID", &fEventData.hitProcessID);
    tree->Branch("fHitVolumeID", &fEventData.hitVolumeID);
    tree->Branch("fHitKineticEnergy", &fEventData.hitKineticEnergy);
    tree->Branch("fHitMomentumDirection", &fEventData.hitMomentumDirection);
    tree->Branch("fHitHadronicTargetIsotopeName", &fEventData.hitHadronicTargetIsotopeName);
    tree->Branch("fHitHadronicTargetIsotopeA", &fEventData.hitHadronicTargetIsotopeA);
    tree->Branch("fHitHadronicTargetIsotopeZ", &fEventData.hitHadronicTargetIsotopeZ);
    tree->Branch("fCrossVolumeNames", &fEventData.crossVolumeNames);
    tree->Branch("fCrossParticleNames", &fEventData.crossParticleNames);
    tree->Branch("fCrossProcessNames", &fEventData.crossProcessNames);
    tree->Branch("fCrossDepositedEnergies", &fEventData.crossDepositedEnergies);
}

void TRestGeant4Event::SetBranchAddresses(TTree* tree) {
    TRestEvent::SetBranchAddresses(tree);

    tree->SetBranchAddress("fSubEventEnergy", &fEventData.subEventEnergy);
    tree->SetBranchAddress("fTotalDepositedEnergy", &fEventData.totalDepositedEnergy);
    tree->SetBranchAddress("fSensitiveVolumeEnergy", &fEventData.sensitiveVolumeEnergy);
    tree->SetBranchAddress("fEventTimeWall", &fEventData.eventTimeWall);
    tree->SetBranchAddress("fEventTimeWallPrimaryGeneration", &fEventData.eventTimeWallPrimaryGeneration);
    tree->SetBranchAddress("fNVolumes", &fEventData.nVolumes);

    fPtr_primaryPosition = &fEventData.primaryPosition;
    fPtr_subEventPosition = &fEventData.subEventPosition;
    fPtr_subEventDirection = &fEventData.subEventDirection;
    fPtr_subEventParticleName = &fEventData.subEventParticleName;
    fPtr_primaryParticleNames = &fEventData.primaryParticleNames;
    fPtr_primaryEnergies = &fEventData.primaryEnergies;
    fPtr_primaryDirections = &fEventData.primaryDirections;
    fPtr_volumeStored = &fEventData.volumeStored;
    fPtr_volumeStoredNames = &fEventData.volumeStoredNames;
    fPtr_volumeDepositedEnergy = &fEventData.volumeDepositedEnergy;
    fPtr_trackIDs = &fEventData.trackIDs;
    fPtr_trackParentIDs = &fEventData.parentIDs;
    fPtr_trackParticleNames = &fEventData.trackParticleNames;
    fPtr_trackCreatorProcesses = &fEventData.trackCreatorProcesses;
    fPtr_trackDepositedEnergy = &fEventData.trackDepositedEnergy;
    fPtr_trackInitialEnergies = &fEventData.trackInitialEnergies;
    fPtr_trackStartIndices = &fEventData.trackStartIndices;
    fPtr_trackNHits = &fEventData.trackNHits;
    fPtr_trackGlobalTimestamp = &fEventData.trackGlobalTimestamps;
    fPtr_trackTimeOffset = &fEventData.trackTimeOffsets;
    fPtr_trackTimeLength = &fEventData.trackTimeLengths;
    fPtr_trackLength = &fEventData.trackLengths;
    fPtr_trackWeight = &fEventData.trackWeights;
    fPtr_trackSecondariesIDs = &fEventData.trackSecondariesIDs;
    fPtr_trackSecondariesIndex = &fEventData.trackSecondariesIndices;
    fPtr_trackSecondariesOffsets = &fEventData.trackSecondariesOffsets;
    fPtr_trackInitialPosition = &fEventData.trackInitialPositions;
    fPtr_hitX = &fEventData.hitsStorage.x;
    fPtr_hitY = &fEventData.hitsStorage.y;
    fPtr_hitZ = &fEventData.hitsStorage.z;
    fPtr_hitEnergy = &fEventData.hitsStorage.energy;
    fPtr_hitTime = &fEventData.hitsStorage.time;
    fPtr_hitType = &fEventData.hitsStorage.type;
    fPtr_hitProcessID = &fEventData.hitProcessID;
    fPtr_hitVolumeID = &fEventData.hitVolumeID;
    fPtr_hitKineticEnergy = &fEventData.hitKineticEnergy;
    fPtr_hitMomentumDirection = &fEventData.hitMomentumDirection;
    fPtr_hitHadronicTargetIsotopeName = &fEventData.hitHadronicTargetIsotopeName;
    fPtr_hitHadronicTargetIsotopeA = &fEventData.hitHadronicTargetIsotopeA;
    fPtr_hitHadronicTargetIsotopeZ = &fEventData.hitHadronicTargetIsotopeZ;
    fPtr_crossVolumeNames = &fEventData.crossVolumeNames;
    fPtr_crossParticleNames = &fEventData.crossParticleNames;
    fPtr_crossProcessNames = &fEventData.crossProcessNames;
    fPtr_crossDepositedEnergies = &fEventData.crossDepositedEnergies;

    tree->SetBranchAddress("fPrimaryPosition", &fPtr_primaryPosition);
    tree->SetBranchAddress("fSubEventPosition", &fPtr_subEventPosition);
    tree->SetBranchAddress("fSubEventDirection", &fPtr_subEventDirection);
    tree->SetBranchAddress("fSubEventParticleName", &fPtr_subEventParticleName);
    tree->SetBranchAddress("fPrimaryParticleNames", &fPtr_primaryParticleNames);
    tree->SetBranchAddress("fPrimaryEnergies", &fPtr_primaryEnergies);
    tree->SetBranchAddress("fPrimaryDirections", &fPtr_primaryDirections);
    tree->SetBranchAddress("fVolumeStored", &fPtr_volumeStored);
    tree->SetBranchAddress("fVolumeStoredNames", &fPtr_volumeStoredNames);
    tree->SetBranchAddress("fVolumeDepositedEnergy", &fPtr_volumeDepositedEnergy);
    tree->SetBranchAddress("fTrackIDs", &fPtr_trackIDs);
    tree->SetBranchAddress("fTrackParentIDs", &fPtr_trackParentIDs);
    tree->SetBranchAddress("fTrackParticleNames", &fPtr_trackParticleNames);
    tree->SetBranchAddress("fTrackCreatorProcesses", &fPtr_trackCreatorProcesses);
    tree->SetBranchAddress("fTrackDepositedEnergy", &fPtr_trackDepositedEnergy);
    tree->SetBranchAddress("fTrackInitialEnergies", &fPtr_trackInitialEnergies);
    tree->SetBranchAddress("fTrackStartIndices", &fPtr_trackStartIndices);
    tree->SetBranchAddress("fTrackNHits", &fPtr_trackNHits);
    tree->SetBranchAddress("fTrackGlobalTimestamp", &fPtr_trackGlobalTimestamp);
    tree->SetBranchAddress("fTrackTimeOffset", &fPtr_trackTimeOffset);
    tree->SetBranchAddress("fTrackTimeLength", &fPtr_trackTimeLength);
    tree->SetBranchAddress("fTrackLength", &fPtr_trackLength);
    tree->SetBranchAddress("fTrackWeight", &fPtr_trackWeight);
    tree->SetBranchAddress("fTrackSecondariesIDs", &fPtr_trackSecondariesIDs);
    tree->SetBranchAddress("fTrackSecondariesIndex", &fPtr_trackSecondariesIndex);
    tree->SetBranchAddress("fTrackSecondariesOffsets", &fPtr_trackSecondariesOffsets);
    tree->SetBranchAddress("fTrackInitialPosition", &fPtr_trackInitialPosition);
    tree->SetBranchAddress("fHitX", &fPtr_hitX);
    tree->SetBranchAddress("fHitY", &fPtr_hitY);
    tree->SetBranchAddress("fHitZ", &fPtr_hitZ);
    tree->SetBranchAddress("fHitEnergy", &fPtr_hitEnergy);
    tree->SetBranchAddress("fHitTime", &fPtr_hitTime);
    tree->SetBranchAddress("fHitType", &fPtr_hitType);
    tree->SetBranchAddress("fHitProcessID", &fPtr_hitProcessID);
    tree->SetBranchAddress("fHitVolumeID", &fPtr_hitVolumeID);
    tree->SetBranchAddress("fHitKineticEnergy", &fPtr_hitKineticEnergy);
    tree->SetBranchAddress("fHitMomentumDirection", &fPtr_hitMomentumDirection);
    tree->SetBranchAddress("fHitHadronicTargetIsotopeName", &fPtr_hitHadronicTargetIsotopeName);
    tree->SetBranchAddress("fHitHadronicTargetIsotopeA", &fPtr_hitHadronicTargetIsotopeA);
    tree->SetBranchAddress("fHitHadronicTargetIsotopeZ", &fPtr_hitHadronicTargetIsotopeZ);
    tree->SetBranchAddress("fCrossVolumeNames", &fPtr_crossVolumeNames);
    tree->SetBranchAddress("fCrossParticleNames", &fPtr_crossParticleNames);
    tree->SetBranchAddress("fCrossProcessNames", &fPtr_crossProcessNames);
    tree->SetBranchAddress("fCrossDepositedEnergies", &fPtr_crossDepositedEnergies);

    RebuildTrackIndex();
    RebuildVolumeIndex();
    RebuildCrossIndex();
}

void TRestGeant4Event::RefreshViews() const {
    RebuildTrackIndex();
    const_cast<TRestGeant4Event*>(this)->RebuildVolumeIndex();
    const_cast<TRestGeant4Event*>(this)->RebuildCrossIndex();
}

void TRestGeant4Event::CopyFrom(const TRestEvent* other) {
    TRestEvent::CopyFrom(other);

    const auto* source = dynamic_cast<const TRestGeant4Event*>(other);
    if (source == nullptr) return;

    fEventData = source->fEventData;
    fMetadata = source->fMetadata;
    fRestRun = source->fRestRun;

    RebuildTrackIndex();
    RebuildVolumeIndex();
    RebuildCrossIndex();
}

void TRestGeant4Event::MoveFrom(TRestGeant4Event&& source) {
    TRestEvent::CopyFrom(&source);

    fEventData = std::move(source.fEventData);
    fMetadata = source.fMetadata;
    fRestRun = source.fRestRun;

    fTrackIDToTrackIndex = std::move(source.fTrackIDToTrackIndex);
    fVolumeIndexMap = std::move(source.fVolumeIndexMap);
    fCrossIndexMap = std::move(source.fCrossIndexMap);

    source.fMetadata = nullptr;
    source.fRestRun = nullptr;
    source.fTrackIDToTrackIndex.clear();
    source.fVolumeIndexMap.clear();
    source.fCrossIndexMap.clear();
    source.fEventData.clear();

    if (fTrackIDToTrackIndex.size() != fEventData.trackIDs.size()) {
        RebuildTrackIndex();
    }
}

void TRestGeant4Event::MoveFrom(TRestEvent* other) {
    TRestEvent::MoveFrom(other);

    auto* source = dynamic_cast<TRestGeant4Event*>(other);
    if (source == nullptr) return;

    fEventData = std::move(source->fEventData);
    fMetadata = source->fMetadata;
    fRestRun = source->fRestRun;

    fTrackIDToTrackIndex = std::move(source->fTrackIDToTrackIndex);
    fVolumeIndexMap = std::move(source->fVolumeIndexMap);
    fCrossIndexMap = std::move(source->fCrossIndexMap);

    source->fMetadata = nullptr;
    source->fRestRun = nullptr;
    source->fTrackIDToTrackIndex.clear();
    source->fVolumeIndexMap.clear();
    source->fCrossIndexMap.clear();
    source->fEventData.clear();

    if (fTrackIDToTrackIndex.size() != fEventData.trackIDs.size()) {
        RebuildTrackIndex();
    }
}

TRestGeant4Track TRestGeant4Event::GetTrack(std::size_t n) {
    if (n >= fEventData.trackIDs.size()) {
        throw std::out_of_range("TRestGeant4Event::GetTrack: track index out of range");
    }

    return TRestGeant4Track(this, n);
}

TRestGeant4Track TRestGeant4Event::GetTrack(std::size_t n) const {
    if (n >= fEventData.trackIDs.size()) {
        throw std::out_of_range("TRestGeant4Event::GetTrack: track index out of range");
    }

    return TRestGeant4Track(const_cast<TRestGeant4Event*>(this), n);
}

std::vector<TRestGeant4Track> TRestGeant4Event::GetTracks() {
    std::vector<TRestGeant4Track> tracks;
    tracks.reserve(fEventData.trackIDs.size());
    for (std::size_t i = 0; i < fEventData.trackIDs.size(); ++i) {
        tracks.emplace_back(this, i);
    }
    return tracks;
}

std::vector<TRestGeant4Track> TRestGeant4Event::GetTracks() const {
    std::vector<TRestGeant4Track> tracks;
    tracks.reserve(fEventData.trackIDs.size());
    for (std::size_t i = 0; i < fEventData.trackIDs.size(); ++i) {
        tracks.emplace_back(const_cast<TRestGeant4Event*>(this), i);
    }
    return tracks;
}

TRestGeant4Track TRestGeant4Event::GetTrackByID(int id) {
    if (fTrackIDToTrackIndex.size() != fEventData.trackIDs.size()) {
        RebuildTrackIndex();
    }

    const auto it = fTrackIDToTrackIndex.find(id);
    if (it == fTrackIDToTrackIndex.end()) {
        throw std::runtime_error("Track ID not found: " + std::to_string(id));
    }

    return TRestGeant4Track(this, static_cast<std::size_t>(it->second));
}

TRestGeant4Track TRestGeant4Event::GetTrackByID(int id) const {
    return const_cast<TRestGeant4Event*>(this)->GetTrackByID(id);
}

void TRestGeant4Event::ClearTracks() {
    fEventData.trackIDs.clear();
    fEventData.parentIDs.clear();
    fEventData.trackParticleNames.clear();
    fEventData.trackCreatorProcesses.clear();
    fEventData.trackDepositedEnergy.clear();
    fEventData.trackInitialEnergies.clear();
    fEventData.trackStartIndices.clear();
    fEventData.trackNHits.clear();
    fEventData.trackGlobalTimestamps.clear();
    fEventData.trackTimeOffsets.clear();
    fEventData.trackTimeLengths.clear();
    fEventData.trackLengths.clear();
    fEventData.trackWeights.clear();
    fEventData.trackSecondariesIDs.clear();
    fEventData.trackSecondariesOffsets.clear();
    fEventData.trackSecondariesIndices.clear();
    fEventData.trackInitialPositions.clear();

    fEventData.hitsStorage.clear();
    fEventData.hitProcessID.clear();
    fEventData.hitVolumeID.clear();
    fEventData.hitKineticEnergy.clear();
    fEventData.hitMomentumDirection.clear();
    fEventData.hitHadronicTargetIsotopeName.clear();
    fEventData.hitHadronicTargetIsotopeA.clear();
    fEventData.hitHadronicTargetIsotopeZ.clear();

    fTrackIDToTrackIndex.clear();
    fHasPendingInitialStep = false;
    fPendingInitialStepIndex = 0;
}

void TRestGeant4Event::RemoveTrackHits(std::size_t trackIndex) {
    if (trackIndex >= fEventData.trackIDs.size()) return;
    if (trackIndex >= fEventData.trackStartIndices.size() ||
        trackIndex >= fEventData.trackNHits.size()) return;

    const std::size_t start = static_cast<std::size_t>(fEventData.trackStartIndices[trackIndex]);
    const std::size_t count = static_cast<std::size_t>(fEventData.trackNHits[trackIndex]);
    if (count == 0) return;

    auto eraseRange = [start, count](auto& values) {
        if (start >= values.size()) return;
        const std::size_t end = std::min(start + count, values.size());
        values.erase(values.begin() + static_cast<std::ptrdiff_t>(start),
                     values.begin() + static_cast<std::ptrdiff_t>(end));
    };

    eraseRange(fEventData.hitsStorage.x);
    eraseRange(fEventData.hitsStorage.y);
    eraseRange(fEventData.hitsStorage.z);
    eraseRange(fEventData.hitsStorage.time);
    eraseRange(fEventData.hitsStorage.energy);
    eraseRange(fEventData.hitsStorage.type);
    eraseRange(fEventData.hitProcessID);
    eraseRange(fEventData.hitVolumeID);
    eraseRange(fEventData.hitKineticEnergy);
    eraseRange(fEventData.hitMomentumDirection);
    eraseRange(fEventData.hitHadronicTargetIsotopeName);
    eraseRange(fEventData.hitHadronicTargetIsotopeA);
    eraseRange(fEventData.hitHadronicTargetIsotopeZ);

    for (std::size_t i = trackIndex + 1; i < fEventData.trackStartIndices.size(); ++i) {
        fEventData.trackStartIndices[i] -= static_cast<int>(count);
    }

    fEventData.trackNHits[trackIndex] = 0;
}

void TRestGeant4Event::AddEnergyInVolumeForParticleForProcess(
    Double_t energy, const std::string& volumeName,
    const std::string& particleName, const std::string& processName) {
    if (energy <= 0) return;

    fEventData.totalDepositedEnergy += energy;

    const auto key =
        std::make_tuple(volumeName, particleName, processName);

    auto itCross = fCrossIndexMap.find(key);
    if (itCross != fCrossIndexMap.end()) {
        fEventData.crossDepositedEnergies[itCross->second] += energy;
    } else {
        const std::size_t newIdx = fEventData.crossVolumeNames.size();

        fEventData.crossVolumeNames.push_back(volumeName);
        fEventData.crossParticleNames.push_back(particleName);
        fEventData.crossProcessNames.push_back(processName);
        fEventData.crossDepositedEnergies.push_back(energy);

        fCrossIndexMap[key] = newIdx;
    }

    auto itVolume = fVolumeIndexMap.find(volumeName);
    if (itVolume != fVolumeIndexMap.end()) {
        fEventData.volumeDepositedEnergy[itVolume->second] += energy;
    } else {
        const std::size_t newVolumeIdx = fEventData.volumeStoredNames.size();

        fEventData.volumeStoredNames.push_back(volumeName);
        fEventData.volumeDepositedEnergy.push_back(energy);
        fEventData.volumeStored.push_back(fEventData.nVolumes);

        fVolumeIndexMap[volumeName] = newVolumeIdx;
        ++fEventData.nVolumes;
    }
}

void TRestGeant4Event::PrintG4Event(int maxTracks, int maxHits) const {
    std::cout << "=========================================================" << std::endl;
    std::cout << " TRestGeant4Event - PrintEvent" << std::endl;
    std::cout << "=========================================================" << std::endl;

    TRestEvent::PrintEvent();

    const int totalTracksAvailable = static_cast<int>(GetNumberOfTracks());
    if (maxTracks < 0) maxTracks = totalTracksAvailable;
    const int nTracks = std::min(maxTracks, totalTracksAvailable);

    std::cout << "- Total deposited energy: "
              << REST_Units::FormatAs(fEventData.totalDepositedEnergy, REST_Units::Energy)
              << std::endl;
    std::cout << "- Sensitive detectors total energy: "
              << REST_Units::FormatAs(fEventData.sensitiveVolumeEnergy, REST_Units::Energy)
              << std::endl;
    std::cout << "- Event Wall Time: "
              << REST_Units::FormatAs(fEventData.eventTimeWall, REST_Units::Time)
              << std::endl;
    std::cout << "- Primary source position: "
              << REST_Units::FormatAs(fEventData.primaryPosition, REST_Units::Length)
              << std::endl;

    std::cout << "- Primary Particles (" << fEventData.primaryParticleNames.size() << "):" << std::endl;

    for (std::size_t i = 0; i < fEventData.primaryParticleNames.size(); ++i) {
        std::cout << "   - Source [" << i << "]: " << fEventData.primaryParticleNames[i] << std::endl;

        if (i < fEventData.primaryDirections.size()) {
            std::cout << "     Direction: ("
                      << fEventData.primaryDirections[i].X() << ", "
                      << fEventData.primaryDirections[i].Y() << ", "
                      << fEventData.primaryDirections[i].Z() << ")" << std::endl;
        }

        if (i < fEventData.primaryEnergies.size()) {
            std::cout << "     Energy: "
                      << REST_Units::FormatAs(fEventData.primaryEnergies[i], REST_Units::Energy)
                      << std::endl;
        }
    }

    std::cout << "- Number of tracks to print: " << nTracks
              << " (Total in event: " << totalTracksAvailable << ")" << std::endl;

    for (int i = 0; i < nTracks; ++i) {
        std::cout << "   -----------------------------------------------------" << std::endl;
        const auto track = GetTrack(static_cast<std::size_t>(i));
        
        track.PrintTrack(maxHits);
    }

    std::cout << "=========================================================" << std::endl;
}
