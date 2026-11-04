#ifndef TRESTYAMLUTILS_H
#define TRESTYAMLUTILS_H

#include <yaml-cpp/yaml.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

class TFile;

/// YAML functions: configuration files, parameters of the nodes and the metadata stored in the REST files.
/// The generic functions (strings, files, ROOT files, geometries) are in TRestTools.
namespace TRestYAMLUtils {

/// \brief Configuration file with its references resolved (ResolveRefs), the parameters with `overrides`
YAML::Node OpenConfigFile(const std::string& fileName,
                          const std::map<std::string, std::string>& overrides = {});
/// \brief Node with `${ENV:VAR}` replaced by the environment variable VAR (empty if it is not defined),
/// `${PARAM:name}` by the parameter `name` and `${a.b}` by the scalar at the path a.b of the node. Throws if
/// a reference is not found or is not a scalar.
///
/// Parameters: the section `parameters` at the root of the configuration, name: value (a number, with units
/// or not, or a text) or `${ENV:VAR}`, e.g.
///
///     parameters:
///       isotope: "U238"
///       foilThickness: 5 um
///
/// They are used in the configuration as `${PARAM:isotope}` (a parameter not defined is an error) and, in
/// restG4, they replace the GDML constants and entities with the same name (TRestGDMLParser::Resolve).
/// `overrides` (the `--set name=value` of the executables) replace their values for this execution (the
/// file is not changed); a name that is not in `parameters` is an error. The section is removed from the
/// returned node (the configurations stored are the resolved ones); its values are kept as the current
/// parameters (GetParameters), those of the last configuration resolved.
YAML::Node ResolveRefs(const YAML::Node& root, const std::map<std::string, std::string>& overrides = {});
/// \brief The parameters of the last configuration resolved (ResolveRefs): name -> value as written, with
/// `${ENV:VAR}` and the overrides applied
const std::map<std::string, std::string>& GetParameters();
/// \brief Marks a parameter as used (by the configuration or the GDML), for CheckParametersUsed
void MarkParameterUsed(const std::string& name);
/// \brief Throws if a parameter of the last configuration resolved is not used, by the configuration nor by
/// the GDML (probably a typo); called by the executables once the configuration and the geometry are read
void CheckParametersUsed();
/// \brief Adds `name=value` (the argument of `--set`) to `overrides`; throws if it is not name=value
void AddParameterOverride(std::map<std::string, std::string>& overrides, const std::string& assignment);
/// \brief First section of the node with `class: className`: (key, section), empty if there is none
std::pair<std::string, YAML::Node> GetMetadataClass(const YAML::Node& cfg, const std::string& className);
/// \brief Node at a path of keys separated by dots (e.g. "runInfo.meshVoltage"), invalid if not found.
YAML::Node GetNodeAtPath(const YAML::Node& node, const std::string& path);
/// \brief Sets `key` to `val` in every map of the node, at any depth, where it is given
void OverrideParam(YAML::Node& node, const std::string& key, const std::string& val);
/// \brief Deep copy where every node is new: unlike YAML::Clone, nodes shared between several places
/// become independent copies (no anchors/aliases when dumped) and tags/styles are dropped.
YAML::Node Detach(const YAML::Node& node);
/// \brief Throws if the map node has keys that are not in `allowed` (for the structures read by a class,
/// e.g. the plots of TRestAnalysisPlot); `context` names the node in the message
void CheckKeys(const YAML::Node& node, const std::set<std::string>& allowed, const std::string& context);

/// \brief Metadata stored in a REST file (directory RESTMetadataStore, a YAML TObjString per instance),
/// read without creating them (no TRestRun is opened): map instance name -> configuration (with `class`).
/// Empty if the file has no metadata. Throws if a configuration cannot be parsed.
YAML::Node ReadMetadataStore(TFile* file);
/// \brief ReadMetadataStore of a file name; throws if the file cannot be opened
YAML::Node ReadMetadataStore(const std::string& fileName);

/// \brief Parameter `key` of the node, or the default if it is not given. Unlike ReadParamOrDefault the
/// node is not modified. T = TRestWithUnits for a number with units (e.g. "300 V").
template <typename T>
T GetParam(const YAML::Node& node, const std::string& key, const T& defaultValue = T()) {
    return node[key] ? node[key].as<T>() : defaultValue;
}

/// \brief Parameter `key` of the node as a list of strings: a sequence or a single scalar (empty if none).
inline std::vector<std::string> GetList(const YAML::Node& node, const std::string& key) {
    if (!node[key]) return {};
    if (node[key].IsScalar()) return {node[key].as<std::string>()};
    return node[key].as<std::vector<std::string>>();
}

/// \brief Parameter `key` of the node; if it is not given the default is returned and written in the node
template <typename T>
T ReadParamOrDefault(YAML::Node& params, const std::string& key, const T& defaultValue) {
    if (params[key]) return params[key].as<T>();
    params[key] = defaultValue;
    return defaultValue;
}

/// \brief Sets the parameter `key` of the node
template <typename T>
inline void SetParam(YAML::Node& node, const std::string& key, const T& value) {
    node[key] = value;
}

}  // namespace TRestYAMLUtils

#endif
