#include "TRestYAMLUtils.h"

#include <TFile.h>
#include <TKey.h>
#include <TObjString.h>

#include <cstdlib>
#include <map>
#include <memory>
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
            str.replace(match.position(0), match.length(0), replacement);
            searchStart = str.cbegin() + match.position(0) + replacement.size();
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

/// The parameters of the last configuration resolved and those used
std::map<std::string, std::string> gParameters;
std::set<std::string> gUsedParameters;

/// \brief Node with `${PARAM:name}` replaced by the parameter `name`; throws if it is not defined
YAML::Node ResolveParameters(const YAML::Node& node) {
    if (node.IsScalar()) {
        std::string str = node.as<std::string>();
        const std::regex re(R"(\$\{PARAM:([^}]+)\})");
        std::smatch match;
        bool replaced = false;
        while (std::regex_search(str, match, re)) {
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

/// \brief Recursively resolves YAML placeholder references.
YAML::Node ResolveYamlRefs(const YAML::Node& root, const YAML::Node& node) {
    if (node.IsScalar()) {
        std::string str = node.as<std::string>();
        std::smatch match;

        std::regex yamlRe(R"(\$\{([^}]+)\})");
        while (std::regex_search(str, match, yamlRe)) {
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

}  // namespace

YAML::Node TRestYAMLUtils::OpenConfigFile(const std::string& fileName,
                                          const std::map<std::string, std::string>& overrides) {
    return ResolveRefs(YAML::LoadFile(fileName), overrides);
}

YAML::Node TRestYAMLUtils::ResolveRefs(const YAML::Node& root,
                                       const std::map<std::string, std::string>& overrides) {
    YAML::Node cfg = YAML::Clone(root);
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
    cfg = ResolveParameters(ResolveEnvVars(cfg));
    return ResolveYamlRefs(cfg, cfg);
}

const std::map<std::string, std::string>& TRestYAMLUtils::GetParameters() { return gParameters; }

void TRestYAMLUtils::MarkParameterUsed(const std::string& name) { gUsedParameters.insert(name); }

void TRestYAMLUtils::CheckParametersUsed() {
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
