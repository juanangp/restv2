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
TRestRun::TRestRun() {
    fName = "TRestRun";
}

TRestRun::TRestRun(const std::string& inputFileName) {
    fName = "TRestRun";
    OpenInputFile(inputFileName);
}
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

    // If not found by GetName(), try to auto-discover the TRestRun metadata instance
    if (!selfConfig || selfConfig.IsNull()) {
        TObjArray* metadataArray = nullptr;
        fInputFile->GetObject("RESTMetadataStore", metadataArray);
        if (metadataArray) {
            for (int i = 0; i < metadataArray->GetEntries(); ++i) {
                TNamed* namedMeta = dynamic_cast<TNamed*>(metadataArray->At(i));
                if (namedMeta && std::string(namedMeta->GetTitle()).find("Class: TRestRun") != std::string::npos) {
                    SetName(namedMeta->GetName()); // Adopt the name from the file
                    selfConfig = GetMetadata(GetName());
                    break;
                }
            }
            delete metadataArray;
        }
    }

    if (selfConfig && !selfConfig.IsNull()) {
        fNode = selfConfig;
        LoadConfig();
    }

    // Auto-discover: any key in the file that is registered in EventRegistry
    TList* keys = fInputFile->GetListOfKeys();
    if (keys) {
        for (int i = 0; i < keys->GetEntries(); ++i) {
            TKey* key = dynamic_cast<TKey*>(keys->At(i));
            if (!key) continue;
            std::string className = key->GetName();
            if (className == "AnalysisTree" || className == "RESTMetadataStore") continue;
            
            if (EventRegistry::Instance().Contains(className)) {
                TTree* tree = nullptr;
                fInputFile->GetObject(className.c_str(), tree);
                if (!tree) {
                    throw std::runtime_error("TRestRun: Tree not found: " + className);
                }
                fInputEventTrees[className] = tree;

                auto eventObj = EventRegistry::Instance().Create(className, className);
                eventObj->Initialize();
                eventObj->SetBranchAddresses(tree);
                
                if (fAnalysisTree && fInputEvents.empty()) {
                    eventObj->TRestEvent::SetBranchAddresses(fAnalysisTree);
                }

                fInputEvents[className] = eventObj.release();
            }
        }
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

TRestEvent& TRestRun::GetInputEvent(const std::string& treeName) {
    auto it = fInputEvents.find(treeName);
    if (it != fInputEvents.end()) {
        return *(it->second);
    }
    throw std::runtime_error("TRestRun: Event tree '" + treeName + "' does not exist");
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
    for (auto& [treeName, eventObj] : fInputEvents) {
        if (eventObj) {
            eventObj->RefreshViews();
        }
    }
    return success;
}

bool TRestRun::HasEvent(const std::string& treeName) const {
    return fInputEvents.find(treeName) != fInputEvents.end();
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
    if (fOutputFile) fOutputFile->cd();
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
