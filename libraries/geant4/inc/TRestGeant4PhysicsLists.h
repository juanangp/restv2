#ifndef TRestGeant4PhysicsLists_H
#define TRestGeant4PhysicsLists_H

#include <set>
#include <string>
#include <vector>

#include "TRestMetadata.h"

/// \class TRestPhysicsListOption
/// \brief An `option` of a physics list: `name` and `value` (a string, e.g. "true").
class TRestPhysicsListOption : public TRestMetadata {
   public:
    std::string name;   ///< `name` of the option (e.g. pixe, ICM, ARM)
    std::string value;  ///< `value` of the option

    /// \brief Empty option.
    TRestPhysicsListOption() : TRestMetadata() { fName = "TRestPhysicsListOption"; }
    /// \brief Option configured from a YAML node.
    TRestPhysicsListOption(const std::string& name, const YAML::Node& node) : TRestMetadata(name, node) {
        LoadConfig();
    }
    /// \brief Reads `name` and `value`.
    void LoadConfig() override { UpdateParamsFromYAML<TRestPhysicsListOption>(fNode); }
    /// \brief No-op.
    void Initialize() override {}
    /// \brief Returns "TRestPhysicsListOption".
    std::string GetClassName() const override { return "TRestPhysicsListOption"; }
};

/// \class TRestPhysicsListItem
/// \brief An entry of `physicsList`: the Geant4 physics list `name` and its `option` list.
class TRestPhysicsListItem : public TRestMetadata {
   public:
    std::string name;                            ///< `name`: Geant4 physics list
    std::vector<TRestPhysicsListOption> option;  ///< `option`: list of TRestPhysicsListOption

    /// \brief Empty entry.
    TRestPhysicsListItem() : TRestMetadata() { fName = "TRestPhysicsListItem"; }
    /// \brief Entry configured from a YAML node.
    TRestPhysicsListItem(const std::string& name, const YAML::Node& node) : TRestMetadata(name, node) {
        LoadConfig();
    }
    /// \brief Reads `name` and `option`.
    void LoadConfig() override { UpdateParamsFromYAML<TRestPhysicsListItem>(fNode); }
    /// \brief No-op.
    void Initialize() override {}
    /// \brief Returns "TRestPhysicsListItem".
    std::string GetClassName() const override { return "TRestPhysicsListItem"; }
};

/// \class TRestGeant4PhysicsLists
/// \brief The Geant4 physics lists and production cuts of a restG4 simulation (restG4 PhysicsList).
///
/// Only the physics lists of fValidPhysicsLists can be used, another name in `physicsList` is a
/// configuration error (LoadConfig throws): G4DecayPhysics, G4RadioactiveDecayPhysics, G4RadioactiveDecay,
/// G4RadioactiveDecayBase, G4Radioactivation, G4EmLivermorePhysics, G4EmPenelopePhysics,
/// G4EmStandardPhysics_option3, G4EmStandardPhysics_option4, G4HadronElasticPhysicsHP,
/// G4IonBinaryCascadePhysics, G4HadronPhysicsQGSP_BIC_HP, G4NeutronTrackingCut and G4EmExtraPhysics.
///
///     physicsG4:
///       class: "TRestGeant4PhysicsLists"
///       cutForGamma: 1 mm               # production cuts (range)
///       cutForElectron: 2 mm
///       cutForPositron: 1 mm
///       cutForMuon: 1 mm
///       cutForNeutron: 1 mm
///       minEnergyRangeProductionCuts: 1 keV
///       maxEnergyRangeProductionCuts: 1 GeV
///       ionLimitStepList: ["F20", "Ne20"]
///       physicsList:
///         - name: "G4EmLivermorePhysics"
///           option:
///             - name: "pixe"
///               value: "true"
///         - name: "G4DecayPhysics"
///         - name: "G4RadioactiveDecayPhysics"
///         - name: "G4HadronElasticPhysicsHP"
///         - name: "G4HadronPhysicsQGSP_BIC_HP"
class TRestGeant4PhysicsLists : public TRestMetadata {
   public:
    /// The physics list names known by restG4
    static const std::set<std::string> fValidPhysicsLists;
    std::vector<TRestPhysicsListItem> fPhysicsLists;  ///< `physicsList`: the physics lists and options

    TRestWithUnits fCutForElectron = 1.0;                ///< `cutForElectron` (mm)
    TRestWithUnits fCutForGamma = 0.01;                  ///< `cutForGamma` (mm)
    TRestWithUnits fCutForPositron = 1.0;                ///< `cutForPositron` (mm)
    TRestWithUnits fCutForMuon = 1.0;                    ///< `cutForMuon` (mm)
    TRestWithUnits fCutForNeutron = 1.0;                 ///< `cutForNeutron` (mm)
    TRestWithUnits fMinEnergyRangeProductionCuts = 1.0;  ///< `minEnergyRangeProductionCuts` (keV)
    TRestWithUnits fMaxEnergyRangeProductionCuts = 1e6;  ///< `maxEnergyRangeProductionCuts` (keV)
    std::vector<std::string> fIonLimitStepList;          ///< `ionLimitStepList`: ions with step limit

   public:
    inline double GetCutForGamma() const { return fCutForGamma.value; }            ///< `cutForGamma` (mm)
    inline double GetCutForElectron() const { return fCutForElectron.value; }      ///< `cutForElectron` (mm)
    inline double GetCutForPositron() const { return fCutForPositron.value; }      ///< `cutForPositron` (mm)
    inline double GetCutForMuon() const { return fCutForMuon.value; }              ///< `cutForMuon` (mm)
    inline double GetCutForNeutron() const { return fCutForNeutron.value; }        ///< `cutForNeutron` (mm)
    std::vector<std::string> GetIonStepList() const { return fIonLimitStepList; }  ///< `ionLimitStepList`
    /// \brief `minEnergyRangeProductionCuts` (keV)
    inline double GetMinimumEnergyProductionCuts() const { return fMinEnergyRangeProductionCuts.value; }
    /// \brief `maxEnergyRangeProductionCuts` (keV)
    inline double GetMaximumEnergyProductionCuts() const { return fMaxEnergyRangeProductionCuts.value; }

    /// \brief Index of a physics list in `physicsList`
    /// \return -1 if it is not listed or not in fValidPhysicsLists.
    int FindPhysicsList(const std::string& physicsListName) const;
    /// \brief True if the name is in fValidPhysicsLists (not whether it is listed).
    bool PhysicsListExists(const std::string& physicsListName) const;
    /// \brief Value of an option of a listed physics list
    /// \return The value, or `defaultValue` if the list or the option is not found.
    std::string GetPhysicsListOptionValue(const std::string& physicsListName, const std::string& option,
                                          const std::string& defaultValue = "NotDefined") const;

    /// \brief Reads the fields from YAML and writes them back. Throws if a name of `physicsList` is not in
    /// fValidPhysicsLists.
    void LoadConfig() override;
    /// \brief No-op.
    void Initialize() override {}
    /// \brief Returns "TRestGeant4PhysicsLists".
    std::string GetClassName() const override { return "TRestGeant4PhysicsLists"; }

    /// \brief Physics lists with the default cuts and no list.
    TRestGeant4PhysicsLists();
    /// \brief Configured from the section `name` of a YAML file.
    TRestGeant4PhysicsLists(const std::string& configFilename, const std::string& name = "");
    /// \brief Configured from a YAML node.
    TRestGeant4PhysicsLists(const std::string& instanceName, const YAML::Node& node);
    virtual ~TRestGeant4PhysicsLists() = default;
};

#endif  // TRestGeant4PhysicsLists_H
