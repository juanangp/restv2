#include "TRestGeant4Event.h"

#include <TGraph.h>
#include <TH1F.h>
#include <TLegend.h>
#include <TLegendEntry.h>
#include <TMarker.h>
#include <TPad.h>

#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <set>

#include "TRestGeant4Metadata.h"
#include "TRestGeant4Track.h"
#include "TRestRun.h"
#include "TRestTools.h"

namespace {
const bool kRegistered = []() {
    EventRegistry::Instance().Register("TRestGeant4Event",
                                       []() { return std::make_unique<TRestGeant4Event>(); });
    return true;
}();
}  // namespace

const TRestGeant4Metadata* TRestGeant4Event::GetGeant4Metadata() const {
    if (fMetadata) return fMetadata;

    if (fRestRun) {
        TRestMetadata* baseMeta = fRestRun->GetMetadataClass("TRestGeant4Metadata");
        if (baseMeta) {
            auto* nonConstThis = const_cast<TRestGeant4Event*>(this);
            nonConstThis->fMetadata = dynamic_cast<const TRestGeant4Metadata*>(baseMeta);
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
    const std::size_t n = std::min(fEventData.volumeIDs.size(), fEventData.volumeDepositedEnergy.size());
    for (std::size_t i = 0; i < n; ++i) fVolumeIndexMap[fEventData.volumeIDs[i]] = i;
}

void TRestGeant4Event::RebuildCrossIndex() {
    fCrossIndexMap.clear();
    const std::size_t n =
        std::min({fEventData.crossVolumeIDs.size(), fEventData.crossParticleIDs.size(),
                  fEventData.crossProcessIDs.size(), fEventData.crossDepositedEnergies.size()});
    for (std::size_t i = 0; i < n; ++i) {
        fCrossIndexMap[std::make_tuple(fEventData.crossVolumeIDs[i], fEventData.crossParticleIDs[i],
                                       fEventData.crossProcessIDs[i])] = i;
    }
}

void TRestGeant4Event::CreateBranches(TTree* tree) {
    TRestEvent::CreateBranches(tree);

    tree->Branch("fSubEventEnergy", &fEventData.subEventEnergy);
    tree->Branch("fTotalDepositedEnergy", &fEventData.totalDepositedEnergy);
    tree->Branch("fSensitiveVolumeEnergy", &fEventData.sensitiveVolumeEnergy);
    tree->Branch("fEventTimeWall", &fEventData.eventTimeWall);
    tree->Branch("fEventTimeWallPrimaryGeneration", &fEventData.eventTimeWallPrimaryGeneration);
    tree->Branch("fPrimaryPositionX", &fEventData.primaryPositionX);
    tree->Branch("fPrimaryPositionY", &fEventData.primaryPositionY);
    tree->Branch("fPrimaryPositionZ", &fEventData.primaryPositionZ);
    tree->Branch("fSubEventParticleID", &fEventData.subEventParticleID);
    tree->Branch("fSubEventPositionX", &fEventData.subEventPositionX);
    tree->Branch("fSubEventPositionY", &fEventData.subEventPositionY);
    tree->Branch("fSubEventPositionZ", &fEventData.subEventPositionZ);
    tree->Branch("fSubEventDirectionX", &fEventData.subEventDirectionX);
    tree->Branch("fSubEventDirectionY", &fEventData.subEventDirectionY);
    tree->Branch("fSubEventDirectionZ", &fEventData.subEventDirectionZ);
    tree->Branch("fPrimaryParticleIDs", &fEventData.primaryParticleIDs);
    tree->Branch("fPrimaryEnergies", &fEventData.primaryEnergies);
    tree->Branch("fPrimaryDirectionX", &fEventData.primaryDirectionX);
    tree->Branch("fPrimaryDirectionY", &fEventData.primaryDirectionY);
    tree->Branch("fPrimaryDirectionZ", &fEventData.primaryDirectionZ);
    tree->Branch("fVolumeIDs", &fEventData.volumeIDs);
    tree->Branch("fVolumeDepositedEnergy", &fEventData.volumeDepositedEnergy);
    tree->Branch("fTrackIDs", &fEventData.trackIDs);
    tree->Branch("fTrackParentIDs", &fEventData.parentIDs);
    tree->Branch("fTrackParticleIDs", &fEventData.trackParticleIDs);
    tree->Branch("fTrackCreatorProcessIDs", &fEventData.trackCreatorProcessIDs);
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
    tree->Branch("fTrackInitialPositionX", &fEventData.trackInitialPositionX);
    tree->Branch("fTrackInitialPositionY", &fEventData.trackInitialPositionY);
    tree->Branch("fTrackInitialPositionZ", &fEventData.trackInitialPositionZ);
    tree->Branch("fHitX", &fEventData.hitsStorage.x);
    tree->Branch("fHitY", &fEventData.hitsStorage.y);
    tree->Branch("fHitZ", &fEventData.hitsStorage.z);
    tree->Branch("fHitEnergy", &fEventData.hitsStorage.energy);
    tree->Branch("fHitTime", &fEventData.hitsStorage.time);
    tree->Branch("fHitType", &fEventData.hitsStorage.type);
    tree->Branch("fHitProcessID", &fEventData.hitProcessID);
    tree->Branch("fHitVolumeID", &fEventData.hitVolumeID);
    tree->Branch("fHitKineticEnergy", &fEventData.hitKineticEnergy);
    tree->Branch("fHitMomentumDirectionX", &fEventData.hitMomentumDirectionX);
    tree->Branch("fHitMomentumDirectionY", &fEventData.hitMomentumDirectionY);
    tree->Branch("fHitMomentumDirectionZ", &fEventData.hitMomentumDirectionZ);
    tree->Branch("fHitHadronicTargetIsotopeA", &fEventData.hitHadronicTargetIsotopeA);
    tree->Branch("fHitHadronicTargetIsotopeZ", &fEventData.hitHadronicTargetIsotopeZ);
    tree->Branch("fCrossVolumeIDs", &fEventData.crossVolumeIDs);
    tree->Branch("fCrossParticleIDs", &fEventData.crossParticleIDs);
    tree->Branch("fCrossProcessIDs", &fEventData.crossProcessIDs);
    tree->Branch("fCrossDepositedEnergies", &fEventData.crossDepositedEnergies);
}

void TRestGeant4Event::SetBranchAddresses(TTree* tree) {
    TRestEvent::SetBranchAddresses(tree);

    tree->SetBranchAddress("fSubEventEnergy", &fEventData.subEventEnergy);
    tree->SetBranchAddress("fTotalDepositedEnergy", &fEventData.totalDepositedEnergy);
    tree->SetBranchAddress("fSensitiveVolumeEnergy", &fEventData.sensitiveVolumeEnergy);
    tree->SetBranchAddress("fEventTimeWall", &fEventData.eventTimeWall);
    tree->SetBranchAddress("fEventTimeWallPrimaryGeneration", &fEventData.eventTimeWallPrimaryGeneration);
    tree->SetBranchAddress("fPrimaryPositionX", &fEventData.primaryPositionX);
    tree->SetBranchAddress("fPrimaryPositionY", &fEventData.primaryPositionY);
    tree->SetBranchAddress("fPrimaryPositionZ", &fEventData.primaryPositionZ);
    tree->SetBranchAddress("fSubEventParticleID", &fEventData.subEventParticleID);
    tree->SetBranchAddress("fSubEventPositionX", &fEventData.subEventPositionX);
    tree->SetBranchAddress("fSubEventPositionY", &fEventData.subEventPositionY);
    tree->SetBranchAddress("fSubEventPositionZ", &fEventData.subEventPositionZ);
    tree->SetBranchAddress("fSubEventDirectionX", &fEventData.subEventDirectionX);
    tree->SetBranchAddress("fSubEventDirectionY", &fEventData.subEventDirectionY);
    tree->SetBranchAddress("fSubEventDirectionZ", &fEventData.subEventDirectionZ);

    fPtr_primaryParticleIDs = &fEventData.primaryParticleIDs;
    fPtr_primaryEnergies = &fEventData.primaryEnergies;
    fPtr_primaryDirectionX = &fEventData.primaryDirectionX;
    fPtr_primaryDirectionY = &fEventData.primaryDirectionY;
    fPtr_primaryDirectionZ = &fEventData.primaryDirectionZ;
    fPtr_volumeIDs = &fEventData.volumeIDs;
    fPtr_volumeDepositedEnergy = &fEventData.volumeDepositedEnergy;
    fPtr_trackIDs = &fEventData.trackIDs;
    fPtr_trackParentIDs = &fEventData.parentIDs;
    fPtr_trackParticleIDs = &fEventData.trackParticleIDs;
    fPtr_trackCreatorProcessIDs = &fEventData.trackCreatorProcessIDs;
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
    fPtr_trackInitialPositionX = &fEventData.trackInitialPositionX;
    fPtr_trackInitialPositionY = &fEventData.trackInitialPositionY;
    fPtr_trackInitialPositionZ = &fEventData.trackInitialPositionZ;
    fPtr_hitX = &fEventData.hitsStorage.x;
    fPtr_hitY = &fEventData.hitsStorage.y;
    fPtr_hitZ = &fEventData.hitsStorage.z;
    fPtr_hitEnergy = &fEventData.hitsStorage.energy;
    fPtr_hitTime = &fEventData.hitsStorage.time;
    fPtr_hitType = &fEventData.hitsStorage.type;
    fPtr_hitProcessID = &fEventData.hitProcessID;
    fPtr_hitVolumeID = &fEventData.hitVolumeID;
    fPtr_hitKineticEnergy = &fEventData.hitKineticEnergy;
    fPtr_hitMomentumDirectionX = &fEventData.hitMomentumDirectionX;
    fPtr_hitMomentumDirectionY = &fEventData.hitMomentumDirectionY;
    fPtr_hitMomentumDirectionZ = &fEventData.hitMomentumDirectionZ;
    fPtr_hitHadronicTargetIsotopeA = &fEventData.hitHadronicTargetIsotopeA;
    fPtr_hitHadronicTargetIsotopeZ = &fEventData.hitHadronicTargetIsotopeZ;
    fPtr_crossVolumeIDs = &fEventData.crossVolumeIDs;
    fPtr_crossParticleIDs = &fEventData.crossParticleIDs;
    fPtr_crossProcessIDs = &fEventData.crossProcessIDs;
    fPtr_crossDepositedEnergies = &fEventData.crossDepositedEnergies;

    tree->SetBranchAddress("fPrimaryParticleIDs", &fPtr_primaryParticleIDs);
    tree->SetBranchAddress("fPrimaryEnergies", &fPtr_primaryEnergies);
    tree->SetBranchAddress("fPrimaryDirectionX", &fPtr_primaryDirectionX);
    tree->SetBranchAddress("fPrimaryDirectionY", &fPtr_primaryDirectionY);
    tree->SetBranchAddress("fPrimaryDirectionZ", &fPtr_primaryDirectionZ);
    tree->SetBranchAddress("fVolumeIDs", &fPtr_volumeIDs);
    tree->SetBranchAddress("fVolumeDepositedEnergy", &fPtr_volumeDepositedEnergy);
    tree->SetBranchAddress("fTrackIDs", &fPtr_trackIDs);
    tree->SetBranchAddress("fTrackParentIDs", &fPtr_trackParentIDs);
    tree->SetBranchAddress("fTrackParticleIDs", &fPtr_trackParticleIDs);
    tree->SetBranchAddress("fTrackCreatorProcessIDs", &fPtr_trackCreatorProcessIDs);
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
    tree->SetBranchAddress("fTrackInitialPositionX", &fPtr_trackInitialPositionX);
    tree->SetBranchAddress("fTrackInitialPositionY", &fPtr_trackInitialPositionY);
    tree->SetBranchAddress("fTrackInitialPositionZ", &fPtr_trackInitialPositionZ);
    tree->SetBranchAddress("fHitX", &fPtr_hitX);
    tree->SetBranchAddress("fHitY", &fPtr_hitY);
    tree->SetBranchAddress("fHitZ", &fPtr_hitZ);
    tree->SetBranchAddress("fHitEnergy", &fPtr_hitEnergy);
    tree->SetBranchAddress("fHitTime", &fPtr_hitTime);
    tree->SetBranchAddress("fHitType", &fPtr_hitType);
    tree->SetBranchAddress("fHitProcessID", &fPtr_hitProcessID);
    tree->SetBranchAddress("fHitVolumeID", &fPtr_hitVolumeID);
    tree->SetBranchAddress("fHitKineticEnergy", &fPtr_hitKineticEnergy);
    tree->SetBranchAddress("fHitMomentumDirectionX", &fPtr_hitMomentumDirectionX);
    tree->SetBranchAddress("fHitMomentumDirectionY", &fPtr_hitMomentumDirectionY);
    tree->SetBranchAddress("fHitMomentumDirectionZ", &fPtr_hitMomentumDirectionZ);
    tree->SetBranchAddress("fHitHadronicTargetIsotopeA", &fPtr_hitHadronicTargetIsotopeA);
    tree->SetBranchAddress("fHitHadronicTargetIsotopeZ", &fPtr_hitHadronicTargetIsotopeZ);
    tree->SetBranchAddress("fCrossVolumeIDs", &fPtr_crossVolumeIDs);
    tree->SetBranchAddress("fCrossParticleIDs", &fPtr_crossParticleIDs);
    tree->SetBranchAddress("fCrossProcessIDs", &fPtr_crossProcessIDs);
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
    auto& d = fEventData;
    auto clear_all = [](auto&... vecs) { (vecs.clear(), ...); };
    clear_all(d.trackIDs, d.parentIDs, d.trackParticleIDs, d.trackCreatorProcessIDs, d.trackDepositedEnergy,
              d.trackInitialEnergies, d.trackStartIndices, d.trackNHits, d.trackGlobalTimestamps,
              d.trackTimeOffsets, d.trackTimeLengths, d.trackLengths, d.trackWeights, d.trackSecondariesIDs,
              d.trackSecondariesOffsets, d.trackSecondariesIndices, d.trackInitialPositionX,
              d.trackInitialPositionY, d.trackInitialPositionZ, d.hitProcessID, d.hitVolumeID,
              d.hitKineticEnergy, d.hitMomentumDirectionX, d.hitMomentumDirectionY, d.hitMomentumDirectionZ,
              d.hitHadronicTargetIsotopeA, d.hitHadronicTargetIsotopeZ);
    d.hitsStorage.clear();

    fTrackIDToTrackIndex.clear();
    fHasPendingInitialStep = false;
    fPendingInitialStepIndex = 0;
}

void TRestGeant4Event::RemoveTrackHits(std::size_t trackIndex) {
    if (trackIndex >= fEventData.trackIDs.size()) return;
    if (trackIndex >= fEventData.trackStartIndices.size() || trackIndex >= fEventData.trackNHits.size())
        return;

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
    eraseRange(fEventData.hitMomentumDirectionX);
    eraseRange(fEventData.hitMomentumDirectionY);
    eraseRange(fEventData.hitMomentumDirectionZ);
    eraseRange(fEventData.hitHadronicTargetIsotopeA);
    eraseRange(fEventData.hitHadronicTargetIsotopeZ);

    for (std::size_t i = trackIndex + 1; i < fEventData.trackStartIndices.size(); ++i) {
        fEventData.trackStartIndices[i] -= static_cast<int>(count);
    }

    fEventData.trackNHits[trackIndex] = 0;
}

void TRestGeant4Event::AddEnergyInVolumeForParticleForProcess(double energy, int volumeID, int particleID,
                                                              int processID) {
    if (energy <= 0) return;

    fEventData.totalDepositedEnergy += energy;

    const auto key = std::make_tuple(volumeID, particleID, processID);
    const auto itCross = fCrossIndexMap.find(key);
    if (itCross != fCrossIndexMap.end()) {
        fEventData.crossDepositedEnergies[itCross->second] += energy;
    } else {
        fCrossIndexMap[key] = fEventData.crossVolumeIDs.size();
        fEventData.crossVolumeIDs.push_back(volumeID);
        fEventData.crossParticleIDs.push_back(particleID);
        fEventData.crossProcessIDs.push_back(processID);
        fEventData.crossDepositedEnergies.push_back(energy);
    }

    const auto itVolume = fVolumeIndexMap.find(volumeID);
    if (itVolume != fVolumeIndexMap.end()) {
        fEventData.volumeDepositedEnergy[itVolume->second] += energy;
    } else {
        fVolumeIndexMap[volumeID] = fEventData.volumeIDs.size();
        fEventData.volumeIDs.push_back(volumeID);
        fEventData.volumeDepositedEnergy.push_back(energy);
    }
}

double TRestGeant4Event::GetEnergyInVolume(const std::string& volumeName) const {
    const auto* metadata = GetGeant4Metadata();
    if (!metadata) return 0.0;
    return GetEnergyInVolume(metadata->GetGeant4GeometryInfo().GetIDFromVolume(volumeName));
}

std::string TRestGeant4Event::GetParticleName(int particleID) const {
    const auto* metadata = GetGeant4Metadata();
    return metadata ? metadata->GetGeant4PhysicsInfo().GetParticleName(particleID) : "";
}

std::string TRestGeant4Event::GetProcessName(int processID) const {
    const auto* metadata = GetGeant4Metadata();
    return metadata ? metadata->GetGeant4PhysicsInfo().GetProcessName(processID) : "";
}

std::string TRestGeant4Event::GetVolumeName(int volumeID) const {
    const auto* metadata = GetGeant4Metadata();
    return metadata ? metadata->GetGeant4GeometryInfo().GetVolumeFromID(volumeID) : "";
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
              << REST_Units::FormatAs(fEventData.totalDepositedEnergy, REST_Units::Energy) << std::endl;
    std::cout << "- Sensitive detectors total energy: "
              << REST_Units::FormatAs(fEventData.sensitiveVolumeEnergy, REST_Units::Energy) << std::endl;
    std::cout << "- Event Wall Time: " << REST_Units::FormatAs(fEventData.eventTimeWall, REST_Units::Time)
              << std::endl;
    std::cout << "- Primary source position: "
              << REST_Units::FormatAs(GetPrimaryPosition(), REST_Units::Length) << std::endl;

    std::cout << "- Primary Particles (" << GetNumberOfPrimaries() << "):" << std::endl;

    for (std::size_t i = 0; i < GetNumberOfPrimaries(); ++i) {
        std::cout << "   - Source [" << i << "]: " << GetPrimaryParticleName(i) << std::endl;

        if (i < fEventData.primaryDirectionX.size()) {
            const auto direction = GetPrimaryDirection(i);
            std::cout << "     Direction: (" << direction.X() << ", " << direction.Y() << ", "
                      << direction.Z() << ")" << std::endl;
        }

        if (i < fEventData.primaryEnergies.size()) {
            std::cout << "     Energy: "
                      << REST_Units::FormatAs(fEventData.primaryEnergies[i], REST_Units::Energy) << std::endl;
        }
    }

    std::cout << "- Number of tracks to print: " << nTracks << " (Total in event: " << totalTracksAvailable
              << ")" << std::endl;

    for (int i = 0; i < nTracks; ++i) {
        std::cout << "   -----------------------------------------------------" << std::endl;
        const auto track = GetTrack(static_cast<std::size_t>(i));

        track.PrintTrack(maxHits);
    }

    std::cout << "=========================================================" << std::endl;
}

namespace {
/// Default bin size of the hist option (mm), as legacy
const double kDefaultBinSize = 3;
/// Marker sizes of the hits of the graph option, from no energy to the maximum one
const double kMinMarkerSize = 0.4, kMaxMarkerSize = 3;
}  // namespace

TPad* TRestGeant4Event::DrawEvent(const std::string& option) const {
    std::vector<std::string> options;
    for (const auto& opt : TRestTools::Split(option, ':')) {
        if (opt == "print") {
            PrintEvent();
        } else if (!opt.empty()) {
            options.push_back(opt);
        }
    }
    if (options.empty()) options = {"graph", "hist(Cont0,colz)"};

    if (fEventData.hitsStorage.x.empty()) {
        std::cout << "Empty event " << std::endl;
        return nullptr;
    }

    TVirtualPad* safePad = gPad;

    delete fPad;
    fPad = new TPad(Form("pad_geant4_event_%d", GetID()), "REST Geant4 Event Viewer", 0, 0, 1, 1);
    fPad->Divide(1, options.size());
    fPad->Draw();

    for (size_t i = 0; i < options.size(); i++) {
        // name(rootOption)[list]: the brackets are a list of processes (graph) or the bin size (hist), so
        // they are not parsed as the DrawOption value
        const auto open = options[i].find('['), close = options[i].find(']');
        std::string brackets;
        if (open != std::string::npos && close != std::string::npos && close > open) {
            brackets = options[i].substr(open + 1, close - open - 1);
        }
        const auto drawOption = ParseDrawOption(options[i].substr(0, open));

        TVirtualPad* optionPad = fPad->cd(i + 1);
        if (drawOption.name == "graph") {
            std::vector<std::string> processes;
            for (const auto& process : TRestTools::Split(brackets, ',')) {
                if (!process.empty()) processes.push_back(process);
            }
            DrawGraphs(optionPad, processes);
        } else if (drawOption.name == "hist") {
            // As legacy, also written binSize=<value>
            const auto equal = brackets.find('=');
            const auto value = brackets.substr(equal == std::string::npos ? 0 : equal + 1);
            const double binSize = value.empty() ? kDefaultBinSize : std::stod(value);

            // The hits with energy, the others (transportation) only change the ranges
            TRestHitsData hits;
            const auto& storage = fEventData.hitsStorage;
            for (size_t n = 0; n < storage.energy.size(); n++) {
                if (storage.energy[n] <= 0) continue;
                hits.x.push_back(storage.x[n]);
                hits.y.push_back(storage.y[n]);
                hits.z.push_back(storage.z[n]);
                hits.time.push_back(storage.time[n]);
                hits.energy.push_back(storage.energy[n]);
                hits.type.push_back(storage.type[n]);
            }
            if (hits.x.empty()) {
                std::cout << "TRestGeant4Event::DrawEvent: no hit with energy" << std::endl;
                continue;
            }
            TRestHits(&hits, 0, static_cast<int>(hits.x.size()))
                .DrawHistograms(optionPad, drawOption.rootOption, binSize);
        } else {
            std::cout << "TRestGeant4Event::DrawEvent: unknown option '" << options[i] << "'" << std::endl;
        }
    }

    fPad->Modified();
    if (safePad) safePad->cd();
    return fPad;
}

void TRestGeant4Event::DrawGraphs(TVirtualPad* pad, const std::vector<std::string>& processes) const {
    const auto& data = fEventData;
    const auto& hits = data.hitsStorage;

    // Process IDs of the marked processes, from the physics information of the simulation
    std::vector<int> processIDs;
    if (!processes.empty()) {
        const auto* metadata = GetGeant4Metadata();
        for (const auto& process : processes) {
            const int id = metadata ? metadata->GetGeant4PhysicsInfo().GetProcessID(process) : -1;
            if (id < 0) {
                std::cout << "TRestGeant4Event::DrawEvent: process '" << process
                          << "' not marked, unknown or no TRestGeant4Metadata" << std::endl;
            }
            processIDs.push_back(id);
        }
    }

    // A colour per track, by particle (once per track, GetParticleColor warns for unknown particles)
    std::vector<Color_t> colors;
    std::vector<std::pair<std::string, Color_t>> particles;
    for (size_t t = 0; t < GetNumberOfTracks(); t++) {
        const auto track = GetTrack(t);
        colors.push_back(track.GetParticleColor());
        const auto particle = track.GetParticleName();
        const auto isParticle = [&particle](const auto& p) { return p.first == particle; };
        if (std::none_of(particles.begin(), particles.end(), isParticle)) {
            particles.emplace_back(particle, colors.back());
        }
    }

    double min[3], max[3];
    double maxEnergy = 0;
    for (int a = 0; a < 3; a++) {
        min[a] = std::numeric_limits<double>::max();
        max[a] = std::numeric_limits<double>::lowest();
    }
    for (size_t n = 0; n < hits.x.size(); n++) {
        const double position[3] = {hits.x[n], hits.y[n], hits.z[n]};
        for (int a = 0; a < 3; a++) {
            min[a] = std::min(min[a], position[a]);
            max[a] = std::max(max[a], position[a]);
        }
        maxEnergy = std::max(maxEnergy, static_cast<double>(hits.energy[n]));
    }
    // A margin around the hits, at least 1 mm (a single hit)
    for (int a = 0; a < 3; a++) {
        const double margin = std::max(0.05 * (max[a] - min[a]), 1.);
        min[a] -= margin;
        max[a] += margin;
    }
    auto markerSize = [maxEnergy](double energy) {
        if (maxEnergy <= 0) return kMinMarkerSize;
        return kMinMarkerSize + (kMaxMarkerSize - kMinMarkerSize) * std::max(energy, 0.) / maxEnergy;
    };

    pad->Divide(3, 1);
    const std::string title = Form("Event ID %d", GetID());
    for (int p = 0; p < 3; p++) {
        const int u = TRestHitsData::kPlanes[p][0], v = TRestHitsData::kPlanes[p][1];
        pad->cd(p + 1);
        SetPadMargins(gPad);

        auto* frame = new TH1F(Form("frame_%d", p), title.c_str(), 1, min[u], max[u]);
        frame->SetMinimum(min[v]);
        frame->SetMaximum(max[v]);
        DrawOwned(frame, "AXIS", TRestHitsData::kAxisTitles[u], TRestHitsData::kAxisTitles[v]);

        auto* legend = new TLegend(0.75, 0.7, 0.95, 0.9);
        legend->SetBit(TObject::kCanDelete);
        for (const auto& [particle, color] : particles) {
            auto* entry = legend->AddEntry(static_cast<TObject*>(nullptr), particle.c_str(), "p");
            entry->SetMarkerColor(color);
            entry->SetMarkerStyle(20);
            entry->SetMarkerSize(1.5);
        }

        std::vector<bool> processInLegend(processIDs.size(), false);
        for (size_t t = 0; t < GetNumberOfTracks(); t++) {
            const size_t start = data.trackStartIndices[t];
            for (size_t n = start; n < start + data.trackNHits[t]; n++) {
                const double position[3] = {hits.x[n], hits.y[n], hits.z[n]};
                auto* marker = new TMarker(position[u], position[v], 20);
                marker->SetMarkerColor(colors[t]);
                marker->SetMarkerSize(markerSize(hits.energy[n]));
                marker->SetBit(TObject::kCanDelete);
                marker->Draw();

                for (size_t k = 0; k < processIDs.size(); k++) {
                    if (processIDs[k] < 0 || data.hitProcessID[n] != processIDs[k]) continue;
                    auto* processMarker = new TMarker(position[u], position[v], 24 + k % 9);
                    processMarker->SetMarkerColor(kBlack);
                    processMarker->SetMarkerSize(2);
                    processMarker->SetBit(TObject::kCanDelete);
                    processMarker->Draw();
                    if (!processInLegend[k]) {
                        auto* entry =
                            legend->AddEntry(static_cast<TObject*>(nullptr), processes[k].c_str(), "p");
                        entry->SetMarkerColor(kBlack);
                        entry->SetMarkerStyle(processMarker->GetMarkerStyle());
                        entry->SetMarkerSize(1.5);
                        processInLegend[k] = true;
                    }
                }
            }
        }
        legend->Draw();
    }
}
