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
/// or with restManager, as a section of the TRestManager with the TRestCut it uses (no TRestRun): --i is
/// its filePattern and --o its outputFileName; with a TRestDataSetProcessManager in the same manager, the
/// dataset generated is processed by it in memory (--o is then the output of the pipeline) and only the
/// processed dataset is written, unless the TRestDataSet has an outputFileName:
///
///    restManager --c dataset.yaml --i "R*_ANA.root"
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
/// The metadata of the files are read directly (TRestYAMLUtils::ReadMetadataStore), no TRestRun is opened.
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
/// * outputFileName: the file written (Write; by the macro if it is not given to it), with [key] of its
///   runs (ResolveFilePattern): [runStart], [runEnd], [runRange] (00010_00012, or 00010 with a single run)
///   and the parameters of the runs that are the same in all of them; [runTag|Mixed] gives Mixed if they are
///   not the same (without default it is an error). E.g. "D_[runStart]_[runEnd]_[runTag|Mixed].root".
///
/// ### Dataset file
/// * AnalysisTree: the columns, with runIndex;
/// * RESTDataSetInfo: the runs, a TObjString with a YAML list, one element per run in the order of runIndex
///   (its position), with `run` (its TRestRunInfo), `fileName` (run file), `entries` (of the run file),
///   `selectedEntries` (in the dataset) and `processing` (index in RESTDataSetProcessing);
/// * RESTDataSetProcessing: the different processings of the runs, a TObjString with a YAML list (each one
///   instance -> configuration of the TRestEventProcessManager); datasets merged join their lists;
/// * RESTMetadataStore: this TRestDataSet (filePattern: what it was generated from, outputFileName: the
///   file) and the TRestCut used; also the metadata of the first input file with runs selected (processing,
///   readouts, TRestGeant4Metadata...), but its TRestRun (each run is in RESTDataSetInfo) and the
///   TRestDataSet it comes from (a dataset as input);
/// * Readouts and Geometry: the readouts (TRestDetectorReadout::Export) and the geometry (restG4) of that
///   file, e.g. a TRestDetectorReadoutManager with `readouts: [{instanceName: ..., decodingName: ...,
///   inputFileName: "Dataset.root"}]` loads a readout of the dataset.
///
/// The runs of a dataset must have the same setup, otherwise Generate throws: the processes (each
/// TRestEventProcessManager with the same processes, name and class, in the same order), the branches of
/// the analysis tree (name and type), the readouts (configuration, decodings and geometry) and the
/// geometry. The parameters of the processes can differ: each different processing (the configurations of
/// the TRestEventProcessManager, without eventsToProcess, nThreads and verbose) is kept (GetProcessings,
/// GetProcessing(runIndex)) and a warning lists the parameters that differ, with their values and runs. The
/// geometries are compared by their hash (TRestTools:: GetGeometryHash: volumes, with shape, material and
/// capacity, and the name and position of their daughters), written with them (TRestTools::WriteGeometry, by
/// restG4 and the readouts): they are not read, except in files written before the hash. The other metadata
/// (e.g. the seed of a simulation) can be different; those of the first file are written.
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
#include <TGeoManager.h>
#include <TH1D.h>
#include <TLeaf.h>
#include <TObjString.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>

#include "TRestCut.h"
#include "TRestSystemOfUnits.h"
#include "TRestTools.h"
#include "TRestYAMLUtils.h"

using TRestYAMLUtils::GetNodeAtPath;
using TRestYAMLUtils::GetParam;

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
/// Folders and objects of the input file written with the dataset: readouts (TRestDetectorReadout::Export)
/// and geometry (restG4)
const std::string kReadoutsDirectory = "Readouts";
const std::string kGeometryKey = "Geometry";
/// Index of the input file of each entry, internal
const std::string kInputFileColumn = "dataSetInputFile_";
/// Folder of RESTMetadataStore with the runs of a dataset
/// The runs of a dataset: a TObjString in the root of the file with a YAML sequence, one element per run
const std::string kRunsKey = "RESTDataSetInfo";
/// The different processings of the runs: a TObjString in the root of the file with a YAML sequence
const std::string kProcessingsKey = "RESTDataSetProcessing";

