#include "TRestYAMLUtils.h"

#include <TFile.h>
#include <TKey.h>
#include <TObjString.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <stdexcept>

#include "TRestTools.h"

namespace {

/// \brief Node with `${ENV:VAR}` replaced by the environment variable VAR (empty if it is not defined)
YAML::Node ResolveEnvVars(const YAML::Node& node) {
    if (node.IsScalar()) {
        std::string str = node.as<std::string>();
        std::regex re(R"(\$\{ENV:([^}]+)\})");
        std::smatch match;
        std::string::const_iterator searchStart(str.cbegin());

        while (std::regex_search(searchStart, str.cend(), match, re)) {
            std::string varName = match[1].str();
            const char* envValue = std::getenv(varName.c_str());
            std::string replacement = envValue ? envValue : "";
            // match.position is relative to searchStart; the value is not searched again
            const size_t position = (searchStart - str.cbegin()) + match.position(0);
            str.replace(position, match.length(0), replacement);
            searchStart = str.cbegin() + position + replacement.size();
        }
        return YAML::Node(str);
    } else if (node.IsSequence()) {
        YAML::Node out(YAML::NodeType::Sequence);
        for (auto n : node) {
            out.push_back(ResolveEnvVars(n));
        }
        return out;
    } else if (node.IsMap()) {
        YAML::Node out(YAML::NodeType::Map);
        for (auto it : node) {
            out[it.first.Scalar()] = ResolveEnvVars(it.second);
        }
        return out;
    }
    return node;
}

/// The parameters of the last configuration resolved and those used. Written by ResolveRefs (normally once,
/// by the executable, in the main thread) but also read by TRestMetadata(fileName, section) and the GDML
/// parser, so they are accessed with gParametersMutex
std::mutex gParametersMutex;
std::map<std::string, std::string> gParameters;
std::set<std::string> gUsedParameters;

/// Substitutions in a scalar: the replaced text is searched again (a value can have references), so a
/// reference to itself (e.g. `a: "${a}"`) would never end
constexpr int kMaxSubstitutions = 1000;

void CheckSubstitutions(int& substitutions, const std::string& original) {
    if (++substitutions > kMaxSubstitutions) {
        throw std::runtime_error("more than " + std::to_string(kMaxSubstitutions) + " substitutions in '" +
                                 original + "': a reference that contains itself (e.g. a: \"${a}\")?");
    }
}

/// \brief Node with `${PARAM:name}` replaced by the parameter `name`; throws if it is not defined or if a
/// parameter contains itself. The caller holds gParametersMutex.
YAML::Node ResolveParameters(const YAML::Node& node) {
    if (node.IsScalar()) {
        std::string str = node.as<std::string>();
        const std::string original = str;
        const std::regex re(R"(\$\{PARAM:([^}]+)\})");
        std::smatch match;
        bool replaced = false;
        int substitutions = 0;
        while (std::regex_search(str, match, re)) {
            CheckSubstitutions(substitutions, original);
            const std::string name = match[1].str();
            const auto parameter = gParameters.find(name);
            if (parameter == gParameters.end()) {
                throw std::runtime_error("${PARAM:" + name + "}: parameter '" + name +
                                         "' not defined in the section `parameters`");
            }
            gUsedParameters.insert(name);
            str.replace(match.position(0), match.length(0), parameter->second);
            replaced = true;
        }
        return replaced ? YAML::Node(str) : node;
    } else if (node.IsSequence()) {
        YAML::Node out(YAML::NodeType::Sequence);
        for (auto n : node) out.push_back(ResolveParameters(n));
        return out;
    } else if (node.IsMap()) {
        YAML::Node out(YAML::NodeType::Map);
        for (auto it : node) out[it.first.Scalar()] = ResolveParameters(it.second);
        return out;
    }
    return node;
}

/// \brief Recursively resolves YAML placeholder references (a referenced value can have references; a
/// reference to itself throws).
YAML::Node ResolveYamlRefs(const YAML::Node& root, const YAML::Node& node) {
    if (node.IsScalar()) {
        std::string str = node.as<std::string>();
        const std::string original = str;
        std::smatch match;
        int substitutions = 0;

        std::regex yamlRe(R"(\$\{([^}]+)\})");
        while (std::regex_search(str, match, yamlRe)) {
            CheckSubstitutions(substitutions, original);
            std::string keyPath = match[1].str();

            YAML::Node refNode = YAML::Clone(root);
            for (auto key : TRestTools::Split(keyPath, '.')) {
                if (!refNode[key]) throw std::runtime_error("key not found: " + keyPath);
                refNode = refNode[key];
            }

            if (!refNode.IsScalar())
                throw std::runtime_error("Cannot substitute non-scalar node: " + keyPath);

            std::string replacement = refNode.as<std::string>();
            str.replace(match.position(0), match.length(0), replacement);
        }
        return YAML::Node(str);
    } else if (node.IsSequence()) {
        YAML::Node out(YAML::NodeType::Sequence);
        for (auto n : node) out.push_back(ResolveYamlRefs(root, n));
        return out;
    } else if (node.IsMap()) {
        YAML::Node out(YAML::NodeType::Map);
        for (auto it : node) out[it.first.Scalar()] = ResolveYamlRefs(root, it.second);
        return out;
    }

    return node;
}

/// \brief `local` over `base`: the maps are merged recursively (the keys of `local` win), any other node of
/// `local` (scalar, list) replaces the one of `base`
YAML::Node MergeNodes(const YAML::Node& base, const YAML::Node& local) {
    if (!base.IsMap() || !local.IsMap()) return YAML::Clone(local);
    YAML::Node out = YAML::Clone(base);
    for (const auto& element : local) {
        const std::string key = element.first.Scalar();
        const YAML::Node& constOut = out;  // Const access: a missing key is not added
        const YAML::Node baseValue = constOut[key];
        if (baseValue && baseValue.IsMap() && element.second.IsMap())
            out[key] = MergeNodes(baseValue, element.second);
        else
            out[key] = YAML::Clone(element.second);
    }
    return out;
}

/// \brief Node with its `select` blocks replaced by the case chosen, at any depth (ResolveRefs, after the
/// parameters): a map with `select: value` and `cases` becomes the case `value` of `cases` (or `default`)
/// merged over the other keys of the map (the common ones, MergeNodes: the case wins); a case that is not
/// a map (e.g. a scalar) replaces the block, which then has no other keys. The case chosen can have other
/// blocks.
YAML::Node ExpandSelects(const YAML::Node& node) {
    if (node.IsSequence()) {
        YAML::Node out(YAML::NodeType::Sequence);
        for (const auto& element : node) out.push_back(ExpandSelects(element));
        return out;
    }
    if (!node.IsMap()) return node;

    const YAML::Node select = node["select"];
    if (!select) {
        YAML::Node out(YAML::NodeType::Map);
        for (const auto& element : node) out[element.first.Scalar()] = ExpandSelects(element.second);
        return out;
    }

    const YAML::Node cases = node["cases"];
    if (!select.IsScalar()) throw std::runtime_error("select: a value expected (e.g. \"${PARAM:name}\")");
    if (!cases || !cases.IsMap()) throw std::runtime_error("select: a map `cases` (value: content) expected");
    const std::string value = select.Scalar();
    // Const access (a missing key is not added) and reset (assigning a node would write into the case); a
    // default YAML::Node is a defined (null) node: whether a case is found is kept apart
    const YAML::Node& constCases = cases;
    YAML::Node chosen;
    bool found = false;
    for (const std::string& name : {value, std::string("default")}) {
        if (!found && constCases[name]) {
            chosen.reset(constCases[name]);
            found = true;
        }
    }
    if (!found) {
        std::string names;
        for (const auto& element : cases) names += (names.empty() ? "" : ", ") + element.first.Scalar();
        throw std::runtime_error("select: no case '" + value + "' nor `default` in the cases (" + names +
                                 ")");
    }

    YAML::Node common(YAML::NodeType::Map);
    for (const auto& element : node) {
        const std::string key = element.first.Scalar();
        if (key != "select" && key != "cases") common[key] = element.second;
    }
    if (!chosen.IsMap()) {
        if (common.size() > 0) {
            throw std::runtime_error("select: the case '" + value +
                                     "' is not a map, the block cannot have other keys");
        }
        return ExpandSelects(chosen);
    }
    return ExpandSelects(MergeNodes(common, chosen));
}

/// \brief Base of the relative includes of a file or URL: its directory, or its URL up to the last '/'
std::string IncludeBase(const std::string& location) {
    if (TRestTools::IsURL(location)) return location.substr(0, location.rfind('/') + 1);
    return std::filesystem::path(location).parent_path().string();
}

/// \brief Node with its `include` keys expanded (LoadConfigFile), at any depth. `directory`: the base of the
/// relative paths, the directory of the file of the node or the URL of its directory (IncludeBase);
/// `stack`: the files being included, to detect cycles
YAML::Node ExpandIncludes(const YAML::Node& node, const std::string& directory,
                          std::vector<std::string>& stack) {
    if (node.IsSequence()) {
        YAML::Node out(YAML::NodeType::Sequence);
        for (const auto& element : node) out.push_back(ExpandIncludes(element, directory, stack));
        return out;
    }
    if (!node.IsMap()) return YAML::Clone(node);

    YAML::Node local(YAML::NodeType::Map);
    // A default YAML::Node is a defined (null) node: whether there is an include is kept apart
    bool hasInclude = false;
    YAML::Node include;
    for (const auto& element : node) {
        const std::string key = element.first.Scalar();
        if (key == "include") {
            hasInclude = true;
            include.reset(element.second);
        } else {
            local[key] = ExpandIncludes(element.second, directory, stack);
        }
    }
    if (!hasInclude) return local;

    if (!include.IsScalar() || include.Scalar().empty())
        throw std::runtime_error("include: a file name expected (\"file.yaml\" or \"file.yaml:key\")");
    // "file.yaml:key": the section `key` of the file
    std::string fileName = ResolveEnvVars(include).Scalar();
    std::string sectionKey;
    const size_t colon = fileName.rfind(':');
    if (colon != std::string::npos) {
        const std::string prefix = fileName.substr(0, colon);
        auto endsWith = [&prefix](const std::string& end) {
            return prefix.size() > end.size() &&
                   prefix.compare(prefix.size() - end.size(), end.size(), end) == 0;
        };
        if (endsWith(".yaml") || endsWith(".yml")) {
            sectionKey = fileName.substr(colon + 1);
            fileName = prefix;
        }
    }

    // A URL (e.g. a versioned file of rest-common) is downloaded; relative to a URL, a URL
    std::string pathString;
    if (TRestTools::IsURL(fileName)) {
        pathString = fileName;
    } else if (TRestTools::IsURL(directory) && std::filesystem::path(fileName).is_relative()) {
        pathString = directory + fileName;
    } else {
        std::filesystem::path path(fileName);
        if (path.is_relative()) path = std::filesystem::path(directory) / path;
        pathString = std::filesystem::weakly_canonical(path).string();
        if (!std::filesystem::exists(path))
            throw std::runtime_error("include: file " + pathString +
                                     " not found (relative paths are relative to " +
                                     "the file that includes it)");
    }
    if (std::find(stack.begin(), stack.end(), pathString) != stack.end())
        throw std::runtime_error("include: " + pathString + " includes itself (directly or not)");

    stack.push_back(pathString);
    YAML::Node included =
        ExpandIncludes(YAML::LoadFile(TRestTools::GetLocalFile(pathString)), IncludeBase(pathString), stack);
    stack.pop_back();

    if (!sectionKey.empty()) {
        const YAML::Node& constIncluded = included;
        if (!constIncluded.IsMap() || !constIncluded[sectionKey])
            throw std::runtime_error("include: no key '" + sectionKey + "' in " + pathString);
        included.reset(YAML::Clone(constIncluded[sectionKey]));
    }
    if (!included.IsMap())
        throw std::runtime_error("include: the content of " + include.Scalar() +
                                 " must be a map (the keys of the section)");
    return MergeNodes(included, local);
}

}  // namespace

