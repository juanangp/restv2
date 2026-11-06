#include "TRestTools.h"

#include <TClass.h>
#include <TFile.h>
#include <TGeoManager.h>
#include <TGeoMatrix.h>
#include <TGeoNode.h>
#include <TGeoVolume.h>
#include <TKey.h>
#include <TMD5.h>
#include <TObjString.h>
#include <TObject.h>
#include <sys/file.h>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <regex>
#include <stdexcept>
#include <string>
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

bool TRestTools::StringToBool(std::string booleanString) {
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

double TRestTools::StringToTimeStamp(const std::string& date) {
    std::tm time{};
    const char separator = date.find('/') != std::string::npos ? '/' : '-';
    const std::string format = std::string("%d") + separator + "%d" + separator + "%d %d:%d:%d";
    const int fields = std::sscanf(date.c_str(), format.c_str(), &time.tm_year, &time.tm_mon, &time.tm_mday,
                                   &time.tm_hour, &time.tm_min, &time.tm_sec);
    if (fields != 3 && fields != 5 && fields != 6) {
        throw std::runtime_error("TRestTools::StringToTimeStamp: not a valid date \"" + date +
                                 "\" (YYYY/MM/DD or YYYY-MM-DD, optionally HH:MM or HH:MM:SS)");
    }
    time.tm_year -= 1900;
    time.tm_mon--;
    time.tm_isdst = -1;
    return static_cast<double>(std::mktime(&time));
}

bool TRestTools::fileExists(const std::string& filename) { return std::filesystem::exists(filename); }

std::string TRestTools::GetFullPath(const std::string& filename) {
    std::string targetPath = filename.empty() ? "." : filename;

    try {
        // Convert the input string into a standard filesystem path
        std::filesystem::path relativePath(targetPath);
        // Convert the relative path to an absolute path
        std::filesystem::path absolutePath = std::filesystem::absolute(relativePath).lexically_normal();
        ;
        // Convert the absolute path back to a standard string and return it
        return absolutePath.string();
    } catch (const std::filesystem::filesystem_error& e) {
        // Fallback or error logging can be placed here if needed
        // For now, it returns an empty string if an unexpected filesystem error occurs
        return "";
    }
}

bool TRestTools::isRootFile(const std::string& filename) {
    if (!TRestTools::fileExists(filename)) {
        return false;
    }

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    char magic[4];
    file.read(magic, 4);

    if (!file) {
        return false;
    }

    return (magic[0] == 'r' && magic[1] == 'o' && magic[2] == 'o' && magic[3] == 't');
}

bool TRestTools::isValidTRestRun(const std::string& filename) {
    if (!TRestTools::isRootFile(filename)) {
        // std::cout<<filename<<" is not a valid root file"<<std::endl;
        return false;
    }

    std::unique_ptr<TFile> file(TFile::Open(filename.c_str(), "READ"));
    if (!file || file->IsZombie()) {
        std::cout << filename << " is zombie or invalid" << std::endl;
        return false;
    }

    TObject* analysisTree = file->Get("AnalysisTree");
    if (!analysisTree) {
        std::cout << filename << " analysisTree not found" << std::endl;
        file->Close();
        return false;
    }

    TDirectory* metadataDir = file->GetDirectory("RESTMetadataStore");
    if (!metadataDir) {
        std::cout << filename << " no TRestMetadata" << std::endl;
        file->Close();
        return false;
    }

    metadataDir->cd();

    TList* keysInDir = metadataDir->GetListOfKeys();
    bool hasRestRunMetadata = false;

    if (keysInDir) {
        for (int i = 0; i < keysInDir->GetEntries(); ++i) {
            TKey* key = dynamic_cast<TKey*>(keysInDir->At(i));
            if (!key) continue;

            std::string instanceName = key->GetName();

            TObjString* yamlObj = nullptr;
            metadataDir->GetObject(instanceName.c_str(), yamlObj);

            if (yamlObj) {
                YAML::Node testConfig = YAML::Load(yamlObj->GetString().Data());

                if (testConfig && !testConfig.IsNull() && testConfig["class"]) {
                    std::string className = testConfig["class"].as<std::string>();

                    if (className == "TRestRun") {
                        hasRestRunMetadata = true;
                        break;
                    }
                }
            }
        }
    }
    if (!hasRestRunMetadata) std::cout << filename << " no valid TRestRun metadata" << std::endl;

    file->Close();
    return hasRestRunMetadata;
}

bool TRestTools::isDataSet(const std::string& filename) {
    if (!TRestTools::isRootFile(filename)) return false;
    std::unique_ptr<TFile> file(TFile::Open(filename.c_str(), "READ"));
    if (!file || file->IsZombie()) return false;
    // The runs of the dataset (TRestDataSet), a TObjString
    std::unique_ptr<TObjString> runs(file->Get<TObjString>("RESTDataSetInfo"));
    return file->GetKey("AnalysisTree") && runs;
}

void TRestTools::CopyDirectory(TDirectory* source, TDirectory* target) {
    if (!source || !target) return;
    for (auto* object : *source->GetListOfKeys()) {
        auto* key = static_cast<TKey*>(object);
        const TClass* type = TClass::GetClass(key->GetClassName());
        if (!type) continue;
        if (type->InheritsFrom(TDirectory::Class())) {
            TDirectory* folder = target->GetDirectory(key->GetName());
            if (!folder) folder = target->mkdir(key->GetName());
            CopyDirectory(source->GetDirectory(key->GetName()), folder);
            continue;
        }
        // Reading a geometry makes it the current one
        TGeoManager* currentGeometry = gGeoManager;
        std::unique_ptr<TObject> copy(key->ReadObj());
        if (copy) {
            target->cd();
            copy->Write(key->GetName(), TObject::kOverwrite);
        }
        copy.reset();
        gGeoManager = currentGeometry;
    }
}

std::string TRestTools::DescribeGeometry(TGeoManager* geometry) {
    std::string text;
    if (!geometry) return text;
    for (auto* object : *geometry->GetListOfVolumes()) {
        const auto* volume = static_cast<TGeoVolume*>(object);
        const TGeoShape* shape = volume->GetShape();
        text += Format("%s %s %s %.9g\n", volume->GetName(), shape ? shape->ClassName() : "",
                       volume->GetMaterial() ? volume->GetMaterial()->GetName() : "",
                       shape ? shape->Capacity() : 0.);
        for (int i = 0; i < volume->GetNdaughters(); i++) {
            const TGeoNode* node = volume->GetNode(i);
            const double* position = node->GetMatrix()->GetTranslation();
            text += Format("  %s %.9g %.9g %.9g\n", node->GetName(), position[0], position[1], position[2]);
        }
    }
    return text;
}

std::string TRestTools::GetGeometryHash(TGeoManager* geometry) {
    const std::string description = DescribeGeometry(geometry);
    TMD5 md5;
    md5.Update(reinterpret_cast<const UChar_t*>(description.data()), description.size());
    md5.Final();
    return md5.AsString();
}

void TRestTools::WriteGeometry(TDirectory* folder, TGeoManager* geometry, const std::string& key) {
    if (!folder || !geometry) return;
    TDirectory::TContext context(folder);
    geometry->Write(key.c_str(), TObject::kOverwrite);
    TObjString hash(GetGeometryHash(geometry).c_str());
    hash.Write((key + "MD5").c_str(), TObject::kOverwrite);
}

namespace {
/// Reading a TGeoManager deletes gGeoManager (TGeoManager::Init) and makes it the current one: the readings
/// are serialized and gGeoManager is kept
std::mutex gGeometryMutex;

/// Reads the geometry with gGeoManager kept (it becomes the geometry read only if there was none); the
/// caller holds gGeometryMutex
TGeoManager* ReadGeometryLocked(TDirectory* folder, const std::string& key) {
    TGeoManager* currentGeometry = gGeoManager;
    gGeoManager = nullptr;
    TGeoManager* geometry = nullptr;
    folder->GetObject(key.c_str(), geometry);
    gGeoManager = currentGeometry ? currentGeometry : geometry;
    return geometry;
}
}  // namespace

TGeoManager* TRestTools::ReadGeometry(TDirectory* folder, const std::string& key) {
    if (!folder || !folder->GetKey(key.c_str())) return nullptr;
    static std::map<std::string, TGeoManager*> geometries;
    // The path includes the file name (file.root:/folder)
    const std::string id = std::string(folder->GetPath()) + "/" + key;
    std::lock_guard<std::mutex> lock(gGeometryMutex);
    const auto it = geometries.find(id);
    if (it != geometries.end()) return it->second;
    TGeoManager* geometry = ReadGeometryLocked(folder, key);
    if (geometry) geometries[id] = geometry;
    return geometry;
}

TGeoManager* TRestTools::ReadGeometryCopy(TDirectory* folder, const std::string& key) {
    if (!folder || !folder->GetKey(key.c_str())) return nullptr;
    std::lock_guard<std::mutex> lock(gGeometryMutex);
    return ReadGeometryLocked(folder, key);
}

std::string TRestTools::ReadGeometryHash(TDirectory* folder, const std::string& key) {
    if (!folder || !folder->GetKey(key.c_str())) return "";
    std::unique_ptr<TObjString> stored(folder->Get<TObjString>((key + "MD5").c_str()));
    if (stored) return stored->GetString().Data();
    // Written before the hash: a copy of the geometry is read (gGeoManager kept)
    std::unique_ptr<TGeoManager> geometry(ReadGeometryCopy(folder, key));
    return geometry ? GetGeometryHash(geometry.get()) : "";
}

int TRestTools::GetRunNumberAuto() {
    const char* restHomeEnv = std::getenv("REST_HOME");
    if (!restHomeEnv) {
        throw std::runtime_error(
            "TRestTools::GetRunNumberAuto: REST_HOME environment variable is not defined.");
    }

    std::filesystem::path runFilePath = std::filesystem::path(restHomeEnv) / ".rest" / "runNumber";

    try {
        std::filesystem::create_directories(runFilePath.parent_path());
    } catch (const std::filesystem::filesystem_error& e) {
        throw std::runtime_error("TRestTools::GetRunNumberAuto: Could not create or access directory: " +
                                 runFilePath.parent_path().string() + " (" + e.what() + ")");
    }

    int fd = ::open(runFilePath.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        throw std::runtime_error("TRestTools::GetRunNumberAuto: Failed to open or create runNumber file: " +
                                 runFilePath.string() + " (" + std::strerror(errno) + ")");
    }

    // Lock the file exclusively to handle multi-process concurrency safely
    ::flock(fd, LOCK_EX);

    int currentRunNr = 0;
    if (std::filesystem::exists(runFilePath) && std::filesystem::file_size(runFilePath) > 0) {
        std::ifstream in(runFilePath);
        if (in.is_open()) in >> currentRunNr;
    }

    // Increment the run number for the next execution
    int nextRunNr = currentRunNr + 1;

    // Overwrite the file with the new incremented value
    std::ofstream out(runFilePath, std::ios::trunc);
    if (out.is_open()) {
        out << nextRunNr << "\n";
        out.flush();
    } else {
        RESTError << "Failed to update runNumber file: " << runFilePath << RESTendl;
        ::flock(fd, LOCK_UN);
        ::close(fd);
        throw std::runtime_error("TRestTools::GetRunNumberAuto: Failed to update runNumber file: " +
                                 runFilePath.string());
    }

    ::flock(fd, LOCK_UN);
    ::close(fd);

    // Return the current allocated run number to the process
    return currentRunNr;
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

std::vector<std::string> TRestTools::Split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) tokens.push_back(item);
    return tokens;
}

