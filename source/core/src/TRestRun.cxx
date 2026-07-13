#include "TRestRun.h"

#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <regex>

#include "TObjString.h"
#include "TRestTools.h"
#include <TKey.h>
#include <TList.h>

using namespace TRestTools;

static const bool TRestRun_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    
    reg.RegisterField<TRestRun>("runNumber",          &TRestRun::fRunNumber);
    reg.RegisterField<TRestRun>("subRunNumber",       &TRestRun::fSubRunNumber);
    reg.RegisterField<TRestRun>("runType",            &TRestRun::fRunType);
    reg.RegisterField<TRestRun>("runUser",            &TRestRun::fRunUser);
    reg.RegisterField<TRestRun>("runTag",             &TRestRun::fRunTag);
    reg.RegisterField<TRestRun>("runDescription",     &TRestRun::fRunDescription);
    reg.RegisterField<TRestRun>("experimentName",     &TRestRun::fExperimentName);

    reg.RegisterField<TRestRun>("inputFileName",      &TRestRun::fInputFileName);
    reg.RegisterField<TRestRun>("outputFileName",     &TRestRun::fOutputFileName);
    reg.RegisterField<TRestRun>("mainDataPath",       &TRestRun::fMainDataPath);
    reg.RegisterField<TRestRun>("inputFormat",        &TRestRun::fInputFormat);

    reg.RegisterField<TRestRun>("startTime",          &TRestRun::fStartTime);
    reg.RegisterField<TRestRun>("endTime",            &TRestRun::fEndTime);
    reg.RegisterField<TRestRun>("entriesSaved",       &TRestRun::fEntriesSaved);

    return true;
}();

