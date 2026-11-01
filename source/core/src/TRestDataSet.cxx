/// \class TRestDataSet
/// Selection of the analysis trees of several runs in a single tree, together with the description of
/// each run (TRestRunInfo: numbers, tags, runInfo, times, entries; not the processing of its TRestRun).
/// Every entry has the index of its run (column `runIndex`, position in the list of runs), so any
/// parameter of the run of an entry (run number, tag, runInfo, start and end times...) is known and the
/// information of the runs is never summarized: totals (e.g. the run time) are computed from the runs
/// when they are needed, also after merging datasets.
///
/// It is generated with the macro REST_GenerateDataSet (no TRestRun nor TRestManager needed):
///
///    restRoot
///    REST_GenerateDataSet("dataset.yaml", "dataSet")
///
/// The inputs (filePattern) are either run files (output of restManager) or datasets: their runs are
/// joined and runIndex renumbered, the result is a dataset like any other. A run cannot be twice (same
/// runNumber and subRunNumber, also one of the mergedSubRuns of a run merged with REST_MergeRuns, or, for
/// runs without number, the same file): it is an error. A TRestAnalysisPlot takes a dataset file as input
/// as it takes a run file (its inputFileName, or `restManager --i`).
/// From restRoot: Generate(cuts), GetDataFrame(), Define(name, expression), Write(fileName); a dataset
/// file is opened as it is with the constructor TRestDataSet(fileName), e.g. as `_dataSet` by
/// `restRoot Dataset.root` (REST_OpenInputFile, REST_OpenDataSet). Each run is a TRestRunInfo built from
/// the TRestRun stored in its file (GetRuns()[i].run, no file opened); PrintDataSetInfo() prints a summary
/// with a line per run, PrintAllMetadata() also the configurations and every run.
///
/// The metadata of the files are read directly (TRestTools::ReadMetadataStore), no TRestRun is opened.
///
/// ### Parameters
/// * filePattern: the files the dataset is generated from (TRestTools::GetFilesMatchingPattern, * and ?
///   in the file name, e.g. "data/R000*_Calibration_*.root", or datasets to merge, "Dataset_*.root").
/// * inputFileName: a dataset already generated, opened as it is; its stored configuration is kept apart,
///   not changed. Not for the generation.
/// * observables: patterns (* and ?) of the analysis tree columns written, e.g. ["trackLineAnalysis_*",
///   "rawAnalysis_NumberOfSignals"]. Empty (default): all. eventID, subEventID, timeSeconds and runIndex are
///   always written.
/// * runConditions: name: expression (as the conditions of TRestCut) on the parameters of each run
///   (TRestRunInfo), as written in its YAML: runNumber, subRunNumber, runTag, runType, runUser,
///   experimentName, startTime, endTime (Unix time), entriesSaved, runInfo.key... A number (also with units,
///   e.g. runInfo.meshVoltage: 300 V, in REST units) is compared as a number, a text as a std::string (e.g.
///   `runTag == "Calibration"`), a date with date("YYYY/MM/DD HH:MM") (local time; also YYYY-MM-DD, with
///   or without HH:MM or HH:MM:SS), e.g. `startTime > date("2022/04/28 00:00")`. A run is selected if it
///   fulfills all of them; a parameter used must be in all the runs.
/// * cuts: names of the TRestCut applied to the entries (sections of the same YAML).
/// * columns: name: expression of new columns, in order (a column can use the previous ones), with
///   observables and run parameters (the value of the run of each entry), e.g.
///   `gainCorrected: "rawAnalysis_ThresholdIntegral / runInfo.gain"`.
/// * outputFileName: the file written (Write; by the macro if it is not given to it).
///
/// ### Dataset file
/// * AnalysisTree: the columns, with runIndex;
/// * RESTMetadataStore: this TRestDataSet (filePattern: what it was generated from, outputFileName: the
///   file) and the TRestCut used, and in its folder RESTDataSetInfo a YAML
///   TObjString per run (run_000000, run_000001...) with `runIndex`, `run` (its TRestRunInfo), `fileName`
///   (run file), `entries` (of the run file) and `selectedEntries` (in the dataset).
///
/// Example:
///
///    fiducial:
///      class: "TRestCut"
///      conditions:
///        originX: "trackLineAnalysis_OriginX > 20 && trackLineAnalysis_OriginX < 225"
///    dataSet:
///      class: "TRestDataSet"
///      filePattern: "R*_LINEAR.root"
///      observables: ["trackLineAnalysis_*"]
///      runConditions:
///        runNumber: "runNumber > 370 && runNumber < 375"
///        dates: "startTime > date(\"2022/04/28\") && endTime < date(\"2022/11/25 13:20\")"
///        mesh: "runInfo.meshVoltage > 300 V"
///      cuts: ["fiducial"]
///      columns:
///        energyPerVolt: "trackLineAnalysis_Energy / runInfo.meshVoltage"
///      outputFileName: "Dataset_Calibration.root"
///
/// Differences with legacy: the runs (their TRestRunInfo) are stored and every entry has its runIndex,
/// instead of the file names and the relevant quantities (`quantity`, a value per dataset with a strategy);
/// the run filters are expressions (`runConditions`) instead of `filter` with contains/greaterThan/lowerThan/
/// equalsTo, and the date range (startTime, endTime) is a condition with date(...); merging keeps the runs
/// (legacy kept the metadata of the first dataset and removed the quantities); no temporary files (a
/// single Snapshot in Write); only ROOT files (no txt/csv export); processObservables are patterns of
/// observables (e.g. "final_*"); the macro REST_GenerateDataSets is REST_GenerateDataSet.

