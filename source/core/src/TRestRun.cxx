#include "TRestRun.h"

#include <TKey.h>
#include <TList.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>

#include <algorithm>
#include <filesystem>
#include <regex>
#include <stdexcept>

#include "TRestSystemOfUnits.h"
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
    reg.RegisterField<TRestRun>("runInfo", &TRestRun::fRunInformation);

    reg.RegisterField<TRestRun>("inputFileName", &TRestRun::fInputFileName);
    reg.RegisterField<TRestRun>("outputFileName", &TRestRun::fOutputFileName);
    reg.RegisterField<TRestRun>("mainDataPath", &TRestRun::fMainDataPath);
    reg.RegisterField<TRestRun>("inputFormat", &TRestRun::fInputFormat);
    reg.RegisterField<TRestRun>("outputEvent", &TRestRun::fOutputEvent);

    reg.RegisterField<TRestRun>("startTime", &TRestRun::fStartTime);
    reg.RegisterField<TRestRun>("endTime", &TRestRun::fEndTime);
    reg.RegisterField<TRestRun>("entriesSaved", &TRestRun::fEntriesSaved);

    reg.RegisterField<TRestRun>("inheritInputRun", &TRestRun::fInheritInputRun);
    reg.RegisterField<TRestRun>("outputBasketSize", &TRestRun::fOutputBasketSize);

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
namespace {
/// Parameters of the run that can be taken from the input file (its name or its run)
const std::vector<std::string> kRunParameters = {"runNumber", "subRunNumber",   "runType",       "runUser",
                                                 "runTag",    "runDescription", "experimentName"};

}  // namespace

void TRestRun::LoadConfig() {
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestRun::LoadConfig - node is missing or null" << RESTendl;
        return;
    }

    // What is written in the configuration always wins
    const YAML::Node config = YAML::Clone(fNode);
    for (const auto& key : kRunParameters) {
        if (config[key] && config[key].IsScalar() && config[key].Scalar() == "preserve") {
            throw std::runtime_error(
                "TRestRun::LoadConfig - " + key +
                ": \"preserve\" is not used, the parameters not given are taken from the name of the input "
                "file (inputFormat) or its run (inheritInputRun: true)");
        }
    }
    const bool autoRunNumber =
        config["runNumber"] && config["runNumber"].IsScalar() && config["runNumber"].Scalar() == "auto";
    YAML::Node fields = YAML::Clone(config);
    if (autoRunNumber) fields.remove("runNumber");
    UpdateParamsFromYAML<TRestRun>(fields);
    // A new run number ($REST_HOME/.rest/runNumber)
    if (autoRunNumber) fRunNumber = GetRunNumberAuto();

    fMainDataPath = GetFullPath(fMainDataPath);

    if ((fInputFileName.empty() || fInputFileName == "Null") && !fInputFormat.empty()) {
        fInputFileName = ResolveFilePattern(fInputFormat);
    }

    // A pattern of several files (e.g. R*_ANA.root) is not opened: it is only for the tasks
    // (TRestAnalysisPlot), the pipeline reads one file
    const bool isPattern = fInputFileName.find_first_of("*?") != std::string::npos;
    if (!fInputFileName.empty() && fInputFileName != "Null" && !isPattern) {
        OpenInputFile(fInputFileName);
    }

    // The parameters not given: from the name of the input file (inputFormat), otherwise from its run
    // (inheritInputRun). The times of the data taking and the runInfo always from its run
    const YAML::Node& inputRun = fInputFileNode;
    const bool hasInputRun = inputRun && inputRun.IsMap();
    const auto tags = ParseInputFormat();
    YAML::Node inherited(YAML::NodeType::Map);
    for (const auto& key : kRunParameters) {
        if (config[key]) continue;
        const auto tag = tags.find(key);
        if (tag != tags.end()) {
            if (key != "runNumber" && key != "subRunNumber") {
                inherited[key] = tag->second;
                continue;
            }
            // Decimal (as a YAML integer 00012 would be octal)
            try {
                inherited[key] = std::stoi(tag->second);
            } catch (...) {
                RESTWarning << "TRestRun: " << key << " " << tag->second << " of the input file name is not "
                            << "a number" << RESTendl;
            }
        } else if (fInheritInputRun && hasInputRun && inputRun[key]) {
            inherited[key] = inputRun[key];
        }
    }
    // A process reading raw data sets them again
    for (const std::string key : {"startTime", "endTime"}) {
        if (!config[key] && hasInputRun && inputRun[key]) inherited[key] = inputRun[key];
    }
    UpdateParamsFromYAML<TRestRun>(inherited);

    // runInfo: the keys of the configuration, of the input file name ([runInfo.key]) and of its run
    std::map<std::string, std::string> runInformation;
    if (hasInputRun && inputRun["runInfo"] && inputRun["runInfo"].IsMap())
        runInformation = inputRun["runInfo"].as<std::map<std::string, std::string>>();
    for (const auto& [name, value] : tags) {
        if (name.rfind("runInfo.", 0) == 0) runInformation[name.substr(8)] = value;
    }
    for (const auto& [key, value] : fRunInformation) runInformation[key] = value;
    fRunInformation = runInformation;

    ResolveOutputFileName();

    // Sync resolved parameters and file names to the node
    UpdateYAMLFromParams<TRestRun>(fNode);
}

