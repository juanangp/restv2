#include "TRestLogManager.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iterator>
#include <map>
#include <vector>

/// The map is only here: rootcling crashes on it in the header
struct TRestLogManager::RepeatedMessages {
    /// Similar messages written from one call site, with the last one suppressed
    struct Repeated {
        std::string first;  ///< First message of the group, the one the others are compared with
        std::chrono::steady_clock::time_point windowStart;  ///< Start of the current time window
        int count = 0;                                      ///< Messages in the current window
        int suppressed = 0;  ///< Messages suppressed, once maxRepeated were reached in a window (for ever)
        std::string_view color;
        std::string prefix, name, lastMessage;
    };
    /// Groups of similar messages of each call site (file, line), in order of appearance
    std::map<std::pair<std::string, int>, std::vector<Repeated>> sites;
};

namespace {
/// Maximum number of different characters (insertions, deletions or substitutions) of similar messages
constexpr size_t kMaxDifferences = 10;
/// Maximum number of groups of similar messages of a call site: beyond it the new messages count in the
/// last group, so a site writing always different messages (e.g. with long numbers) neither grows nor
/// slows down without limit
constexpr size_t kMaxGroups = 100;

/// True if the edit distance of a and b is at most kMaxDifferences, e.g. "DaqID 12 not found" and
/// "DaqID 135 not found" are similar, "Loading process: hitsAna (TRestDetectorHitsAnalysisProcess)" and
/// "Loading process: trackAna (TRestTrackAnalysisProcess)" are not
bool AreSimilar(const std::string& a, const std::string& b) {
    constexpr size_t k = kMaxDifferences, kOut = k + 1;
    const size_t n = a.size(), m = b.size();
    if ((n > m ? n - m : m - n) > k) return false;
    // Levenshtein distance by rows, only in the band |i - j| <= k (the cells out of it are > k), so the cost
    // is n (2k + 1) whatever the length; stopped when no path can stay within the limit
    std::vector<size_t> previous(m + 1, kOut), current(m + 1, kOut);
    for (size_t j = 0; j <= std::min(m, k); j++) previous[j] = j;
    for (size_t i = 1; i <= n; i++) {
        const size_t from = i > k ? i - k : 1, to = std::min(m, i + k);
        current[from - 1] = from == 1 ? i : kOut;
        size_t rowMin = current[from - 1];
        for (size_t j = from; j <= to; j++) {
            const size_t substitution = previous[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
            current[j] = std::min({previous[j] + 1, current[j - 1] + 1, substitution, kOut});
            rowMin = std::min(rowMin, current[j]);
        }
        // Out of the band for the next row
        if (to < m) current[to + 1] = kOut;
        if (rowMin > k) return false;
        std::swap(previous, current);
    }
    return previous[m] <= k;
}

/// \brief Line with the number of suppressed messages of a group (empty if none)
std::string SuppressedSummary(int suppressed, const std::string& lastMessage, const std::string& file,
                              int line) {
    if (suppressed <= 0) return "";
    const std::string fileName = file.substr(file.find_last_of('/') + 1);
    return std::to_string(suppressed) + " similar messages suppressed (" + fileName + ":" +
           std::to_string(line) + "), the last one: " + lastMessage;
}
}  // namespace

TRestLogManager::TRestLogManager() : fRepeated(new RepeatedMessages) {}

void TRestLogManager::Write(const std::string_view& color, const std::string& prefix, const std::string& name,
                            const std::string& msg, const char* file, int line) {
    std::lock_guard<std::mutex> lock(fMutex);
    const int limit = maxRepeated;
    const double interval = repeatedInterval;
    const auto now = std::chrono::steady_clock::now();

    if (!file || limit <= 0) {
        WriteText(color, prefix, name, msg);
        return;
    }

    auto& groups = fRepeated->sites[{file, line}];
    auto group = std::find_if(groups.begin(), groups.end(), [&msg](const RepeatedMessages::Repeated& g) {
        return AreSimilar(g.first, msg);
    });
    if (group == groups.end() && groups.size() >= kMaxGroups) group = std::prev(groups.end());
    if (group == groups.end()) {
        groups.emplace_back();
        group = std::prev(groups.end());
        group->first = msg;
        group->windowStart = now;
    }
    auto& repeated = *group;
    if (repeated.suppressed > 0) {
        repeated.suppressed++;
        repeated.color = color;
        repeated.prefix = prefix;
        repeated.name = name;
        repeated.lastMessage = msg;
        return;
    }
    // A new time window (interval > 0): the messages of the previous one were fewer than the limit
    if (interval > 0 && std::chrono::duration<double>(now - repeated.windowStart).count() >= interval) {
        repeated.windowStart = now;
        repeated.count = 0;
    }
    repeated.count++;
    if (repeated.count < limit) {
        WriteText(color, prefix, name, msg);
        return;
    }
    // The limit reached in a window: this is the last one written, marked before its final line breaks, and
    // the next ones are suppressed for ever (counted from 1 here, the summary subtracts it)
    repeated.suppressed = 1;
    std::string text = msg;
    text.insert(text.find_last_not_of('\n') + 1, " (further similar messages suppressed)");
    WriteText(color, prefix, name, text);
}

TRestLogManager::~TRestLogManager() {
    std::lock_guard<std::mutex> lock(fMutex);
    // The messages suppressed of each group
    for (const auto& [site, groups] : fRepeated->sites) {
        for (const auto& repeated : groups) {
            const std::string summary =
                SuppressedSummary(repeated.suppressed - 1, repeated.lastMessage, site.first, site.second);
            if (!summary.empty()) WriteText(repeated.color, repeated.prefix, repeated.name, summary);
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
    char timebuf[32];
    std::tm local{};
    localtime_r(&t, &local);  // std::localtime is not thread safe (static buffer)
    if (std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &local) == 0) timebuf[0] = '\0';
    logFile << timebuf << " [" << name << "] " << prefix << msg << std::endl;
}
