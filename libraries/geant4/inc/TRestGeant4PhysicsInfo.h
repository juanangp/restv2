#ifndef TRESTGEANT4PHYSICSINFO_H
#define TRESTGEANT4PHYSICSINFO_H

#include <map>
#include <set>
#include <string>

class G4VProcess;

/// \class TRestGeant4PhysicsInfo
/// \brief Class to store and map Geant4 process names, particle names, and process types to unique integer
/// identifiers.
///
/// Part of TRestGeant4Metadata, filled by restG4 while simulating (thread safe insertions): the particle
/// IDs are the PDG encodings, the process IDs Geant4 process type * 1000 + subtype (processes sharing
/// type and subtype, e.g. eIoni and ionIoni, share the ID and the first name inserted is kept).
///
/// \author Luis Obis <luis.antonio.obis@gmail.com>
/// \author JuanAn García <juanangp@unizar.es>
class TRestGeant4PhysicsInfo {
   public:
    std::map<int, std::string> fProcessNamesMap;         ///< Process ID -> process name
    std::map<std::string, int> fProcessNamesReverseMap;  ///< Process name -> process ID

    std::map<int, std::string> fParticleNamesMap;         ///< Particle ID (PDG encoding) -> particle name
    std::map<std::string, int> fParticleNamesReverseMap;  ///< Particle name -> particle ID

    std::map<std::string, std::string> fProcessTypesMap;  ///< Process name -> Process type

    /// Process IDs of REST (Geant4 IDs are type * 1000 + subtype, 0 the initial step "Init"): a hit killed in
    /// a kill volume and the creator of the primary tracks
    static constexpr int kKillProcessID = 1000000;
    /// Process ID of the creator of the primary tracks
    static constexpr int kPrimaryGeneratorProcessID = 1000001;

    TRestGeant4PhysicsInfo() = default;
    virtual ~TRestGeant4PhysicsInfo() = default;

    // --- Process Mapping Definitions ---
    /// \brief Returns process name associated with a process id ("" if not found).
    std::string GetProcessName(int id) const;
    /// \brief Returns process id associated with a process name (-1 if not found).
    int GetProcessID(const std::string& processName) const;
    /// \brief Registers one process id/name pair and its process type tag (nothing if the ID exists).
    void InsertProcessName(int id, const std::string& processName, const std::string& processType);
    /// \brief Returns the set of all registered process names.
    std::set<std::string> GetAllProcesses() const;

    // --- Particle Mapping Definitions ---
    /// \brief Returns particle name associated with a particle id ("" if not found).
    std::string GetParticleName(int id) const;
    /// \brief Returns particle id associated with a particle name (-1 if not found).
    int GetParticleID(const std::string& particleName) const;
    /// \brief Registers one particle id/name pair (nothing if the ID exists).
    void InsertParticleName(int id, const std::string& particleName);
    /// \brief Returns the set of all registered particle names.
    std::set<std::string> GetAllParticles() const;

    // --- Process Type Utilities ---
    /// \brief Returns the process type string associated with a process name ("" if not found).
    std::string GetProcessType(const std::string& processName) const;
    /// \brief Returns the set of all registered process type tags.
    std::set<std::string> GetAllProcessTypes() const;

    // --- Informational Logging Reporting ---
    /// \brief Prints the particles and the processes with their IDs.
    void Print() const;
    /// \brief Prints process id-name mappings.
    void PrintProcesses() const;
    /// \brief Prints particle id-name mappings.
    void PrintParticles() const;

    /// \brief Translates a Geant4 pointer structure into a unique local framework ID: process type * 1000 +
    /// subtype (0 for a null process).
    /// \note Implemented inside the restG4 package pipeline.
    static int GetProcessIDFromGeant4Process(const G4VProcess* process);

    // Grants reflection registration macro privileges
    friend class TRestMetadataFieldRegistry;
    friend class TRestGeant4Metadata;
};

#endif