/// Identities of a run: runNumber and each of its subruns (subRunNumber, or the mergedSubRuns of a merged
/// run file) or, without run number, its file
/// The configuration without `verbose` (it does not change the processing)
YAML::Node WithoutVerbose(const YAML::Node& node) {
    YAML::Node copy = TRestYAMLUtils::Detach(node);
    std::function<void(YAML::Node)> strip = [&strip](YAML::Node n) {
        if (n.IsMap()) {
            n.remove("verbose");
            for (auto element : n) strip(element.second);
        } else if (n.IsSequence()) {
            for (auto element : n) strip(element);
        }
    };
    strip(copy);
    return copy;
}

/// The processing of a file: each TRestEventProcessManager, instance -> configuration, without
/// eventsToProcess, nThreads nor verbose (they do not change the processing)
YAML::Node Processing(const YAML::Node& store) {
    YAML::Node processing(YAML::NodeType::Map);
    for (const auto& entry : store) {
        const YAML::Node& config = entry.second;
        if (!config.IsMap() || !config["class"] ||
            config["class"].as<std::string>() != "TRestEventProcessManager")
            continue;
        YAML::Node manager = WithoutVerbose(config);
        manager.remove("eventsToProcess");
        manager.remove("nThreads");
        processing[entry.first.as<std::string>()] = manager;
    }
    return processing;
}

