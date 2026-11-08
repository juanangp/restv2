#ifndef TRESTTOOLS_H
#define TRESTTOOLS_H

#include <functional>
#include <optional>
#include <string>
#include <vector>

class TDirectory;
class TFile;
class TGeoManager;

/// \brief Generic functions: strings, files, ROOT files and geometries. The YAML functions are in
/// TRestYAMLUtils.
namespace TRestTools {

/// \brief Regular files of a directory whose name matches a pattern, e.g. "data/Run_*.root": `*` is the
/// only wildcard (PatternToRegex), the match ignores case and only the directory of the pattern (the current
/// one if it has none) is searched, not its subdirectories. Empty (with a warning) for an empty pattern.
std::vector<std::string> GetFilesMatchingPattern(const std::string& filePattern);
/// \brief Local date and time of a Unix time, "YYYY-MM-DD HH:MM:SS"; empty if it cannot be converted
std::string GetTimeStampFromUnixTime(const double tm);
/// \brief Current Unix time in seconds (with microseconds)
double GetCurrentTime();
/// \brief Unix time of a local date, "YYYY/MM/DD", "YYYY-MM-DD" and optionally " HH:MM" or " HH:MM:SS" (as
/// legacy REST_StringHelper::StringToTimeStamp). Throws if it is not a valid date.
double StringToTimeStamp(const std::string& date);
/// \brief Absolute, lexically normalised path of a file (the current directory for an empty name); empty
/// on a filesystem error. The file does not need to exist.
std::string GetFullPath(const std::string& filename);
/// \brief Whether a file or directory exists
bool fileExists(const std::string& filename);
/// \brief Whether the file exists and starts with the ROOT magic bytes ("root")
bool isRootFile(const std::string& filename);
/// \brief Whether the file is a REST run file: a ROOT file with an AnalysisTree and a RESTMetadataStore
/// holding the YAML of a TRestRun (the reason is logged, RESTWarning/RESTError, when it is not)
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
/// \brief Geometry `key` of a folder, read once per process: the same file and key give the same TGeoManager
/// (cached, never deleted), so the metadata of several TRestRun of the same file (e.g. one per thread of
/// restManager) share it, and reading it does not delete another geometry (TGeoManager::Init deletes
/// gGeoManager). Thread safe. gGeoManager becomes this geometry only if there was none. nullptr if the folder
/// has no geometry.
TGeoManager* ReadGeometry(TDirectory* folder, const std::string& key = "Geometry");
/// \brief A new copy of the geometry `key` of a folder, owned by the caller, for a geometry navigated
/// (FindNode, cd) by each thread, e.g. the readouts (one per thread of restManager): a TGeoManager cannot
/// be navigated by several threads. Read with the same lock as ReadGeometry, without deleting gGeoManager;
/// gGeoManager becomes this geometry only if there was none. nullptr if the folder has no geometry.
TGeoManager* ReadGeometryCopy(TDirectory* folder, const std::string& key = "Geometry");
/// \brief Hash of the geometry `key` of a folder: the one written with it or, if there is none (files
/// written before), computed reading the geometry (slow; the current gGeoManager is kept). Empty if the
/// folder has no geometry.
std::string ReadGeometryHash(TDirectory* folder, const std::string& key = "Geometry");
/// \brief Deletes a geometry (e.g. one of ReadGeometryCopy) with the same lock as ReadGeometry: ~TGeoManager
/// sets gGeoManager and gGeoIdentity to nullptr, so they are restored afterwards unless the deleted geometry
/// was the current one (gGeoIdentity is taken from the current geometry if it belonged to the deleted one).
/// nullptr does nothing. Use it instead of `delete` (or a std::unique_ptr) for any TGeoManager.
void DeleteGeometry(TGeoManager* geometry);
/// \brief Local copy of a URL (curl), cached in $REST_HOME/.rest/download (or $HOME) and reused if it is
/// already there; throws if it cannot be downloaded or if the URL has characters that are not safe in a
/// shell command (', $, `, \, spaces, control characters...: only the RFC 3986 ones) or no scheme (http://)
std::string DownloadFile(const std::string& url);
/// \brief Directory of the files of REST of the user (history, settings, downloads...): $REST_HOME/.rest, or
/// $HOME/.rest without REST_HOME; created if it does not exist. Throws if neither is defined.
std::string GetRESTUserDirectory();
/// \brief Path of a file: `filename` itself if it exists, otherwise the first `<path>/<filename>` found in
/// the paths or in their subdirectories (up to 5 levels deep); empty if it is not found
std::string SearchFileInPath(const std::vector<std::string>& paths, const std::string& filename);
/// \brief (directory, file name) of a path, e.g. ("/data", "run.root") for "/data/run.root"
std::pair<std::string, std::string> SeparatePathAndName(const std::string& fullname);
/// \brief True for "true", "on" or "1" (ignoring case), false for anything else
bool StringToBool(std::string booleanString);
/// \brief Duration with its unit, e.g. "2.50 hours" (seconds, minutes, hours, days or weeks)
std::string ToTimeStringLong(double seconds);
/// \brief Tokens of a string separated by `delim` (empty tokens kept, no trailing empty token)
std::vector<std::string> Split(const std::string& s, char delim);
/// \brief Number of non-overlapping occurrences of `substring` in `in` (0 if either is empty)
int CountString(const std::string& in, const std::string& substring);
/// \brief Whether the whole string (trimmed) is a floating point number (std::strtod), e.g. "1.5e3"
bool isANumber(const std::string& str);
/// \brief Integer value of a string: hexadecimal if it contains "0x", otherwise a number truncated to an
/// integer (e.g. "12.7" gives 12); 0 for an empty string or (with an error message) one that is not a
/// number
int StringToInteger(const std::string& in);
/// \brief Next automatic run number: the one stored in <GetRESTUserDirectory()>/runNumber ($REST_HOME/.rest,
/// or $HOME/.rest without REST_HOME; 0 the first time), which is incremented for the next call. The file is
/// locked, so concurrent processes get different numbers. Throws if neither REST_HOME nor HOME is defined
/// or the file cannot be written.
int GetRunNumberAuto();
/// \brief Random positive seed (std::random_device seeding a per-thread std::mt19937_64)
long GetRandomSeed();
/// \brief The expression without whitespace (spaces, tabs, new lines)
std::string CleanExpression(const std::string& expr);

/// \brief Regular expression of a file name pattern: `*` becomes `.*` and every other punctuation
/// character is escaped (so `?` is not a wildcard)
std::string PatternToRegex(const std::string& pattern);
/// \brief The string without whitespace and the characters not valid in file names (/ \\ : * ? " < > |)
std::string CleanString(const std::string& str);
/// \brief Replaces in place every occurrence of `from` by `to` (the replacements are not scanned again);
/// nothing if `from` is empty
void ReplaceAll(std::string& str, const std::string& from, const std::string& to);
/// \brief The string in lower case (ASCII)
std::string ToLower(std::string str);
/// \brief The option of `options` equal to `value` ignoring case (its spelling, e.g. "tripleMaxAverage" for
/// "TRIPLEMAXAVERAGE"), to normalise a configuration field in LoadConfig (the YAML saved gets it too).
/// Throws std::invalid_argument naming `field` and the options when none matches.
std::string NormalizeOption(const std::string& field, const std::string& value,
                            const std::vector<std::string>& options);
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
