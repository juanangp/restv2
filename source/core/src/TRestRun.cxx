#include "TRestRun.h"

#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <filesystem>

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
        RESTError << "TRestRun::LoadConfig – node is missing or null" << RESTendl;
        return;
    }

    // Read configuration values from YAML (only first time)
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
        fConfigOutputFileName =
            ReadYAMLParamOrDefault<std::string>(fNode, "outputFileName", fConfigOutputFileName);
        fIsInitializedFromConfig = true;
    }

    // Apply configuration values to member variables (respecting "preserve" and "auto")
    if (fConfigRunNumber != "preserve" && fConfigRunNumber != "auto") {
        try {
            fRunNumber = std::stoi(fConfigRunNumber);
        } catch (...) {
            RESTError << "TRestRun::LoadConfig - Cannot parse runNumber as integer: " << fConfigRunNumber << RESTendl;
        }
    }

    if (fConfigSubRunNumber != "preserve" && fConfigSubRunNumber != "auto") {
        try {
            fParentRunNumber = std::stoi(fConfigSubRunNumber);
        } catch (...) {
            RESTError << "TRestRun::LoadConfig - Cannot parse subRunNumber as integer: " << fConfigRunNumber << RESTendl;
        }
    }

    if (fConfigRunType != "preserve") fRunType = fConfigRunType;
    if (fConfigRunUser != "preserve") fRunUser = fConfigRunUser;
    if (fConfigRunTag != "preserve") fRunTag = fConfigRunTag;
    if (fConfigRunDescription != "preserve") fRunDescription = fConfigRunDescription;
    if (fConfigExperimentName != "preserve") fExperimentName = fConfigExperimentName;
    if (fConfigOutputFileName != "preserve") fOutputFileName = fConfigOutputFileName;

    // Read other parameters that don't have "preserve"/"auto" override logic
    fInputFileName = ReadYAMLParamOrDefault<std::string>(fNode, "inputFileName", fInputFileName);
    fMainDataPath = ReadYAMLParamOrDefault<std::string>(fNode, "mainDataPath", fMainDataPath);
    fInputFormat = ReadYAMLParamOrDefault<std::string>(fNode, "inputFormat", fInputFormat);
    fEntriesSaved = ReadYAMLParamOrDefault<int>(fNode, "entriesSaved", fEntriesSaved);

    if ((fInputFileName.empty() || fInputFileName == "Null") && !fInputFormat.empty()) {
        fInputFileName = PrefixMainDataPath(ResolveFilePattern(fInputFormat));
    } else if (!fInputFileName.empty() && fInputFileName != "Null") {
        fInputFileName = PrefixMainDataPath(ResolveFilePattern(fInputFileName));
    }

    // Update node with current outputFileName value for later retrieval
    if (!fNode["outputFileName"]) {
        TRestTools::SetNodeParameter(fNode, "outputFileName", fOutputFileName);
    }

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
    ResolveOutputFileName();
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
    YAML::Node metadata = fNode ? YAML::Clone(fNode) : YAML::Node(YAML::NodeType::Map);

    TRestTools::SetNodeParameter(metadata, "runNumber", fRunNumber);
    TRestTools::SetNodeParameter(metadata, "subRunNumber", fParentRunNumber);
    TRestTools::SetNodeParameter(metadata, "runType", fRunType);
    TRestTools::SetNodeParameter(metadata, "runUser", fRunUser);
    TRestTools::SetNodeParameter(metadata, "runTag", fRunTag);
    TRestTools::SetNodeParameter(metadata, "runDescription", fRunDescription);
    TRestTools::SetNodeParameter(metadata, "experimentName", fExperimentName);
    TRestTools::SetNodeParameter(metadata, "mainDataPath", fMainDataPath);
    TRestTools::SetNodeParameter(metadata, "inputFormat", fInputFormat);
    TRestTools::SetNodeParameter(metadata, "inputFileName", fInputFileName);
    TRestTools::SetNodeParameter(metadata, "outputFileName", fOutputFileName);
    TRestTools::SetNodeParameter(metadata, "entriesSaved", fEntriesSaved);
    TRestTools::SetNodeParameter(metadata, "startTime", fStartTime);
    TRestTools::SetNodeParameter(metadata, "endTime", fEndTime);

    RESTMetadata << "=== TRestRun ===" << RESTendl;
    RESTMetadata << metadata << RESTendl;
}

