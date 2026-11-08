#ifndef TRESTLOGMANAGER_H
#define TRESTLOGMANAGER_H

#include <atomic>
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
/// \class TRestProgressBar
/// \brief Console progress bar with the count, the estimated time left (ETA, minutes) and the percentage,
/// redrawn in place on std::cout, e.g. `120 Events, 1.5 min ETA, 12.0%[===>      ]`. Not thread safe: it
/// is updated by one thread (the global instance RESTProgress is used by the executables).
class TRestProgressBar {
   public:
    /// \brief Bar not started: Update and Finish do nothing until Reset
    TRestProgressBar() : total(0), current(0), width(60), started(false) {}

    /// \brief Starts the bar (count 0, ETA clock started) and draws it
    /// \param totalEvents Count at 100%; with 0 or less the bar is not drawn
    /// \param barWidth Number of characters of the bar
    /// \param countUnit What is counted, written before the ETA (e.g. "Events", "Files")
    void Reset(int totalEvents, int barWidth = 30, const std::string& countUnit = "Events") {
        total = totalEvents;
        width = barWidth;
        unit = countUnit;
        current = 0;
        started = true;
        startTime = std::chrono::steady_clock::now();
        Update(0);
    }

    /// \brief Redraws the bar with the current count (clamped to the total); the ETA assumes a constant
    /// rate since Reset
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

        std::cout << "\r\033[K" << current << " " << unit << ", " << etaStr << " min ETA, "
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

    /// \brief Draws the bar at 100%, ends the line and stops the bar
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
    std::string unit = "Events";
    std::chrono::steady_clock::time_point startTime;
};

/// Global progress bar of the executables (restManager, restG4...)
inline TRestProgressBar RESTProgress;

/// \class TRestLogManager
/// \brief Singleton writing the messages of REST (RESTError, RESTWarning, RESTInfo... macros) to the console,
/// with colors, and to an optional log file, from any thread. It holds the verbose levels and the global
/// settings of the suppression of repeated messages.
class TRestLogManager {
   public:
    /// Verbose levels: a message is written if its level is not above the verbose of its context
    enum class REST_Verbose_Level {
        REST_Error = 0,    //!< show minimum information of the software, as well as error messages
        REST_Silent = 1,   //!< no message has this level: in practice the same output as REST_Error
        REST_Warning = 2,  //!< +show some essential information, as well as warnings
        REST_Info = 3,     //!< +show most of the information for each steps
        REST_Debug = 4     //!< +show the defined debug messages
    };

    /// Verbose level of each name of the configuration (`verbose: info`)
    static inline std::unordered_map<std::string, REST_Verbose_Level> verboseMap{
        {"silent", REST_Verbose_Level::REST_Silent},
        {"error", REST_Verbose_Level::REST_Error},
        {"warning", REST_Verbose_Level::REST_Warning},
        {"info", REST_Verbose_Level::REST_Info},
        {"debug", REST_Verbose_Level::REST_Debug}};

    /// Name of each verbose level (inverse of verboseMap)
    static inline std::map<REST_Verbose_Level, std::string> inverseVerboseMap = [] {
        std::map<REST_Verbose_Level, std::string> inv;
        for (const auto& [str, lvl] : verboseMap) {
            inv[lvl] = str;
        }
        return inv;
    }();

    /// \brief Name of a verbose level (silent, error, warning, info or debug), throws if unknown
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

    /// \brief The single instance (created on first use)
    static TRestLogManager& instance() {
        static TRestLogManager mgr;
        return mgr;
    }

    /// \brief Also writes the messages to a file (appended), closing the previous one if any
    void SetLogFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(fMutex);
        if (logFile.is_open()) logFile.close();
        logFile.open(filename, std::ios::out | std::ios::app);
    }

    /// \brief Writes a message to the console (with color) and to the log file if any, from any thread.
    /// With a call site (file not nullptr), the similar messages written from it (at most 10 different
    /// characters) are counted in time windows of repeatedInterval seconds: if maxRepeated of them arrive in
    /// a window, the next ones are suppressed for ever (the number of the suppressed ones is written at the
    /// end of the program); slower repetitions are always written.
    void Write(const std::string_view& color, const std::string& prefix, const std::string& name,
               const std::string& msg, const char* file = nullptr, int line = 0);

    /// \brief Similar messages (at most 10 different characters) written from one call site (warning
    /// and info) before the next ones are suppressed,
    /// 0: no suppression. The messages of a metadata (or context) with verbose debug are never suppressed.
    /// Atomic: set by the main thread, read by any.
    static inline std::atomic<int> maxRepeated{10};
    /// \brief Sets maxRepeated
    static inline void setMaxRepeated(int n) { maxRepeated = n; }

    /// \brief Length in seconds of the time windows of the similar messages of a call site (Write): they are
    /// suppressed (for ever) only if maxRepeated of them arrive within one window. 0: a single window, from
    /// the start of the program (the first maxRepeated written).
    /// Atomic: set by the main thread, read by any.
    static inline std::atomic<double> repeatedInterval{10};
    /// \brief Sets repeatedInterval
    static inline void setRepeatedInterval(double seconds) { repeatedInterval = seconds; }

    /// \brief Verbose level of the messages written outside a metadata, and of the metadata without
    /// `verbose`. Atomic: set by the main thread, read by any.
    static inline std::atomic<REST_Verbose_Level> globalVerboseLevel{REST_Verbose_Level::REST_Info};
    /// \brief Sets globalVerboseLevel
    static inline void setGlobalVerboseLevel(REST_Verbose_Level lvl) { globalVerboseLevel = lvl; }

   private:
    std::ofstream logFile;
    std::mutex fMutex;
    /// Messages written from each call site (defined in the source)
    struct RepeatedMessages;
    RepeatedMessages* fRepeated;

    TRestLogManager();
    /// \brief Writes the number of messages suppressed from each call site
    ~TRestLogManager();

    /// \brief Writes a message, with fMutex locked
    void WriteText(const std::string_view& color, const std::string& prefix, const std::string& name,
                   const std::string& msg);
};