void TRestRun::OpenInputFile(const std::string& filename) {
    if (fInputFile) return;

    fInputFileName = GetFullPath(filename);

    if (!isValidTRestRun(fInputFileName)) {
        RESTError << filename << " is not a valid TRestRun " << RESTendl;
        delete fAnalysisTree;
        fAnalysisTree = nullptr;
        fInputFile.reset();
        return;
    }

    fInputFile = std::make_unique<TFile>(fInputFileName.c_str(), "READ");
    if (!fInputFile || fInputFile->IsZombie()) {
        throw std::runtime_error("TRestRun: Cannot open file " + filename);
    }

    fInputFile->GetObject("AnalysisTree", fAnalysisTree);

    fInputEntries = fAnalysisTree->GetEntries();

    YAML::Node selfConfig;

    TDirectory* metadataDir = fInputFile->GetDirectory("RESTMetadataStore");
    if (metadataDir) {
        TList* keysInDir = metadataDir->GetListOfKeys();
        if (keysInDir) {
            for (auto* meta : fMetadataStore) {
                if (meta) delete meta;
            }
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
                        continue;
                    }

                    try {
                        std::unique_ptr<TRestMetadata> metadata =
                            MetadataClassRegistry::Instance().Create(className, keyName, testConfig, false);

                        if (metadata) {
                            metadata->SetRunInfo(this);
                            fMetadataStore.push_back(metadata.release());
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[-] Error loading metadata '" << keyName << "' from: " << e.what()
                                  << std::endl;
                    }
                }
            }
        }
    }

    if (selfConfig && !selfConfig.IsNull()) {
        fInputFileNode = selfConfig;
        // Preserved unless the processes of this run produce another (TRestManager::SaveMetadata)
        fOutputEvent = ReadYAMLParamOrDefault<std::string>(fInputFileNode, "outputEvent", fOutputEvent);
    } else {
        RESTWarning << "Not valid TRestRun Metadata in " << filename << RESTendl;
    }

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
                eventObj->SetRestRun(this);

                if (fAnalysisTree && fInputEvents.empty()) {
                    eventObj->TRestEvent::SetBranchAddresses(fAnalysisTree);
                }

                fInputEvents[className] = eventObj.release();
                fEvent = fInputEvents[className];
            }
        }
    }

    // The last saved event type by default, otherwise the last event tree
    const auto outputIt = fInputEvents.find(fOutputEvent);
    if (outputIt != fInputEvents.end()) fEvent = outputIt->second;
}

void TRestRun::AddMetadata(TRestMetadata* metadata) {
    if (!metadata) {
        throw std::runtime_error("TRestRun::AddMetadata: NULL metadata object");
    }
    metadata->WriteMetadata(fOutputFile.get());
}

void TRestRun::AddHistoricMetadata() {
    for (auto* metadata : fMetadataStore) {
        if (metadata && metadata->GetClassName() != "TRestRun") {
            metadata->WriteMetadata(fOutputFile.get());
        }
    }
}