int TRestTools::CountString(const std::string& in, const std::string& substring) {
    if (substring.empty() || in.empty()) {
        return 0;
    }

    int count = 0;
    size_t nPos = in.find(substring, 0);

    while (nPos != std::string::npos) {
        count++;
        nPos = in.find(substring, nPos + substring.length());
    }

    return count;
}

bool TRestTools::isANumber(const std::string& str) {
    if (str.empty()) return false;

    const std::string cleanStr = Trim(str);
    if (cleanStr.empty()) return false;

    char* endptr = nullptr;
    std::strtod(cleanStr.c_str(), &endptr);

    return (endptr != nullptr && *endptr == '\0');
}

int TRestTools::StringToInteger(const std::string& in) {
    if (in.empty()) return 0;

    try {
        if (in.find("0x") != std::string::npos) {
            return static_cast<int>(std::stoul(in, nullptr, 16));
        }

        return static_cast<int>(std::stod(in));
    } catch (const std::exception& e) {
        RESTError << "StringToInteger - Error '" << in << "': " << e.what() << RESTendl;
        return 0;
    }
}

long TRestTools::GetRandomSeed() {
    thread_local std::random_device rd;
    thread_local std::mt19937_64 engine(rd());
    std::uniform_int_distribution<long> dist(1, std::numeric_limits<long>::max());

    return dist(engine);
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

// Helper function to remove spaces and special characters from a string
std::string TRestTools::ReplaceDelimited(const std::string& text, char open, char close,
                                         const TextReplacement& replacement) {
    std::string result;
    size_t position = 0;
    while (true) {
        const size_t first = text.find(open, position);
        const size_t last = first == std::string::npos ? first : text.find(close, first + 1);
        if (last == std::string::npos) {
            result += text.substr(position);
            break;
        }
        result += text.substr(position, first - position);
        const auto value = replacement(text.substr(first + 1, last - first - 1));
        result += value ? *value : text.substr(first, last - first + 1);
        position = last + 1;
    }
    return result;
}

std::string TRestTools::ReplaceIdentifiers(const std::string& expression, const std::string& identifier,
                                           const TextReplacement& replacement) {
    const std::regex regex(identifier);
    std::string result;
    size_t last = 0;
    for (auto it = std::sregex_iterator(expression.begin(), expression.end(), regex);
         it != std::sregex_iterator(); ++it) {
        const size_t position = it->position();
        const char previous = position > 0 ? expression[position - 1] : ' ';
        if (std::isdigit(static_cast<unsigned char>(previous)) || previous == '.') continue;
        const auto value = replacement(it->str());
        if (!value) continue;
        result += expression.substr(last, position - last) + *value;
        last = position + it->length();
    }
    return result + expression.substr(last);
}

std::string TRestTools::Trim(const std::string& str, const std::string& characters) {
    const size_t first = str.find_first_not_of(characters);
    if (first == std::string::npos) return "";
    return str.substr(first, str.find_last_not_of(characters) - first + 1);
}

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

std::string TRestTools::ToLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return str;
}

