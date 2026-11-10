#include "TRestDataSetCombine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <regex>
#include <stdexcept>
#include <unordered_map>

#include "ROOT/RVec.hxx"
#include "TRestDataSet.h"
#include "TRestTools.h"

/// \class TRestDataSetCombine
///
/// Combination of two datasets of the same events, a process of TRestDataSetProcessManager: the columns of
/// another dataset (`combineFileName`) are added to the dataset processed, matching the entries by run
/// (runNumber and subRunNumber of their runs), eventID and subEventID. E.g. in IAXO the vetoes and the
/// Micromegas are processed in separate pipelines (TRestGeant4ToDetectorHitsProcess or
/// TRestRawSignalChannelSelectionProcess keep the channels of each detector) and analysed apart; their
/// datasets are combined to analyse them together (e.g. a veto cut on the events of the Micromegas).
///
/// The dataset processed is the base: an entry of combineFileName without match in it is not added (e.g. a
/// signal in the vetoes without event in the Micromegas). An entry of the dataset without match in
/// combineFileName (e.g. an event of the Micromegas without signal in the vetoes, rejected by their pipeline)
/// is kept with keepUnmatched (default) and its combined columns take a default value: NaN (floating point),
/// 0 (integers, bool), empty (texts, vectors). The column `<name>_matched` (int, `name` the instance name of
/// the process) is 1 for the matched entries and 0 for the others, e.g. for a cut
/// `vetoCombine_matched == 0 || vetoAna_MaxPeakAmplitude < 100`. Without keepUnmatched only the matched
/// entries are kept (as a filter: the selectedEntries of the runs are those of the output).
///
/// The columns combined are those of combineFileName matching `observables` (patterns with * and ?; all if
/// empty), but eventID, subEventID, timeSeconds and runIndex, with `prefix` before their name. The names of
/// the observables have the instance name of their process (e.g. vetoAna_...), so different pipelines do not
/// collide; a column that already exists in the dataset (e.g. the same TRestGeant4AnalysisProcess in both
/// pipelines) is an error: give a prefix or select the observables. Types: the scalars (double, float, int,
/// unsigned int, long long, unsigned long long, bool, std::string) and vectors of double, float and int (read
/// as ROOT::RVec). The columns of combineFileName are read in memory once (a single event loop). An event
/// twice in combineFileName (same run, eventID and subEventID) is an error; a run of the dataset that is not
/// in combineFileName is reported (all its entries are unmatched).
///
/// ### Parameters
/// * combineFileName: the dataset whose columns are added.
/// * observables: patterns of its columns added (empty: all).
/// * prefix: added to the names of the columns combined (empty by default).
/// * keepUnmatched: keep the entries without match (true by default).
///
/// The referenceFileName and cuts of TRestDataSetProcess are not used (there are no parameters).
///
/// Example (in the TRestDataSetProcessManager of the dataset of the Micromegas):
///
///    vetoCombine:
///      class: "TRestDataSetCombine"
///      combineFileName: "Dataset_Veto.root"
///      observables: ["vetoAna_*"]
///      keepUnmatched: true

static const bool TRestDataSetCombine_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestDataSetCombine>("combineFileName", &TRestDataSetCombine::fCombineFileName);
    reg.RegisterField<TRestDataSetCombine>("observables", &TRestDataSetCombine::fObservables);
    reg.RegisterField<TRestDataSetCombine>("prefix", &TRestDataSetCombine::fPrefix);
    reg.RegisterField<TRestDataSetCombine>("keepUnmatched", &TRestDataSetCombine::fKeepUnmatched);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDataSetCombine", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDataSetCombine>(instanceName, params);
        });
    return true;
}();

/// Columns of every event and the run index: the keys, not combined
const std::vector<std::string> kKeyColumns = {"eventID", "subEventID", "timeSeconds", "runIndex"};

/// An event: run (index in combineFileName), eventID, subEventID
struct EventKey {
    int run;
    int event;
    int subEvent;
    bool operator==(const EventKey& other) const {
        return run == other.run && event == other.event && subEvent == other.subEvent;
    }
};
struct EventKeyHash {
    size_t operator()(const EventKey& key) const {
        size_t hash = std::hash<int>()(key.run);
        hash = hash * 1000003u ^ std::hash<int>()(key.event);
        return hash * 1000003u ^ std::hash<int>()(key.subEvent);
    }
};

/// A column of combineFileName read in memory (booked before the event loop), defined in the dataset
class CombinedColumn {
   public:
    virtual ~CombinedColumn() = default;
    /// \brief Defines `name` in frame: the value of the entry of combineFileName in the column `entry`, the
    /// default where it is -1
    virtual ROOT::RDF::RNode Define(ROOT::RDF::RNode frame, const std::string& name,
                                    const std::string& entry) = 0;
};

