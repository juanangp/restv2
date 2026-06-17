#include "TRestRun.h"
#include "TRestTools.h"

#include <stdexcept>

#include "TObjString.h"

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
} // namespace

// ---------------------------------------------------------------------------
TRestRun::TRestRun(const std::string& instanceName, const std::string& sectionName, const YAML::Node& node)
    : TRestMetadata(instanceName, sectionName, node) {
    LoadConfig();
}

TRestRun::TRestRun(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    YAML::Node raw = YAML::LoadFile(fileName);
    YAML::Node cfg = ResolveAllRefs(raw);
    fNode          = cfg[sectionName]["params"];
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

    fParentRunNumber = ReadYAMLParamOrDefault<int>(fNode,         "parentRunNumber", fParentRunNumber);
    fRunType         = ReadYAMLParamOrDefault<std::string>(fNode, "runType",         fRunType);
    fRunUser         = ReadYAMLParamOrDefault<std::string>(fNode, "runUser",         fRunUser);
    fRunTag          = ReadYAMLParamOrDefault<std::string>(fNode, "runTag",          fRunTag);
    fRunDescription  = ReadYAMLParamOrDefault<std::string>(fNode, "runDescription",  fRunDescription);
    fExperimentName  = ReadYAMLParamOrDefault<std::string>(fNode, "experimentName",  fExperimentName);
    fInputFileName   = ReadYAMLParamOrDefault<std::string>(fNode, "inputFileName",   fInputFileName);
    fOutputFileName  = ReadYAMLParamOrDefault<std::string>(fNode, "outputFileName",  fOutputFileName);
    fEntriesSaved    = ReadYAMLParamOrDefault<int>(fNode,         "entriesSaved",    fEntriesSaved);

    TRestMetadata::ReadYAMLVerbose(fNode);
}

void TRestRun::OpenInputFile(const std::string& filename) {
    fInputFileName = filename;
    fInputFile = std::make_unique<TFile>(fInputFileName.c_str(), "READ");
    if (!fInputFile || fInputFile->IsZombie()) {
        throw std::runtime_error("TRestRun: Cannot open file" + filename);
    }

    fInputFile->GetObject("AnalysisTree", fAnalysisTree);
    if (!fInputEventTree) return;

    fInputFile->GetObject("EventTree", fInputEventTree);
    if (!fInputEventTree) return;

    TObjArray* branches = fInputEventTree->GetListOfBranches();
    for (int i = 0; i < branches->GetEntries(); ++i) {
        auto* branch = dynamic_cast<TBranch*>(branches->At(i));
        if (!branch) continue;

        std::string branchName = branch->GetName();
        std::string className  = branch->GetClassName();
        if (className.empty()) continue;

        TClass* cl = TClass::GetClass(className.c_str());
        if (!cl) continue;

        auto* obj = static_cast<TRestEvent*>(cl->New());
        
        fInputEvents[branchName] = obj;
        
        fInputEventTree->SetBranchAddress(branchName.c_str(), &fInputEvents[branchName]);
    }

    TObjArray* metadataArray = nullptr;
    fInputFile->GetObject("RESTMetadataStore", metadataArray);

    YAML::Node selfConfig = GetMetadata(GetName()); 
    if (selfConfig && !selfConfig.IsNull()) {
        fNode = selfConfig;
        LoadConfig();
    }

}

void TRestRun::AddMetadata(const std::string& instanceName, const std::string& className, const YAML::Node& configNode) {
    if (!fOutputFile) throw std::runtime_error("TRestRun::AddMetadata - Output file not found.");
    if (!configNode || configNode.IsNull()) return;

    fOutputFile->cd();
    TObjArray* metadataArray = nullptr;
    fOutputFile->GetObject("RESTMetadataStore", metadataArray);
    if (!metadataArray) {
        metadataArray = new TObjArray();
        metadataArray->SetOwner(kTRUE);
    }

    std::string yamlDump = YAML::Dump(configNode);
    auto* namedMeta = new TNamed(instanceName.c_str(), "");
    
    std::string classAndYaml = "Class: " + className + "\n---\n" + yamlDump;
    namedMeta->SetTitle(classAndYaml.c_str());

    TObject* old = metadataArray->FindObject(instanceName.c_str());
    if (old) metadataArray->Remove(old);

    metadataArray->Add(namedMeta);
    metadataArray->Write("RESTMetadataStore", TObject::kOverwrite);
}

