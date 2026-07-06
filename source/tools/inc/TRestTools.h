#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

namespace TRestTools {

std::vector<std::string> GetFilesMatchingPattern(const std::string& filePattern);

std::string GetTimeStampFromUnixTime(const double tm);
// Helper: split string por delimitador
std::vector<std::string> Split(const std::string& s, char delim);
YAML::Node ResolveEnvVars(const YAML::Node& node);

YAML::Node ResolveYamlRefs(const YAML::Node& root, const YAML::Node& node);

// Función principal: resuelve recursivamente mapas y secuencias
YAML::Node ResolveAllRefs(const YAML::Node& root);
std::string PatternToRegex(const std::string& pattern);
double ReadYAMLParamWithUnits(const YAML::Node& node);
std::vector<std::string> ReadYALMObservables(const YAML::Node& node);
std::string CleanString(const std::string& str);
void ReplaceAll(std::string& str, const std::string& from, const std::string& to);

// ==================================================
// Templates genéricos
// ==================================================
template <typename T>
inline T ReadYAMLParam(const YAML::Node& node) {
    return node.as<T>();
}

// Especialización: double con unidades
template <>
inline double ReadYAMLParam<double>(const YAML::Node& node) {
    return TRestTools::ReadYAMLParamWithUnits(node);
}

// Especialización: std::pair<T,T>
template <typename T>
inline std::pair<T, T> ReadYAMLParamPair(const YAML::Node& node) {
    if (!node.IsSequence() || node.size() != 2) {
        throw std::runtime_error("Expected sequence of size 2 for std::pair");
    }
    return {node[0].as<T>(), node[1].as<T>()};
}

// Especialización: std::vector<T>
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