YAML::Node TRestYAMLUtils::LoadConfigFile(const std::string& fileName) {
    // The file itself can also be a URL
    std::vector<std::string> stack{
        TRestTools::IsURL(fileName) ? fileName : std::filesystem::weakly_canonical(fileName).string()};
    return ExpandIncludes(YAML::LoadFile(TRestTools::GetLocalFile(fileName)), IncludeBase(stack.front()),
                          stack);
}

YAML::Node TRestYAMLUtils::OpenConfigFile(const std::string& fileName,
                                          const std::map<std::string, std::string>& overrides) {
    return ResolveRefs(LoadConfigFile(fileName), overrides);
}

YAML::Node TRestYAMLUtils::ResolveRefs(const YAML::Node& root,
                                       const std::map<std::string, std::string>& overrides,
                                       bool setCurrentParameters) {
    YAML::Node cfg = YAML::Clone(root);
    std::lock_guard<std::mutex> lock(gParametersMutex);
    // Without setCurrentParameters the current ones are restored when leaving (also on errors)
    struct Restore {
        bool active;
        std::map<std::string, std::string> parameters;
        std::set<std::string> used;
        ~Restore() {
            if (!active) return;
            gParameters = std::move(parameters);
            gUsedParameters = std::move(used);
        }
    } restore{!setCurrentParameters, setCurrentParameters ? decltype(gParameters){} : gParameters,
              setCurrentParameters ? decltype(gUsedParameters){} : gUsedParameters};
    gParameters.clear();
    gUsedParameters.clear();
    YAML::Node parameters;
    if (cfg.IsMap() && cfg["parameters"]) {
        parameters = cfg["parameters"];
        cfg.remove("parameters");
    }
    if (parameters && !parameters.IsNull() && !parameters.IsMap()) {
        throw std::runtime_error("`parameters` must be a map name: value");
    }
    for (const auto& [name, value] : overrides) {
        if (!parameters || !parameters.IsMap() || !parameters[name]) {
            throw std::runtime_error("--set " + name + ": no parameter '" + name +
                                     "' in the section `parameters` of the configuration");
        }
        parameters[name] = value;
    }
    if (parameters && parameters.IsMap()) {
        for (const auto& parameter : ResolveEnvVars(parameters)) {
            const std::string name = parameter.first.as<std::string>();
            if (!parameter.second.IsScalar()) {
                throw std::runtime_error("parameter '" + name + "': the value must be a number or a text");
            }
            gParameters[name] = parameter.second.Scalar();
        }
    }
    // The parameters are resolved in all the cases of the select blocks (those of the cases not chosen are
    // also defined and used), then the blocks are replaced by their case
    cfg = ExpandSelects(ResolveParameters(ResolveEnvVars(cfg)));
    return ResolveYamlRefs(cfg, cfg);
}

