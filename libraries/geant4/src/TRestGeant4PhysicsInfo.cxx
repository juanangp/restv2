#include "TRestGeant4PhysicsInfo.h"

#include <algorithm>
#include <iostream>
#include <mutex>

#include "TRestMetadata.h"

using namespace std;

// Global thread-safety guard interface for parallel multi-threaded Geant4 execution
static std::mutex insertMutex;

// Modern REST v3 field registry reflection hooks for composite metadata storage loading
static const bool TRestGeant4PhysicsInfo_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestGeant4PhysicsInfo>("processNamesMap", &TRestGeant4PhysicsInfo::fProcessNamesMap);
    reg.RegisterField<TRestGeant4PhysicsInfo>("particleNamesMap", &TRestGeant4PhysicsInfo::fParticleNamesMap);
    reg.RegisterField<TRestGeant4PhysicsInfo>("processTypesMap", &TRestGeant4PhysicsInfo::fProcessTypesMap);
    return true;
}();

// --- Structural Map Analytical Lookups ---

std::set<std::string> TRestGeant4PhysicsInfo::GetAllParticles() const {
    std::set<std::string> particles;
    for (const auto& [_, name] : fParticleNamesMap) {
        particles.insert(name);
    }
    return particles;
}

std::set<std::string> TRestGeant4PhysicsInfo::GetAllProcesses() const {
    std::set<std::string> processes;
    for (const auto& [_, name] : fProcessNamesMap) {
        processes.insert(name);
    }
    return processes;
}

std::set<std::string> TRestGeant4PhysicsInfo::GetAllProcessTypes() const {
    std::set<std::string> types;
    for (const auto& [_, type] : fProcessTypesMap) {
        types.insert(type);
    }
    return types;
}

// --- Thread-Safe Insertion Routines (Multi-Threading Protected) ---

void TRestGeant4PhysicsInfo::InsertProcessName(int id, const std::string& processName,
                                               const std::string& processType) {
    if (fProcessNamesMap.count(id) > 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(insertMutex);
    fProcessNamesMap[id] = processName;
    fProcessNamesReverseMap[processName] = id;
    fProcessTypesMap[processName] = processType;
}

void TRestGeant4PhysicsInfo::InsertParticleName(int id, const std::string& particleName) {
    if (fParticleNamesMap.count(id) > 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(insertMutex);
    fParticleNamesMap[id] = particleName;
    fParticleNamesReverseMap[particleName] = id;
}

// --- Modernized Template Helper for Safe Map Access ---

template <typename T, typename U>
static U GetOrDefaultMapValueFromKey(const std::map<T, U>& internalMap, const T& key) {
    auto it = internalMap.find(key);
    if (it != internalMap.end()) {
        return it->second;
    }
    return U{};  // Returns default empty construct safely (e.g. "" or -1)
}

std::string TRestGeant4PhysicsInfo::GetProcessName(int id) const {
    return GetOrDefaultMapValueFromKey<int, std::string>(fProcessNamesMap, id);
}

int TRestGeant4PhysicsInfo::GetProcessID(const std::string& processName) const {
    auto id = GetOrDefaultMapValueFromKey<std::string, int>(fProcessNamesReverseMap, processName);
    return (id == 0 && fProcessNamesReverseMap.count(processName) == 0) ? -1 : id;
}

std::string TRestGeant4PhysicsInfo::GetParticleName(int id) const {
    return GetOrDefaultMapValueFromKey<int, std::string>(fParticleNamesMap, id);
}

int TRestGeant4PhysicsInfo::GetParticleID(const std::string& particleName) const {
    auto id = GetOrDefaultMapValueFromKey<std::string, int>(fParticleNamesReverseMap, particleName);
    return (id == 0 && fParticleNamesReverseMap.count(particleName) == 0) ? -1 : id;
}

std::string TRestGeant4PhysicsInfo::GetProcessType(const std::string& processName) const {
    return GetOrDefaultMapValueFromKey<std::string, std::string>(fProcessTypesMap, processName);
}

// --- Informational Logging Reports ---

void TRestGeant4PhysicsInfo::PrintParticles() const {
    const auto particleNames = GetAllParticles();
    std::cout << "Particles:" << std::endl;
    for (const auto& name : particleNames) {
        const auto id = GetParticleID(name);
        std::cout << "\t" << name << " - " << id << std::endl;
    }
}

void TRestGeant4PhysicsInfo::PrintProcesses() const {
    const auto processNames = GetAllProcesses();
    std::cout << "Processes:" << std::endl;
    for (const auto& name : processNames) {
        const auto id = GetProcessID(name);
        std::cout << "\t" << name << " - " << id << std::endl;
    }
}

void TRestGeant4PhysicsInfo::Print() const {
    PrintParticles();
    PrintProcesses();
}
