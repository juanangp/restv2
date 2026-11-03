#include "TRestRunInfo.h"

#include <cstdio>
#include <stdexcept>

#include "TRestSystemOfUnits.h"
#include "TRestTools.h"
#include "TRestYAMLUtils.h"

using namespace TRestTools;

static const bool TRestRunInfo_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();

    reg.RegisterField<TRestRunInfo>("runNumber", &TRestRunInfo::fRunNumber);
    reg.RegisterField<TRestRunInfo>("subRunNumber", &TRestRunInfo::fSubRunNumber);
    reg.RegisterField<TRestRunInfo>("mergedSubRuns", &TRestRunInfo::fMergedSubRuns);
    reg.RegisterField<TRestRunInfo>("runType", &TRestRunInfo::fRunType);
    reg.RegisterField<TRestRunInfo>("runUser", &TRestRunInfo::fRunUser);
    reg.RegisterField<TRestRunInfo>("runTag", &TRestRunInfo::fRunTag);
    reg.RegisterField<TRestRunInfo>("runDescription", &TRestRunInfo::fRunDescription);
    reg.RegisterField<TRestRunInfo>("experimentName", &TRestRunInfo::fExperimentName);
    reg.RegisterField<TRestRunInfo>("runInfo", &TRestRunInfo::fRunInformation);
    reg.RegisterField<TRestRunInfo>("startTime", &TRestRunInfo::fStartTime);
    reg.RegisterField<TRestRunInfo>("endTime", &TRestRunInfo::fEndTime);
    reg.RegisterField<TRestRunInfo>("entriesSaved", &TRestRunInfo::fEntriesSaved);

    return true;
}();

std::shared_ptr<TRestRunInfo> TRestRunInfo::FromConfig(const std::string& instanceName,
                                                       const YAML::Node& node) {
    auto info = std::make_shared<TRestRunInfo>();
    info->SetName(instanceName);
    // Only the fields of TRestRunInfo are read; the node keeps only them
    info->UpdateParamsFromYAML<TRestRunInfo>(node);
    info->SetYAMLNode(info->GetParameters());
    return info;
}

void TRestRunInfo::PrintMetadata() const {
    RESTMetadata << "=== " << GetClassName() << " " << fName << " ===" << RESTendl;
    RESTMetadata << YAML::Dump(GetParameters()) << RESTendl;
}

YAML::Node TRestRunInfo::GetParameters() const {
    // From the members (the setters do not update the node), of a TRestRunInfo copy: the registered fields
    // of the derived classes (the processing of TRestRun) are not written
    TRestRunInfo info(*this);
    YAML::Node parameters(YAML::NodeType::Map);
    info.UpdateYAMLFromParams<TRestRunInfo>(parameters);
    return parameters;
}

std::optional<std::string> TRestRunInfo::GetParameter(const std::string& name) const {
    const YAML::Node node = TRestYAMLUtils::GetNodeAtPath(GetParameters(), name);
    if (!node || !node.IsScalar()) return std::nullopt;
    return node.Scalar();
}

std::optional<double> TRestRunInfo::GetParameterValue(const std::string& name) const {
    const YAML::Node node = TRestYAMLUtils::GetNodeAtPath(GetParameters(), name);
    if (!node || !node.IsScalar()) return std::nullopt;
    return REST_Units::ParseValueWithUnits(node.Scalar());
}

std::string TRestRunInfo::ResolveFilePattern(const std::string& pattern) const {
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
        const YAML::Node node = TRestYAMLUtils::GetNodeAtPath(parameters, name);
        if (!node || !node.IsScalar()) {
            throw std::runtime_error("TRestRunInfo: [" + name + "] in \"" + pattern +
                                     "\" is not a parameter of the run (its keys, e.g. [runNumber] or "
                                     "[runTag], or [runInfo.key])");
        }
        return CleanString(node.Scalar());
    });
}
