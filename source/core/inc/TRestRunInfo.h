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
class TRestRunInfo : public TRestMetadata {
   public:
    int fRunNumber = -1;
    int fSubRunNumber = 0;
    /// Subruns of a run file merged from several (REST_MergeRuns), consecutive; fSubRunNumber is the first.
    /// Empty for a single subrun. Kept when the file is processed again
    std::vector<int> fMergedSubRuns;
    std::string fRunType = "Null";
    std::string fRunUser = "Null";
    std::string fRunTag = "Null";
    std::string fRunDescription = "Null";
    std::string fExperimentName = "Null";
    /// Information of the run (`runInfo`), e.g. driftVoltage: 1000 V, gain: 50, as written (with units):
    /// the one of the input file, with the keys of the configuration added or replacing it
    std::map<std::string, std::string> fRunInformation;
    double fStartTime = 0;
    double fEndTime = 0;
    long long fEntriesSaved = 0;

    TRestRunInfo() = default;
    TRestRunInfo(const std::string& instanceName, const YAML::Node& node)
        : TRestMetadata(instanceName, node) {}
    TRestRunInfo(const std::string& fileName, const std::string& sectionName)
        : TRestMetadata(fileName, sectionName) {}

    /// \brief The description of a run from the configuration of a TRestRun (e.g. the one stored in a run
    /// file): only its parameters, the keys of the processing are ignored. No file is opened.
    static std::shared_ptr<TRestRunInfo> FromConfig(const std::string& instanceName, const YAML::Node& node);

    std::string GetClassName() const override { return "TRestRunInfo"; }

    void LoadConfig() override { UpdateParamsFromYAML<TRestRunInfo>(fNode); }
    void Initialize() override {}
    /// \brief The parameters of the run (GetParameters)
    void PrintMetadata() const override;

    int GetRunNumber() const { return fRunNumber; }
    int GetSubRunNumber() const { return fSubRunNumber; }
    std::string GetRunType() const { return fRunType; }
    std::string GetRunUser() const { return fRunUser; }
    std::string GetRunTag() const { return fRunTag; }
    std::string GetRunDescription() const { return fRunDescription; }
    std::string GetExperimentName() const { return fExperimentName; }
    /// \brief Returns the information of the run (`runInfo`), key: value as written.
    const std::map<std::string, std::string>& GetRunInformation() const { return fRunInformation; }
    double GetStartTime() const { return fStartTime; }
    double GetEndTime() const { return fEndTime; }
    long long GetEntriesSaved() const { return fEntriesSaved; }

    void SetRunNumber(int v) { fRunNumber = v; }
    void SetSubRunNumber(int v) { fSubRunNumber = v; }
    void SetRunType(const std::string& v) { fRunType = v; }
    void SetRunUser(const std::string& v) { fRunUser = v; }
    void SetRunTag(const std::string& v) { fRunTag = v; }
    void SetRunDescription(const std::string& v) { fRunDescription = v; }
    void SetExperimentName(const std::string& v) { fExperimentName = v; }
    void SetStartTimeStamp(double v) { fStartTime = v; }
    void SetEndTimeStamp(double v) { fEndTime = v; }

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
    /// [runNumber] or [runInfo.meshVoltage]), and [version]. Throws for any other.
    /// \param pattern Input filename pattern.
    /// \return Resolved filename.
    std::string ResolveFilePattern(const std::string& pattern) const;
};

#endif