// ---------------- Logger ----------------
/// \class TRestLogger
/// \brief ANSI escape codes of the console colors of the messages
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
    /// Verbose level: the messages above it are not written
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
    /// \brief Makes a copy of `context` the current one of the thread (the previous one restored on
    /// destruction)
    explicit TRestLogScope(const TRestLogContext& context) : fContext(context), fPrevious(currentLogContext) {
        currentLogContext = &fContext;
    }
    /// \brief Restores the previous context of the thread
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
/// threads are not mixed. The similar messages (at most 10 different characters) of RESTWarning and
/// RESTInfo (file not nullptr) repeated from the same call site (file and line) are suppressed after
/// TRestLogManager::maxRepeated, unless the verbose of the context is debug.
class TRestLogLine {
   public:
    using Level = TRestLogManager::REST_Verbose_Level;

    /// \brief Message of `level` in `context`; `file` and `line` are the call site for the suppression of
    /// repeated messages (file nullptr: never suppressed, also when the context verbose is debug)
    TRestLogLine(Level level, const std::string_view& color, const char* prefix,
                 const TRestLogContext& context, const char* file, int line)
        : fColor(color),
          fPrefix(prefix),
          fName(context.name),
          fEnabled(level <= context.level),
          fFile(context.level < Level::REST_Debug ? file : nullptr),
          fLine(line) {}

    TRestLogLine(const TRestLogLine&) = delete;
    TRestLogLine& operator=(const TRestLogLine&) = delete;

    /// \brief Writes the message (TRestLogManager::Write) if its level is enabled and it is not empty
    ~TRestLogLine() {
        if (!fEnabled) return;
        const std::string message = fStream.str();
        if (!message.empty()) {
            TRestLogManager::instance().Write(fColor, fPrefix, fName ? *fName : "Global", message, fFile,
                                              fLine);
        }
    }

    /// \brief Appends a value to the message (nothing if its level is not enabled)
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
    /// Call site, nullptr if its messages are not suppressed
    const char* fFile;
    int fLine;
    std::ostringstream fStream;
};

/// \brief End of line of a message (a new line; the message is written when the statement ends)
inline std::ostream& RESTendl(std::ostream& os) {
    os.put('\n');
    return os;
}

// The verbose level of a message is that of the metadata writing it (its `verbose`, otherwise the global
// one), or the global one outside a metadata. FILE: the call site for the suppression of repeated messages
// (only warnings and info), nullptr for none
#define REST_LOG_LINE(LEVEL, COLOR, PREFIX, FILE)                                                         \
    TRestLogLine(TRestLogManager::REST_Verbose_Level::LEVEL, TRestLogger::COLOR, PREFIX, GetLogContext(), \
                 FILE, __LINE__)

/// Error message (red), written at any verbose level; never suppressed
#define RESTError REST_LOG_LINE(REST_Error, COLOR_RED, " ERROR ", nullptr)
/// Warning message (yellow), verbose warning or above; repeated ones are suppressed
#define RESTWarning REST_LOG_LINE(REST_Warning, COLOR_YELLOW, " WARNING ", __FILE__)
/// Metadata description (green), verbose warning or above
#define RESTMetadata REST_LOG_LINE(REST_Warning, COLOR_GREEN, " METADATA ", nullptr)
/// Log message (blue), verbose warning or above
#define RESTLog REST_LOG_LINE(REST_Warning, COLOR_BLUE, " LOG ", nullptr)
/// Information message (blue), verbose info or above; repeated ones are suppressed
#define RESTInfo REST_LOG_LINE(REST_Info, COLOR_BLUE, " INFO ", __FILE__)
/// Debug message, verbose debug only
#define RESTDebug REST_LOG_LINE(REST_Debug, COLOR_RESET, " DEBUG ", nullptr)

#endif
