

#include "TRestTools.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <regex>
#include <thread>
#include <string>
#include <charconv>
#include <cmath>
#include <string_view>


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

Bool_t TRestTools::StringToBool(std::string booleanString) {
    // Convert the full string to uppercase in a single pass.
    std::transform(booleanString.begin(), booleanString.end(), booleanString.begin(), ::toupper);

    return booleanString == "TRUE" || booleanString == "ON" || booleanString == "1";
}

std::pair<std::string, std::string> TRestTools::SeparatePathAndName(const std::string& fullname) {
    fs::path path(fullname);
    return {path.parent_path().string(), path.filename().string()};
}

std::string TRestTools::GetTimeStampFromUnixTime(const double tm) {
    char tmpstm[20];  //"YYYY-MM-DD HH:MM:SS" + \0
    std::time_t time = static_cast<std::time_t>(tm);

    std::strftime(tmpstm, sizeof(tmpstm), "%Y-%m-%d %H:%M:%S", std::localtime(&time));

    return std::string(tmpstm);
}

bool TRestTools::fileExists(const std::string& filename) { return std::filesystem::exists(filename); }

std::string TRestTools::GetFullPath(const std::string& filename) {
    if (filename.empty()) {
        return "";
    }

    try {
        // Convert the input string into a standard filesystem path
        std::filesystem::path relativePath(filename);
        // Convert the relative path to an absolute path
        std::filesystem::path absolutePath = std::filesystem::absolute(relativePath);
        // Convert the absolute path back to a standard string and return it
        return absolutePath.string();
    } catch (const std::filesystem::filesystem_error& e) {
        // Fallback or error logging can be placed here if needed
        // For now, it returns an empty string if an unexpected filesystem error occurs
        return "";
    }
}

std::string TRestTools::SearchFileInPath(const std::vector<std::string>& paths, const std::string& filename) {
    if (fs::exists(filename)) return filename;

    for (const auto& p : paths) {
        fs::path basePath(p);
        if (fs::exists(basePath / filename)) return (basePath / filename).string();

        if (fs::is_directory(basePath)) {
            // Use explicit iterator to access depth() control
            for (fs::recursive_directory_iterator it(basePath, fs::directory_options::skip_permission_denied),
                 end;
                 it != end; ++it) {
                // Keeps the 5-level recursion limit using the iterator depth
                if (it.depth() >= 5) {
                    it.disable_recursion_pending();  // Optimization: stop descending deeper into this branch
                    continue;
                }

                const auto& entry = *it;
                if (entry.is_directory() && fs::exists(entry.path() / filename)) {
                    return (entry.path() / filename).string();
                }
            }
        }
    }
    return "";
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

YAML::Node TRestTools::OpenConfigFile(const std::string& fileName) {
    YAML::Node raw = YAML::LoadFile(fileName);
    YAML::Node cfg = TRestTools::ResolveAllRefs(raw);
    return cfg;
}

std::pair<std::string, YAML::Node> TRestTools::GetMetadataClass(const YAML::Node& cfg,
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

/// \brief Recursively resolves YAML placeholder references.
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

void TRestTools::OverrideYAMLParam(YAML::Node& node, const std::string& key, const std::string& val) {
    if (node.IsMap()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            if (it->first.IsScalar() && it->first.as<std::string>() == key) {
                node[it->first] = val;
            }

            YAML::Node child = it->second;
            OverrideYAMLParam(child, key, val);
        }
    } else if (node.IsSequence()) {
        for (size_t i = 0; i < node.size(); ++i) {
            YAML::Node child = node[i];
            OverrideYAMLParam(child, key, val);
        }
    }
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

// Helper function to remove spaces and special characters from a string
std::string TRestTools::CleanString(const std::string& str) {
    std::string s = str;
    s.erase(std::remove_if(s.begin(), s.end(),
                           [](unsigned char c) {
                               return std::isspace(c) || c == '/' || c == '\\' || c == ':' || c == '*' ||
                                      c == '?' || c == '"' || c == '<' || c == '>' || c == '|';
                           }),
            s.end());
    return s;
}

// Helper to replace all occurrences of a substring
void TRestTools::ReplaceAll(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

std::string TRestTools::ToTimeStringLong(double seconds) {
    const auto absSeconds = std::abs(seconds); 
    
    char buffer[32];
    double valueToFormat = seconds;
    std::string_view unit;

    if (absSeconds < 60) {
        unit = " seconds";
    } else if (absSeconds < 3600) {
        valueToFormat = seconds / 60.0;
        unit = " minutes";
    } else if (absSeconds < 86400) {
        valueToFormat = seconds / 3600.0;
        unit = " hours";
    } else {
        valueToFormat = seconds / 86400.0;
        unit = " days";
    }

    auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), valueToFormat, std::chars_format::fixed, 2);

    if (ec == std::errc{}) {
        return std::string(buffer, ptr - buffer) + std::string(unit);
    }

    return "0.00 seconds";
}

