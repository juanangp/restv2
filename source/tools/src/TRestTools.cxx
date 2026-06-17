

#include "TRestTools.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <regex>
#include <thread>

#include "TRestLogManager.h"
#include "TRestSystemOfUnits.h"

namespace fs = std::filesystem;

std::vector<std::string> TRestTools::GetFilesMatchingPattern(const std::string& filePattern) {
    std::vector<std::string> outputFileNames;

    if (filePattern.empty()) {
        RESTWarning << "Warning pattern cannot be empty" << RESTendl;
        return outputFileNames;
    }

    fs::path path(filePattern);

    // Separate path and file pattern
    fs::path directory = path.parent_path();
    std::string filenamePattern = path.filename().string();

    if (directory.empty()) {
        directory = ".";  // Use current directory if path is missing
    }

    std::regex regex_pattern(TRestTools::PatternToRegex(filenamePattern), std::regex::icase);

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (fs::is_regular_file(entry.status())) {
            std::string filename = entry.path().filename().string();
            if (std::regex_match(filename, regex_pattern)) {
                outputFileNames.push_back(entry.path().string());
            }
        }
    }
    return outputFileNames;
}

std::string TRestTools::GetTimeStampFromUnixTime(const double tm) {
    char tmpstm[20];  //"YYYY-MM-DD HH:MM:SS" + \0
    std::time_t time = static_cast<std::time_t>(tm);

    std::strftime(tmpstm, sizeof(tmpstm), "%Y-%m-%d %H:%M:%S", std::localtime(&time));

    return std::string(tmpstm);
}

YAML::Node TRestTools::ResolveEnvVars(const YAML::Node& node) {
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

std::vector<std::string> TRestTools::Split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) tokens.push_back(item);
    return tokens;
}

// Función recursiva para resolver referencias YAML ${...}
YAML::Node TRestTools::ResolveYamlRefs(const YAML::Node& root, const YAML::Node& node) {
    if (node.IsScalar()) {
        std::string str = node.as<std::string>();
        std::regex re(R"(\$\{([^}]+)\})");
        std::smatch match;
        std::string::const_iterator searchStart(str.cbegin());

        std::regex yamlRe(R"(\$\{([^}]+)\})");
        while (std::regex_search(str, match, yamlRe)) {
            std::string keyPath = match[1].str();

            YAML::Node refNode = YAML::Clone(root);
            for (auto key : Split(keyPath, '.')) {
                if (!refNode[key]) throw std::runtime_error("key not found: " + keyPath);
                refNode = refNode[key];
            }

            if (!refNode.IsScalar())
                throw std::runtime_error("Cannot substitute non-scalar node: " + keyPath);

            std::string replacement = refNode.as<std::string>();
            str.replace(match.position(0), match.length(0), replacement);
        }
        // std::cout<<"Done "<<std::endl;
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

YAML::Node TRestTools::ResolveAllRefs(const YAML::Node& root) {
    auto cfg = ResolveEnvVars(root);
    auto solved = ResolveYamlRefs(cfg, cfg);
    return solved;
}

std::string TRestTools::PatternToRegex(const std::string& pattern) {
    std::string regex_pattern;
    for (char c : pattern) {
        if (c == '*') {
            regex_pattern += ".*";  // '*' -> Wildcard
        } else if (std::ispunct(c)) {
            regex_pattern += "\\";
            regex_pattern += c;  // Special characters
        } else {
            regex_pattern += c;
        }
    }
    return regex_pattern;
}

double TRestTools::ReadYAMLParamWithUnits(const YAML::Node& node) {
    double value = 0;
    if (node.IsScalar()) {
        value = node.as<double>();
    } else if (node.IsMap()) {
        value = node["value"].as<double>();
        std::string units = node["units"].as<std::string>();
        value *= REST_Units::ParseUnit(units);
    }
    return value;
}

std::vector<std::string> TRestTools::ReadYALMObservables(const YAML::Node& node) {
    std::vector<std::string> observables;

    if (node["observables"]) {
        for (auto obs : node["observables"]) {
            observables.push_back(obs.as<std::string>());
        }
    }

    return observables;
}
