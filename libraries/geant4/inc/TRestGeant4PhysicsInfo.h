#pragma once

#include <string>
#include <map>
#include <set>
#include <vector>

class G4VProcess;

/// \class TRestGeant4PhysicsInfo
/// \brief Class to store and map Geant4 process names, particle names, and process types to unique integer identifiers.
class TRestGeant4PhysicsInfo {
   public:
    std::map<int, std::string> fProcessNamesMap;
    std::map<std::string, int> fProcessNamesReverseMap;

    std::map<int, std::string> fParticleNamesMap;
    std::map<std::string, int> fParticleNamesReverseMap;

    std::map<std::string, std::string> fProcessTypesMap; // Process name -> Process type

    TRestGeant4PhysicsInfo() = default;
    virtual ~TRestGeant4PhysicsInfo() = default;

    // --- Process Mapping Definitions ---
    std::string GetProcessName(int id) const;
    int GetProcessID(const std::string& processName) const;
    void InsertProcessName(int id, const std::string& processName, const std::string& processType);
    std::set<std::string> GetAllProcesses() const;

    // --- Particle Mapping Definitions ---
    std::string GetParticleName(int id) const;
    int GetParticleID(const std::string& particleName) const;
    void InsertParticleName(int id, const std::string& particleName);
    std::set<std::string> GetAllParticles() const;

    // --- Process Type Utilities ---
    std::string GetProcessType(const std::string& processName) const;
    std::set<std::string> GetAllProcessTypes() const;

    // --- Informational Logging Reporting ---
    void Print() const;
    void PrintProcesses() const;
    void PrintParticles() const;

    /// \brief Translates a Geant4 pointer structure into a unique local framework ID.
    /// \note Implemented inside the restG4 package pipeline.
    static int GetProcessIDFromGeant4Process(const G4VProcess* process);

    // Grants reflection registration macro privileges
    friend class TRestMetadataFieldRegistry;
};