static int ParseRunNumberFromFileName(const std::string& filename) {
    if (filename.empty() || filename == "Null") return 0;
    size_t last_slash = filename.find_last_of("/\\");
    std::string base = (last_slash == std::string::npos) ? filename : filename.substr(last_slash + 1);

    size_t dot_pos = base.find_last_of('.');
    if (dot_pos != std::string::npos) {
        base = base.substr(0, dot_pos);
    }

    std::vector<std::string> parts;
    std::string current;
    for (char c : base) {
        if (c == '_') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) parts.push_back(current);

    // Expected legacy pattern:
    // Run_<exp>_<user>_<type>_<tag>_<runNumber>_<subRunNumber>
    if (parts.size() >= 2) {
        const std::string& candidate = parts[parts.size() - 2];
        bool allDigits = !candidate.empty();
        for (char c : candidate) {
            if (!std::isdigit(c)) {
                allDigits = false;
                break;
            }
        }
        if (allDigits) {
            try {
                return std::stoi(candidate);
            } catch (...) {
            }
        }
    }

    // Fallback: first digit block in basename
    std::string digits;
    for (char c : base) {
        if (std::isdigit(c)) {
            digits += c;
        } else if (!digits.empty()) {
            break;
        }
    }
    if (!digits.empty()) {
        try {
            return std::stoi(digits);
        } catch (...) {
        }
    }
    return 0;
}

static int ParseParentRunNumberFromFileName(const std::string& filename) {
    if (filename.empty() || filename == "Null") return 0;
    size_t last_slash = filename.find_last_of("/\\");
    std::string base = (last_slash == std::string::npos) ? filename : filename.substr(last_slash + 1);

    size_t dot_pos = base.find_last_of('.');
    if (dot_pos != std::string::npos) {
        base = base.substr(0, dot_pos);
    }

    std::vector<std::string> parts;
    std::string current;
    for (char c : base) {
        if (c == '_') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) parts.push_back(current);

    if (!parts.empty()) {
        const std::string& candidate = parts.back();
        bool allDigits = !candidate.empty();
        for (char c : candidate) {
            if (!std::isdigit(c)) {
                allDigits = false;
                break;
            }
        }
        if (allDigits) {
            try {
                return std::stoi(candidate);
            } catch (...) {
            }
        }
    }

    return 0;
}

static std::string ParseTagFromFileName(const std::string& filename) {
    if (filename.empty() || filename == "Null") return "preserve";
    size_t last_slash = filename.find_last_of("/\\");
    std::string base = (last_slash == std::string::npos) ? filename : filename.substr(last_slash + 1);

    size_t dot_pos = base.find_last_of('.');
    if (dot_pos != std::string::npos) {
        base = base.substr(0, dot_pos);
    }

    std::vector<std::string> parts;
    std::string current;
    for (char c : base) {
        if (c == '_') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) parts.push_back(current);

    // Expected legacy pattern:
    // Run_<exp>_<user>_<type>_<tag>_<runNumber>_<subRunNumber>
    // Tag is third token from end when run/subrun are numeric.
    if (parts.size() >= 3) {
        const std::string& runCandidate = parts[parts.size() - 2];
        const std::string& subRunCandidate = parts[parts.size() - 1];

        bool runIsNumber = !runCandidate.empty();
        for (char c : runCandidate) {
            if (!std::isdigit(c)) {
                runIsNumber = false;
                break;
            }
        }

        bool subRunIsNumber = !subRunCandidate.empty();
        for (char c : subRunCandidate) {
            if (!std::isdigit(c)) {
                subRunIsNumber = false;
                break;
            }
        }

        if (runIsNumber && subRunIsNumber) {
            return parts[parts.size() - 3];
        }
    }

    if (parts.size() >= 5) {
        return parts[4];
    }
    return "preserve";
}