/// What the runs of a dataset must share (name -> description): the processes of each
/// TRestEventProcessManager (name and class, in order; their parameters can differ, see Processing), the
/// branches of the analysis tree (name and type), the readouts (configuration, decodings and hash of the
/// geometry) and the hash of the geometry (TRestTools::ReadGeometryHash, the geometry is not read)
std::map<std::string, std::string> DescribeSetup(TFile* file, const YAML::Node& store) {
    std::map<std::string, std::string> setup;
    for (const auto& manager : Processing(store)) {
        std::string processes;
        for (const auto& element : manager.second) {
            if (element.second.IsMap() && element.second["class"]) {
                processes +=
                    element.first.as<std::string>() + ": " + element.second["class"].as<std::string>() + "\n";
            }
        }
        setup["processes of '" + manager.first.as<std::string>() + "'"] = processes;
    }
    if (auto* tree = file->Get<TTree>("AnalysisTree")) {
        std::string branches;
        for (const auto* object : *tree->GetListOfLeaves()) {
            const auto* leaf = static_cast<const TLeaf*>(object);
            branches += std::string(leaf->GetName()) + "/" + leaf->GetTypeName() + "\n";
        }
        setup["branches of the analysis tree"] = branches;
    }
    if (TDirectory* readouts = file->GetDirectory(kReadoutsDirectory.c_str())) {
        for (auto* object : *readouts->GetListOfKeys()) {
            const std::string name = object->GetName();
            TDirectory* folder = readouts->GetDirectory(name.c_str());
            if (!folder) continue;
            std::string text = store[name] ? YAML::Dump(WithoutVerbose(store[name])) : "";
            if (TDirectory* decodings = folder->GetDirectory("Decodings")) {
                for (auto* key : *decodings->GetListOfKeys()) {
                    std::unique_ptr<TObjString> decoding(decodings->Get<TObjString>(key->GetName()));
                    if (decoding) text += std::string(key->GetName()) + ": " + decoding->GetString().Data();
                }
            }
            setup["readout '" + name + "'"] = text + TRestTools::ReadGeometryHash(folder, kGeometryKey);
        }
    }
    const std::string geometry = TRestTools::ReadGeometryHash(file, kGeometryKey);
    if (!geometry.empty()) setup["geometry"] = geometry;
    return setup;
}

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
    fOpened = false;
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
    std::vector<YAML::Node> stores;
    std::vector<std::map<std::string, std::string>> setups;
    // The processings of each input file: its own, or those of a dataset (Run::processing)
    std::vector<std::vector<YAML::Node>> inputProcessings;
    std::optional<bool> dataSets;
    fInputEntries = 0;
    fInputFileNode.reset();
    fInputMetadata.reset();
    fMetadataFile.clear();
    fMetadataStore.Clear();
    fMetadataStoreLoaded = false;
    for (const auto& fileName : fInputFiles) {
        std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "READ"));
        if (!file || file->IsZombie()) {
            throw std::runtime_error("TRestDataSet '" + fName + "': cannot open " + fileName);
        }
        auto* tree = file->Get<TTree>("AnalysisTree");
        if (tree) fInputEntries += tree->GetEntries();
        auto runs = ReadRuns(file.get());
        const bool isDataSet = runs.has_value();
        // The metadata of the first input file with runs selected are kept and written, the runs must have
        // the same setup (readouts, geometry, processes and observables; the parameters can differ)
        const YAML::Node store = TRestYAMLUtils::ReadMetadataStore(file.get());
        stores.push_back(store);
        setups.push_back(DescribeSetup(file.get(), store));
        auto processings = isDataSet ? ReadProcessings(file.get()) : std::vector<YAML::Node>{};
        if (processings.empty()) processings.push_back(Processing(store));
        inputProcessings.push_back(std::move(processings));
        if (!isDataSet) {
            const auto [instance, runNode] = TRestYAMLUtils::GetMetadataClass(store, "TRestRun");
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
                .Define("runIndex", [](unsigned long long entry) { return static_cast<int>(entry); },
                        {"rdfentry_"});
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
    fProcessings.clear();
    std::map<std::string, int> processingIndex;
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
            // Its processing, once in fProcessings
            const auto& processings = inputProcessings[f];
            if (run.processing < 0 || run.processing >= static_cast<int>(processings.size())) {
                throw std::runtime_error("TRestDataSet '" + fName + "': processing " +
                                         std::to_string(run.processing) + " of a run of " + fInputFiles[f] +
                                         " not found");
            }
            const std::string text = YAML::Dump(processings[run.processing]);
            const auto it = processingIndex.find(text);
            if (it != processingIndex.end()) {
                fRuns.back().processing = it->second;
            } else {
                fRuns.back().processing = fProcessings.size();
                processingIndex[text] = fProcessings.size();
                fProcessings.push_back(processings[run.processing]);
            }
        }
    }
    if (fRuns.empty()) throw std::runtime_error("TRestDataSet '" + fName + "': no run selected");

    // The setup of the files with runs selected, the same as the first one
    std::optional<size_t> first;
    for (size_t f = 0; f < inputRuns.size(); f++) {
        if (std::all_of(newIndex[f].begin(), newIndex[f].end(), [](int index) { return index < 0; }))
            continue;
        if (!first) {
            first = f;
            continue;
        }
        const auto& reference = setups[*first];
        const auto& current = setups[f];
        for (const auto* names : {&reference, &current}) {
            for (const auto& [name, description] : *names) {
                const auto it = setups[f].find(name);
                const auto referenceIt = reference.find(name);
                if (it != setups[f].end() && referenceIt != reference.end() &&
                    it->second == referenceIt->second)
                    continue;
                throw std::runtime_error("TRestDataSet '" + fName + "': the " + name + " of " +
                                         fInputFiles[f] + " is not the same as in " + fInputFiles[*first] +
                                         " (the runs of a dataset must have the same readouts, geometry, "
                                         "processes and observables)");
            }
        }
    }
    WarnProcessingDifferences();
    fInputMetadata = stores[*first];
    fMetadataFile = fInputFiles[*first];
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
    fInputMetadata = TRestYAMLUtils::ReadMetadataStore(file.get());
    const auto [instance, node] = TRestYAMLUtils::GetMetadataClass(fInputMetadata, "TRestDataSet");
    fInputFileNode.reset(node);
    if ((!fNode || !fNode.IsMap()) && !instance.empty()) SetName(instance);

    fRuns = std::move(*runs);
    fProcessings = ReadProcessings(file.get());
    if (fProcessings.empty()) fProcessings.push_back(Processing(fInputMetadata));
    fOpened = true;
    fMetadataFile = path;
    fMetadataStore.Clear();
    fMetadataStoreLoaded = false;
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

void TRestDataSet::SetDataFrame(ROOT::RDF::RNode frame, const std::vector<std::string>& newColumns) {
    if (!fFrame) throw std::runtime_error("TRestDataSet '" + fName + "': Generate before SetDataFrame");
    fDataFrame = frame;
    for (const auto& name : newColumns) {
        if (std::find(fColumns.begin(), fColumns.end(), name) == fColumns.end()) fColumns.push_back(name);
    }
}

