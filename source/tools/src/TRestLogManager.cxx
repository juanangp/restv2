#include "TRestLogManager.h"

#include <algorithm>
#include <ctime>
#include <iterator>
#include <map>
#include <vector>

/// The map is only here: rootcling crashes on it in the header
struct TRestLogManager::RepeatedMessages {
    /// Similar messages written from one call site, with the last one suppressed
    struct Repeated {
        std::string first;  ///< First message of the group, the one the others are compared with
        int count = 0;
        std::string_view color;
        std::string prefix, name, lastMessage;
    };
    /// Groups of similar messages of each call site (file, line), in order of appearance
    std::map<std::pair<std::string, int>, std::vector<Repeated>> sites;
};

namespace {
/// Maximum number of different characters (insertions, deletions or substitutions) of similar messages
constexpr size_t kMaxDifferences = 10;

/// True if the edit distance of a and b is at most kMaxDifferences, e.g. "DaqID 12 not found" and
/// "DaqID 135 not found" are similar, "Loading process: hitsAna (TRestDetectorHitsAnalysisProcess)" and
/// "Loading process: trackAna (TRestTrackAnalysisProcess)" are not
bool AreSimilar(const std::string& a, const std::string& b) {
    const size_t n = a.size(), m = b.size();
    if ((n > m ? n - m : m - n) > kMaxDifferences) return false;
    // Levenshtein distance by rows, stopped when no path can stay within the limit
    std::vector<size_t> previous(m + 1), current(m + 1);
    for (size_t j = 0; j <= m; j++) previous[j] = j;
    for (size_t i = 1; i <= n; i++) {
        current[0] = i;
        size_t rowMin = current[0];
        for (size_t j = 1; j <= m; j++) {
            const size_t substitution = previous[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
            current[j] = std::min({previous[j] + 1, current[j - 1] + 1, substitution});
            rowMin = std::min(rowMin, current[j]);
        }
        if (rowMin > kMaxDifferences) return false;
        std::swap(previous, current);
    }
    return previous[m] <= kMaxDifferences;
}
}  // namespace

TRestLogManager::TRestLogManager() : fRepeated(new RepeatedMessages) {}

void TRestLogManager::Write(const std::string_view& color, const std::string& prefix, const std::string& name,
                            const std::string& msg, const char* file, int line) {
    std::lock_guard<std::mutex> lock(fMutex);

    if (!file || maxRepeated <= 0) {
        WriteText(color, prefix, name, msg);
        return;
    }

    auto& groups = fRepeated->sites[{file, line}];
    auto group = std::find_if(groups.begin(), groups.end(), [&msg](const RepeatedMessages::Repeated& g) {
        return AreSimilar(g.first, msg);
    });
    if (group == groups.end()) {
        groups.emplace_back();
        group = std::prev(groups.end());
        group->first = msg;
    }
    auto& repeated = *group;
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
    for (const auto& [site, groups] : fRepeated->sites) {
        const std::string fileName = site.first.substr(site.first.find_last_of('/') + 1);
        for (const auto& repeated : groups) {
            if (repeated.count <= maxRepeated) continue;
            WriteText(repeated.color, repeated.prefix, repeated.name,
                      std::to_string(repeated.count - maxRepeated) + " similar messages suppressed (" +
                          fileName + ":" + std::to_string(site.second) +
                          "), the last one: " + repeated.lastMessage);
        }
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