std::string TRestRun::BuildAutoOutputFileName() const {
    std::string exp = CleanString(fExperimentName);
    std::string user = CleanString(fRunUser);
    std::string type = CleanString(fRunType);
    std::string tag = CleanString(fRunTag);

    char runNumberStr[32];
    snprintf(runNumberStr, sizeof(runNumberStr), "%05d", fRunNumber);
    char parentRunNumberStr[32];
    snprintf(parentRunNumberStr, sizeof(parentRunNumberStr), "%03d", fParentRunNumber);

    return "Run_" + exp + "_" + user + "_" + type + "_" + tag + "_" + runNumberStr + "_" +
           parentRunNumberStr + ".root";
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
        } else if (token == "fParentRunNumber" || token == "parentRunNumber" ||
                   token == "fSubRunNumber" || token == "subRunNumber") {
            char parentRunNumberStr[32];
            snprintf(parentRunNumberStr, sizeof(parentRunNumberStr), "%03d", fParentRunNumber);
            replacement = parentRunNumberStr;
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

void TRestRun::ResolveOutputFileName() {
    std::string filename = fOutputFileName;

    // Check which placeholders are present in the output file name
    bool hasRunNumber = (filename.find("[runNumber]") != std::string::npos) || (filename.find("[fRunNumber]") != std::string::npos);
    bool hasRunTag = (filename.find("[runTag]") != std::string::npos) || (filename.find("[fRunTag]") != std::string::npos);
    bool hasRunDescription = (filename.find("[runDescription]") != std::string::npos) || (filename.find("[fRunDescription]") != std::string::npos);
    bool hasExperimentName = (filename.find("[experimentName]") != std::string::npos) || (filename.find("[fExperimentName]") != std::string::npos);

    // Resolve parameter overrides if the placeholders are present, OR if the whole filename is [auto] or [preserve]
    bool needsResolution = (filename == "[auto]" || filename == "auto" || filename == "[preserve]" || filename == "preserve");

    if (hasRunNumber || needsResolution) {
        if (fConfigRunNumber == "preserve") {
            if (fRunNumber == 0) {
                fRunNumber = ParseRunNumberFromFileName(fInputFileName);
            }
        } else if (fConfigRunNumber == "auto") {
            if (fRunNumber == 0) {
                fRunNumber = ParseRunNumberFromFileName(fInputFileName);
                if (fRunNumber == 0) fRunNumber = 1;
            }
        }
    }

    if (fConfigSubRunNumber == "preserve") {
        if (fParentRunNumber == 0) {
            fParentRunNumber = ParseParentRunNumberFromFileName(fInputFileName);
        }
    } else if (fConfigSubRunNumber == "auto") {
        if (fParentRunNumber == 0) {
            fParentRunNumber = ParseParentRunNumberFromFileName(fInputFileName);
        }
    }

    if (hasRunTag || needsResolution) {
        if (fConfigRunTag == "preserve") {
            if (fRunTag == "Null" || fRunTag.empty() || fRunTag == "preserve") {
                fRunTag = ParseTagFromFileName(fInputFileName);
            }
        } else if (fConfigRunTag == "auto") {
            if (fRunTag == "Null" || fRunTag.empty() || fRunTag == "auto") {
                fRunTag = "AutoTag";
            }
        }
    }

    if (hasRunDescription || needsResolution) {
        if (fRunDescription == "preserve" || fRunDescription == "auto") {
            fRunDescription = "Null";
        }
    }

    if (hasExperimentName || needsResolution) {
        if (fExperimentName == "preserve" || fExperimentName == "auto") {
            fExperimentName = "Null";
        }
    }

    // Handle [auto] or auto
    if (filename == "[auto]" || filename == "auto") {
        filename = BuildAutoOutputFileName();
    }
    // Handle [preserve] or preserve
    else if (filename == "[preserve]" || filename == "preserve") {
        if (fInputFileName != "Null" && !fInputFileName.empty()) {
            // Find base filename (excluding path)
            size_t last_slash = fInputFileName.find_last_of("/\\");
            std::string base = (last_slash == std::string::npos) ? fInputFileName : fInputFileName.substr(last_slash + 1);

            // Replace extension with .root
            size_t dot_pos = base.find_last_of('.');
            if (dot_pos != std::string::npos) {
                base = base.substr(0, dot_pos) + ".root";
            } else {
                base += ".root";
            }
            filename = base;
        } else {
            // Fallback to auto pattern
            filename = BuildAutoOutputFileName();
        }
    }

    filename = ResolveFilePattern(filename);

    // Normalize duplicated separators introduced by empty placeholder substitutions.
    std::filesystem::path resolvedPath(filename);
    std::string leaf = resolvedPath.filename().string();
    std::string normalizedLeaf;
    normalizedLeaf.reserve(leaf.size());

    bool previousUnderscore = false;
    for (char c : leaf) {
        if (c == '_') {
            if (!previousUnderscore) normalizedLeaf.push_back(c);
            previousUnderscore = true;
        } else {
            normalizedLeaf.push_back(c);
            previousUnderscore = false;
        }
    }

    if (normalizedLeaf != leaf) {
        filename = resolvedPath.has_parent_path() ? (resolvedPath.parent_path() / normalizedLeaf).string()
                                                  : normalizedLeaf;
    }

    fOutputFileName = PrefixMainDataPath(filename);
}
