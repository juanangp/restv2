#include "TRestGDMLParser.h"

#include <TGeoManager.h>
#include <unistd.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "TRestSystemOfUnits.h"
#include "TRestTools.h"
#include "TRestYAMLUtils.h"

namespace fs = std::filesystem;

namespace {

// Raw strings with the delimiter "re": the expressions contain )"
const std::regex kDoctype(R"re(<!DOCTYPE[^\[>]*\[([\s\S]*?)\]\s*>)re");
const std::regex kEntity(R"re(<!ENTITY\s+([A-Za-z_][\w.\-]*)\s+(SYSTEM\s+)?(?:"([^"]*)"|'([^']*)')\s*>)re");
const std::regex kXmlDeclaration(R"re(<\?xml[^>]*\?>)re");
const std::regex kReference(R"re(&([A-Za-z_][\w.\-]*);)re");
const std::regex kConstant(R"re(<(constant|variable)\b[^>]*>)re");
const std::regex kName(R"re(\bname\s*=\s*"([^"]*)")re");
const std::regex kValue(R"re(\bvalue\s*=\s*"[^"]*")re");

using TRestTools::IsURL;

std::string ReadText(const std::string& fileName) {
    std::ifstream input(fileName);
    if (!input) throw std::runtime_error("GDML: cannot read " + fileName);
    std::stringstream text;
    text << input.rdbuf();
    return text.str();
}

/// The text of a GDML file or URL
std::string ReadLocation(const std::string& location) { return ReadText(TRestTools::GetLocalFile(location)); }

/// Location of an entity file, relative to the file (or URL) that includes it
std::string EntityLocation(const std::string& system, const std::string& base) {
    if (IsURL(system) || fs::path(system).is_absolute()) return system;
    if (IsURL(base)) return base.substr(0, base.rfind('/') + 1) + system;
    return (fs::path(base).parent_path() / system).lexically_normal().string();
}

/// The text with each match of `re` replaced by replacement(match)
template <typename Function>
std::string ReplaceMatches(const std::string& text, const std::regex& re, Function replacement) {
    std::string out;
    auto last = text.cbegin();
    for (std::sregex_iterator it(text.begin(), text.end(), re), end; it != end; ++it) {
        out.append(last, (*it)[0].first);
        out += replacement(*it);
        last = (*it)[0].second;
    }
    out.append(last, text.cend());
    return out;
}

/// Value of a parameter for a GDML constant: the number in REST units
std::string ConstantValue(const std::string& name, const std::string& value) {
    const auto number = REST_Units::ParseValueWithUnits(value);
    if (!number) {
        throw std::runtime_error("GDML: parameter '" + name + "' (" + value +
                                 ") replaces a constant, it must be a number");
    }
    std::ostringstream text;
    text << std::setprecision(15) << *number;
    return text.str();
}

/// The constants and variables with the name of a parameter with its value
std::string ApplyConstants(const std::string& text, const std::map<std::string, std::string>& parameters,
                           std::set<std::string>* applied) {
    return ReplaceMatches(text, kConstant, [&](const std::smatch& tag) {
        const std::string element = tag[0].str();
        std::smatch name;
        if (!std::regex_search(element, name, kName)) return element;
        const auto parameter = parameters.find(name[1].str());
        if (parameter == parameters.end()) return element;
        if (applied) applied->insert(parameter->first);
        const std::string value = "value=\"" + ConstantValue(parameter->first, parameter->second) + "\"";
        return std::regex_replace(element, kValue, value, std::regex_constants::format_first_only);
    });
}

struct Entity {
    bool external = false;
    /// The text, or the location of the file
    std::string value;
};

/// The text of a GDML file with its entities replaced (recursively) and without DOCTYPE
std::string Expand(const std::string& location, std::map<std::string, Entity>& entities,
                   std::vector<std::string>& includes, const std::map<std::string, std::string>& parameters,
                   std::set<std::string>* applied) {
    std::string text = ReadLocation(location);
    // An included file: its content without its own XML declaration
    if (!includes.empty()) text = std::regex_replace(text, kXmlDeclaration, "");
    includes.push_back(location);

    std::smatch doctype;
    if (std::regex_search(text, doctype, kDoctype)) {
        const std::string subset = doctype[1].str();
        for (std::sregex_iterator it(subset.begin(), subset.end(), kEntity), end; it != end; ++it) {
            const std::string name = (*it)[1].str();
            const std::string value = (*it)[3].matched ? (*it)[3].str() : (*it)[4].str();
            // As XML, the first declaration of an entity is the one used
            if (entities.count(name)) continue;
            Entity entity;
            entity.external = (*it)[2].matched;
            if (entity.external) {
                entity.value = EntityLocation(value, location);
            } else {
                const auto parameter = parameters.find(name);
                entity.value = parameter == parameters.end() ? value : parameter->second;
                if (parameter != parameters.end() && applied) applied->insert(name);
            }
            entities[name] = entity;
        }
        text.erase(doctype.position(0), doctype.length(0));
    }

    text = ReplaceMatches(text, kReference, [&](const std::smatch& reference) {
        const auto entity = entities.find(reference[1].str());
        // Not declared (e.g. &lt; or &amp;): kept
        if (entity == entities.end()) return reference[0].str();
        if (!entity->second.external) return entity->second.value;
        for (const auto& include : includes) {
            if (include == entity->second.value) {
                throw std::runtime_error("GDML: " + entity->second.value + " includes itself");
            }
        }
        return Expand(entity->second.value, entities, includes, parameters, applied);
    });
    includes.pop_back();
    return text;
}

/// Writes the parameters in a local GDML file and in the local files it includes
void UpdateFile(const std::string& fileName, const std::map<std::string, std::string>& parameters,
                std::set<std::string>* applied, std::set<std::string>& visited) {
    if (!visited.insert(fs::weakly_canonical(fileName).string()).second) return;
    const std::string text = ReadText(fileName);
    std::vector<std::string> includes;
    std::string updated = ReplaceMatches(text, kEntity, [&](const std::smatch& declaration) {
        const std::string name = declaration[1].str();
        const std::string value = declaration[3].matched ? declaration[3].str() : declaration[4].str();
        if (declaration[2].matched) {
            const std::string location = EntityLocation(value, fileName);
            if (!IsURL(location)) includes.push_back(location);
            return declaration[0].str();
        }
        const auto parameter = parameters.find(name);
        if (parameter == parameters.end()) return declaration[0].str();
        if (parameter->second.find('"') != std::string::npos) {
            throw std::runtime_error("GDML: parameter '" + name + "' cannot contain '\"'");
        }
        if (applied) applied->insert(name);
        // In an entity value % (parameter entity) and & are not allowed: character references
        std::string escaped;
        for (const char character : parameter->second) {
            if (character == '%') {
                escaped += "&#37;";
            } else if (character == '&') {
                escaped += "&#38;";
            } else {
                escaped += character;
            }
        }
        return "<!ENTITY " + name + " \"" + escaped + "\">";
    });
    updated = ApplyConstants(updated, parameters, applied);
    if (updated != text) {
        std::ofstream output(fileName);
        if (!output) throw std::runtime_error("GDML: cannot write " + fileName);
        output << updated;
        std::cout << "GDML: parameters written in " << fileName << std::endl;
    }
    for (const auto& include : includes) UpdateFile(include, parameters, applied, visited);
}

}  // namespace

std::string TRestGDMLParser::Resolve(const std::string& gdmlFile,
                                     const std::map<std::string, std::string>& parameters,
                                     std::set<std::string>* applied) {
    std::map<std::string, Entity> entities;
    std::vector<std::string> includes;
    const std::string location = IsURL(gdmlFile) ? gdmlFile : TRestTools::GetFullPath(gdmlFile);
    return ApplyConstants(Expand(location, entities, includes, parameters, applied), parameters, applied);
}

std::string TRestGDMLParser::WriteResolved(const std::string& gdmlFile) {
    std::set<std::string> applied;
    const std::string text = Resolve(gdmlFile, TRestYAMLUtils::GetParameters(), &applied);
    for (const auto& name : applied) TRestYAMLUtils::MarkParameterUsed(name);

    static std::atomic<int> counter{0};
    const fs::path fileName =
        fs::temp_directory_path() / ("REST_" + std::to_string(getpid()) + "_" + std::to_string(counter++) +
                                     "_" + fs::path(gdmlFile).filename().string());
    std::ofstream output(fileName);
    if (!output) throw std::runtime_error("GDML: cannot write the temporary file " + fileName.string());
    output << text;
    return fileName.string();
}

TGeoManager* TRestGDMLParser::Import(const std::string& gdmlFile) {
    const std::string resolved = WriteResolved(gdmlFile);
    // TGeoManager::Import deletes the current gGeoManager, which may be owned by someone else (e.g. the
    // geometry of a readout, deleted by it): it is only replaced as current
    gGeoManager = nullptr;
    TGeoManager* geometry = TGeoManager::Import(resolved.c_str());
    fs::remove(resolved);
    return geometry;
}

void TRestGDMLParser::UpdateParameters(const std::string& gdmlFile,
                                       const std::map<std::string, std::string>& parameters,
                                       std::set<std::string>* applied) {
    if (IsURL(gdmlFile)) throw std::runtime_error("GDML: " + gdmlFile + " is a URL, it cannot be updated");
    std::set<std::string> visited;
    UpdateFile(TRestTools::GetFullPath(gdmlFile), parameters, applied, visited);
}
