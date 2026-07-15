#pragma once

#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <chrono>

// ---------------- TRestProgressBar ----------------
class TRestProgressBar {
   public:
    TRestProgressBar() : total(0), current(0), width(60), started(false) {}

    void Reset(int totalEvents, int barWidth = 30) {
        total = totalEvents;
        width = barWidth;
        current = 0;
        started = true;
        startTime = std::chrono::steady_clock::now();
        Update(0);
    }

    void Update(int currentEvent) {
        if (!started || total <= 0) return;
        current = currentEvent;
        if (current > total) current = total;

        double percentage = (double)current / total;
        int progress = width * percentage;

        std::string etaStr = "inf";
        if (current > 0) {
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
            double secondsPerEvent = (double)elapsed / current;
            int remainingEvents = total - current;
            int etaSeconds = remainingEvents * secondsPerEvent;
            
            int etaMinutes = etaSeconds / 60;
            etaStr = std::to_string(etaMinutes) + "." + std::to_string((etaSeconds % 60) * 10 / 60);
        }

        std::cout << "\r\033[K" << current << " Events, " << etaStr << " min ETA, "
                  << (int)(percentage * 100) << ".0%[";

        for (int i = 0; i < width; ++i) {
            if (i < progress) {
                std::cout << "=";
            } else if (i == progress && current < total) {
                std::cout << ">";
            } else {
                std::cout << " ";
            }
        }
        std::cout << "]";
        std::cout.flush();
    }

    void Finish() {
        if (!started) return;
        Update(total);
        std::cout << std::endl;
        started = false;
    }

   private:
    int total;
    int current;
    int width;
    bool started;
    std::chrono::steady_clock::time_point startTime;
};

inline TRestProgressBar RESTProgress;


class TRestLogManager {
   public:
    enum class REST_Verbose_Level {
        REST_Error = 0,  //!< show minimum information of the software, as well as error messages
        REST_Silent = 1,
        REST_Warning = 2,  //!< +show some essential information, as well as warnings
        REST_Info = 3,     //!< +show most of the information for each steps
        REST_Debug = 4     //!< +show the defined debug messages
    };

    static inline std::unordered_map<std::string, REST_Verbose_Level> verboseMap{
        {"silent", REST_Verbose_Level::REST_Silent},
        {"error", REST_Verbose_Level::REST_Error},
        {"warning", REST_Verbose_Level::REST_Warning},
        {"info", REST_Verbose_Level::REST_Info},
        {"debug", REST_Verbose_Level::REST_Debug}};

    static inline std::map<REST_Verbose_Level, std::string> inverseVerboseMap = [] {
        std::map<REST_Verbose_Level, std::string> inv;
        for (const auto& [str, lvl] : verboseMap) {
            inv[lvl] = str;
        }
        return inv;
    }();

    static inline std::string GetStringFromVerbose(REST_Verbose_Level vLevel) {
        auto it = inverseVerboseMap.find(vLevel);
        if (it != inverseVerboseMap.end()) {
            return it->second;
        } else {
            std::cout << "ERROR verbose level " << (int)vLevel << " not found" << std::endl;
        }
        return GetStringFromVerbose(globalVerboseLevel);
    }

    static inline REST_Verbose_Level GetVerboseLevelFromString(const std::string& vStr) {
        auto it = verboseMap.find(vStr);
        if (it != verboseMap.end()) {
            return it->second;
        } else {
            std::cout << "ERROR verbose string " << vStr << " not found" << std::endl;
        }
        return globalVerboseLevel;
    }

    static TRestLogManager& instance() {
        static TRestLogManager mgr;
        return mgr;
    }

    void inline SetLevel(const std::string& className, REST_Verbose_Level lvl) {
        classLevels[className] = lvl;
    }

    REST_Verbose_Level GetVerboseLevel(const std::string& className) const {
        auto it = classLevels.find(className);
        if (it != classLevels.end()) {
            return it->second;
        }
        return globalVerboseLevel;
    }

    void SetLogFile(const std::string& filename) {
        if (logFile.is_open()) logFile.close();
        logFile.open(filename, std::ios::out | std::ios::app);
    }