std::map<std::string, std::string> TRestYAMLUtils::GetParameters() {
    std::lock_guard<std::mutex> lock(gParametersMutex);
    return gParameters;
}

void TRestYAMLUtils::MarkParameterUsed(const std::string& name) {
    std::lock_guard<std::mutex> lock(gParametersMutex);
    gUsedParameters.insert(name);
}

void TRestYAMLUtils::CheckParametersUsed() {
    std::lock_guard<std::mutex> lock(gParametersMutex);
    std::string unused;
    for (const auto& [name, value] : gParameters) {
        if (!gUsedParameters.count(name)) unused += (unused.empty() ? "" : ", ") + name;
    }
    if (!unused.empty()) {
        throw std::runtime_error(
            "parameters not used in the configuration (${PARAM:name}) nor in the GDML "
            "(constant or entity with that name): " +
            unused);
    }
}

void TRestYAMLUtils::AddParameterOverride(std::map<std::string, std::string>& overrides,
                                          const std::string& assignment) {
    const size_t equal = assignment.find('=');
    if (equal == std::string::npos || equal == 0) {
        throw std::runtime_error("--set needs name=value, e.g. --set isotope=Pb210 (given: '" + assignment +
                                 "')");
    }
    overrides[assignment.substr(0, equal)] = assignment.substr(equal + 1);
}