YAML::Node TRestRun::GetMetadata(const std::string& instanceName) const {
    if (!fInputFile) return YAML::Node();

    return ReadMetadata(fInputFile.get(), instanceName);
}

TRestMetadata* TRestRun::GetMetadataClass(const std::string& className) const {
    for (auto* metadata : fMetadataStore) {
        if (metadata && metadata->GetClassName() == className) {
            return metadata;
        }
    }

    return nullptr;
}

TRestEvent* TRestRun::GetEvent(const std::string& treeName) {
    auto it = fInputEvents.find(treeName);
    if (it == fInputEvents.end()) {
        RESTError << "TRestRun: Tree '" << treeName << "' does not exist." << RESTendl;
        return nullptr;
    }
    fEvent = it->second;
    return fEvent;
}

void TRestRun::SetEvent(const std::string& treeName) {
    auto it = fInputEvents.find(treeName);
    if (it == fInputEvents.end()) {
        RESTError << "TRestRun: Tree '" << treeName << "' does not exist." << RESTendl;
        return;
    }
    fEvent = it->second;
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

bool TRestRun::GetNextEntry() { return GetEntry(++fEntry); }

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

void TRestRun::ResolveOutputFileName() {
    const std::string name = TRestTools::Trim(fOutputFileName);
    if (name.empty() || name == "Null" || name == "null" || name == "NULL") {
        fOutputFileName = "/dev/null";
        return;
    }
    fOutputFileName = PrefixMainDataPath(ResolveFilePattern(fOutputFileName));

    // The output is recreated: the input would be lost while it is read
    std::error_code error;
    if (fOutputFileName != "/dev/null" &&
        std::filesystem::equivalent(fOutputFileName, fInputFileName, error)) {
        throw std::runtime_error("TRestRun: the outputFileName " + fOutputFileName +
                                 " is the input file, it would be overwritten; give another name");
    }
}

void TRestRun::FormOutputFile() {
    ResolveOutputFileName();

    UpdateYAMLFromParams<TRestRun>(fNode);

    OpenOutputFile();
}

void TRestRun::OpenOutputFile(std::string fileName, const std::string& option) {
    if (fileName.empty()) fileName = fOutputFileName;
    fOutputFileName = fileName;

    fOutputFile = std::make_unique<TFile>(fOutputFileName.c_str(), option.c_str());
    if (!fOutputFile || fOutputFile->IsZombie()) {
        throw std::runtime_error("TRestRun: Cannot open output file: " + fOutputFileName);
    }

    fOutputFile->cd();

    if (option == "UPDATE") {
        return;
    }

    if (fAnalysisTree) {
        fOutputAnalysisTree = fAnalysisTree->CloneTree(0);
        fAnalysisTree->CopyAddresses(fOutputAnalysisTree);
        SyncAnalysisTreeBranches();
    } else {
        fOutputAnalysisTree = new TTree("AnalysisTree", "REST Generic Analysis Observables");
    }

    fOutputAnalysisTree->SetAutoSave(0);
}

void TRestRun::SyncAnalysisTreeBranches() {
    if (!fAnalysisTree || !fOutputAnalysisTree) return;

    TObjArray* inputBranches = fAnalysisTree->GetListOfBranches();

    for (int i = 0; i < inputBranches->GetEntries(); ++i) {
        auto* inputBranch = dynamic_cast<TBranch*>(inputBranches->At(i));

        if (!inputBranch) continue;

        const char* branchName = inputBranch->GetName();

        TBranch* outputBranch = fOutputAnalysisTree->GetBranch(branchName);

        if (!outputBranch) {
            continue;
        }

        if (outputBranch->GetAddress() != nullptr) {
            continue;
        }

        outputBranch->SetAddress(inputBranch->GetAddress());
    }
}

void TRestRun::Fill() {
    if (fOutputFile) fOutputFile->cd();

    for (auto& [treeName, tree] : fOutputEventTrees) tree->Fill();

    if (fOutputAnalysisTree) {
        fOutputAnalysisTree->Fill();
    }

    ++fEntriesSaved;
}

void TRestRun::CloseFiles() {
    if (fOutputFile) {
        fOutputFile->cd();
        for (auto& [treeName, tree] : fOutputEventTrees) {
            if (tree) {
                tree->Write("", TObject::kOverwrite);
            }
        }

        if (fOutputAnalysisTree) {
            fOutputAnalysisTree->Write("", TObject::kOverwrite);
        }

        fOutputFile->Close();
        fOutputFile.reset();
        fOutputEventTrees.clear();
        fOutputAnalysisTree = nullptr;
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
    for (auto* metadata : fMetadataStore) metadata->PrintMetadata();
}

YAML::Node TRestRun::GetParameters() const {
    // From the members, the setters do not update the node
    YAML::Node parameters(YAML::NodeType::Map);
    const_cast<TRestRun*>(this)->UpdateYAMLFromParams<TRestRun>(parameters);
    return parameters;
}

std::optional<std::string> TRestRun::GetParameter(const std::string& name) const {
    const YAML::Node node = GetNodeAtPath(GetParameters(), name);
    if (!node || !node.IsScalar()) return std::nullopt;
    return node.Scalar();
}

std::optional<double> TRestRun::GetParameterValue(const std::string& name) const {
    const YAML::Node node = GetNodeAtPath(GetParameters(), name);
    if (!node || !node.IsScalar()) return std::nullopt;
    return REST_Units::ParseValueWithUnits(node.Scalar());
}

std::string TRestRun::ResolveFilePattern(const std::string& pattern) const {
    const YAML::Node parameters = GetParameters();
    return ReplaceDelimited(pattern, '[', ']', [&](const std::string& name) -> std::optional<std::string> {
        char number[32];
        if (name == "runNumber") {
            snprintf(number, sizeof(number), "%05d", fRunNumber);
            return number;
        }
        if (name == "subRunNumber") {
            snprintf(number, sizeof(number), "%03d", fSubRunNumber);
            return number;
        }
        if (name == "version") return "2.0.0";
        const YAML::Node node = GetNodeAtPath(parameters, name);
        if (!node || !node.IsScalar()) {
            throw std::runtime_error("TRestRun: [" + name + "] in \"" + pattern +
                                     "\" is not a parameter of the run (its keys, e.g. [runNumber] or "
                                     "[runTag], or [runInfo.key])");
        }
        return CleanString(node.Scalar());
    });
}

std::string TRestRun::PrefixMainDataPath(const std::string& fileName) const {
    if (fileName.empty() || fileName == "Null" || fileName == "/dev/null") return fileName;

    std::filesystem::path path(fileName);
    if (path.is_absolute() || fMainDataPath.empty()) return fileName;

    return (std::filesystem::path(fMainDataPath) / path).lexically_normal().string();
}

std::map<std::string, std::string> TRestRun::ParseInputFormat() const {
    std::map<std::string, std::string> parameters;
    if (fInputFormat.empty() || fInputFileName.empty() || fInputFileName == "Null") return parameters;

    std::string filenameBase = std::filesystem::path(fInputFileName).stem().string();
    std::string regexStr = fInputFormat;
    std::vector<std::string> tags;

    std::regex bracketRe(R"(\[([\w.]+)\])");
    std::smatch m;
    while (std::regex_search(regexStr, m, bracketRe)) {
        const std::string tag = m[1].str();
        if (std::find(kRunParameters.begin(), kRunParameters.end(), tag) == kRunParameters.end() &&
            tag.rfind("runInfo.", 0) != 0) {
            throw std::runtime_error("TRestRun: [" + tag + "] in inputFormat \"" + fInputFormat +
                                     "\" is not a run parameter (runNumber, subRunNumber, runType, runUser, "
                                     "runTag, runDescription, experimentName) nor runInfo.key");
        }
        tags.push_back(tag);
        regexStr = m.prefix().str() + "([^\\_]+)" + m.suffix().str();
    }

    if (regexStr.size() > 5 && regexStr.substr(regexStr.size() - 5) == ".root")
        regexStr.resize(regexStr.size() - 5);

    std::smatch matches;
    if (std::regex_match(filenameBase, matches, std::regex(regexStr))) {
        for (size_t i = 1; i < matches.size(); ++i) parameters[tags[i - 1]] = matches[i].str();
    }
    return parameters;
}
