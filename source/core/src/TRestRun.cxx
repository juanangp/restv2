#include "TRestRun.h"

#include <stdexcept>

#include "TObjString.h"
#include "TRestTools.h"
#include <TKey.h>
#include <TList.h>

using namespace TRestTools;

// ---------------------------------------------------------------------------
// Self-registration (no macro)
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    MetadataRegistry::Instance().Register(
        "TRestRun",
        [](const std::string& instanceName, const std::string& sectionName, const YAML::Node& params) {
            return std::make_unique<TRestRun>(instanceName, sectionName, params);
        });
    return true;
}();
}  // namespace

// ---------------------------------------------------------------------------
TRestRun::TRestRun(const std::string& instanceName, const std::string& sectionName, const YAML::Node& node)
    : TRestMetadata(instanceName, sectionName, node) {
    LoadConfig();
}

TRestRun::TRestRun(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    YAML::Node raw = YAML::LoadFile(fileName);
    YAML::Node cfg = ResolveAllRefs(raw);
    fNode = cfg[sectionName]["params"];
    LoadConfig();
}

TRestRun::~TRestRun() {
    CloseFiles();

    for (auto& pair : fInputEvents) {
        delete pair.second;
    }
    // Output event objects are owned by TRestManager (or the caller), not TRestRun.
}

// ---------------------------------------------------------------------------
void TRestRun::LoadConfig() {
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestRun::LoadConfig – node is missing or null" << RESTendl;
        return;
    }

    if (fNode["runNumber"]) {
        if (ReadYAMLParam<std::string>(fNode["runNumber"]) == "auto") {
            // TODO: implement automatic run numbering
        } else {
            fRunNumber = ReadYAMLParam<int>(fNode["runNumber"]);
        }
    }

    fParentRunNumber = ReadYAMLParamOrDefault<int>(fNode, "parentRunNumber", fParentRunNumber);
    fRunType = ReadYAMLParamOrDefault<std::string>(fNode, "runType", fRunType);
    fRunUser = ReadYAMLParamOrDefault<std::string>(fNode, "runUser", fRunUser);
    fRunTag = ReadYAMLParamOrDefault<std::string>(fNode, "runTag", fRunTag);
    fRunDescription = ReadYAMLParamOrDefault<std::string>(fNode, "runDescription", fRunDescription);
    fExperimentName = ReadYAMLParamOrDefault<std::string>(fNode, "experimentName", fExperimentName);
    fInputFileName = ReadYAMLParamOrDefault<std::string>(fNode, "inputFileName", fInputFileName);
    fOutputFileName = ReadYAMLParamOrDefault<std::string>(fNode, "outputFileName", fOutputFileName);
    fEntriesSaved = ReadYAMLParamOrDefault<int>(fNode, "entriesSaved", fEntriesSaved);

    TRestMetadata::ReadYAMLVerbose(fNode);
}

void TRestRun::OpenInputFile(const std::string& filename) {
    fInputFileName = filename;
    fInputFile = std::make_unique<TFile>(fInputFileName.c_str(), "READ");
    if (!fInputFile || fInputFile->IsZombie()) {
        throw std::runtime_error("TRestRun: Cannot open file " + filename);
    }

    fInputFile->GetObject("AnalysisTree", fAnalysisTree);

    YAML::Node selfConfig = GetMetadata(GetName());
    if (selfConfig && !selfConfig.IsNull()) {
        fNode = selfConfig;
        LoadConfig();

        if (selfConfig["eventBranches"]) {
            for (auto it = selfConfig["eventBranches"].begin(); it != selfConfig["eventBranches"].end(); ++it) {
                fEventBranches[it->first.as<std::string>()] = it->second.as<std::string>();
            }
        }
    }

    if (fEventBranches.empty()) {
        TList* keys = fInputFile->GetListOfKeys();
        if (keys) {
            for (int i = 0; i < keys->GetEntries(); ++i) {
                TKey* key = dynamic_cast<TKey*>(keys->At(i));
                if (!key) continue;
                std::string keyName = key->GetName();
                if (keyName == "AnalysisTree" || keyName == "RESTMetadataStore") continue;
                if (keyName.size() > 4 && keyName.substr(keyName.size() - 4) == "Tree") {
                    std::string className = keyName.substr(0, keyName.size() - 4);
                    if (EventRegistry::Instance().Contains(className)) {
                        fEventBranches[className] = className;
                    }
                }
            }
        }
    }

    for (const auto& [branchName, className] : fEventBranches) {
        if (!EventRegistry::Instance().Contains(className)) {
            throw std::runtime_error("TRestRun: EventRegistry does not contain class: " + className);
        }
        std::string treeName = className + "Tree";
        TTree* tree = nullptr;
        fInputFile->GetObject(treeName.c_str(), tree);
        if (!tree) {
            throw std::runtime_error("TRestRun: Tree not found: " + treeName);
        }
        fInputEventTrees[treeName] = tree;

        auto eventObj = EventRegistry::Instance().Create(className, branchName);
        eventObj->Initialize();
        eventObj->SetBranchAddresses(tree);
        fInputEvents[branchName] = eventObj.release();
    }
}

