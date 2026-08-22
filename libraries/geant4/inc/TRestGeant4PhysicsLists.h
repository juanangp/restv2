#pragma once

#include <TRestMetadata.h>

#include <iostream>
#include <string>
#include <vector>

/// \class TRestGeant4PhysicsLists
/// \brief Metadata component managing physics list configurations and region cuts via modern REST v3 YAML
/// reflection.
class TRestGeant4PhysicsLists : public TRestMetadata {
   public:
    std::vector<std::string> fPhysicsLists;
    std::vector<std::string> fPhysicsListOptions;

    TRestWithUnits fCutForElectron = 1.0;
    TRestWithUnits fCutForGamma = 0.01;
    TRestWithUnits fCutForPositron = 1.0;
    TRestWithUnits fCutForMuon = 1.0;
    TRestWithUnits fCutForNeutron = 1.0;
    TRestWithUnits fMinEnergyRangeProductionCuts = 1.0;
    TRestWithUnits fMaxEnergyRangeProductionCuts = 1e6;

    std::vector<std::string> fIonLimitStepList;

   public:
    inline double GetCutForGamma() const { return fCutForGamma.value; }
    inline double GetCutForElectron() const { return fCutForElectron.value; }
    inline double GetCutForPositron() const { return fCutForPositron.value; }
    inline double GetCutForMuon() const { return fCutForMuon.value; }
    inline double GetCutForNeutron() const { return fCutForNeutron.value; }

    std::vector<std::string> GetIonStepList() const { return fIonLimitStepList; }

    inline double GetMinimumEnergyProductionCuts() const { return fMinEnergyRangeProductionCuts.value; }
    inline double GetMaximumEnergyProductionCuts() const { return fMaxEnergyRangeProductionCuts.value; }

    /// \brief Returns index of a physics list by name, or -1 when not found.
    int FindPhysicsList(const std::string& physicsListName) const;
    /// \brief Returns true if a given physics list name is configured.
    bool PhysicsListExists(const std::string& physicsListName) const;

    /// \brief Retrieves one option value associated with a physics list (or defaultValue).
    std::string GetPhysicsListOptionValue(const std::string& physicsListName, const std::string& option,
                                          const std::string& defaultValue = "NotDefined") const;

    /// \brief Loads physics lists, options and production cuts from YAML.
    void LoadConfig() override;
    /// \brief No-op hook kept for REST metadata lifecycle compatibility.
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestGeant4PhysicsLists"; }

    // Constructors & Destructors
    /// \brief Constructs an empty physics-list metadata container.
    TRestGeant4PhysicsLists();
    /// \brief Loads physics list metadata from an RML/YAML configuration file section.
    TRestGeant4PhysicsLists(const char* configFilename, const std::string& name = "");
    /// \brief Builds metadata directly from REST v3 YAML node content.
    TRestGeant4PhysicsLists(const std::string& instanceName, const YAML::Node& node);
    /// \brief Destructor.
    virtual ~TRestGeant4PhysicsLists();
};