std::pair<std::string, YAML::Node> TRestYAMLUtils::GetMetadataClass(const YAML::Node& cfg,
                                                                    const std::string& className) {
    for (const auto& element : cfg) {
        const auto key = element.first.as<std::string>();
        auto value = element.second;

        if (!value || value.IsScalar() || !value.IsMap()) continue;

        if (!value["class"]) continue;

        std::string cName = value["class"].as<std::string>();
        if (cName == className) return std::make_pair(key, value);
    }

    return {};
}

YAML::Node TRestYAMLUtils::GetNodeAtPath(const YAML::Node& node, const std::string& path) {
    YAML::Node current;
    current.reset(node);
    for (const auto& key : TRestTools::Split(path, '.')) {
        // Const access: a missing key is not added to the node
        const YAML::Node parent = current;
        if (!parent || !parent.IsMap() || !parent[key]) return YAML::Node(YAML::NodeType::Undefined);
        current.reset(parent[key]);
    }
    return current;
}

void TRestYAMLUtils::OverrideParam(YAML::Node& node, const std::string& key, const std::string& val) {
    if (node.IsMap()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            if (it->first.IsScalar() && it->first.as<std::string>() == key) {
                node[it->first] = val;
            }

            YAML::Node child = it->second;
            OverrideParam(child, key, val);
        }
    } else if (node.IsSequence()) {
        for (size_t i = 0; i < node.size(); ++i) {
            YAML::Node child = node[i];
            OverrideParam(child, key, val);
        }
    }
}

