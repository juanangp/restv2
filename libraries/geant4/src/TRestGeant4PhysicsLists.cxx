#include "TRestGeant4PhysicsLists.h"
#include <iostream>

const std::set<std::string> TRestGeant4PhysicsLists::fValidPhysicsLists = {
    "G4DecayPhysics", "G4RadioactiveDecayPhysics", "G4RadioactiveDecay",
    "G4RadioactiveDecayBase", "G4Radioactivation", "G4EmLivermorePhysics",
    "G4EmPenelopePhysics", "G4EmStandardPhysics_option3", "G4EmStandardPhysics_option4",
    "G4HadronElasticPhysicsHP", "G4IonBinaryCascadePhysics", "G4HadronPhysicsQGSP_BIC_HP",
    "G4NeutronTrackingCut", "G4EmExtraPhysics"
};

// ---------------------------------------------------------------------------
// Self-registration
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestGeant4PhysicsLists", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestGeant4PhysicsLists>(instanceName, params);
        });
    return true;
}();
}  // namespace

static const bool TRestPhysicsListOption_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestPhysicsListOption>("name", &TRestPhysicsListOption::name);
    reg.RegisterField<TRestPhysicsListOption>("value", &TRestPhysicsListOption::value);
    return true;
}();

static const bool TRestPhysicsListItem_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestPhysicsListItem>("name", &TRestPhysicsListItem::name);
    reg.RegisterField<TRestPhysicsListItem>("option", &TRestPhysicsListItem::option);
    return true;
}();

static const bool TRestGeant4PhysicsLists_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    
    reg.RegisterField<TRestGeant4PhysicsLists>("cutForElectron", &TRestGeant4PhysicsLists::fCutForElectron);
    reg.RegisterField<TRestGeant4PhysicsLists>("cutForGamma", &TRestGeant4PhysicsLists::fCutForGamma);
    reg.RegisterField<TRestGeant4PhysicsLists>("cutForPositron", &TRestGeant4PhysicsLists::fCutForPositron);
    reg.RegisterField<TRestGeant4PhysicsLists>("cutForMuon", &TRestGeant4PhysicsLists::fCutForMuon);
    reg.RegisterField<TRestGeant4PhysicsLists>("cutForNeutron", &TRestGeant4PhysicsLists::fCutForNeutron);
    reg.RegisterField<TRestGeant4PhysicsLists>("minEnergyRangeProductionCuts", &TRestGeant4PhysicsLists::fMinEnergyRangeProductionCuts);
    reg.RegisterField<TRestGeant4PhysicsLists>("maxEnergyRangeProductionCuts", &TRestGeant4PhysicsLists::fMaxEnergyRangeProductionCuts);
    reg.RegisterField<TRestGeant4PhysicsLists>("ionLimitStepList", &TRestGeant4PhysicsLists::fIonLimitStepList);

    reg.RegisterField<TRestGeant4PhysicsLists>("physicsList", &TRestGeant4PhysicsLists::fPhysicsLists);
    return true;
}();

TRestGeant4PhysicsLists::TRestGeant4PhysicsLists() : TRestMetadata() { fName = "TRestGeant4PhysicsLists"; }
TRestGeant4PhysicsLists::TRestGeant4PhysicsLists(const std::string& configFilename, const std::string& name) : TRestMetadata(configFilename, name) { LoadConfig(); }
TRestGeant4PhysicsLists::TRestGeant4PhysicsLists(const std::string& instanceName, const YAML::Node& node) : TRestMetadata(instanceName, node) { LoadConfig(); }

void TRestGeant4PhysicsLists::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4PhysicsLists>(fNode);
    ReadYAMLVerbose(fNode);
    UpdateYAMLFromParams<TRestGeant4PhysicsLists>(fNode);
}

int TRestGeant4PhysicsLists::FindPhysicsList(const std::string& physicsListName) const {
    if (!PhysicsListExists(physicsListName)) return -1;
    for (size_t n = 0; n < fPhysicsLists.size(); n++) {
        if (fPhysicsLists[n].name == physicsListName) {
            return static_cast<int>(n);
        }
    }
    return -1;
}

std::string TRestGeant4PhysicsLists::GetPhysicsListOptionValue(const std::string& physicsListName,
                                                               const std::string& option,
                                                               const std::string& defaultValue) const {
    const int index = FindPhysicsList(physicsListName);
    if (index == -1) return defaultValue;

    for (const auto& opt : fPhysicsLists[index].option) {
        if (opt.name == option) {
            return opt.value;
        }
    }
    return defaultValue;
}

bool TRestGeant4PhysicsLists::PhysicsListExists(const std::string& physicsListName) const {
    return fValidPhysicsLists.count(physicsListName) > 0;
}