std::string TRestDataSet::ResolveFilePattern(const std::string& pattern) const {
    return TRestTools::ReplaceDelimited(
        pattern, '[', ']', [&](const std::string& content) -> std::optional<std::string> {
            // [key] or [key|default]: the default when the runs do not share the value
            const size_t bar = content.find('|');
            const std::string key = content.substr(0, bar);
            const std::optional<std::string> fallback =
                bar == std::string::npos ? std::nullopt : std::optional<std::string>(content.substr(bar + 1));
            if (fRuns.empty()) {
                throw std::runtime_error("TRestDataSet '" + fName + "': [" + content + "] in \"" + pattern +
                                         "\" but the dataset has no runs");
            }
            if (key == "runStart" || key == "runEnd" || key == "runRange") {
                int start = fRuns.front().run->GetRunNumber(), end = start;
                for (const auto& run : fRuns) {
                    start = std::min(start, run.run->GetRunNumber());
                    end = std::max(end, run.run->GetRunNumber());
                }
                const std::string first = TRestTools::Format("%05d", start);
                const std::string last = TRestTools::Format("%05d", end);
                if (key == "runStart") return first;
                if (key == "runEnd") return last;
                return start == end ? first : first + "_" + last;
            }
            // A parameter of the runs: the same in all of them, otherwise the default or an error
            std::optional<std::string> value;
            for (const auto& run : fRuns) {
                std::string runValue;
                try {
                    runValue = run.run->ResolveFilePattern("[" + key + "]");
                } catch (const std::exception&) {
                    if (fallback) return fallback;
                    throw;
                }
                if (!value) value = runValue;
                if (runValue == *value) continue;
                if (fallback) return fallback;
                throw std::runtime_error("TRestDataSet '" + fName + "': [" + key + "] in \"" + pattern +
                                         "\" is not the same in all the runs: give a default, [" + key +
                                         "|Mixed], or e.g. [runRange] for the run numbers");
            }
            return value;
        });
}