YAML::Node TRestYAMLUtils::Detach(const YAML::Node& node) {
    if (!node) return YAML::Node();
    if (node.IsNull()) return YAML::Node(YAML::NodeType::Null);
    if (node.IsScalar()) return YAML::Node(node.Scalar());

    YAML::Node detached(node.Type());
    if (node.IsSequence()) {
        for (const auto& child : node) detached.push_back(Detach(child));
    } else if (node.IsMap()) {
        for (const auto& entry : node) {
            detached[Detach(entry.first)] = Detach(entry.second);
        }
    }
    return detached;
}

void TRestYAMLUtils::CheckKeys(const YAML::Node& node, const std::set<std::string>& allowed,
                               const std::string& context) {
    if (!node || !node.IsMap()) return;
    std::string unknown;
    for (const auto& element : node) {
        const auto key = element.first.as<std::string>();
        if (allowed.count(key) == 0) unknown += " '" + key + "'";
    }
    if (unknown.empty()) return;
    std::string valid;
    for (const auto& key : allowed) valid += " " + key;
    throw std::runtime_error(context + ": unknown parameter(s)" + unknown + ". Valid parameters:" + valid);
}

YAML::Node TRestYAMLUtils::ReadMetadataStore(TFile* file) {
    YAML::Node store(YAML::NodeType::Map);
    if (!file || file->IsZombie()) return store;
    TDirectory* directory = file->GetDirectory("RESTMetadataStore");
    if (!directory) return store;
    for (const auto* key : *directory->GetListOfKeys()) {
        const std::string instance = key->GetName();
        std::unique_ptr<TObjString> text(directory->Get<TObjString>(instance.c_str()));
        if (!text) continue;
        try {
            store[instance] = YAML::Load(text->GetString().Data());
        } catch (const std::exception& error) {
            throw std::runtime_error("TRestYAMLUtils::ReadMetadataStore: cannot read '" + instance + "' of " +
                                     file->GetName() + ": " + error.what());
        }
    }
    return store;
}

