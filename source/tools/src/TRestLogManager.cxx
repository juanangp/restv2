#include "TRestLogManager.h"

#include <ctime>

/// The map is only here: rootcling crashes on it in the header
struct TRestLogManager::RepeatedMessages {
    /// Messages written from one call site, with the last one suppressed
    struct Repeated {
        int count = 0;
        std::string_view color;
        std::string prefix, name, lastMessage;
    };
    std::map<std::pair<std::string, int>, Repeated> sites;
};

TRestLogManager::TRestLogManager() : fRepeated(new RepeatedMessages) {}

void TRestLogManager::Write(const std::string_view& color, const std::string& prefix, const std::string& name,
                            const std::string& msg, const char* file, int line) {
    std::lock_guard<std::mutex> lock(fMutex);

    if (!file || maxRepeated <= 0) {
        WriteText(color, prefix, name, msg);
        return;
    }

    auto& repeated = fRepeated->sites[{file, line}];
    repeated.count++;
    if (repeated.count > maxRepeated) {
        repeated.color = color;
        repeated.prefix = prefix;
        repeated.name = name;
        repeated.lastMessage = msg;
        return;
    }
    if (repeated.count < maxRepeated) {
        WriteText(color, prefix, name, msg);
        return;
    }
    // The last one written, marked before its final line breaks
    std::string text = msg;
    text.insert(text.find_last_not_of('\n') + 1, " (further similar messages suppressed)");
    WriteText(color, prefix, name, text);
}

TRestLogManager::~TRestLogManager() {
    std::lock_guard<std::mutex> lock(fMutex);
    for (const auto& [site, repeated] : fRepeated->sites) {
        if (repeated.count <= maxRepeated) continue;
        const std::string& path = site.first;
        const std::string fileName = path.substr(path.find_last_of('/') + 1);
        WriteText(repeated.color, repeated.prefix, repeated.name,
                  std::to_string(repeated.count - maxRepeated) + " similar messages suppressed (" + fileName +
                      ":" + std::to_string(site.second) + "), the last one: " + repeated.lastMessage);
    }
    delete fRepeated;
}

void TRestLogManager::WriteText(const std::string_view& color, const std::string& prefix,
                                const std::string& name, const std::string& msg) {
    // -------- Console output (with colors) --------
    std::cout << color << msg << TRestLogger::COLOR_RESET << std::flush;

    // -------- File output (without colors) --------
    if (!logFile.is_open()) return;

    // Timestamp
    std::time_t t = std::time(nullptr);
    char timebuf[20];
    std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    logFile << timebuf << " [" << name << "] " << prefix << msg << std::endl;
}