#include "TRestDataSet.h"

#include <TDirectory.h>
#include <TFile.h>
#include <TH1D.h>
#include <TObjString.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>

#include "TRestCut.h"
#include "TRestSystemOfUnits.h"
#include "TRestTools.h"

using TRestTools::GetNodeAtPath;
using TRestTools::GetYAMLParam;

static const bool TRestDataSet_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestDataSet>("inputFileName", &TRestDataSet::fInputFileName);
    reg.RegisterField<TRestDataSet>("filePattern", &TRestDataSet::fFilePattern);
    reg.RegisterField<TRestDataSet>("observables", &TRestDataSet::fObservables);
    reg.RegisterField<TRestDataSet>("runConditions", &TRestDataSet::fRunConditions);
    reg.RegisterField<TRestDataSet>("cuts", &TRestDataSet::fCutNames);
    reg.RegisterField<TRestDataSet>("columns", &TRestDataSet::fColumnsConfig);
    reg.RegisterField<TRestDataSet>("outputFileName", &TRestDataSet::fOutputFileName);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDataSet", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDataSet>(instanceName, params);
        });
    return true;
}();

/// Columns of every analysis tree, always written
const std::vector<std::string> kEventColumns = {"eventID", "subEventID", "timeSeconds"};
/// Index of the input file of each entry, internal
const std::string kInputFileColumn = "dataSetInputFile_";
/// Folder of RESTMetadataStore with the runs of a dataset
const std::string kRunsDirectory = "RESTDataSetInfo";

/// Identities of a run: runNumber and each of its subruns (subRunNumber, or the mergedSubRuns of a merged
/// run file) or, without run number, its file
std::vector<std::string> RunKeys(const TRestDataSet::Run& run) {
    const int number = run.run->GetRunNumber();
    if (number < 0) return {"file " + run.fileName};
    auto subRuns = run.run->fMergedSubRuns;
    if (subRuns.empty()) subRuns.push_back(run.run->GetSubRunNumber());
    std::vector<std::string> keys;
    for (const int subRun : subRuns)
        keys.push_back("run " + std::to_string(number) + "." + std::to_string(subRun));
    return keys;
}
}  // namespace