// ---------------------------------------------------------------------------
// Self-registration (no macro)
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestRun",
        [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestRun>(instanceName, params);
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

TRestRun::TRestRun(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestRun::TRestRun(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
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
        RESTError << "TRestRun::LoadConfig - node is missing or null" << RESTendl;
        return;
    }

    if (!fIsInitializedFromConfig) {
        fConfigRunNumber      = ReadYAMLParamOrDefault<std::string>(fNode, "runNumber", fConfigRunNumber);
        fConfigSubRunNumber   = ReadYAMLParamOrDefault<std::string>(fNode, "subRunNumber", fConfigSubRunNumber);
        fConfigRunType        = ReadYAMLParamOrDefault<std::string>(fNode, "runType", fConfigRunType);
        fConfigRunUser        = ReadYAMLParamOrDefault<std::string>(fNode, "runUser", fConfigRunUser);
        fConfigRunTag         = ReadYAMLParamOrDefault<std::string>(fNode, "runTag", fConfigRunTag);
        fConfigRunDescription = ReadYAMLParamOrDefault<std::string>(fNode, "runDescription", fConfigRunDescription);
        fConfigExperimentName = ReadYAMLParamOrDefault<std::string>(fNode, "experimentName", fConfigExperimentName);
        
        if (fConfigRunNumber != "preserve" && fConfigRunNumber != "auto") {
            try { fRunNumber = std::stoi(fConfigRunNumber); } catch (...) {}
        }
        if (fConfigSubRunNumber != "preserve") {
            try { fSubRunNumber = std::stoi(fConfigSubRunNumber); } catch (...) {}
        }
        if (fConfigRunType != "preserve")        fRunType = fConfigRunType;
        if (fConfigRunUser != "preserve")        fRunUser = fConfigRunUser;
        if (fConfigRunTag != "preserve")         fRunTag = fConfigRunTag;
        if (fConfigRunDescription != "preserve") fRunDescription = fConfigRunDescription;
        if (fConfigExperimentName != "preserve") fExperimentName = fConfigExperimentName;

        fOutputFileName = ReadYAMLParamOrDefault<std::string>(fNode, "outputFileName", fOutputFileName);
        fMainDataPath   = ReadYAMLParamOrDefault<std::string>(fNode, "mainDataPath", fMainDataPath);
        fInputFormat    = ReadYAMLParamOrDefault<std::string>(fNode, "inputFormat", fInputFormat);
        fEntriesSaved   = ReadYAMLParamOrDefault<int>(fNode, "entriesSaved", fEntriesSaved);
        fInputFileName  = ReadYAMLParamOrDefault<std::string>(fNode, "inputFileName", fInputFileName);

        fIsInitializedFromConfig = true;
    }

    if(!fInputFileName.empty() && fInputFileName != "Null")OpenInputFile(fInputFileName);

    //Input format is resolved if we have fInputFileName and fInputFormat
    ResolveInputFormat();

    //In case inputFormat is empty we get preserve variables fron input file 
    if (fInputFormat.empty()) {
        if (fInputFileNode && !fInputFileNode.IsNull()) {
            if (fConfigRunNumber == "preserve")      fRunNumber = ReadYAMLParamOrDefault<int>(fInputFileNode, "runNumber", fRunNumber);
            if (fConfigSubRunNumber == "preserve")   fSubRunNumber = ReadYAMLParamOrDefault<int>(fInputFileNode, "subRunNumber", fSubRunNumber);
            if (fConfigRunType == "preserve")        fRunType = ReadYAMLParamOrDefault<std::string>(fInputFileNode, "runType", "Null");
            if (fConfigRunTag == "preserve")         fRunTag = ReadYAMLParamOrDefault<std::string>(fInputFileNode, "runTag", "Null");
            if (fConfigRunDescription == "preserve") fRunDescription = ReadYAMLParamOrDefault<std::string>(fInputFileNode, "runDescription", "Null");
            if (fConfigExperimentName == "preserve") fExperimentName = ReadYAMLParamOrDefault<std::string>(fInputFileNode, "experimentName", "Null");
        }
    }

    //In case inputFormat is not empty we generate the variables from the input file name
    if( (fInputFileName.empty() || fInputFileName == "Null" ) && !fInputFormat.empty() )fInputFileName = ResolveFilePattern(fInputFormat);

    fOutputFileName = PrefixMainDataPath( ResolveFilePattern(fOutputFileName) );

    TRestMetadata::ReadYAMLVerbose(fNode);

    //Sync resolved file names to the node
    UpdateYAMLFromParams<TRestRun>(fNode);

}

void TRestRun::OpenInputFile(const std::string& filename) {
    fInputFileName = filename;
    fInputFile = std::make_unique<TFile>(fInputFileName.c_str(), "READ");
    if (!fInputFile || fInputFile->IsZombie()) {
        throw std::runtime_error("TRestRun: Cannot open file " + filename);
    }

    fInputFile->GetObject("AnalysisTree", fAnalysisTree);

    if(!fAnalysisTree){
      RESTError << filename << "is not a valid TRestRun " << RESTendl;
      return;
    }

    YAML::Node selfConfig = GetMetadata(GetName());

    if (!selfConfig || selfConfig.IsNull()) {
        TDirectory* metadataDir = fInputFile->GetDirectory("RESTMetadataStore");
        if (metadataDir) {
            TList* keysInDir = metadataDir->GetListOfKeys();
            if (keysInDir) {
                for (int i = 0; i < keysInDir->GetEntries(); ++i) {
                    TKey* key = dynamic_cast<TKey*>(keysInDir->At(i));
                    if (!key) continue;

                    std::string keyName = key->GetName();

                    YAML::Node testConfig = GetMetadata(keyName);

                    if (testConfig && !testConfig.IsNull()) {
                        if (testConfig["class"] && testConfig["class"].as<std::string>() == "TRestRun") {
                            SetName(keyName);
                            selfConfig = testConfig;
                            break;
                        }
                    }
                }
            }
        }
    }
    // =========================================================================

    if (selfConfig && !selfConfig.IsNull()) {
        fInputFileNode = selfConfig;
    } else {
       RESTWarning <<"Not valid TRestRun Metadata in " << filename << RESTendl;
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

void TRestRun::AddMetadata(const std::string& instanceName,
                           const YAML::Node& configNode) {
    WriteMetadata(fOutputFile.get(), instanceName, configNode);
}

YAML::Node TRestRun::GetMetadata(const std::string& instanceName) const {
    if (!fInputFile) return YAML::Node();

    return ReadMetadata(fInputFile.get(), instanceName);
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
            AddMetadata(GetName(), fNode);
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
    if(fNode && !fNode.IsNull())RESTMetadata << YAML::Dump(fNode) << RESTendl;
    else if (fInputFileNode && !fInputFileNode.IsNull())RESTMetadata << YAML::Dump(fInputFileNode) << RESTendl;
}

std::string TRestRun::ResolveFilePattern(const std::string& pattern) const {
    std::string result = pattern;
    size_t pos = 0;
    while (true) {
        size_t pos1 = result.find("[", pos);
        if (pos1 == std::string::npos) break;
        size_t pos2 = result.find("]", pos1 + 1);
        if (pos2 == std::string::npos) break;

        std::string token = result.substr(pos1 + 1, pos2 - pos1 - 1);
        std::string replacement;
        bool hasReplacement = true;

        if (token == "fRunNumber" || token == "runNumber") {
            char runNumberStr[32];
            snprintf(runNumberStr, sizeof(runNumberStr), "%05d", fRunNumber);
            replacement = runNumberStr;
        } else if (token == "fSubRunNumber" || token == "subRunNumber") {
            char subRunNumberStr[32];
            snprintf(subRunNumberStr, sizeof(subRunNumberStr), "%03d", fSubRunNumber);
            replacement = subRunNumberStr;
        } else if (token == "fRunType" || token == "runType") {
            replacement = CleanString(fRunType);
        } else if (token == "fRunUser" || token == "runUser") {
            replacement = CleanString(fRunUser);
        } else if (token == "fRunTag" || token == "runTag") {
            replacement = CleanString(fRunTag);
        } else if (token == "fExperimentName" || token == "experimentName") {
            replacement = CleanString(fExperimentName);
        } else if (token == "fRunDescription" || token == "runDescription") {
            replacement = CleanString(fRunDescription);
        } else if (token == "fVersion" || token == "version") {
            replacement = "2.0.0";
        } else {
            hasReplacement = false;
        }

        if (hasReplacement) {
            result.replace(pos1, pos2 - pos1 + 1, replacement);
            pos = pos1 + replacement.size();
        } else {
            pos = pos2 + 1;
        }
    }

    return result;
}

std::string TRestRun::PrefixMainDataPath(const std::string& fileName) const {
    if (fileName.empty() || fileName == "Null" || fileName == "/dev/null") return fileName;

    std::filesystem::path path(fileName);
    if (path.is_absolute() || fMainDataPath.empty()) return fileName;

    return (std::filesystem::path(fMainDataPath) / path).lexically_normal().string();
}

void TRestRun::ResolveInputFormat() {
    if (fInputFormat.empty() || fInputFileName.empty() || fInputFileName == "Null") return;

    std::string filenameBase = std::filesystem::path(fInputFileName).stem().string();
    std::string regexStr = fInputFormat;
    std::vector<std::string> tags;

    std::regex bracketRe(R"(\[(\w+)\])");
    std::smatch m;
    while (std::regex_search(regexStr, m, bracketRe)) {
        tags.push_back(m[1].str());
        regexStr = m.prefix().str() + "([^\\_]+)" + m.suffix().str();
    }
    
    if (regexStr.size() > 5 && regexStr.substr(regexStr.size() - 5) == ".root") regexStr.resize(regexStr.size() - 5);

    std::smatch matches;
    if (std::regex_match(filenameBase, matches, std::regex(regexStr))) {
        for (size_t i = 1; i < matches.size(); ++i) {
            std::string t = tags[i - 1];
            std::string val = matches[i].str();

            if ((t == "runNumber")       && fConfigRunNumber == "preserve")    try { fRunNumber = std::stoi(val); } catch(...) {}
            if ((t == "subRunNumber")    && fConfigSubRunNumber == "preserve") try { fSubRunNumber = std::stoi(val); } catch(...) {}
            if ((t == "runType")         && fConfigRunType == "preserve")      fRunType = val;
            if ((t == "runUser")         && fConfigRunUser == "preserve")      fRunUser = val;
            if ((t == "runTag")          && fConfigRunTag == "preserve")       fRunTag = val;
            if ((t == "runDescription")  && fConfigRunDescription == "preserve") fRunDescription = val;
            if ((t == "experimentName")  && fConfigExperimentName == "preserve") fExperimentName = val;
        }
    }
}