template <typename T>
class TypedColumn : public CombinedColumn {
    ROOT::RDF::RResultPtr<std::vector<T>> fValues;
    T fDefault;

   public:
    TypedColumn(ROOT::RDF::RNode& frame, const std::string& column, T fallback)
        : fValues(frame.Take<T>(column)), fDefault(std::move(fallback)) {}
    ROOT::RDF::RNode Define(ROOT::RDF::RNode frame, const std::string& name,
                            const std::string& entry) override {
        auto values = std::make_shared<const std::vector<T>>(std::move(*fValues));
        const T fallback = fDefault;
        return frame.Define(
            name,
            [values, fallback](long long index) -> T { return index >= 0 ? T((*values)[index]) : fallback; },
            {entry});
    }
};

/// The type of a column without the ROOT aliases (Double_t: double...) and spaces
std::string NormalizeType(std::string type) {
    type.erase(std::remove(type.begin(), type.end(), ' '), type.end());
    // The unsigned ones first (UInt_t contains Int_t)
    static const std::vector<std::pair<std::string, std::string>> aliases = {
        {"UInt_t", "unsignedint"},
        {"ULong64_t", "unsignedlonglong"},
        {"Double_t", "double"},
        {"Float_t", "float"},
        {"Int_t", "int"},
        {"Long64_t", "longlong"},
        {"Bool_t", "bool"},
        {"std::", ""},
        {"ROOT::VecOps::RVec", "vector"},
        {"ROOT::RVec", "vector"},
        {"RVec", "vector"}};
    for (const auto& [alias, name] : aliases) {
        for (size_t at = type.find(alias); at != std::string::npos; at = type.find(alias, at + name.size())) {
            type.replace(at, alias.size(), name);
        }
    }
    return type;
}

/// Books the reading of a column of the type given, nullptr if the type is not supported
std::unique_ptr<CombinedColumn> BookColumn(ROOT::RDF::RNode& frame, const std::string& column,
                                           const std::string& type) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const std::string t = NormalizeType(type);
    if (t == "double") return std::make_unique<TypedColumn<double>>(frame, column, nan);
    if (t == "float") return std::make_unique<TypedColumn<float>>(frame, column, static_cast<float>(nan));
    if (t == "int") return std::make_unique<TypedColumn<int>>(frame, column, 0);
    if (t == "unsignedint") return std::make_unique<TypedColumn<unsigned int>>(frame, column, 0u);
    if (t == "longlong") return std::make_unique<TypedColumn<long long>>(frame, column, 0LL);
    if (t == "unsignedlonglong")
        return std::make_unique<TypedColumn<unsigned long long>>(frame, column, 0ULL);
    if (t == "bool") return std::make_unique<TypedColumn<bool>>(frame, column, false);
    if (t == "string") return std::make_unique<TypedColumn<std::string>>(frame, column, std::string());
    if (t == "vector<double>")
        return std::make_unique<TypedColumn<ROOT::RVec<double>>>(frame, column, ROOT::RVec<double>());
    if (t == "vector<float>")
        return std::make_unique<TypedColumn<ROOT::RVec<float>>>(frame, column, ROOT::RVec<float>());
    if (t == "vector<int>")
        return std::make_unique<TypedColumn<ROOT::RVec<int>>>(frame, column, ROOT::RVec<int>());
    return nullptr;
}
}  // namespace

TRestDataSetCombine::TRestDataSetCombine() : TRestDataSetProcess() { fName = "TRestDataSetCombine"; }

TRestDataSetCombine::TRestDataSetCombine(const std::string& instanceName, const YAML::Node& node)
    : TRestDataSetProcess(instanceName, node) {
    LoadConfig();
}

void TRestDataSetCombine::LoadConfig() {
    UpdateParamsFromYAML<TRestDataSetCombine>(fNode);
    if (fCombineFileName.empty())
        throw std::runtime_error("TRestDataSetCombine '" + fName + "': no combineFileName");
    UpdateYAMLFromParams<TRestDataSetCombine>(fNode);
}

void TRestDataSetCombine::ComputeParameters(const TRestDataSet&, ROOT::RDF::RNode) {}