TRestDataSet::TRestDataSet() : TRestMetadata() { fName = "TRestDataSet"; }

TRestDataSet::TRestDataSet(const std::string& fileName) : TRestMetadata() {
    fName = "TRestDataSet";
    fInputFileName = fileName;
    Open(fileName);
}

TRestDataSet::TRestDataSet(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestDataSet::TRestDataSet(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

void TRestDataSet::LoadConfig() {
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestDataSet::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }
    ReadConfig();
    // A dataset file already generated: opened as it is, as TRestRun opens its input
    if (!fInputFileName.empty()) Open(fInputFileName);
}

void TRestDataSet::ReadConfig() {
    UpdateParamsFromYAML<TRestDataSet>(fNode);
    for (const auto& [name, condition] : fRunConditions) {
        if (condition.empty()) {
            throw std::runtime_error("TRestDataSet '" + fName + "': empty run condition " + name);
        }
    }
    fColumnExpressions.clear();
    if (fColumnsConfig && !fColumnsConfig.IsNull()) {
        if (!fColumnsConfig.IsMap()) {
            throw std::runtime_error("TRestDataSet '" + fName + "': columns must be a map name: expression");
        }
        // In the order written
        for (const auto& column : fColumnsConfig) {
            fColumnExpressions.emplace_back(column.first.as<std::string>(), column.second.as<std::string>());
        }
    }
    UpdateYAMLFromParams<TRestDataSet>(fNode);
}

std::string TRestDataSet::DefineRunParameters(ROOT::RDF::RNode& frame, const std::string& expression) const {
    std::set<std::string> columns;
    for (const auto& column : frame.GetColumnNames()) columns.insert(column);
    std::vector<YAML::Node> parameters;
    for (const auto& run : fRuns) parameters.push_back(run.run->GetParameters());

    // Dates become numbers, text literals are kept; a path followed by a member (runTag.size()) is the
    // longest run parameter
    const std::string identifier =
        "date\\(\\s*\"[^\"]*\"\\s*\\)|\"(?:[^\"\\\\]|\\\\.)*\"|[A-Za-z_][A-Za-z0-9_]*(?:\\.[A-Za-z_][A-Za-z0-"
        "9_]*)*";
    return TRestTools::ReplaceIdentifiers(
        expression, identifier, [&](const std::string& match) -> std::optional<std::string> {
            if (match.front() == '"') return std::nullopt;
            if (match.rfind("date", 0) == 0 && match.find('(') != std::string::npos) {
                const size_t open = match.find('"');
                const std::string date = match.substr(open + 1, match.rfind('"') - open - 1);
                char number[32];
                snprintf(number, sizeof(number), "%.0f", TRestTools::StringToTimeStamp(date));
                return std::string(number);
            }
            const auto parts = TRestTools::Split(match, '.');
            if (columns.count(parts.front())) return std::nullopt;
            for (size_t length = parts.size(); length > 0; length--) {
                std::string path = parts[0];
                for (size_t i = 1; i < length; i++) path += "." + parts[i];
                std::string remainder;
                for (size_t i = length; i < parts.size(); i++) remainder += "." + parts[i];

                std::vector<std::string> texts;
                for (const auto& runParameters : parameters) {
                    const YAML::Node node = GetNodeAtPath(runParameters, path);
                    if (!node || !node.IsScalar()) break;
                    texts.push_back(node.Scalar());
                }
                if (texts.empty()) continue;
                if (texts.size() != fRuns.size()) {
                    throw std::runtime_error("TRestDataSet '" + fName + "': run parameter " + path +
                                             " is not in the run " + RunKeys(fRuns[texts.size()]).front());
                }

                std::string column = "runParameter_" + path;
                std::replace(column.begin(), column.end(), '.', '_');
                if (!frame.HasColumn(column)) {
                    std::vector<double> values;
                    for (const auto& text : texts) {
                        const auto value = REST_Units::ParseValueWithUnits(text);
                        if (!value) break;
                        values.push_back(*value);
                    }
                    if (values.size() == texts.size()) {
                        frame =
                            frame.Define(column, [values](int index) { return values[index]; }, {"runIndex"});
                    } else {
                        frame =
                            frame.Define(column, [texts](int index) { return texts[index]; }, {"runIndex"});
                    }
                }
                return column + remainder;
            }
            return std::nullopt;
        });
}

void TRestDataSet::Generate(const std::vector<TRestCut*>& cuts) {
    if (fFilePattern.empty()) throw std::runtime_error("TRestDataSet '" + fName + "': no filePattern");
    fInputFiles.clear();
    for (const auto& file : TRestTools::GetFilesMatchingPattern(fFilePattern)) {
        fInputFiles.push_back(TRestTools::GetFullPath(file));
    }
    std::sort(fInputFiles.begin(), fInputFiles.end());
    if (fInputFiles.empty()) {
        throw std::runtime_error("TRestDataSet '" + fName + "': no files match " + fFilePattern);
    }

    // Runs of each input file: its TRestRun, or the runs of a dataset
    const bool showProgress = fVerboseLevel >= TRestLogManager::REST_Verbose_Level::REST_Info;
    RESTInfo << "TRestDataSet '" << fName << "': reading the metadata of " << fInputFiles.size() << " files"
             << RESTendl;
    TRestProgressBar progress;
    if (showProgress) progress.Reset(static_cast<int>(fInputFiles.size()), 30, "Files");
    std::vector<std::vector<Run>> inputRuns;
    std::optional<bool> dataSets;
    fInputEntries = 0;
    fInputFileNode.reset();
    fInputMetadata.reset();
    for (const auto& fileName : fInputFiles) {
        std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "READ"));
        if (!file || file->IsZombie()) {
            throw std::runtime_error("TRestDataSet '" + fName + "': cannot open " + fileName);
        }
        auto* tree = file->Get<TTree>("AnalysisTree");
        if (tree) fInputEntries += tree->GetEntries();
        auto runs = ReadRuns(file.get());
        const bool isDataSet = runs.has_value();
        // The metadata of the first input file are kept (PrintAllMetadata, {instance.key} of
        // TRestAnalysisPlot)
        YAML::Node store;
        if (!isDataSet || inputRuns.empty()) store = TRestTools::ReadMetadataStore(file.get());
        if (inputRuns.empty()) fInputMetadata = store;
        if (!isDataSet) {
            const auto [instance, runNode] = TRestTools::GetMetadataClass(store, "TRestRun");
            if (!runNode || !tree) {
                throw std::runtime_error("TRestDataSet '" + fName + "': " + fileName +
                                         " is not a run file (AnalysisTree and TRestRun) nor a dataset");
            }
            runs = std::vector<Run>{
                {TRestRunInfo::FromConfig(instance, runNode), fileName, tree->GetEntries(), 0}};
        }
        if (dataSets && *dataSets != isDataSet) {
            throw std::runtime_error("TRestDataSet '" + fName +
                                     "': the inputs must be all run files or all datasets, " + fileName);
        }
        dataSets = isDataSet;
        inputRuns.push_back(std::move(*runs));
        if (showProgress) progress.Update(static_cast<int>(inputRuns.size()));
    }
    if (showProgress) progress.Finish();

    // Selection of the runs (runConditions), on a frame with an entry per run
    fRuns.clear();
    for (const auto& runs : inputRuns) fRuns.insert(fRuns.end(), runs.begin(), runs.end());
    std::set<int> selected;
    for (size_t i = 0; i < fRuns.size(); i++) selected.insert(i);
    if (!fRunConditions.empty()) {
        ROOT::RDF::RNode runFrame =
            ROOT::RDataFrame(fRuns.size())
                .Define("runIndex", [](ULong64_t entry) { return static_cast<int>(entry); }, {"rdfentry_"});
        for (const auto& [name, condition] : fRunConditions) {
            runFrame = runFrame.Filter(DefineRunParameters(runFrame, condition), name);
        }
        auto passed = runFrame.Take<int>("runIndex");
        selected = std::set<int>(passed->begin(), passed->end());
    }

    // New index of the runs of each input file, -1 if not selected
    std::vector<Run> allRuns = std::move(fRuns);
    fRuns.clear();
    std::vector<std::vector<int>> newIndex;
    std::map<std::string, std::string> keys;
    size_t position = 0;
    for (size_t f = 0; f < inputRuns.size(); f++) {
        newIndex.emplace_back(inputRuns[f].size(), -1);
        for (size_t r = 0; r < inputRuns[f].size(); r++, position++) {
            if (selected.count(position) == 0) continue;
            const Run& run = allRuns[position];
            for (const auto& key : RunKeys(run)) {
                const auto [it, inserted] = keys.emplace(key, run.fileName);
                if (!inserted) {
                    throw std::runtime_error("TRestDataSet '" + fName + "': " + it->first + " is twice, in " +
                                             it->second + " and " + run.fileName);
                }
            }
            newIndex[f][r] = fRuns.size();
            fRuns.push_back(run);
        }
    }
    if (fRuns.empty()) throw std::runtime_error("TRestDataSet '" + fName + "': no run selected");
    RESTInfo << "TRestDataSet '" << fName << "': " << fRuns.size() << " runs selected of " << allRuns.size()
             << RESTendl;

    fFrame = std::make_unique<ROOT::RDataFrame>("AnalysisTree", fInputFiles);
    std::vector<std::string> treeColumns = fFrame->GetColumnNames();
    ROOT::RDF::RNode frame(*fFrame);

    // Input file of each sample (file), from its id "fileName/AnalysisTree"
    std::map<std::string, int> sampleIds;
    for (size_t i = 0; i < fInputFiles.size(); i++) sampleIds[fInputFiles[i] + "/AnalysisTree"] = i;
    frame = frame.DefinePerSample(
        kInputFileColumn, [sampleIds](unsigned int, const ROOT::RDF::RSampleInfo& info) {
            const auto it = sampleIds.find(info.AsString());
            if (it == sampleIds.end()) {
                throw std::runtime_error("TRestDataSet: unknown input file " + info.AsString());
            }
            return it->second;
        });
    if (*dataSets) {
        frame = frame.Redefine("runIndex", [newIndex](int file, int run) { return newIndex[file][run]; },
                               {kInputFileColumn, "runIndex"});
    } else {
        frame =
            frame.Define("runIndex", [newIndex](int file) { return newIndex[file][0]; }, {kInputFileColumn});
    }
    if (fRuns.size() < allRuns.size())
        frame = frame.Filter([](int index) { return index >= 0; }, {"runIndex"});

    // Cuts on the entries
    fCutConfigs.clear();
    for (const auto& cutName : fCutNames) {
        TRestCut* cut = nullptr;
        for (auto* c : cuts) {
            if (c && c->GetName() == cutName) cut = c;
        }
        if (!cut)
            throw std::runtime_error("TRestDataSet '" + fName + "': TRestCut '" + cutName + "' not found");
        fCutConfigs.emplace_back(cut->GetName(), cut->GetYAMLNode());
        const std::string expression = cut->GetExpression();
        if (!expression.empty()) frame = frame.Filter(DefineRunParameters(frame, expression), cutName);
    }
    fDataFrame = frame;

    // Columns written: those of every event, runIndex, the observables and the new columns
    fColumns = kEventColumns;
    fColumns.push_back("runIndex");
    for (const auto& column : treeColumns) {
        if (std::find(fColumns.begin(), fColumns.end(), column) != fColumns.end()) continue;
        bool match = fObservables.empty();
        for (const auto& pattern : fObservables) {
            if (std::regex_match(column, std::regex(TRestTools::PatternToRegex(pattern)))) match = true;
        }
        if (match) fColumns.push_back(column);
    }
    for (const auto& [name, expression] : fColumnExpressions) Define(name, expression);
}

