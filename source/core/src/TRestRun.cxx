#include "TRestRun.h"

#include <TKey.h>
#include <TList.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <regex>
#include <stdexcept>

#include "TObjString.h"
#include "TRestTools.h"

using namespace TRestTools;

static const bool TRestRun_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();

    reg.RegisterField<TRestRun>("runNumber", &TRestRun::fRunNumber);
    reg.RegisterField<TRestRun>("subRunNumber", &TRestRun::fSubRunNumber);
    reg.RegisterField<TRestRun>("runType", &TRestRun::fRunType);
    reg.RegisterField<TRestRun>("runUser", &TRestRun::fRunUser);
    reg.RegisterField<TRestRun>("runTag", &TRestRun::fRunTag);
    reg.RegisterField<TRestRun>("runDescription", &TRestRun::fRunDescription);
    reg.RegisterField<TRestRun>("experimentName", &TRestRun::fExperimentName);

    reg.RegisterField<TRestRun>("inputFileName", &TRestRun::fInputFileName);
    reg.RegisterField<TRestRun>("outputFileName", &TRestRun::fOutputFileName);
    reg.RegisterField<TRestRun>("mainDataPath", &TRestRun::fMainDataPath);
    reg.RegisterField<TRestRun>("inputFormat", &TRestRun::fInputFormat);

    reg.RegisterField<TRestRun>("startTime", &TRestRun::fStartTime);
    reg.RegisterField<TRestRun>("endTime", &TRestRun::fEndTime);
    reg.RegisterField<TRestRun>("entriesSaved", &TRestRun::fEntriesSaved);

    return true;
}();

// ---------------------------------------------------------------------------
// Self-registration (no macro)
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register("TRestRun",
                                               [](const std::string& instanceName, const YAML::Node& params) {
                                                   return std::make_unique<TRestRun>(instanceName, params);
                                               });
    return true;
}();
}  // namespace

// ---------------------------------------------------------------------------
TRestRun::TRestRun() { fName = "TRestRun"; }

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

    for (auto* metadata : fMetadataStore) {
        if (metadata) {
            delete metadata;
        }
    }
    fMetadataStore.clear();
}

// ---------------------------------------------------------------------------
void TRestRun::LoadConfig() {
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestRun::LoadConfig - node is missing or null" << RESTendl;
        return;
    }

    if (!fIsInitializedFromConfig) {
        fConfigRunNumber = ReadYAMLParamOrDefault<std::string>(fNode, "runNumber", fConfigRunNumber);
        fConfigSubRunNumber = ReadYAMLParamOrDefault<std::string>(fNode, "subRunNumber", fConfigSubRunNumber);
        fConfigRunType = ReadYAMLParamOrDefault<std::string>(fNode, "runType", fConfigRunType);
        fConfigRunUser = ReadYAMLParamOrDefault<std::string>(fNode, "runUser", fConfigRunUser);
        fConfigRunTag = ReadYAMLParamOrDefault<std::string>(fNode, "runTag", fConfigRunTag);
        fConfigRunDescription =
            ReadYAMLParamOrDefault<std::string>(fNode, "runDescription", fConfigRunDescription);
        fConfigExperimentName =
            ReadYAMLParamOrDefault<std::string>(fNode, "experimentName", fConfigExperimentName);

        if (fConfigRunNumber != "preserve" && fConfigRunNumber != "auto") {
            try {
                fRunNumber = std::stoi(fConfigRunNumber);
            } catch (...) {
            }
        }
        if (fConfigSubRunNumber != "preserve") {
            try {
                fSubRunNumber = std::stoi(fConfigSubRunNumber);
            } catch (...) {
            }
        }
        if (fConfigRunType != "preserve") fRunType = fConfigRunType;
        if (fConfigRunUser != "preserve") fRunUser = fConfigRunUser;
        if (fConfigRunTag != "preserve") fRunTag = fConfigRunTag;
        if (fConfigRunDescription != "preserve") fRunDescription = fConfigRunDescription;
        if (fConfigExperimentName != "preserve") fExperimentName = fConfigExperimentName;

        fOutputFileName = ReadYAMLParamOrDefault<std::string>(fNode, "outputFileName", fOutputFileName);
        fMainDataPath = GetFullPath(ReadYAMLParamOrDefault<std::string>(fNode, "mainDataPath", fMainDataPath));
        fInputFormat = ReadYAMLParamOrDefault<std::string>(fNode, "inputFormat", fInputFormat);
        fEntriesSaved = ReadYAMLParamOrDefault<Long64_t>(fNode, "entriesSaved", fEntriesSaved);
        fInputFileName = ReadYAMLParamOrDefault<std::string>(fNode, "inputFileName", fInputFileName);

        fIsInitializedFromConfig = true;
    }

    if (!fInputFileName.empty() && fInputFileName != "Null") OpenInputFile(fInputFileName);

    // Input format is resolved if we have fInputFileName and fInputFormat
    ResolveInputFormat();

    // In case inputFormat is empty we get preserve variables fron input file
    if (fInputFormat.empty()) {
        if (fInputFileNode && !fInputFileNode.IsNull()) {
            if (fConfigRunNumber == "preserve")
                fRunNumber = ReadYAMLParamOrDefault<int>(fInputFileNode, "runNumber", fRunNumber);
            if (fConfigSubRunNumber == "preserve")
                fSubRunNumber = ReadYAMLParamOrDefault<int>(fInputFileNode, "subRunNumber", fSubRunNumber);
            if (fConfigRunType == "preserve")
                fRunType = ReadYAMLParamOrDefault<std::string>(fInputFileNode, "runType", fRunType);
            if (fConfigRunTag == "preserve")
                fRunTag = ReadYAMLParamOrDefault<std::string>(fInputFileNode, "runTag", fConfigRunTag);
            if (fConfigRunDescription == "preserve")
                fRunDescription =
                    ReadYAMLParamOrDefault<std::string>(fInputFileNode, "runDescription", fRunDescription);
            if (fConfigExperimentName == "preserve")
                fExperimentName =
                    ReadYAMLParamOrDefault<std::string>(fInputFileNode, "experimentName", fExperimentName);
        }
    }

    if (fInputFileName.empty() || fInputFileName == "Null") {
      if(fInputFormat.empty()){
        //In case no input is provided we perform the automatic run numbering
        if (fConfigRunNumber == "auto") fRunNumber = GetRunNumberAuto();
      } else {
        // In case inputFormat is not empty we generate the variables from the input file name
        fInputFileName = ResolveFilePattern(fInputFormat);
      }
    }

    fOutputFileName = PrefixMainDataPath(ResolveFilePattern(fOutputFileName));

    TRestMetadata::ReadYAMLVerbose(fNode);

    // Sync resolved file names to the node
    UpdateYAMLFromParams<TRestRun>(fNode);
}

