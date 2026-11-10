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
///
/// \author JuanAn García <juanangp@unizar.es>
namespace TRestYAMLUtils {

/// \brief A YAML configuration file with its `include` keys expanded, at any depth: a map with
/// `include: "file.yaml"` takes the content of that file (a map, the keys of the section) and its own keys
/// override it (the maps are merged recursively, lists and scalars replaced); `include: "file.yaml:key"`
/// takes the section `key` of a file with several. The path is relative to the file that includes it
/// (absolute paths and ${ENV:VAR} are accepted; not ${PARAM:...}: the parameters are resolved after, by
/// ResolveRefs, with the `parameters` of the main file only). Included files can include others; a cycle,
/// a missing file or key, or a content that is not a map throw. E.g.
///
///     run:
///       include: "common/run.yaml"
///       runTag: "calib"            # overrides the one of common/run.yaml
YAML::Node LoadConfigFile(const std::string& fileName);
/// \brief Configuration file (LoadConfigFile, its includes expanded) with its references resolved
/// (ResolveRefs), the parameters with `overrides`
YAML::Node OpenConfigFile(const std::string& fileName,
                          const std::map<std::string, std::string>& overrides = {});
/// \brief Node with `${ENV:VAR}` replaced by the environment variable VAR (empty if it is not defined),
/// `${PARAM:name}` by the parameter `name` and `${a.b}` by the scalar at the path a.b of the node. Throws if
/// a reference is not found or is not a scalar. The replaced values of `${PARAM:name}` and `${a.b}` are
/// resolved again (they can have references); a reference that contains itself (e.g. `a: "${a}"`) throws
/// (more than 1000 substitutions in a value). Thread safe (the parameters are global, see GetParameters).
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
/// parameters (GetParameters), those of the last configuration resolved, unless setCurrentParameters is false
/// (another file read by a metadata, TRestMetadata(fileName, section): the parameters of the main
/// configuration and those used are kept).
///
/// Conditional blocks: a map with `select` and `cases` is replaced, after the parameters are resolved, by
/// the case whose name is the value of `select` (`default` if none matches; neither is an error), merged
/// over the other keys of the map (common to all the cases; the maps are merged recursively and the case
/// wins). Only the case chosen remains in the returned node (the configuration stored); the parameters of
/// all the cases must be defined. A case can be a scalar (then the block has no other keys). E.g.
///
///     parameters:
///       generator: "Gammas"
///     ...
///     generator:
///       select: "${PARAM:generator}"
///       source:                      # common to all the cases
///         particle: "gamma"
///       cases:
///         Gammas:
///           type: "point"
///         SolarAxions:
///           type: "surface"
///           shape: "circle"
///     energy:
///       select: "${PARAM:generator}"
///       cases: {Gammas: 5.5 MeV, default: 10 MeV}
YAML::Node ResolveRefs(const YAML::Node& root, const std::map<std::string, std::string>& overrides = {},
                       bool setCurrentParameters = true);
/// \brief The parameters of the last configuration resolved (ResolveRefs): name -> value as written, with
/// `${ENV:VAR}` and the overrides applied (a copy: thread safe)
std::map<std::string, std::string> GetParameters();
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
/// \brief Sets values of a YAML file in its text, keeping the rest as it is (comments, order, format): each
/// (path of keys separated by dots, e.g. "run.runInfo.operator"; value). A value is written plain if it reads
/// back the same, otherwise double quoted. A key that is not there is added at the start of its map, and so
/// are its missing parent keys (e.g. a value of a section taken from an included file: it is written in this
/// file, overriding the included one). Throws if a path cannot be set (a parent that is not a block map, a
/// value that is not a scalar of one line).
void SetValuesInFile(const std::string& fileName,
                     const std::vector<std::pair<std::string, std::string>>& values);

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
