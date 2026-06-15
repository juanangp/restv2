#include "TRestLogManager.h"

void TRestLogger::log(TRestLogManager::REST_Verbose_Level level, const std::string_view& color,
                      const std::string& prefix, const std::string& className, const std::string& msg) {
    TRestLogManager::REST_Verbose_Level minLevel = TRestLogManager::instance().GetVerboseLevel(className);
    if (level > minLevel) return;

    // -------- Consola (con colores) --------
    std::cout << color << msg << COLOR_RESET;

    // -------- Archivo (sin colores) --------
    if (!TRestLogManager::instance().hasFile()) return;

    // Timestamp
    std::time_t t = std::time(nullptr);
    char timebuf[20];
    std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    auto& file = TRestLogManager::instance().GetLogFile();
    file << timebuf << " [" << className << "] " << prefix << msg << std::endl;
}
