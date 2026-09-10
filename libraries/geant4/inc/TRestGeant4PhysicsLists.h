#ifndef TRestGeant4PhysicsLists_H
#define TRestGeant4PhysicsLists_H

#include <string>
#include <vector>
#include <set>
#include "TRestMetadata.h"

class TRestPhysicsListOption : public TRestMetadata {
   public:
    std::string name;
    std::string value;

    TRestPhysicsListOption() : TRestMetadata() { fName = "TRestPhysicsListOption"; }
    TRestPhysicsListOption(const std::string& name, const YAML::Node& node) : TRestMetadata(name, node) { LoadConfig(); }
    void LoadConfig() override { UpdateParamsFromYAML<TRestPhysicsListOption>(fNode); }
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestPhysicsListOption"; }
};

class TRestPhysicsListItem : public TRestMetadata {
   public:
    std::string name;
    std::vector<TRestPhysicsListOption> option;

    TRestPhysicsListItem() : TRestMetadata() { fName = "TRestPhysicsListItem"; }
    TRestPhysicsListItem(const std::string& name, const YAML::Node& node) : TRestMetadata(name, node) { LoadConfig(); }
    void LoadConfig() override { UpdateParamsFromYAML<TRestPhysicsListItem>(fNode); }
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestPhysicsListItem"; }
};

class TRestGeant4PhysicsLists : public TRestMetadata {
   public:
    static const std::set<std::string> fValidPhysicsLists;
    std::vector<TRestPhysicsListItem> fPhysicsLists;

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

    int FindPhysicsList(const std::string& physicsListName) const;
    bool PhysicsListExists(const std::string& physicsListName) const;
    std::string GetPhysicsListOptionValue(const std::string& physicsListName, const std::string& option,
                                          const std::string& defaultValue = "NotDefined") const;

    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestGeant4PhysicsLists"; }

    TRestGeant4PhysicsLists();
    TRestGeant4PhysicsLists(const std::string& configFilename, const std::string& name = "");
    TRestGeant4PhysicsLists(const std::string& instanceName, const YAML::Node& node);
    virtual ~TRestGeant4PhysicsLists() = default;
};

#endif // TRestGeant4PhysicsLists_H
