#ifndef TRESTDATASET_H
#define TRESTDATASET_H

#include <ROOT/RDataFrame.hxx>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "TRestMetadata.h"
#include "TRestRunInfo.h"

class TFile;
class TRestCut;

/// \class TRestDataSet
/// \brief Selection of the analysis trees of several runs (or of several datasets, merged) in a single
/// tree, with the description of each run (TRestRunInfo): every entry has the index of its run (`runIndex`),
/// so the information of the runs is never lost or summarized. See the class documentation in
/// TRestDataSet.cxx.
class TRestDataSet : public TRestMetadata {
   public:
    /// A run of the dataset
    struct Run {
        /// The description of the run, from the TRestRun stored in the run file (no file opened), e.g.
        /// run->GetRunNumber(), run->GetParameter("runInfo.meshVoltage"), run->PrintMetadata()
        std::shared_ptr<TRestRunInfo> run;
        /// File the run was read from (the run file, also when it comes from another dataset)
        std::string fileName;
        /// Entries of the analysis tree of the run file
        Long64_t entries = 0;
        /// Entries in the dataset (after the cuts), set by Write
        Long64_t selectedEntries = 0;
    };

   private:
    std::vector<Run> fRuns;
    /// Input files of the last Generate
    std::vector<std::string> fInputFiles;
    /// Entries of the analysis trees of the input files (progress of Write)
    Long64_t fInputEntries = 0;
    /// Columns written by Write
    std::vector<std::string> fColumns;
    /// `columns` as written (ordered: a column can use the previous ones)
    std::vector<std::pair<std::string, std::string>> fColumnExpressions;
    /// Configuration of the TRestCut applied, written with the dataset
    std::vector<std::pair<std::string, YAML::Node>> fCutConfigs;
    /// Configuration of the dataset of the input file (opened), as TRestRun::fInputFileNode
    YAML::Node fInputFileNode;
    /// Metadata of the input file: instance -> configuration; the dataset and its TRestCut of a dataset file
    /// opened, those of the first input file in Generate (e.g. the processing of a run file)
    YAML::Node fInputMetadata;
    std::unique_ptr<ROOT::RDataFrame> fFrame;           //!
    ROOT::RDF::RNode fDataFrame = ROOT::RDataFrame(0);  //!

    /// \brief The expression with the run parameters (identifiers that are paths of the TRestRunInfo of the
    /// runs, e.g. runTag, runInfo.meshVoltage) replaced by columns defined in `frame` from runIndex (a
    /// number in REST units if it is a number in all the runs, otherwise a std::string), and the dates
    /// date("2022/04/28 00:00") by their Unix time. Identifiers that are columns of the frame are kept.
    /// Throws if a parameter is not in all the runs.
    std::string DefineRunParameters(ROOT::RDF::RNode& frame, const std::string& expression) const;

    /// \brief Reads the registered fields of fNode (and the columns)
    void ReadConfig();

    /// \brief Opens a dataset file as it is (no run selection nor cuts): its runs and its AnalysisTree as
    /// data frame, and its stored configuration in fInputFileNode (the configuration of this instance is not
    /// changed). Throws if it is not a dataset.
    void Open(const std::string& fileName);

   public:
    /// Dataset file already generated, opened as it is (as TRestDataSet(fileName)); its stored
    /// configuration is not changed (fInputFileNode)
    std::string fInputFileName = "";
    /// Files the dataset is generated from (Generate): pattern with * and ? (TRestTools::
    /// GetFilesMatchingPattern) of run files or datasets
    std::string fFilePattern = "";
    /// Patterns (* and ?) of the analysis tree columns written, e.g. "trackLineAnalysis_*". Empty: all.
    /// eventID, subEventID, timeSeconds and runIndex are always written
    std::vector<std::string> fObservables = {};
    /// Conditions on the TRestRunInfo of each run (name: expression, as TRestCut) to select the runs
    std::map<std::string, std::string> fRunConditions = {};
    /// Names of the TRestCut applied to the entries
    std::vector<std::string> fCutNames = {};
    /// `columns`: name: expression of the new columns (observables and run parameters)
    YAML::Node fColumnsConfig;
    /// File the dataset is written to (Write; REST_GenerateDataSet if it is not given to the macro)
    std::string fOutputFileName = "";

    TRestDataSet();
    /// \brief A dataset file, opened as it is (as TRestRun(inputFileName)); throws if it is not a dataset
    explicit TRestDataSet(const std::string& fileName);
    TRestDataSet(const std::string& instanceName, const YAML::Node& node);
    TRestDataSet(const std::string& fileName, const std::string& sectionName);

    std::string GetClassName() const override { return "TRestDataSet"; }

    void LoadConfig() override;
    void Initialize() override {}
    /// \brief Configuration of the dataset: the one of this instance or, for a dataset file opened without
    /// configuration (TRestDataSet(fileName)), the one stored in the file
    void PrintMetadata() const override;
    /// \brief Summary: input and output, number of runs, times, entries, and a line per run
    void PrintDataSetInfo() const;
    /// \brief The configuration, the TRestCut, the metadata of the input file, the TRestRunInfo of every run
    /// and the summary (PrintDataSetInfo)
    void PrintAllMetadata() const;

    /// \brief Configuration of the dataset (see PrintMetadata)
    YAML::Node GetConfig() const;
    /// \brief Metadata of the input file (see fInputMetadata): instance -> configuration
    YAML::Node GetInputMetadata() const { return fInputMetadata; }

    /// \brief Selects the runs of the files of filePattern and builds the data frame. The inputs are either
    /// run files (output of restManager, with AnalysisTree and TRestRun) or datasets (merged: their runs
    /// are joined); the metadata are read directly from the files, no TRestRun is opened. Throws if a run
    /// is repeated (same runNumber and subRunNumber, or the same file for runs without number).
    /// \param cuts TRestCut available (those of `cuts` are taken by name, it is an error if one is missing).
    void Generate(const std::vector<TRestCut*>& cuts = {});

    /// \brief Adds a column to the data frame (also written), the expression can use observables and run
    /// parameters (see DefineRunParameters), e.g. "trackLineAnalysis_Energy / runInfo.gain"
    void Define(const std::string& name, const std::string& expression);

    /// \brief Writes the dataset: AnalysisTree with the columns, the metadata (this dataset and its
    /// TRestCut) in RESTMetadataStore and the runs in its folder RESTDataSetInfo. A single event loop, no
    /// intermediate files.
    void Write(const std::string& fileName);

    /// \brief Whether there is a data frame (dataset generated or opened)
    bool HasDataFrame() const { return fFrame != nullptr; }
    /// \brief Data frame of the dataset (generated or opened), with the column runIndex
    ROOT::RDF::RNode GetDataFrame() const { return fDataFrame; }
    const std::vector<Run>& GetRuns() const { return fRuns; }
    const std::vector<std::string>& GetColumns() const { return fColumns; }

    /// \brief Parameter of the TRestRunInfo of each run as written (e.g. "runTag", "runInfo.meshVoltage"),
    /// nullopt where it does not exist
    std::vector<std::optional<std::string>> GetRunParameter(const std::string& name) const;
    /// \brief Sum of endTime - startTime of the runs, in seconds
    double GetTotalTime() const;
    /// \brief Earliest startTime and latest endTime of the runs
    double GetStartTime() const;
    double GetEndTime() const;

    /// \brief Runs of a dataset file (RESTMetadataStore/RESTDataSetInfo), nullopt if it is not a dataset
    static std::optional<std::vector<Run>> ReadRuns(TFile* file);
};

#endif