void TRestDataSet::Open(const std::string& fileName) {
    const std::string path = TRestTools::GetFullPath(fileName);
    std::unique_ptr<TFile> file(TFile::Open(path.c_str(), "READ"));
    if (!file || file->IsZombie()) throw std::runtime_error("TRestDataSet: cannot open " + fileName);
    auto runs = ReadRuns(file.get());
    if (!runs || !file->Get<TTree>("AnalysisTree")) {
        throw std::runtime_error("TRestDataSet: " + fileName + " is not a dataset");
    }

    // Its configuration and the TRestCut it applied, kept apart as TRestRun::fInputFileNode
    fInputMetadata = TRestTools::ReadMetadataStore(file.get());
    const auto [instance, node] = TRestTools::GetMetadataClass(fInputMetadata, "TRestDataSet");
    fInputFileNode.reset(node);
    if ((!fNode || !fNode.IsMap()) && !instance.empty()) SetName(instance);

    fRuns = std::move(*runs);
    fInputFiles = {path};
    fInputEntries = file->Get<TTree>("AnalysisTree")->GetEntries();
    fFrame = std::make_unique<ROOT::RDataFrame>("AnalysisTree", path);
    fDataFrame = *fFrame;
    fColumns = fFrame->GetColumnNames();
}

void TRestDataSet::Define(const std::string& name, const std::string& expression) {
    if (!fFrame) throw std::runtime_error("TRestDataSet '" + fName + "': Generate before Define");
    ROOT::RDF::RNode frame = fDataFrame;
    const std::string definition = DefineRunParameters(frame, expression);
    fDataFrame = frame.Define(name, definition);
    if (std::find(fColumns.begin(), fColumns.end(), name) == fColumns.end()) fColumns.push_back(name);
}