void TRestRun::AddMetadata(const std::string& instanceName, const std::string& className,
                           const YAML::Node& configNode) {
    if (!fOutputFile) throw std::runtime_error("TRestRun::AddMetadata - Output file not found.");
    if (!configNode || configNode.IsNull()) return;

    fOutputFile->cd();
    TObjArray* metadataArray = nullptr;
    fOutputFile->GetObject("RESTMetadataStore", metadataArray);
    if (!metadataArray) {
        TKey* key = fOutputFile->GetKey("RESTMetadataStore");
        if (key) {
            fOutputFile->Delete("RESTMetadataStore;*");
        }
        metadataArray = new TObjArray();
        metadataArray->SetName("RESTMetadataStore");
        metadataArray->SetOwner(kTRUE);
    }

    std::string yamlDump = YAML::Dump(configNode);
    auto* namedMeta = new TNamed(instanceName.c_str(), "");

    std::string classAndYaml = "Class: " + className + "\n---\n" + yamlDump;
    namedMeta->SetTitle(classAndYaml.c_str());

    TObject* old = metadataArray->FindObject(instanceName.c_str());
    if (old) {
        metadataArray->Remove(old);
        delete old;
    }

    metadataArray->Add(namedMeta);
    metadataArray->Write("RESTMetadataStore", TObject::kSingleKey | TObject::kOverwrite);
    
    delete metadataArray;
}

YAML::Node TRestRun::GetMetadata(const std::string& instanceName) const {
    if (!fInputFile) return YAML::Node();

    TObjArray* metadataArray = nullptr;
    fInputFile->GetObject("RESTMetadataStore", metadataArray);
    if (!metadataArray) return YAML::Node();

    TObject* obj = metadataArray->FindObject(instanceName.c_str());
    if (!obj) {
        delete metadataArray;
        return YAML::Node();
    }

    auto* namedMeta = dynamic_cast<TNamed*>(obj);
    if (!namedMeta) {
        delete metadataArray;
        return YAML::Node();
    }

    std::string fullText = namedMeta->GetTitle();
    size_t separator = fullText.find("---\n");
    YAML::Node node;
    if (separator == std::string::npos) {
        node = YAML::Load(fullText);
    } else {
        std::string yamlStr = fullText.substr(separator + 4);
        node = YAML::Load(yamlStr);
    }

    delete metadataArray;
    return node;
}

TRestEvent& TRestRun::GetInputEvent(const std::string& branchName) {
    auto it = fInputEvents.find(branchName);
    if (it != fInputEvents.end()) {
        return *(it->second);
    }
    throw std::runtime_error("TRestRun: Event branch '" + branchName + "' does not exist");
}

bool TRestRun::GetEntry(Long64_t entry) {
    if (fInputEventTrees.empty()) return false;
    bool success = false;
    for (auto& [treeName, tree] : fInputEventTrees) {
        if (tree) {
            if (tree->GetEntry(entry) > 0) {
                success = true;
            }
        }
    }
    for (auto& [branchName, eventObj] : fInputEvents) {
        if (eventObj) {
            eventObj->RefreshViews();
        }
    }
    return success;
}

bool TRestRun::HasEvent(const std::string& branchName) const {
    return fInputEvents.find(branchName) != fInputEvents.end();
}

void TRestRun::OpenOutputFile() {
    fOutputFile = std::make_unique<TFile>(fOutputFileName.c_str(), "RECREATE");
    if (!fOutputFile || fOutputFile->IsZombie()) {
        throw std::runtime_error("TRestRun: Cannot open output file.");
    }

    fOutputFile->cd();
    fAnalysisTree = new TTree("AnalysisTree", "REST Generic Analysis Observables");
    fAnalysisTree->SetAutoSave(0);
}

void TRestRun::Fill() {
    //for (auto& [name, eventObj] : fOutputEvents) {
    //    if (eventObj) {
    //        eventObj->PrepareForWriting();
    //    }
    //}
    for (auto& [treeName, tree] : fOutputEventTrees) {
        if (tree) tree->Fill();
    }
    if (fAnalysisTree) fAnalysisTree->Fill();
    ++fEntriesSaved;
}

void TRestRun::CloseFiles() {
    if (fOutputFile) {
        fOutputFile->cd();
        for (auto& [treeName, tree] : fOutputEventTrees) {
            if (tree) tree->Write("", TObject::kOverwrite);
        }
        if (fAnalysisTree) fAnalysisTree->Write("", TObject::kOverwrite);

        if (fNode && !fNode.IsNull()) {
            YAML::Node branchesNode;
            for (const auto& [name, cls] : fEventBranches) {
                branchesNode[name] = cls;
            }
            fNode["eventBranches"] = branchesNode;

            AddMetadata(GetName(), GetClassName(), fNode);
        }

        fOutputFile->Close();
        fOutputFile.reset();
        fOutputEventTrees.clear();
        fAnalysisTree = nullptr;
    }
    if (fInputFile) {
        fInputFile->Close();
        fInputFile.reset();
        fInputEventTrees.clear();
        fAnalysisTree = nullptr;
    }
}

// ---------------------------------------------------------------------------
void TRestRun::PrintMetadata() {
    RESTMetadata << "=== TRestRun ===" << RESTendl;
    RESTMetadata << fNode << RESTendl;
}
