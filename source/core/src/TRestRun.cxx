#include "TRestRun.h"

#include <TBranchElement.h>
#include <TClass.h>
#include <TDataType.h>
#include <TKey.h>
#include <TLeaf.h>
#include <TList.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>

#include <algorithm>
#include <filesystem>
#include <regex>
#include <stdexcept>

#include "TRestSystemOfUnits.h"
#include "TRestTools.h"
#include "TRestYAMLUtils.h"

using namespace TRestTools;

static const bool TRestRun_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();

    // The description of the run (runNumber, runTag, runInfo, times...): fields of TRestRunInfo
    reg.RegisterField<TRestRun>("inputFileName", &TRestRun::fInputFileName);
    reg.RegisterField<TRestRun>("outputFileName", &TRestRun::fOutputFileName);
    reg.RegisterField<TRestRun>("mainDataPath", &TRestRun::fMainDataPath);
    reg.RegisterField<TRestRun>("inputFormat", &TRestRun::fInputFormat);
    reg.RegisterField<TRestRun>("outputEvent", &TRestRun::fOutputEvent);
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
    : TRestRunInfo(instanceName, node) {
    LoadConfig();
}

TRestRun::TRestRun(const std::string& fileName, const std::string& sectionName)
    : TRestRunInfo(fileName, sectionName) {
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
        // Other files (e.g. a DAQ file) are not opened: they are the input of a process reading it
        if (isValidTRestRun(GetFullPath(fInputFileName))) {
            OpenInputFile(fInputFileName);
        } else {
            fInputFileName = GetFullPath(fInputFileName);
            if (!fileExists(fInputFileName)) {
                RESTWarning << "TRestRun: input file " << fInputFileName << " not found" << RESTendl;
            } else {
                RESTInfo << "TRestRun: " << fInputFileName << " is not a REST run file, not opened (input "
                         << "of a process)" << RESTendl;
            }
        }
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
    // A process reading raw data sets them again; the merged subruns of the data
    for (const std::string key : {"startTime", "endTime", "mergedSubRuns"}) {
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

void TRestRun::SetInputFileName(const std::string& v) {
    fInputFileName = v;
    if (fNode) TRestYAMLUtils::SetParam(fNode, "inputFileName", fInputFileName);
}

void TRestRun::SetOutputFileName(const std::string& v) {
    fOutputFileName = v;
    if (fNode) TRestYAMLUtils::SetParam(fNode, "outputFileName", fOutputFileName);
}

void TRestRun::SetOutputEvent(const std::string& v) {
    fOutputEvent = v;
    if (fNode) TRestYAMLUtils::SetParam(fNode, "outputEvent", fOutputEvent);
}

void TRestRun::OpenInputFile(const std::string& filename) {
    if (fInputFile || filename.empty()) return;

    fInputFileName = GetFullPath(filename);

    if (!isValidTRestRun(fInputFileName)) {
        throw std::runtime_error("TRestRun: " + filename +
                                 " is not a valid REST run file (not found, not a ROOT file, or without "
                                 "AnalysisTree or TRestRun metadata)");
    }

    fInputFile = std::make_unique<TFile>(fInputFileName.c_str(), "READ");
    if (!fInputFile || fInputFile->IsZombie()) {
        throw std::runtime_error("TRestRun: Cannot open file " + filename);
    }

    fInputFile->GetObject("AnalysisTree", fAnalysisTree);

    fInputEntries = fAnalysisTree->GetEntries();

    YAML::Node selfConfig;

    // Its TRestRun is this run; the other metadata are created, with the objects stored in the file
    const auto [runInstance, runConfig] =
        TRestYAMLUtils::GetMetadataClass(TRestYAMLUtils::ReadMetadataStore(fInputFile.get()), "TRestRun");
    if (!runInstance.empty()) {
        SetName(runInstance);
        selfConfig = runConfig;
    }
    fMetadataStore.Load(fInputFile.get(), this, {"TRestRun"});

    if (selfConfig && !selfConfig.IsNull()) {
        fInputFileNode = selfConfig;
        // Preserved unless the processes of this run produce another (TRestManager::SaveMetadata)
        fOutputEvent =
            TRestYAMLUtils::ReadParamOrDefault<std::string>(fInputFileNode, "outputEvent", fOutputEvent);
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
                // The branch layout of the file must be one this version of the event class reads
                const int version = TRestEvent::ReadFormatVersion(tree);
                if (version < eventObj->GetMinimumFormatVersion() || version > eventObj->GetFormatVersion()) {
                    throw std::runtime_error("TRestRun: the " + className + " of " + fInputFile->GetName() +
                                             " has format version " + std::to_string(version) +
                                             (version == 0 ? " (no version, older format)" : "") +
                                             ", this REST reads versions " +
                                             std::to_string(eventObj->GetMinimumFormatVersion()) + " to " +
                                             std::to_string(eventObj->GetFormatVersion()));
                }
                eventObj->SetInputFormatVersion(version);
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
    for (const auto& metadata : fMetadataStore.GetAll()) metadata->WriteMetadata(fOutputFile.get());
}

YAML::Node TRestRun::GetMetadata(const std::string& instanceName) const {
    if (!fInputFile) return YAML::Node();

    return ReadMetadata(fInputFile.get(), instanceName);
}

TRestMetadata* TRestRun::GetMetadataClass(const std::string& className) const {
    return fMetadataStore.GetClass(className);
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

bool TRestRun::GetEntry(long long entry) {
    bool success = false;
    for (auto& [treeName, tree] : fInputEventTrees) {
        if (tree) {
            if (tree->GetEntry(entry) > 0) {
                success = true;
            }
        }
    }
    // The observables of the input file: copied to the output analysis tree, and read by the processes and
    // macros (GetObservable). A file without event trees (only the analysis tree) is read entry by entry
    // from the analysis tree alone
    if (fInputEventTrees.empty()) {
        success = fAnalysisTree && fAnalysisTree->GetEntry(entry) > 0;
    } else if (success && fAnalysisTree) {
        fAnalysisTree->GetEntry(entry);
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

long long TRestRun::GetEntryWithID(int eventID, int subEventID, const std::string& tag) {
    if (!fAnalysisTree) return -1;

    TTreeReader reader(fAnalysisTree);

    if (!fAnalysisTree->GetBranch("eventID") || !fAnalysisTree->GetBranch("subEventID")) {
        RESTError << "Branches 'eventID' or 'subEventID' not found in AnalysisTree." << RESTendl;
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

            long long currentEntry = reader.GetCurrentEntry();
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

    // Without inputAnalysisStorage only the event info and the observables of the processes, as without
    // input analysis tree
    if (CopiesInputAnalysisTree()) {
        fOutputAnalysisTree = fAnalysisTree->CloneTree(0);
        fAnalysisTree->CopyAddresses(fOutputAnalysisTree);
        SyncAnalysisTreeBranches();
    } else {
        fOutputAnalysisTree = new TTree("AnalysisTree", "REST Generic Analysis Observables");
    }

    fOutputAnalysisTree->SetAutoSave(0);
}

const void* TRestRun::GetObservableAddress(const std::string& name, const std::type_info& type) const {
    const std::string prefix = "TRestRun: observable '" + name + "' ";
    // While processing the output tree (input and previous processes), reading a file the input tree
    TTree* tree = fOutputAnalysisTree ? fOutputAnalysisTree : fAnalysisTree;
    if (!tree) throw std::runtime_error(prefix + "requested without analysis tree");
    TBranch* branch = tree->GetBranch(name.c_str());
    if (!branch) {
        throw std::runtime_error(
            prefix +
            "not in the analysis tree (of the input file or of a previous process: is it "
            "in the observables of that process?)");
    }

    TClass* branchClass = nullptr;
    EDataType branchType = kNoType_t;
    if (branch->GetExpectedType(branchClass, branchType) != 0) {
        throw std::runtime_error(prefix + "has an unknown type");
    }
    TClass* requestedClass = TClass::GetClass(type);
    const std::string requestedName =
        requestedClass ? requestedClass->GetName() : TDataType::GetTypeName(TDataType::GetType(type));

    const void* address = nullptr;
    if (branchClass) {
        // Object (std::string, std::vector...): the object bound to the branch
        if (branchClass != requestedClass) {
            throw std::runtime_error(prefix + "is " + branchClass->GetName() + ", not " + requestedName);
        }
        auto* element = dynamic_cast<TBranchElement*>(branch);
        address = element ? element->GetObject() : nullptr;
    } else {
        // Number or boolean: the buffer of its single leaf (the variable of the process or the input buffer)
        if (requestedClass || branchType != TDataType::GetType(type)) {
            throw std::runtime_error(prefix + "is " + TDataType::GetTypeName(branchType) + ", not " +
                                     requestedName);
        }
        auto* leaf = static_cast<TLeaf*>(branch->GetListOfLeaves()->At(0));
        if (branch->GetListOfLeaves()->GetEntries() != 1 || !leaf || leaf->GetLenStatic() != 1) {
            throw std::runtime_error(prefix + "is not a single value");
        }
        address = leaf->GetValuePointer();
    }
    if (!address) throw std::runtime_error(prefix + "has no data bound to its branch");
    return address;
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
        fObservablePointers.clear();
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
    for (const auto& metadata : fMetadataStore.GetAll()) metadata->PrintMetadata();
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

    // The pattern of the file name (without directories nor the extension of the file) as a regex: each
    // [tag] a part without "_", the rest literal
    std::string pattern = std::filesystem::path(fInputFormat).filename().string();
    const std::string extension = std::filesystem::path(fInputFileName).extension().string();
    if (!extension.empty() && pattern.size() > extension.size() &&
        pattern.compare(pattern.size() - extension.size(), extension.size(), extension) == 0)
        pattern.resize(pattern.size() - extension.size());
    const std::regex bracketRe(R"(\[([\w.]+)\])");
    const std::regex special(R"([.^$|()\[\]{}*+?\\])");
    std::vector<std::string> tags;
    std::string regexStr;
    std::string rest = pattern;
    std::smatch m;
    while (std::regex_search(rest, m, bracketRe)) {
        const std::string tag = m[1].str();
        if (std::find(kRunParameters.begin(), kRunParameters.end(), tag) == kRunParameters.end() &&
            tag.rfind("runInfo.", 0) != 0) {
            throw std::runtime_error("TRestRun: [" + tag + "] in inputFormat \"" + fInputFormat +
                                     "\" is not a run parameter (runNumber, subRunNumber, runType, runUser, "
                                     "runTag, runDescription, experimentName) nor runInfo.key");
        }
        tags.push_back(tag);
        regexStr += std::regex_replace(m.prefix().str(), special, R"(\$&)") + "([^_]+)";
        rest = m.suffix().str();
    }
    regexStr += std::regex_replace(rest, special, R"(\$&)");

    const std::string filenameBase = std::filesystem::path(fInputFileName).stem().string();
    std::smatch matches;
    // Not an error: the input file can have another name (e.g. a tag with "_"), the parameters keep their
    // values
    if (!std::regex_match(filenameBase, matches, std::regex(regexStr))) {
        RESTWarning << "TRestRun: the input " << fInputFileName << " does not match the inputFormat \""
                    << fInputFormat << "\": no run parameters taken from its name" << RESTendl;
        return parameters;
    }
    for (size_t i = 1; i < matches.size(); ++i) parameters[tags[i - 1]] = matches[i].str();
    return parameters;
}
