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

    Double_t fCutForElectron = 1.0;
    Double_t fCutForGamma = 0.01;
    Double_t fCutForPositron = 1.0;
    Double_t fCutForMuon = 1.0;
    Double_t fCutForNeutron = 1.0;
    Double_t fMinEnergyRangeProductionCuts = 1.0;
    Double_t fMaxEnergyRangeProductionCuts = 1e6;

    std::vector<std::string> fIonLimitStepList;

   public:
    inline Double_t GetCutForGamma() const { return fCutForGamma; }
    inline Double_t GetCutForElectron() const { return fCutForElectron; }
    inline Double_t GetCutForPositron() const { return fCutForPositron; }
    inline Double_t GetCutForMuon() const { return fCutForMuon; }
    inline Double_t GetCutForNeutron() const { return fCutForNeutron; }

    std::vector<std::string> GetIonStepList() const { return fIonLimitStepList; }

    inline Double_t GetMinimumEnergyProductionCuts() const { return fMinEnergyRangeProductionCuts; }
    inline Double_t GetMaximumEnergyProductionCuts() const { return fMaxEnergyRangeProductionCuts; }

    /// \brief Returns index of a physics list by name, or -1 when not found.
    Int_t FindPhysicsList(const std::string& physicsListName) const;
    /// \brief Returns true if a given physics list name is configured.
    Bool_t PhysicsListExists(const std::string& physicsListName) const;

    /// \brief Retrieves one option value associated with a physics list (or defaultValue).
    std::string GetPhysicsListOptionValue(const std::string& physicsListName, const std::string& option,
                                          const std::string& defaultValue = "NotDefined") const;

    /// \brief Loads physics lists, options and production cuts from YAML.
    void LoadConfig() override;
    /// \brief No-op hook kept for REST metadata lifecycle compatibility.
    void Initialize() override {}
    /// \brief Prints configured physics lists and cut settings.
    void PrintMetadata() override;

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