YAML::Node TRestYAMLUtils::ReadMetadataStore(const std::string& fileName) {
    std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "READ"));
    if (!file || file->IsZombie()) {
        throw std::runtime_error("TRestYAMLUtils::ReadMetadataStore: cannot open " + fileName);
    }
    return ReadMetadataStore(file.get());
}

namespace {
/// Text of a scalar in a YAML file: plain if it reads back the same, otherwise double quoted
std::string ScalarText(const std::string& value) {
    try {
        const YAML::Node node = YAML::Load("key: " + value);
        if (!value.empty() && node.IsMap() && node.size() == 1 && node["key"].IsScalar() &&
            node["key"].Tag() == "?" && node["key"].Scalar() == value)
            return value;
    } catch (const YAML::Exception&) {
    }
    std::string quoted = "\"";
    for (const char c : value) {
        if (c == '"' || c == '\\') quoted += '\\';
        quoted += c;
    }
    return quoted + "\"";
}

/// End (column after it) of the scalar starting at `start` of a line: a quoted one after its closing quote
/// (npos if it continues in the next line), a plain one before a comment (" #") or the end of the line
size_t ScalarEnd(const std::string& line, size_t start) {
    if (start < line.size() && (line[start] == '"' || line[start] == '\'')) {
        const char quote = line[start];
        for (size_t i = start + 1; i < line.size(); i++) {
            if (quote == '"' && line[i] == '\\') {
                i++;  // Escaped character
            } else if (line[i] == quote) {
                if (quote == '\'' && i + 1 < line.size() && line[i + 1] == '\'') {
                    i++;  // '' inside a single quoted scalar
                } else {
                    return i + 1;
                }
            }
        }
        return std::string::npos;
    }
    size_t end = line.find(" #", start);
    if (end == std::string::npos) end = line.size();
    while (end > start && std::isspace(static_cast<unsigned char>(line[end - 1]))) end--;
    return end;
}
}  // namespace