std::string TRestTools::NormalizeOption(const std::string& field, const std::string& value,
                                        const std::vector<std::string>& options) {
    const std::string lower = ToLower(value);
    for (const auto& option : options) {
        if (ToLower(option) == lower) return option;
    }
    std::string list;
    for (const auto& option : options) list += (list.empty() ? "\"" : ", \"") + option + "\"";
    throw std::invalid_argument("Invalid " + field + " \"" + value + "\", options: " + list);
}

std::string TRestTools::Format(const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list argsCopy;
    va_copy(argsCopy, args);
    const int size = std::vsnprintf(nullptr, 0, format, argsCopy);
    va_end(argsCopy);
    std::string result(size > 0 ? size : 0, '\0');
    if (size > 0) std::vsnprintf(result.data(), size + 1, format, args);
    va_end(args);
    return result;
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

    auto [ptr, ec] =
        std::to_chars(buffer, buffer + sizeof(buffer), valueToFormat, std::chars_format::fixed, 2);

    if (ec == std::errc{}) {
        return std::string(buffer, ptr - buffer) + std::string(unit);
    }

    return "0.00 seconds";
}

std::string TRestTools::CleanExpression(const std::string& expr) {
    std::string clean = expr;

    clean.erase(std::remove_if(clean.begin(), clean.end(),
                               [](unsigned char c) { return std::isspace(c) || c == '\r' || c == '\n'; }),
                clean.end());

    return clean;
}

std::string TRestTools::DownloadFile(const std::string& url) {
    const char* home = std::getenv("REST_HOME");
    if (!home) home = std::getenv("HOME");
    if (!home) throw std::runtime_error("DownloadFile: neither REST_HOME nor HOME are defined");
    const std::filesystem::path directory = std::filesystem::path(home) / ".rest" / "download";
    std::string name = url.substr(url.find("://") + 3);
    for (char& character : name) {
        if (character == '/' || character == ':' || character == '?' || character == '&') character = '_';
    }
    const std::filesystem::path fileName = directory / name;
    if (std::filesystem::exists(fileName)) return fileName.string();

    std::filesystem::create_directories(directory);
    const std::filesystem::path partial = fileName.string() + ".part" + std::to_string(getpid());
    const std::string command = "curl -sSfL -o '" + partial.string() + "' '" + url + "'";
    std::cout << "Downloading " << url << " to " << fileName.string() << std::endl;
    if (std::system(command.c_str()) != 0 || !std::filesystem::exists(partial)) {
        std::filesystem::remove(partial);
        throw std::runtime_error("DownloadFile: cannot download " + url + " (curl)");
    }
    std::filesystem::rename(partial, fileName);
    return fileName.string();
}