void TRestDataSet::Write(const std::string& fileNamePattern) {
    if (!fFrame) throw std::runtime_error("TRestDataSet '" + fName + "': Generate before Write");
    if (fileNamePattern.empty())
        throw std::runtime_error("TRestDataSet '" + fName + "': no output file name");
    const std::string fileName = ResolveFilePattern(fileNamePattern);
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
        const unsigned long long step = std::max<unsigned long long>(fInputEntries / 200, 1);
        readEntries.OnPartialResult(
            step, [&progress](unsigned long long& entries) { progress.Update(static_cast<int>(entries)); });
        RESTInfo << "TRestDataSet '" << fName << "': writing " << fileName << RESTendl;
        progress.Reset(static_cast<int>(fInputEntries), 30, "Entries");
    }
    fDataFrame.Snapshot("AnalysisTree", fileName, fColumns);
    if (showProgress && fInputEntries > 0) progress.Finish();
    for (int i = 0; i < nRuns; i++) fRuns[i].selectedEntries = std::llround(runEntries->GetBinContent(i + 1));

    std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "UPDATE"));
    if (!file || file->IsZombie())
        throw std::runtime_error("TRestDataSet '" + fName + "': cannot open " + fileName);
    fOutputFileName = path;
    if (fOpened) {
        // A dataset file opened: its metadata (its TRestCut, the dataset processes applied...) and its
        // configuration, with this output
        for (const auto& entry : fInputMetadata) {
            const std::string instance = entry.first.as<std::string>();
            YAML::Node config = TRestYAMLUtils::Detach(entry.second);
            if (config.IsMap() && config["class"] && config["class"].as<std::string>() == GetClassName())
                config["outputFileName"] = path;
            TRestMetadata::WriteMetadata(file.get(), instance, config);
        }
    } else {
        // The metadata of the input file (processing, readouts...) but its run (in RESTDataSetInfo) and the
        // dataset it comes from (replaced by this one)
        for (const auto& entry : fInputMetadata) {
            const YAML::Node& config = entry.second;
            const std::string className =
                config.IsMap() && config["class"] ? config["class"].as<std::string>() : "";
            if (className == "TRestRun" || className == GetClassName()) continue;
            TRestMetadata::WriteMetadata(file.get(), entry.first.as<std::string>(), config);
        }
        // Its configuration, with its input (the files or dataset it comes from) and this output
        if (!fNode || !fNode.IsMap()) {
            fNode = YAML::Node(YAML::NodeType::Map);
            fNode["class"] = GetClassName();
        }
        WriteMetadata(file.get());
        for (const auto& [name, config] : fCutConfigs) TRestMetadata::WriteMetadata(file.get(), name, config);
    }

    // The readouts and the geometry of the input file
    if (!fMetadataFile.empty()) {
        std::unique_ptr<TFile> input(TFile::Open(fMetadataFile.c_str(), "READ"));
        if (input && !input->IsZombie()) {
            if (TDirectory* readouts = input->GetDirectory(kReadoutsDirectory.c_str())) {
                TDirectory* target = file->mkdir(kReadoutsDirectory.c_str(), "", true);
                TRestTools::CopyDirectory(readouts, target);
            }
            if (input->GetKey(kGeometryKey.c_str())) {
                // A copy read to write it, with its hash (also for files without it); gGeoManager kept
                std::unique_ptr<TGeoManager> geometry(
                    TRestTools::ReadGeometryCopy(input.get(), kGeometryKey));
                TRestTools::WriteGeometry(file.get(), geometry.get(), kGeometryKey);
            }
        }
        file->cd();
    }

    // The runs, in the order of runIndex: a YAML sequence in a TObjString (RESTDataSetInfo)
    YAML::Node runs(YAML::NodeType::Sequence);
    for (const auto& run : fRuns) {
        YAML::Node node;
        node["fileName"] = run.fileName;
        node["entries"] = run.entries;
        node["selectedEntries"] = run.selectedEntries;
        node["processing"] = run.processing;
        node["run"] = TRestYAMLUtils::Detach(run.run->GetYAMLNode());
        runs.push_back(node);
    }
    file->cd();
    TObjString text(YAML::Dump(runs).c_str());
    text.Write(kRunsKey.c_str(), TObject::kOverwrite);
    // The different processings, in the order of Run::processing (RESTDataSetProcessing)
    YAML::Node processings(YAML::NodeType::Sequence);
    for (const auto& processing : fProcessings) processings.push_back(TRestYAMLUtils::Detach(processing));
    TObjString processingsText(YAML::Dump(processings).c_str());
    processingsText.Write(kProcessingsKey.c_str(), TObject::kOverwrite);
    file->Close();

    RESTInfo << "TRestDataSet '" << fName << "': " << fRuns.size() << " runs written in " << fileName
             << RESTendl;
}

std::optional<std::vector<TRestDataSet::Run>> TRestDataSet::ReadRuns(TFile* file) {
    if (!file) return std::nullopt;
    std::unique_ptr<TObjString> text(file->Get<TObjString>(kRunsKey.c_str()));
    if (!text) return std::nullopt;
    const YAML::Node sequence = YAML::Load(text->GetString().Data());
    if (!sequence.IsSequence()) {
        throw std::runtime_error("TRestDataSet: " + kRunsKey + " of " + file->GetName() +
                                 " is not a list of runs");
    }

    // The runIndex of a run is its position
    std::vector<Run> runs;
    for (const auto& node : sequence) {
        Run run;
        const YAML::Node runNode = node["run"];
        const std::string name = TRestTools::Format("run_%06zu", runs.size());
        run.run = TRestRunInfo::FromConfig(GetParam<std::string>(runNode, "name", name), runNode);
        run.fileName = GetParam<std::string>(node, "fileName");
        run.entries = GetParam<long long>(node, "entries", 0);
        run.selectedEntries = GetParam<long long>(node, "selectedEntries", 0);
        run.processing = GetParam<int>(node, "processing", 0);
        runs.push_back(std::move(run));
    }
    return runs;
}

std::vector<YAML::Node> TRestDataSet::ReadProcessings(TFile* file) {
    std::vector<YAML::Node> processings;
    if (!file) return processings;
    std::unique_ptr<TObjString> text(file->Get<TObjString>(kProcessingsKey.c_str()));
    if (!text) return processings;
    const YAML::Node sequence = YAML::Load(text->GetString().Data());
    if (!sequence.IsSequence()) {
        throw std::runtime_error("TRestDataSet: " + kProcessingsKey + " of " + file->GetName() +
                                 " is not a list of processings");
    }
    for (const auto& processing : sequence) processings.push_back(processing);
    return processings;
}

