#include "TRestGeant4PhysicsLists.h"

#include <TRestTools.h>

#include <set>

using namespace std;

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

static const bool TRestGeant4PhysicsLists_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();

    reg.RegisterField<TRestGeant4PhysicsLists>("cutForElectron", &TRestGeant4PhysicsLists::fCutForElectron);
    reg.RegisterField<TRestGeant4PhysicsLists>("cutForGamma", &TRestGeant4PhysicsLists::fCutForGamma);
    reg.RegisterField<TRestGeant4PhysicsLists>("cutForPositron", &TRestGeant4PhysicsLists::fCutForPositron);
    reg.RegisterField<TRestGeant4PhysicsLists>("cutForMuon", &TRestGeant4PhysicsLists::fCutForMuon);
    reg.RegisterField<TRestGeant4PhysicsLists>("cutForNeutron", &TRestGeant4PhysicsLists::fCutForNeutron);
    reg.RegisterField<TRestGeant4PhysicsLists>("minEnergyRangeProductionCuts",
                                               &TRestGeant4PhysicsLists::fMinEnergyRangeProductionCuts);
    reg.RegisterField<TRestGeant4PhysicsLists>("maxEnergyRangeProductionCuts",
                                               &TRestGeant4PhysicsLists::fMaxEnergyRangeProductionCuts);
    reg.RegisterField<TRestGeant4PhysicsLists>("ionLimitStepList",
                                               &TRestGeant4PhysicsLists::fIonLimitStepList);

    return true;
}();

TRestGeant4PhysicsLists::TRestGeant4PhysicsLists() : TRestMetadata() { fName = "TRestGeant4PhysicsLists"; }

TRestGeant4PhysicsLists::TRestGeant4PhysicsLists(const char* configFilename, const std::string& name)
    : TRestMetadata(configFilename, name) {
    LoadConfig();
}

TRestGeant4PhysicsLists::TRestGeant4PhysicsLists(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestGeant4PhysicsLists::~TRestGeant4PhysicsLists() = default;

void TRestGeant4PhysicsLists::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4PhysicsLists>(fNode);

    fPhysicsLists.clear();
    fPhysicsListOptions.clear();

    if (fNode["physicsList"]) {
        YAML::Node pListNode = fNode["physicsList"];
        std::vector<YAML::Node> listElements;

        if (pListNode.IsSequence()) {
            for (const auto& item : pListNode) listElements.push_back(item);
        } else if (pListNode.IsMap()) {
            listElements.push_back(pListNode);
        }

        for (const auto& item : listElements) {
            if (!item["name"]) continue;
            std::string physicsListName = item["name"].as<std::string>();

            if (!PhysicsListExists(physicsListName)) {
                cerr << "TRestPhysicsList: Physics list: '" << physicsListName
                     << "' not found among valid options" << endl;
                exit(1);
            }

            std::string optionString = "";
            if (item["option"]) {
                YAML::Node optNode = item["option"];
                std::vector<YAML::Node> optElements;

                if (optNode.IsSequence()) {
                    for (const auto& o : optNode) optElements.push_back(o);
                } else if (optNode.IsMap()) {
                    optElements.push_back(optNode);
                }

                for (const auto& opt : optElements) {
                    if (opt["name"] && opt["value"]) {
                        std::string optName = opt["name"].as<std::string>();
                        std::string optValue = opt["value"].as<std::string>();
                        if (!optionString.empty()) optionString += ":";
                        optionString += optName + ":" + optValue;
                    }
                }
            }

            fPhysicsLists.push_back(physicsListName);
            fPhysicsListOptions.push_back(optionString);
        }
    }

    UpdateYAMLFromParams<TRestGeant4PhysicsLists>(fNode);
}

int TRestGeant4PhysicsLists::FindPhysicsList(const std::string& physicsListName) const {
    if (!PhysicsListExists(physicsListName)) return -1;

    for (unsigned int n = 0; n < fPhysicsLists.size(); n++) {
        if (fPhysicsLists[n] == physicsListName) {
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

    vector<string> optList = TRestTools::Split(fPhysicsListOptions[index], ':');
    for (unsigned int n = 0; n < optList.size(); n = n + 2) {
        if (optList[n] == option) {
            return optList[n + 1];
        }
    }
    return defaultValue;
}

bool TRestGeant4PhysicsLists::PhysicsListExists(const std::string& physicsListName) const {
    const set<std::string> validPhysicsLists = {"G4DecayPhysics",
                                                "G4RadioactiveDecayPhysics",
                                                "G4RadioactiveDecay",
                                                "G4RadioactiveDecayBase",
                                                "G4Radioactivation",
                                                "G4EmLivermorePhysics",
                                                "G4EmPenelopePhysics",
                                                "G4EmStandardPhysics_option3",
                                                "G4EmStandardPhysics_option4",
                                                "G4HadronElasticPhysicsHP",
                                                "G4IonBinaryCascadePhysics",
                                                "G4HadronPhysicsQGSP_BIC_HP",
                                                "G4NeutronTrackingCut",
                                                "G4EmExtraPhysics"};

    return validPhysicsLists.count(physicsListName) > 0;
}