void TRestDataSet::Write(const std::string& fileName) {
    if (!fFrame) throw std::runtime_error("TRestDataSet '" + fName + "': Generate before Write");
    if (fileName.empty()) throw std::runtime_error("TRestDataSet '" + fName + "': no output file name");
    const std::string path = TRestTools::GetFullPath(fileName);
    if (std::find(fInputFiles.begin(), fInputFiles.end(), path) != fInputFiles.end()) {
        throw std::runtime_error("TRestDataSet '" + fName + "': the output file " + fileName +
                                 " is an input file");
    }

    // Booked before the Snapshot: filled in its event loop
    const int nRuns = fRuns.size();
    auto runEntries = fDataFrame.Histo1D<int>({"runEntries", "", nRuns, -0.5, nRuns - 0.5}, "runIndex");
    // Progress: entries read (before the cuts), from a count of the input in the same loop
    const bool showProgress = fVerboseLevel >= TRestLogManager::REST_Verbose_Level::REST_Info;
    TRestProgressBar progress;
    auto readEntries = ROOT::RDF::RNode(*fFrame).Count();
    if (showProgress && fInputEntries > 0) {
        const ULong64_t step = std::max<ULong64_t>(fInputEntries / 200, 1);
        readEntries.OnPartialResult(
            step, [&progress](ULong64_t& entries) { progress.Update(static_cast<int>(entries)); });
        RESTInfo << "TRestDataSet '" << fName << "': writing " << fileName << RESTendl;
        progress.Reset(static_cast<int>(fInputEntries), 30, "Entries");
    }
    fDataFrame.Snapshot("AnalysisTree", fileName, fColumns);
    if (showProgress && fInputEntries > 0) progress.Finish();
    for (int i = 0; i < nRuns; i++) fRuns[i].selectedEntries = std::llround(runEntries->GetBinContent(i + 1));

    std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "UPDATE"));
    if (!file || file->IsZombie())
        throw std::runtime_error("TRestDataSet '" + fName + "': cannot open " + fileName);
    // Its configuration, with its input (the files or dataset it comes from) and this output
    if (!fNode || !fNode.IsMap()) {
        fNode = YAML::Node(YAML::NodeType::Map);
        fNode["class"] = GetClassName();
    }
    fOutputFileName = path;
    WriteMetadata(file.get());
    for (const auto& [name, config] : fCutConfigs) TRestMetadata::WriteMetadata(file.get(), name, config);

    // A TObjString per run in RESTMetadataStore/RESTDataSetInfo (the store is created by WriteMetadata)
    TDirectory* store = file->GetDirectory("RESTMetadataStore");
    TDirectory* directory = store ? store->mkdir(kRunsDirectory.c_str(), "", true) : nullptr;
    if (!directory) throw std::runtime_error("TRestDataSet '" + fName + "': cannot create the runs folder");
    for (size_t i = 0; i < fRuns.size(); i++) {
        YAML::Node node;
        node["runIndex"] = i;
        node["fileName"] = fRuns[i].fileName;
        node["entries"] = fRuns[i].entries;
        node["selectedEntries"] = fRuns[i].selectedEntries;
        node["run"] = TRestTools::DetachYAMLNode(fRuns[i].run->GetYAMLNode());
        char key[32];
        snprintf(key, sizeof(key), "run_%06zu", i);
        directory->cd();
        TObjString text(YAML::Dump(node).c_str());
        text.Write(key, TObject::kOverwrite);
    }
    file->Close();

    RESTInfo << "TRestDataSet '" << fName << "': " << fRuns.size() << " runs written in " << fileName
             << RESTendl;
}

