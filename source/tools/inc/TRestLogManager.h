#ifndef TRESTLOGMANAGER_H
#define TRESTLOGMANAGER_H

#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

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

        std::cout << "\r\033[K" << current << " Events, " << etaStr << " min ETA, " << (int)(percentage * 100)
                  << ".0%[";

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
        if (it != inverseVerboseMap.end()) return it->second;
        throw std::invalid_argument("unknown verbose level " + std::to_string(static_cast<int>(vLevel)));
    }

    /// \brief Level of a verbose string (silent, error, warning, info or debug), throws if unknown
    static inline REST_Verbose_Level GetVerboseLevelFromString(const std::string& vStr) {
        auto it = verboseMap.find(vStr);
        if (it != verboseMap.end()) return it->second;
        throw std::invalid_argument("unknown verbose '" + vStr + "' (silent, error, warning, info or debug)");
    }

    static TRestLogManager& instance() {
        static TRestLogManager mgr;
        return mgr;
    }

    void SetLogFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(fMutex);
        if (logFile.is_open()) logFile.close();
        logFile.open(filename, std::ios::out | std::ios::app);
    }

    /// \brief Writes a message to the console (with color) and to the log file if any, from any thread
    void Write(const std::string_view& color, const std::string& prefix, const std::string& name,
               const std::string& msg);

    /// \brief Verbose level of the messages written outside a metadata, and of the metadata without
    /// `verbose`
    static inline REST_Verbose_Level globalVerboseLevel = REST_Verbose_Level::REST_Info;
    static inline void setGlobalVerboseLevel(REST_Verbose_Level lvl) { globalVerboseLevel = lvl; }

   private:
    std::ofstream logFile;
    std::mutex fMutex;
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
};

// ---------------- Log context ----------------
/// \brief Who writes a message: its verbose level and name (in the log file)
struct TRestLogContext {
    TRestLogManager::REST_Verbose_Level level;
    /// e.g. the name of the metadata instance, nullptr outside a metadata
    const std::string* name = nullptr;
};

/// \brief Context set by TRestLogScope in this thread, nullptr if none
inline thread_local const TRestLogContext* currentLogContext = nullptr;

/// \brief Context of the messages written outside a metadata (the metadata have their own
/// TRestMetadata::GetLogContext, found first inside their methods): the one of the TRestLogScope of the
/// thread (e.g. the process running, for the events), otherwise the global verbose level
inline TRestLogContext GetLogContext() {
    if (currentLogContext) return *currentLogContext;
    return {TRestLogManager::globalVerboseLevel, nullptr};
}

/// \brief Sets the context of the messages written outside a metadata in this thread while it lives
class TRestLogScope {
   public:
    explicit TRestLogScope(const TRestLogContext& context) : fContext(context), fPrevious(currentLogContext) {
        currentLogContext = &fContext;
    }
    ~TRestLogScope() { currentLogContext = fPrevious; }

    TRestLogScope(const TRestLogScope&) = delete;
    TRestLogScope& operator=(const TRestLogScope&) = delete;

   private:
    TRestLogContext fContext;
    const TRestLogContext* fPrevious;
};

// ---------------- Log line ----------------
/// \brief One message, written when the statement ends (RESTInfo << ... << RESTendl;) if its level is
/// enabled by the verbose of the context. Each statement has its own buffer, so messages of different
/// threads are not mixed.
class TRestLogLine {
   public:
    TRestLogLine(TRestLogManager::REST_Verbose_Level level, const std::string_view& color,
                 const char* prefix, const TRestLogContext& context)
        : fColor(color), fPrefix(prefix), fName(context.name), fEnabled(level <= context.level) {}

    TRestLogLine(const TRestLogLine&) = delete;
    TRestLogLine& operator=(const TRestLogLine&) = delete;

    ~TRestLogLine() {
        if (!fEnabled) return;
        const std::string message = fStream.str();
        if (!message.empty()) {
            TRestLogManager::instance().Write(fColor, fPrefix, fName ? *fName : "Global", message);
        }
    }

    template <typename T>
    TRestLogLine& operator<<(const T& value) {
        if (fEnabled) fStream << value;
        return *this;
    }

    /// \brief Manipulators as RESTendl or std::endl
    TRestLogLine& operator<<(std::ostream& (*manipulator)(std::ostream&)) {
        if (fEnabled) manipulator(fStream);
        return *this;
    }

   private:
    std::string_view fColor;
    const char* fPrefix;
    const std::string* fName;
    bool fEnabled;
    std::ostringstream fStream;
};

inline std::ostream& RESTendl(std::ostream& os) {
    os.put('\n');
    return os;
}

// The verbose level of a message is that of the metadata writing it (its `verbose`, otherwise the global
// one), or the global one outside a metadata
#define REST_LOG_LINE(LEVEL, COLOR, PREFIX) \
    TRestLogLine(TRestLogManager::REST_Verbose_Level::LEVEL, TRestLogger::COLOR, PREFIX, GetLogContext())

#define RESTError REST_LOG_LINE(REST_Error, COLOR_RED, " ERROR ")
#define RESTWarning REST_LOG_LINE(REST_Warning, COLOR_YELLOW, " WARNING ")
#define RESTMetadata REST_LOG_LINE(REST_Warning, COLOR_GREEN, " METADATA ")
#define RESTLog REST_LOG_LINE(REST_Warning, COLOR_BLUE, " LOG ")
#define RESTInfo REST_LOG_LINE(REST_Info, COLOR_BLUE, " INFO ")
#define RESTDebug REST_LOG_LINE(REST_Debug, COLOR_RESET, " DEBUG ")

#endif
