#ifndef TRESTTOOLS_H
#define TRESTTOOLS_H

#include <yaml-cpp/yaml.h>

#include <functional>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

class TFile;

namespace TRestTools {

std::vector<std::string> GetFilesMatchingPattern(const std::string& filePattern);
std::string GetTimeStampFromUnixTime(const double tm);
/// \brief Unix time of a local date, "YYYY/MM/DD", "YYYY-MM-DD" and optionally " HH:MM" or " HH:MM:SS" (as
/// legacy REST_StringHelper::StringToTimeStamp). Throws if it is not a valid date.
double StringToTimeStamp(const std::string& date);
std::string GetFullPath(const std::string& filename);
bool fileExists(const std::string& filename);
bool isRootFile(const std::string& filename);
bool isValidTRestRun(const std::string& filename);
/// \brief Whether the file is a dataset (TRestDataSet: AnalysisTree and RESTMetadataStore/RESTDataSetInfo)
bool isDataSet(const std::string& filename);
/// \brief Metadata stored in a REST file (directory RESTMetadataStore, a YAML TObjString per instance),
/// read without creating them (no TRestRun is opened): map instance name -> configuration (with `class`).
/// Empty if the file has no metadata. Throws if a configuration cannot be parsed.
YAML::Node ReadMetadataStore(TFile* file);
/// \brief ReadMetadataStore of a file name; throws if the file cannot be opened
YAML::Node ReadMetadataStore(const std::string& fileName);
std::string SearchFileInPath(const std::vector<std::string>& paths, const std::string& filename);
std::pair<std::string, std::string> SeparatePathAndName(const std::string& fullname);
bool StringToBool(std::string booleanString);
std::string ToTimeStringLong(double seconds);
std::vector<std::string> Split(const std::string& s, char delim);
int CountString(const std::string& in, const std::string& substring);
bool isANumber(const std::string& str);
int StringToInteger(const std::string& in);
int GetRunNumberAuto();
long GetRandomSeed();
std::string CleanExpression(const std::string& expr);

YAML::Node OpenConfigFile(const std::string& fileName);
std::pair<std::string, YAML::Node> GetMetadataClass(const YAML::Node& cfg, const std::string& className);
YAML::Node ResolveEnvVars(const YAML::Node& node);
YAML::Node ResolveYamlRefs(const YAML::Node& root, const YAML::Node& node);
void OverrideYAMLParam(YAML::Node& node, const std::string& key, const std::string& val);
YAML::Node ResolveAllRefs(const YAML::Node& root);
/// \brief Deep copy where every node is new: unlike YAML::Clone, nodes shared between several places
/// become independent copies (no anchors/aliases when dumped) and tags/styles are dropped.
YAML::Node DetachYAMLNode(const YAML::Node& node);
/// \brief Node at a path of keys separated by dots (e.g. "runInfo.meshVoltage"), invalid if not found.
YAML::Node GetNodeAtPath(const YAML::Node& node, const std::string& path);
std::string PatternToRegex(const std::string& pattern);
std::vector<std::string> ReadYALMObservables(const YAML::Node& node);
std::string CleanString(const std::string& str);
void ReplaceAll(std::string& str, const std::string& from, const std::string& to);
/// \brief The string in lower case (ASCII)
std::string ToLower(std::string str);
/// \brief printf-style formatted string (replaces TString::Format)
std::string Format(const char* format, ...) __attribute__((format(printf, 1, 2)));
/// Replacement of a match (or of the content between delimiters), nullopt: kept as it is
using TextReplacement = std::function<std::optional<std::string>(const std::string&)>;
/// \brief Text with each `open`content`close` (e.g. [runNumber], {entries|%.1f}) replaced by
/// `replacement(content)`; kept as it is when it returns nullopt. The replacements are not scanned again.
std::string ReplaceDelimited(const std::string& text, char open, char close,
                             const TextReplacement& replacement);
/// \brief Expression (formula) with each match of the regex `identifier` replaced by `replacement(match)`,
/// kept when it returns nullopt. Matches after a digit or a dot are not identifiers (exponent of a number,
/// e.g. 1e5). The regex is a string: <regex> in the headers makes rootcling crash.
std::string ReplaceIdentifiers(const std::string& expression, const std::string& identifier,
                               const TextReplacement& replacement);
/// \brief The string without the leading and trailing characters of `characters` (whitespace)
std::string Trim(const std::string& str, const std::string& characters = " \t\n\r");

/// \brief Throws if the map node has keys that are not in `allowed` (for the structures read by a class,
/// e.g. the plots of TRestAnalysisPlot); `context` names the node in the message
void CheckYAMLKeys(const YAML::Node& node, const std::set<std::string>& allowed, const std::string& context);

// ==================================================
/// \brief Generic template helpers.
// ==================================================
template <typename T>
inline T ReadYAMLParam(const YAML::Node& node) {
    return node.as<T>();
}

/// \brief Specialization for `std::pair<T, T>`.
template <typename T>
inline std::pair<T, T> ReadYAMLParamPair(const YAML::Node& node) {
    if (!node.IsSequence() || node.size() != 2) {
        throw std::runtime_error("Expected sequence of size 2 for std::pair");
    }
    return {node[0].as<T>(), node[1].as<T>()};
}

/// \brief Specialization for `std::vector<T>`.
template <typename T>
inline std::vector<T> ReadYAMLParamVector(const YAML::Node& node) {
    if (!node.IsSequence()) {
        throw std::runtime_error("Expected sequence for std::vector");
    }
    std::vector<T> vec;
    for (auto it : node) {
        vec.push_back(it.as<T>());
    }
    return vec;
}

/// \brief Parameter `key` of the node, or the default if it is not given. Unlike ReadYAMLParamOrDefault the
/// node is not modified. T = TRestWithUnits for a number with units (e.g. "300 V").
template <typename T>
T GetYAMLParam(const YAML::Node& node, const std::string& key, const T& defaultValue = T()) {
    return node[key] ? node[key].as<T>() : defaultValue;
}

/// \brief Parameter `key` of the node as a list of strings: a sequence or a single scalar (empty if none).
inline std::vector<std::string> GetYAMLList(const YAML::Node& node, const std::string& key) {
    if (!node[key]) return {};
    if (node[key].IsScalar()) return {node[key].as<std::string>()};
    return node[key].as<std::vector<std::string>>();
}

template <typename T>
T ReadYAMLParamOrDefault(YAML::Node& params, const std::string& key, const T& defaultValue) {
    if (params[key]) {
        return ReadYAMLParam<T>(params[key]);
    } else {
        params[key] = defaultValue;
        return defaultValue;
    }
}

template <typename T>
inline void SetNodeParameter(YAML::Node& node, const std::string& key, const T& value) {
    node[key] = value;
}

}  // namespace TRestTools

#endif