YAML::Node TRestRun::GetMetadata(const std::string& instanceName) const {
    if (!fInputFile) return YAML::Node();

    TObjArray* metadataArray = nullptr;
    fInputFile->GetObject("RESTMetadataStore", metadataArray);
    if (!metadataArray) return YAML::Node();

    TObject* obj = metadataArray->FindObject(instanceName.c_str());
    if (!obj) return YAML::Node();

    auto* namedMeta = dynamic_cast<TNamed*>(obj);
    if (!namedMeta) return YAML::Node();

    std::string fullText = namedMeta->GetTitle();
    size_t separator = fullText.find("---\n");
    if (separator == std::string::npos) return YAML::Load(fullText);
    
    std::string yamlStr = fullText.substr(separator + 4);
    return YAML::Load(yamlStr);
}

TRestEvent& TRestRun::GetInputEvent(const std::string& branchName) {
    if (!fAnalysisTree) {
        throw std::runtime_error("TRestRun: No input event found");
    }

    auto it = fInputEvents.find(branchName);
    if (it == fInputEvents.end()) {
        TBranch* branch = fAnalysisTree->GetBranch(branchName.c_str());
        if (!branch) {
            throw std::runtime_error("TRestRun: '" + branchName + "' does not exist");
        }

        std::string className = branch->GetClassName();
        TClass* cl = TClass::GetClass(className.c_str());
        if (!cl) {
            throw std::runtime_error("TRestRun: No dictionary for: " + className);
        }

        auto* event = static_cast<TRestEvent*>(cl->New());
        event->SetName(branchName);

        fInputEvents[branchName] = event;

        fAnalysisTree->SetBranchAddress(branchName.c_str(), &fInputEvents[branchName]);
        
        return *fInputEvents[branchName];
    }

    return *(it->second);
}

bool TRestRun::GetEntry(Long64_t entry) {
    if (!fInputEventTree) return false;
    return fInputEventTree->GetEntry(entry) > 0;
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
    fOutputEventTree = new TTree("EventTree", "REST AOD Event Tree");
    fOutputEventTree->SetAutoSave(0);

    fAnalysisTree = new TTree("AnalysisTree", "REST Generic Analysis Observables");
    fAnalysisTree->SetAutoSave(0);
}


void TRestRun::Fill() {
    if (fOutputEventTree) fOutputEventTree->Fill();
    if (fAnalysisTree)    fAnalysisTree->Fill();
    ++fEntriesSaved;
}

void TRestRun::CloseFiles() {
    if (fOutputFile) {
        fOutputFile->cd();
        if (fOutputEventTree) fOutputEventTree->Write("", TObject::kOverwrite);
        if (fAnalysisTree)    fAnalysisTree->Write("", TObject::kOverwrite);
        
        if (fNode && !fNode.IsNull()) {
            AddMetadata(GetName(), GetClassName(), fNode);
        }

        fOutputFile->Close();
        fOutputFile.reset();
        fOutputEventTree = nullptr;
        fAnalysisTree    = nullptr;
    }
    if (fInputFile) {
        fInputFile->Close();
        fInputFile.reset();
        fInputEventTree = nullptr;
    }
}

// ---------------------------------------------------------------------------
void TRestRun::PrintMetadata() {
    RESTMetadata << "=== TRestRun ===" << RESTendl;
    RESTMetadata << fNode << RESTendl;
}