std::optional<std::vector<TRestDataSet::Run>> TRestDataSet::ReadRuns(TFile* file) {
    if (!file) return std::nullopt;
    TDirectory* directory = file->GetDirectory(("RESTMetadataStore/" + kRunsDirectory).c_str());
    if (!directory) return std::nullopt;

    // In the order of runIndex
    std::map<int, Run> byIndex;
    for (const auto* key : *directory->GetListOfKeys()) {
        std::unique_ptr<TObjString> text(directory->Get<TObjString>(key->GetName()));
        if (!text) continue;
        const YAML::Node node = YAML::Load(text->GetString().Data());
        Run run;
        const YAML::Node runNode = node["run"];
        run.run =
            TRestRunInfo::FromConfig(GetYAMLParam<std::string>(runNode, "name", key->GetName()), runNode);
        run.fileName = GetYAMLParam<std::string>(node, "fileName");
        run.entries = GetYAMLParam<Long64_t>(node, "entries", 0);
        run.selectedEntries = GetYAMLParam<Long64_t>(node, "selectedEntries", 0);
        byIndex[GetYAMLParam<int>(node, "runIndex", byIndex.size())] = run;
    }
    std::vector<Run> runs;
    for (auto& [index, run] : byIndex) {
        if (index != static_cast<int>(runs.size())) {
            throw std::runtime_error(std::string("TRestDataSet: runs of ") + file->GetName() +
                                     " not consecutive");
        }
        runs.push_back(std::move(run));
    }
    return runs;
}

