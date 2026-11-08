#ifndef TRESTGEANT4MANAGER_H
#define TRESTGEANT4MANAGER_H

#include "TRestMetadata.h"

class TRestGeant4Metadata;
class TRestGeant4PhysicsLists;
class TRestRun;

/// \class TRestGeant4Manager
/// \brief Top-level orchestrator that loads configured metadata classes.
///
/// `TRestGeant4Manager` parses configuration and instantiates metadata objects. It is the top section of
/// a restG4 configuration: each child section with a registered `class` is created, and the TRestRun,
/// the TRestGeant4Metadata and the TRestGeant4PhysicsLists are kept (other classes are ignored).
///
///     g4Manager:
///       class: "TRestGeant4Manager"
///       run:
///         class: "TRestRun"
///         runTag: "NLDBD"
///         outputFileName: "Run[runNumber]_[runTag]_[experimentName].root"
///       geant4Metadata:
///         class: "TRestGeant4Metadata"     # see TRestGeant4Metadata
///         gdmlFilename: "geometry/setup.gdml"
///         ...
///       physicsG4:
///         class: "TRestGeant4PhysicsLists" # see TRestGeant4PhysicsLists
///         ...
class TRestGeant4Manager : public TRestMetadata {
   public:
    /// \brief Pointer to Geant4 run metadata loaded from configuration.
    TRestGeant4Metadata* fG4Metadata = nullptr;
    /// \brief Pointer to configured Geant4 physics list collection.
    TRestGeant4PhysicsLists* fG4PhysicsLists = nullptr;

    /// \brief Manager configured from a YAML node.
    TRestGeant4Manager(const std::string& instanceName, const YAML::Node& node);
    /// \brief Manager configured from the section `sectionName` of a YAML file.
    TRestGeant4Manager(const std::string& fileName, const std::string& sectionName);
    ~TRestGeant4Manager() override = default;

    /// \brief Returns "TRestGeant4Manager".
    std::string GetClassName() const override { return "TRestGeant4Manager"; }

    /// \brief Resolves linked metadata objects from the YAML section: creates the child sections and keeps
    /// the run (fRunInfo), the Geant4 metadata and the physics lists.
    void LoadConfig() override;
    /// \brief No-op for this manager: initialization is done during config loading.
    void Initialize() override {}
    /// \brief Prints discovered Geant4 metadata and physics list associations.
    void PrintMetadata() const override;
    /// \brief Writes the metadata in the output file of the run: the run itself, the physics lists and the
    /// Geant4 metadata (with its ID maps synchronized from the simulation), the ones configured. Throws if
    /// there is no run.
    void SaveMetadata();
};

#endif