void TRestRun::OpenInputFile(const std::string& filename) {
    fInputFileName = GetFullPath(filename);
    fInputFile = std::make_unique<TFile>(fInputFileName.c_str(), "READ");
    if (!fInputFile || fInputFile->IsZombie()) {
        throw std::runtime_error("TRestRun: Cannot open file " + filename);
    }

    fInputFile->GetObject("AnalysisTree", fAnalysisTree);

    if (!fAnalysisTree) {
        RESTError << filename << " is not a valid TRestRun " << RESTendl;
        return;
    }

    YAML::Node selfConfig = GetMetadata(GetName());

    if (!selfConfig || selfConfig.IsNull()) {
        TDirectory* metadataDir = fInputFile->GetDirectory("RESTMetadataStore");
        if (metadataDir) {
            TList* keysInDir = metadataDir->GetListOfKeys();
            if (keysInDir) {
                for (auto* meta : fMetadataStore) { if (meta) delete meta; }
                fMetadataStore.clear();

                for (int i = 0; i < keysInDir->GetEntries(); ++i) {
                    TKey* key = dynamic_cast<TKey*>(keysInDir->At(i));
                    if (!key) continue;

                    std::string keyName = key->GetName();
                    YAML::Node testConfig = GetMetadata(keyName);

                    if (testConfig && !testConfig.IsNull() && testConfig["class"]) {
                        std::string className = testConfig["class"].as<std::string>();

                        if (className == "TRestRun") {
                            SetName(keyName);
                            selfConfig = testConfig;
                        }
                        try {
                            std::unique_ptr<TRestMetadata> metadata =
                                MetadataClassRegistry::Instance().Create(className, keyName, testConfig);
                              
                            if (metadata) {
                                fMetadataStore.push_back(metadata.release());
                            }
                        } catch (const std::exception& e) {
                            std::cerr << "[-] Error loading metadata '" << keyName 
                                      << "' from: " << e.what() << std::endl;
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
        RESTWarning << "Not valid TRestRun Metadata in " << filename << RESTendl;
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
                fInputEvent = fInputEvents[className];
            }
        }
    }
}

void TRestRun::AddMetadata(TRestMetadata* metadata) {
    if (!metadata) {
        throw std::runtime_error("TRestRun::AddMetadata: NULL metadata object");
    }
    metadata->WriteMetadata(fOutputFile.get());
}

YAML::Node TRestRun::GetMetadata(const std::string& instanceName) const {
    if (!fInputFile) return YAML::Node();

    return ReadMetadata(fInputFile.get(), instanceName);
}

TRestMetadata* TRestRun::GetMetadataClass(const std::string &className) const {
    for (auto* metadata : fMetadataStore) {
        if (metadata && metadata->GetClassName() == className) {
            return metadata;
        }
    }

    return nullptr;
}

TRestEvent& TRestRun::GetInputEvent(const std::string& treeName) {
    auto it = fInputEvents.find(treeName);
    if (it == fInputEvents.end()) {
        throw std::runtime_error("TRestRun: Tree '" + treeName + "' does not exist.");
    }
    fInputEvent = it->second;
    return *(fInputEvent);
}

void TRestRun::SetInputEvent(const std::string& treeName) {
    auto it = fInputEvents.find(treeName);
    if (it == fInputEvents.end()) {
        throw std::runtime_error("TRestRun: Tree '" + treeName + "' does not exist.");
    }
    fInputEvent = it->second;
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
    fEntry = entry;
    return success;
}

bool TRestRun::GetNextEntry(){
  return GetEntry(fEntry++);

}

Long64_t TRestRun::GetEntryWithID(int eventID, int subEventID, const std::string& tag) {
    if (!fAnalysisTree) return -1;

    TTreeReader reader(fAnalysisTree);

    if (!fAnalysisTree->GetBranch("eventID") || !fAnalysisTree->GetBranch("subEventID")) {
        RESTError << "Branches 'eventID' o 'subEventID' not found in AnalysisTree." << RESTendl;
        return -1;
    }

    if (!tag.empty() && !fAnalysisTree->GetBranch("subEventTag")) {
        RESTError << "Branch 'subEventTag' not found in AnalysisTree" << RESTendl;
        return -1;
    }

    TTreeReaderValue<int> rvEventID(reader, "eventID");
    TTreeReaderValue<int> rvSubEventID(reader, "subEventID");
    
    std::unique_ptr<TTreeReaderValue<std::string>> rvSubEventTag = nullptr;
    if (fAnalysisTree->GetBranch("subEventTag")) {
        rvSubEventTag = std::make_unique<TTreeReaderValue<std::string>>(reader, "subEventTag");
    }

    while (reader.Next()) {
        if (*rvEventID == eventID) {
            if (subEventID != -1 && *rvSubEventID != subEventID) {
                continue;
            }
            if (!tag.empty() && rvSubEventTag) {
                if (**rvSubEventTag != tag) {
                    continue;
                }
            }

            Long64_t currentEntry = reader.GetCurrentEntry();
            this->GetEntry(currentEntry); 
            return currentEntry;
        }
    }

    return -1;
}

void TRestRun::PrintObservables() {
    if (!fAnalysisTree) return;

    RESTInfo << "--- Analysis Tree Observables for Current Entry ---" << RESTendl;

    fAnalysisTree->Show(fEntry);
}


bool TRestRun::HasEvent(const std::string& treeName) const {
    return fInputEvents.find(treeName) != fInputEvents.end();
}

void TRestRun::FormOutputFile() {
    fOutputFileName = PrefixMainDataPath(ResolveFilePattern(fOutputFileName));

    TRestMetadata::ReadYAMLVerbose(fNode);

    UpdateYAMLFromParams<TRestRun>(fNode);

    OpenOutputFile();
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
void TRestRun::PrintMetadata() const {
    RESTMetadata << "=== TRestRun ===" << RESTendl;
    if (fNode && !fNode.IsNull())
        RESTMetadata << YAML::Dump(fNode) << RESTendl;
    else if (fInputFileNode && !fInputFileNode.IsNull())
        RESTMetadata << YAML::Dump(fInputFileNode) << RESTendl;
}

void TRestRun::PrintAllMetadata() const {
  for (auto* metadata : fMetadataStore)
     metadata->PrintMetadata();
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

    if (regexStr.size() > 5 && regexStr.substr(regexStr.size() - 5) == ".root")
        regexStr.resize(regexStr.size() - 5);

    std::smatch matches;
    if (std::regex_match(filenameBase, matches, std::regex(regexStr))) {
        for (size_t i = 1; i < matches.size(); ++i) {
            std::string t = tags[i - 1];
            std::string val = matches[i].str();

            if ((t == "runNumber") && fConfigRunNumber == "preserve") try {
                    fRunNumber = std::stoi(val);
                } catch (...) {
                }
            if ((t == "subRunNumber") && fConfigSubRunNumber == "preserve") try {
                    fSubRunNumber = std::stoi(val);
                } catch (...) {
                }
            if ((t == "runType") && fConfigRunType == "preserve") fRunType = val;
            if ((t == "runUser") && fConfigRunUser == "preserve") fRunUser = val;
            if ((t == "runTag") && fConfigRunTag == "preserve") fRunTag = val;
            if ((t == "runDescription") && fConfigRunDescription == "preserve") fRunDescription = val;
            if ((t == "experimentName") && fConfigExperimentName == "preserve") fExperimentName = val;
        }
    }
}