std::vector<std::optional<std::string>> TRestDataSet::GetRunParameter(const std::string& name) const {
    std::vector<std::optional<std::string>> values;
    for (const auto& run : fRuns) {
        const YAML::Node node = GetNodeAtPath(run.run->GetParameters(), name);
        if (node && node.IsScalar())
            values.push_back(node.Scalar());
        else
            values.push_back(std::nullopt);
    }
    return values;
}

double TRestDataSet::GetTotalTime() const {
    double total = 0;
    for (const auto& run : fRuns) {
        total += run.run->fEndTime - run.run->fStartTime;
    }
    return total;
}

double TRestDataSet::GetStartTime() const {
    double start = 0;
    for (size_t i = 0; i < fRuns.size(); i++) {
        const double time = fRuns[i].run->fStartTime;
        if (i == 0 || time < start) start = time;
    }
    return start;
}

double TRestDataSet::GetEndTime() const {
    double end = 0;
    for (size_t i = 0; i < fRuns.size(); i++) {
        const double time = fRuns[i].run->fEndTime;
        if (i == 0 || time > end) end = time;
    }
    return end;
}

YAML::Node TRestDataSet::GetConfig() const {
    if (fNode && fNode.IsMap()) return fNode;
    return fInputFileNode;
}

void TRestDataSet::PrintMetadata() const {
    const YAML::Node config = GetConfig();
    if (!config || config.IsNull()) return;
    RESTMetadata << "=== TRestDataSet " << fName << " ===" << RESTendl;
    RESTMetadata << YAML::Dump(config) << RESTendl;
}

