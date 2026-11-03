#ifndef TRESTTOOLS_H
#define TRESTTOOLS_H

#include <functional>
#include <optional>
#include <string>
#include <vector>

class TDirectory;
class TFile;
class TGeoManager;

/// Generic functions: strings, files, ROOT files and geometries. The YAML functions are in TRestYAMLUtils.
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
/// \brief Whether the file is a dataset (TRestDataSet: AnalysisTree and RESTDataSetInfo)
bool isDataSet(const std::string& filename);
/// \brief Copies the objects of a ROOT folder, and its subfolders, into another one (objects replaced). A
/// geometry (TGeoManager) read does not change the current one (gGeoManager).
void CopyDirectory(TDirectory* source, TDirectory* target);
/// \brief Structure of a geometry: each volume (shape, material, capacity) and its daughters (name and
/// position)
std::string DescribeGeometry(TGeoManager* geometry);
/// \brief MD5 of DescribeGeometry: two geometries with the same structure have the same hash
std::string GetGeometryHash(TGeoManager* geometry);
/// \brief Writes a geometry in a folder as `key`, with its hash (GetGeometryHash) as the TObjString
/// `<key>MD5`, so it is compared without reading it (ReadGeometryHash)
void WriteGeometry(TDirectory* folder, TGeoManager* geometry, const std::string& key = "Geometry");
/// \brief Hash of the geometry `key` of a folder: the one written with it or, if there is none (files
/// written before), computed reading the geometry (slow; the current gGeoManager is kept). Empty if the
/// folder has no geometry.
std::string ReadGeometryHash(TDirectory* folder, const std::string& key = "Geometry");
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

std::string PatternToRegex(const std::string& pattern);
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

}  // namespace TRestTools

#endif