    std::ofstream& GetLogFile() { return logFile; }
    bool hasFile() const { return logFile.is_open(); }

    static inline REST_Verbose_Level globalVerboseLevel = REST_Verbose_Level::REST_Info;
    static inline void setGlobalVerboseLevel(REST_Verbose_Level lvl) { globalVerboseLevel = lvl; }

   private:
    std::map<std::string, REST_Verbose_Level> classLevels;
    std::ofstream logFile;
    TRestLogManager() = default;
};

// ---------------- Logger ----------------
class TRestLogger {
   public:
    static constexpr std::string_view COLOR_RESET = "\033[0m";
    static constexpr std::string_view COLOR_BLACK = "\033[30m";   /* Black */
    static constexpr std::string_view COLOR_RED = "\033[31m";     /* Red */
    static constexpr std::string_view COLOR_GREEN = "\033[32m";   /* Green */
    static constexpr std::string_view COLOR_YELLOW = "\033[33m";  /* Yellow */
    static constexpr std::string_view COLOR_BLUE = "\033[34m";    /* Blue */
    static constexpr std::string_view COLOR_MAGENTA = "\033[35m"; /* Magenta */
    static constexpr std::string_view COLOR_CYAN = "\033[36m";    /* Cyan */
    static constexpr std::string_view COLOR_WHITE = "\033[37m";   /* White */

    static void log(TRestLogManager::REST_Verbose_Level level, const std::string_view& color,
                    const std::string& prefix, const std::string& className, const std::string& msg);
};

// ---------------- Contexto de clase ----------------
inline thread_local std::string currentClassName;

#define DECLARE_LOG_CLASS(CLASSNAME)                        \
    static const std::string& getLogClassName() {           \
        static const std::string name = #CLASSNAME;         \
        return name;                                        \
    }                                                       \
    struct LogClassSetter {                                 \
        LogClassSetter() { currentClassName = #CLASSNAME; } \
    } _logClassSetter;

// ---------------- LogStream ----------------
class TRestLogBuffer : public std::stringbuf {
   public:
    TRestLogBuffer(TRestLogManager::REST_Verbose_Level lvl, const std::string_view& color,
                   const std::string& prefix)
        : level(lvl), color(color), prefix(prefix) {}

    int sync() override {
        if (!str().empty()) {
            TRestLogger::log(level, color, prefix, currentClassName, str());
            str("");  // limpiar buffer
        }
        return 0;
    }

   private:
    TRestLogManager::REST_Verbose_Level level;
    std::string_view color;
    std::string prefix;
};

class TRestLogStream : public std::ostream {
   public:
    TRestLogStream(TRestLogManager::REST_Verbose_Level lvl, const std::string_view& color,
                   const std::string& prefix)
        : std::ostream(&buf), buf(lvl, color, prefix) {}

   private:
    TRestLogBuffer buf;
};

// ---------------- Manipulador RESTendl ----------------
inline std::ostream& RESTendl(std::ostream& os) {
    os.put('\n');  // salto de línea
    os.flush();    // forzar volcado inmediato
    return os;
}

// --- Instancias globales accesibles ---
inline TRestLogStream RESTError(TRestLogManager::REST_Verbose_Level::REST_Error, TRestLogger::COLOR_RED,
                                " ERROR ");
inline TRestLogStream RESTWarning(TRestLogManager::REST_Verbose_Level::REST_Warning,
                                  TRestLogger::COLOR_YELLOW, " WARNING ");
inline TRestLogStream RESTMetadata(TRestLogManager::REST_Verbose_Level::REST_Warning,
                                   TRestLogger::COLOR_GREEN, " METADATA ");
inline TRestLogStream RESTLog(TRestLogManager::REST_Verbose_Level::REST_Warning, TRestLogger::COLOR_BLUE,
                              " LOG ");
inline TRestLogStream RESTInfo(TRestLogManager::REST_Verbose_Level::REST_Info, TRestLogger::COLOR_BLUE,
                               " INFO ");
inline TRestLogStream RESTDebug(TRestLogManager::REST_Verbose_Level::REST_Debug, TRestLogger::COLOR_RESET,
                                " DEBUG ");
