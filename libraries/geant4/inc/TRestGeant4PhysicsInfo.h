#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

class G4VProcess;

/// \class TRestGeant4PhysicsInfo
/// \brief Class to store and map Geant4 process names, particle names, and process types to unique integer
/// identifiers.
class TRestGeant4PhysicsInfo {
   public:
    std::map<int, std::string> fProcessNamesMap;
    std::map<std::string, int> fProcessNamesReverseMap;

    std::map<int, std::string> fParticleNamesMap;
    std::map<std::string, int> fParticleNamesReverseMap;

    std::map<std::string, std::string> fProcessTypesMap;  // Process name -> Process type

    TRestGeant4PhysicsInfo() = default;
    virtual ~TRestGeant4PhysicsInfo() = default;

    // --- Process Mapping Definitions ---
    /// \brief Returns process name associated with a process id.
    std::string GetProcessName(int id) const;
    /// \brief Returns process id associated with a process name.
    int GetProcessID(const std::string& processName) const;
    /// \brief Registers one process id/name pair and its process type tag.
    void InsertProcessName(int id, const std::string& processName, const std::string& processType);
    /// \brief Returns the set of all registered process names.
    std::set<std::string> GetAllProcesses() const;

    // --- Particle Mapping Definitions ---
    /// \brief Returns particle name associated with a particle id.
    std::string GetParticleName(int id) const;
    /// \brief Returns particle id associated with a particle name.
    int GetParticleID(const std::string& particleName) const;
    /// \brief Registers one particle id/name pair.
    void InsertParticleName(int id, const std::string& particleName);
    /// \brief Returns the set of all registered particle names.
    std::set<std::string> GetAllParticles() const;

    // --- Process Type Utilities ---
    /// \brief Returns the process type string associated with a process name.
    std::string GetProcessType(const std::string& processName) const;
    /// \brief Returns the set of all registered process type tags.
    std::set<std::string> GetAllProcessTypes() const;

    // --- Informational Logging Reporting ---
    /// \brief Prints a complete process/particle/type registry summary.
    void Print() const;
    /// \brief Prints process id-name mappings.
    void PrintProcesses() const;
    /// \brief Prints particle id-name mappings.
    void PrintParticles() const;

    /// \brief Translates a Geant4 pointer structure into a unique local framework ID.
    /// \note Implemented inside the restG4 package pipeline.
    static int GetProcessIDFromGeant4Process(const G4VProcess* process);

    // Grants reflection registration macro privileges
    friend class TRestMetadataFieldRegistry;
};