void TRestYAMLUtils::SetValuesInFile(const std::string& fileName,
                                     const std::vector<std::pair<std::string, std::string>>& values) {
    if (values.empty()) return;

    std::vector<std::string> lines;
    {
        std::ifstream in(fileName);
        if (!in.is_open()) throw std::runtime_error("SetValuesInFile: cannot read " + fileName);
        std::string line;
        while (std::getline(in, line)) lines.push_back(line);
    }

    for (const auto& [path, value] : values) {
        auto error = [&, &path = path](const std::string& reason) {
            return std::runtime_error("SetValuesInFile: cannot set '" + path + "' in " + fileName + ": " +
                                      reason);
        };

        const auto keys = TRestTools::Split(path, '.');
        if (keys.empty()) throw error("empty path");
        const std::string& key = keys.back();
        const std::string scalar = ScalarText(value);

        // A missing parent key is added (e.g. runInfo of a section whose runInfo is in an included file),
        // then the text is parsed again: at most one more attempt per key
        bool done = false;
        for (size_t attempt = 0; !done; attempt++) {
            if (attempt > keys.size()) throw error("its parent keys cannot be added");

            // Parsed again after each change: the positions of the nodes in the text (lines and columns
            // from 0)
            std::string text;
            for (const auto& line : lines) text += line + "\n";
            const YAML::Node root = YAML::Load(text);

            YAML::Node parent;
            parent.reset(root);
            YAML::Mark parentKeyMark;  // Key of `parent` in its map (not for the root)
            bool parentIsRoot = true;
            bool added = false;
            for (size_t k = 0; k + 1 < keys.size(); k++) {
                const YAML::Node current = parent;  // Const access: a missing key is not added
                if (current.IsNull() && !parentIsRoot) {
                    // A key without value: its first key added below it
                    lines.insert(lines.begin() + parentKeyMark.line + 1,
                                 std::string(parentKeyMark.column + 2, ' ') + ScalarText(keys[k]) + ":");
                    added = true;
                    break;
                }
                if (!current.IsMap() || current.size() == 0 || current.Style() == YAML::EmitterStyle::Flow)
                    throw error("the parent of " + keys[k] + " is not a block map");
                if (!current[keys[k]]) {
                    // Added before the first key of the map, with its indentation
                    const YAML::Mark first = current.begin()->first.Mark();
                    lines.insert(lines.begin() + first.line,
                                 std::string(first.column, ' ') + ScalarText(keys[k]) + ":");
                    added = true;
                    break;
                }
                for (const auto& element : current) {
                    if (element.first.Scalar() != keys[k]) continue;
                    parentKeyMark = element.first.Mark();
                    break;
                }
                parent.reset(current[keys[k]]);
                parentIsRoot = false;
            }
            if (added) continue;
            done = true;

            if (parent.IsNull() && !parentIsRoot) {
                lines.insert(lines.begin() + parentKeyMark.line + 1,
                             std::string(parentKeyMark.column + 2, ' ') + ScalarText(key) + ": " + scalar);
                break;
            }
            if (!parent.IsMap() || parent.size() == 0 || parent.Style() == YAML::EmitterStyle::Flow)
                throw error("its parent is not a block map");

            bool found = false;
            for (const auto& element : parent) {
                if (element.first.Scalar() != key) continue;
                found = true;
                const YAML::Node valueNode = element.second;
                if (!valueNode.IsScalar() && !valueNode.IsNull()) throw error("not a scalar");

                const YAML::Mark keyMark = element.first.Mark();
                std::string& line = lines.at(keyMark.line);
                const bool quotedKey = line[keyMark.column] == '"' || line[keyMark.column] == '\'';
                const size_t keyEnd =
                    quotedKey ? ScalarEnd(line, keyMark.column) : keyMark.column + key.size();
                const size_t colon = keyEnd == std::string::npos ? std::string::npos : line.find(':', keyEnd);
                if (colon == std::string::npos) throw error("key not found in its line");

                size_t start = line.find_first_not_of(" \t", colon + 1);
                if (start == std::string::npos || line[start] == '#') {
                    // No value (null): written after the colon, before the comment
                    const std::string comment = start == std::string::npos ? "" : " " + line.substr(start);
                    line = line.substr(0, colon + 1) + " " + scalar + comment;
                    break;
                }
                if (valueNode.Mark().line != keyMark.line)
                    throw error("the value is not in the line of its key");
                start = valueNode.Mark().column;
                if (line[start] == '|' || line[start] == '>') throw error("block scalar");
                const size_t end = ScalarEnd(line, start);
                if (end == std::string::npos) throw error("value of several lines");
                line = line.substr(0, start) + scalar + line.substr(end);
                break;
            }

            if (!found) {
                // Added before the first key of the map, with its indentation
                const YAML::Mark first = parent.begin()->first.Mark();
                lines.insert(lines.begin() + first.line,
                             std::string(first.column, ' ') + ScalarText(key) + ": " + scalar);
            }
        }
    }

    std::ofstream out(fileName, std::ios::trunc);
    if (!out.is_open()) throw std::runtime_error("SetValuesInFile: cannot write " + fileName);
    for (const auto& line : lines) out << line << "\n";
}