YAML::Node TRestDataSet::GetProcessing(size_t runIndex) const {
    if (runIndex >= fRuns.size()) {
        throw std::runtime_error("TRestDataSet '" + fName + "': no run " + std::to_string(runIndex));
    }
    const int processing = fRuns[runIndex].processing;
    if (processing < 0 || processing >= static_cast<int>(fProcessings.size())) return YAML::Node();
    return fProcessings[processing];
}

void TRestDataSet::WarnProcessingDifferences() const {
    if (fProcessings.size() < 2) return;
    // Each parameter (path of keys) of each processing, the sequences in a line
    std::function<void(const YAML::Node&, const std::string&, std::map<std::string, std::string>&)> flatten =
        [&flatten](const YAML::Node& node, const std::string& path, std::map<std::string, std::string>& out) {
            if (node.IsMap()) {
                for (const auto& element : node) {
                    const std::string key = element.first.as<std::string>();
                    flatten(element.second, path.empty() ? key : path + "." + key, out);
                }
            } else if (node.IsScalar()) {
                out[path] = node.Scalar();
            } else {
                YAML::Emitter emitter;
                emitter << YAML::Flow << node;
                out[path] = emitter.c_str();
            }
        };
    std::vector<std::map<std::string, std::string>> parameters(fProcessings.size());
    std::set<std::string> paths;
    for (size_t i = 0; i < fProcessings.size(); i++) {
        flatten(fProcessings[i], "", parameters[i]);
        for (const auto& [path, value] : parameters[i]) paths.insert(path);
    }
    // The runs of each processing
    std::vector<std::vector<std::string>> runs(fProcessings.size());
    for (const auto& run : fRuns) runs[run.processing].push_back(RunKeys(run).front());

    RESTWarning
        << "TRestDataSet '" << fName << "': the runs have " << fProcessings.size()
        << " different processings (the same processes, different parameters); all of them are written "
           "(GetProcessings, RESTDataSetProcessing). Parameters that differ:"
        << RESTendl;
    for (const auto& path : paths) {
        // Value -> runs with it
        std::map<std::string, std::vector<std::string>> values;
        for (size_t i = 0; i < fProcessings.size(); i++) {
            const auto it = parameters[i].find(path);
            auto& list = values[it == parameters[i].end() ? "(not given)" : it->second];
            list.insert(list.end(), runs[i].begin(), runs[i].end());
        }
        if (values.size() < 2) continue;
        std::string text = " - " + path + ":";
        for (const auto& [value, runList] : values) {
            text += " " + value + " (" + std::to_string(runList.size()) + " runs: ";
            for (size_t r = 0; r < runList.size() && r < 3; r++) text += (r ? ", " : "") + runList[r];
            text += runList.size() > 3 ? ", ...)" : ")";
        }
        RESTWarning << text << RESTendl;
    }
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

void TRestDataSet::LoadMetadataStore() const {
    fMetadataStoreLoaded = true;
    fMetadataStore.Clear();
    if (fMetadataFile.empty()) return;
    std::unique_ptr<TFile> file(TFile::Open(fMetadataFile.c_str(), "READ"));
    if (!file || file->IsZombie()) {
        RESTError << "TRestDataSet '" << fName << "': cannot open " << fMetadataFile << RESTendl;
        return;
    }
    fMetadataStore.Load(file.get(), nullptr, {"TRestRun", GetClassName()});
}

const TRestMetadataStore& TRestDataSet::GetMetadataStore() const {
    if (!fMetadataStoreLoaded) LoadMetadataStore();
    return fMetadataStore;
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
    const std::string pattern = GetParam<std::string>(config, "filePattern", "");
    const std::string input = GetParam<std::string>(config, "inputFileName", "");
    if (!pattern.empty()) RESTMetadata << " - Generated from: " << pattern << RESTendl;
    if (!input.empty()) RESTMetadata << " - Input dataset: " << input << RESTendl;
    RESTMetadata << " - File: " << GetParam<std::string>(config, "outputFileName", "") << RESTendl;
    long long entries = 0, selectedEntries = 0;
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