void TRestDataSetCombine::Apply(TRestDataSet& dataSet) const {
    const std::string context = "TRestDataSetCombine '" + fName + "'";
    TRestDataSet other(fCombineFileName);
    ROOT::RDF::RNode otherFrame = other.GetDataFrame();

    // The columns combined
    std::vector<std::string> columns;
    for (const auto& column : other.GetColumns()) {
        if (std::find(kKeyColumns.begin(), kKeyColumns.end(), column) != kKeyColumns.end()) continue;
        bool match = fObservables.empty();
        for (const auto& pattern : fObservables) {
            if (std::regex_match(column, std::regex(TRestTools::PatternToRegex(pattern)))) match = true;
        }
        if (match) columns.push_back(column);
    }
    if (columns.empty()) {
        throw std::runtime_error(context + ": no column of " + fCombineFileName + " matches the observables");
    }

    ROOT::RDF::RNode frame = dataSet.GetDataFrame();
    const auto existing = frame.GetColumnNames();
    auto exists = [&existing](const std::string& name) {
        return std::find(existing.begin(), existing.end(), name) != existing.end();
    };
    const std::string entryColumn = fName + "_entry";
    const std::string matchedColumn = fName + "_matched";
    for (const auto& name : {entryColumn, matchedColumn}) {
        if (exists(name)) throw std::runtime_error(context + ": the dataset already has a column " + name);
    }

    // Reading of the keys and the columns of combineFileName, booked for a single event loop
    auto runs = otherFrame.Take<int>("runIndex");
    auto events = otherFrame.Take<int>("eventID");
    auto subEvents = otherFrame.Take<int>("subEventID");
    std::vector<std::pair<std::string, std::unique_ptr<CombinedColumn>>> combined;
    for (const auto& column : columns) {
        const std::string name = fPrefix + column;
        if (exists(name)) {
            throw std::runtime_error(context + ": the dataset already has a column " + name +
                                     " (give a prefix or select the observables)");
        }
        const std::string type = otherFrame.GetColumnType(column);
        auto booked = BookColumn(otherFrame, column, type);
        if (!booked) {
            throw std::runtime_error(context + ": the column " + column + " of " + fCombineFileName +
                                     " has a type that cannot be combined (" + type + ")");
        }
        combined.emplace_back(name, std::move(booked));
    }

    // The runs of the dataset in combineFileName (runNumber and subRunNumber), -1 if not there
    auto runMap = std::make_shared<std::vector<int>>();
    int missingRuns = 0;
    for (const auto& run : dataSet.GetRuns()) {
        int index = -1;
        for (size_t r = 0; r < other.GetRuns().size(); r++) {
            const auto& candidate = *other.GetRuns()[r].run;
            if (candidate.GetRunNumber() == run.run->GetRunNumber() &&
                candidate.GetSubRunNumber() == run.run->GetSubRunNumber()) {
                index = static_cast<int>(r);
                break;
            }
        }
        if (index < 0) missingRuns++;
        runMap->push_back(index);
    }
    if (missingRuns > 0) {
        RESTWarning << context << ": " << missingRuns << " runs of the dataset are not in "
                    << fCombineFileName << " (their entries are unmatched)" << RESTendl;
    }

    // Entry of each event of combineFileName (the event loop runs here)
    auto entries = std::make_shared<std::unordered_map<EventKey, long long, EventKeyHash>>();
    for (size_t n = 0; n < runs->size(); n++) {
        const EventKey key{(*runs)[n], (*events)[n], (*subEvents)[n]};
        if (!entries->emplace(key, static_cast<long long>(n)).second) {
            throw std::runtime_error(context + ": the event " + std::to_string(key.event) + " (subevent " +
                                     std::to_string(key.subEvent) + ") of the run index " +
                                     std::to_string(key.run) + " is twice in " + fCombineFileName);
        }
    }
    RESTInfo << context << ": " << columns.size() << " columns of " << entries->size() << " entries of "
             << fCombineFileName << RESTendl;

    frame = frame.Define(entryColumn,
                         [runMap, entries](int run, int event, int subEvent) -> long long {
                             if (run < 0 || run >= static_cast<int>(runMap->size()) || (*runMap)[run] < 0)
                                 return -1;
                             const auto found = entries->find({(*runMap)[run], event, subEvent});
                             return found == entries->end() ? -1 : found->second;
                         },
                         {"runIndex", "eventID", "subEventID"});
    frame = frame.Define(matchedColumn, [](long long entry) { return entry >= 0 ? 1 : 0; }, {entryColumn});

    std::vector<std::string> newColumns;
    for (auto& [name, column] : combined) {
        frame = column->Define(frame, name, entryColumn);
        newColumns.push_back(name);
    }
    newColumns.push_back(matchedColumn);
    if (!fKeepUnmatched)
        frame = frame.Filter([](long long entry) { return entry >= 0; }, {entryColumn}, fName);
    dataSet.SetDataFrame(frame, newColumns);
}