void TRestDataSet::PrintDataSetInfo() const {
    const YAML::Node config = GetConfig();
    RESTMetadata << "=== TRestDataSet " << fName << " ===" << RESTendl;
    // Generated from filePattern (or from the dataset inputFileName, opened and written again), its file
    const std::string pattern = GetYAMLParam<std::string>(config, "filePattern", "");
    const std::string input = GetYAMLParam<std::string>(config, "inputFileName", "");
    if (!pattern.empty()) RESTMetadata << " - Generated from: " << pattern << RESTendl;
    if (!input.empty()) RESTMetadata << " - Input dataset: " << input << RESTendl;
    RESTMetadata << " - File: " << GetYAMLParam<std::string>(config, "outputFileName", "") << RESTendl;
    Long64_t entries = 0, selectedEntries = 0;
    for (const auto& run : fRuns) {
        entries += run.entries;
        selectedEntries += run.selectedEntries;
    }
    RESTMetadata << " - Runs: " << fRuns.size() << RESTendl;
    if (fRuns.empty()) return;
    RESTMetadata << " - Start time: " << TRestTools::GetTimeStampFromUnixTime(GetStartTime()) << RESTendl;
    RESTMetadata << " - End time: " << TRestTools::GetTimeStampFromUnixTime(GetEndTime()) << RESTendl;
    RESTMetadata << " - Total run time: " << GetTotalTime() / 3600. << " hours" << RESTendl;
    RESTMetadata << " - Entries: " << selectedEntries << " of " << entries << RESTendl;
    for (size_t i = 0; i < fRuns.size(); i++) {
        const auto& run = fRuns[i];
        char line[64];
        snprintf(line, sizeof(line), " [%zu] run %d.%d", i, run.run->GetRunNumber(),
                 run.run->GetSubRunNumber());
        RESTMetadata << line << ", tag " << run.run->GetRunTag() << ", "
                     << TRestTools::GetTimeStampFromUnixTime(run.run->fStartTime) << ", "
                     << (run.run->fEndTime - run.run->fStartTime) / 3600. << " h, entries "
                     << run.selectedEntries << " of " << run.entries << ", " << run.fileName << RESTendl;
    }
}

void TRestDataSet::PrintAllMetadata() const {
    PrintMetadata();
    for (const auto& [name, config] : fCutConfigs) {
        RESTMetadata << "=== TRestCut " << name << " ===" << RESTendl;
        RESTMetadata << YAML::Dump(config) << RESTendl;
    }
    // Metadata of the input file (the dataset configuration is printed above if it is the one used)
    for (const auto& entry : fInputMetadata) {
        if (entry.second.is(GetConfig())) continue;
        RESTMetadata << "=== Input file: " << entry.first.as<std::string>() << " ===" << RESTendl;
        RESTMetadata << YAML::Dump(entry.second) << RESTendl;
    }
    for (const auto& run : fRuns) run.run->PrintMetadata();
    PrintDataSetInfo();
}
