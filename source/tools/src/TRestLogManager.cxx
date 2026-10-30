#include "TRestLogManager.h"

#include <ctime>

void TRestLogManager::Write(const std::string_view& color, const std::string& prefix, const std::string& name,
                            const std::string& msg) {
    std::lock_guard<std::mutex> lock(fMutex);

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
