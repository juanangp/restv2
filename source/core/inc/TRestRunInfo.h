#ifndef TRESTRUNINFO_H
#define TRESTRUNINFO_H

#include <Rtypes.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "TRestMetadata.h"

/// \class TRestRunInfo
/// \brief Description of a run: numbers, tags, runInfo, times and entries, without anything of the
/// processing (files, input format...). TRestRun is a TRestRunInfo plus the processing; the runs of a
/// TRestDataSet are TRestRunInfo (built from the TRestRun stored in the run files, no file opened).
///
/// Its keys are part of the TRestRun section of a configuration:
///
/// \code{.yaml}
/// run:
///   class: TRestRun
///   runNumber: 123
///   subRunNumber: 0
///   runType: calibration
///   runUser: rest
///   runTag: AlphaCAMM
///   runDescription: Calibration with an Am-241 source
///   experimentName: AlphaCAMM
///   runInfo:
///     meshVoltage: 300 V
///     driftVoltage: 1000 V
///   # mergedSubRuns, startTime, endTime and entriesSaved are written by REST
/// \endcode
class TRestRunInfo : public TRestMetadata {
   public:
    int fRunNumber = -1;    ///< Run number (`runNumber`), -1 if not set.
    int fSubRunNumber = 0;  ///< Subrun number (`subRunNumber`).
    /// Subruns of a run file merged from several (REST_MergeRuns), consecutive; fSubRunNumber is the first.
    /// Empty for a single subrun. Kept when the file is processed again
    std::vector<int> fMergedSubRuns;
    std::string fRunType = "Null";         ///< Run type (`runType`).
    std::string fRunUser = "Null";         ///< User of the run (`runUser`).
    std::string fRunTag = "Null";          ///< Run tag (`runTag`), often used in output file names.
    std::string fRunDescription = "Null";  ///< Free description (`runDescription`).
    std::string fExperimentName = "Null";  ///< Experiment name (`experimentName`).
    /// Information of the run (`runInfo`), e.g. driftVoltage: 1000 V, gain: 50, as written (with units):
    /// the one of the input file, with the keys of the configuration added or replacing it
    std::map<std::string, std::string> fRunInformation;
    double fStartTime = 0;        ///< Start timestamp of the run, unix time in seconds (`startTime`).
    double fEndTime = 0;          ///< End timestamp of the run, unix time in seconds (`endTime`).
    long long fEntriesSaved = 0;  ///< Number of entries saved in the run file (`entriesSaved`).

    /// \brief Default constructor.
    TRestRunInfo() = default;
    /// \brief Constructs from a YAML node (see TRestMetadata).
    TRestRunInfo(const std::string& instanceName, const YAML::Node& node)
        : TRestMetadata(instanceName, node) {}
    /// \brief Constructs from a section of a YAML file (see TRestMetadata).
    TRestRunInfo(const std::string& fileName, const std::string& sectionName)
        : TRestMetadata(fileName, sectionName) {}

    /// \brief The description of a run from the configuration of a TRestRun (e.g. the one stored in a run
    /// file): only its parameters, the keys of the processing are ignored. No file is opened.
    static std::shared_ptr<TRestRunInfo> FromConfig(const std::string& instanceName, const YAML::Node& node);

    /// \brief Returns "TRestRunInfo".
    std::string GetClassName() const override { return "TRestRunInfo"; }

    /// \brief Reads the registered fields from the node.
    void LoadConfig() override { UpdateParamsFromYAML<TRestRunInfo>(fNode); }
    /// \brief Nothing to initialize.
    void Initialize() override {}
    /// \brief The parameters of the run (GetParameters)
    void PrintMetadata() const override;

    int GetRunNumber() const { return fRunNumber; }                    ///< Run number.
    int GetSubRunNumber() const { return fSubRunNumber; }              ///< Subrun number.
    std::string GetRunType() const { return fRunType; }                ///< Run type.
    std::string GetRunUser() const { return fRunUser; }                ///< Run user.
    std::string GetRunTag() const { return fRunTag; }                  ///< Run tag.
    std::string GetRunDescription() const { return fRunDescription; }  ///< Run description.
    std::string GetExperimentName() const { return fExperimentName; }  ///< Experiment name.
    /// \brief Returns the information of the run (`runInfo`), key: value as written.
    const std::map<std::string, std::string>& GetRunInformation() const { return fRunInformation; }
    double GetStartTime() const { return fStartTime; }           ///< Start timestamp (unix seconds).
    double GetEndTime() const { return fEndTime; }               ///< End timestamp (unix seconds).
    long long GetEntriesSaved() const { return fEntriesSaved; }  ///< Entries saved in the run file.

    /// \name Setters
    /// The setters change the members only, not the YAML node (GetParameters reads the members).
    ///@{
    void SetRunNumber(int v) { fRunNumber = v; }                           ///< Sets the run number.
    void SetSubRunNumber(int v) { fSubRunNumber = v; }                     ///< Sets the subrun number.
    void SetRunType(const std::string& v) { fRunType = v; }                ///< Sets the run type.
    void SetRunUser(const std::string& v) { fRunUser = v; }                ///< Sets the run user.
    void SetRunTag(const std::string& v) { fRunTag = v; }                  ///< Sets the run tag.
    void SetRunDescription(const std::string& v) { fRunDescription = v; }  ///< Sets the description.
    void SetExperimentName(const std::string& v) { fExperimentName = v; }  ///< Sets the experiment name.
    void SetStartTimeStamp(double v) { fStartTime = v; }                   ///< Sets the start time (unix s).
    void SetEndTimeStamp(double v) { fEndTime = v; }                       ///< Sets the end time (unix s).
    void SetEntriesSaved(long long v) { fEntriesSaved = v; }               ///< Sets the saved entries.
    ///@}

    /// \brief Parameters of the run (the fields of TRestRunInfo, current values; also for a TRestRun, without
    /// its processing fields) as a YAML node.
    YAML::Node GetParameters() const;

    /// \brief Returns a parameter of the run as written, e.g. "runNumber", "runTag" or "runInfo.meshVoltage"
    /// (a key of runInfo). From other metadata through their run, e.g. fRunInfo->GetParameter(...).
    /// \return Text of the parameter, nullopt if it does not exist.
    std::optional<std::string> GetParameter(const std::string& name) const;

    /// \brief Returns a numerical parameter of the run in REST units, e.g. "runInfo.meshVoltage" (300 V).
    /// \return Value, nullopt if it does not exist or it is not a number.
    std::optional<double> GetParameterValue(const std::string& name) const;

    /// \brief Resolves placeholders in file patterns: [parameter], a key of the run (see GetParameter, e.g.
    /// [runNumber] or [runInfo.meshVoltage]), and [version] (the project version, PROJECT_VERSION of CMake).
    /// Throws for any other. [runNumber] is written
    /// with 5 digits and [subRunNumber] with 3 (zero padded).
    /// \param pattern Input filename pattern.
    /// \return Resolved filename.
    std::string ResolveFilePattern(const std::string& pattern) const;
};

#endif
