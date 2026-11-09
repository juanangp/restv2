#include "TRestSystemOfUnits.h"

#include <TError.h>
#include <TGeoManager.h>

#include <algorithm>
#include <charconv>
#include <stdexcept>

#include "TRestTools.h"

namespace REST_Units {

double ParseUnit(const std::string& UnitsExpr) {
    if (UnitsExpr.empty()) return 1.0;

    std::string cleanExpr = TRestTools::CleanExpression(UnitsExpr);

    double totalFactor = 1.0;
    bool currentMultiply = true;

    size_t i = 0;
    while (i < cleanExpr.length()) {
        if (cleanExpr[i] == '*') {
            currentMultiply = true;
            i++;
            continue;
        } else if (cleanExpr[i] == '/') {
            currentMultiply = false;
            i++;
            continue;
        }

        size_t startBase = i;
        while (i < cleanExpr.length() && std::isalpha(static_cast<unsigned char>(cleanExpr[i]))) {
            i++;
        }
        std::string base = cleanExpr.substr(startBase, i - startBase);

        if (base.empty()) {
            throw std::invalid_argument("invalid units '" + UnitsExpr +
                                        "': unexpected character at position " + std::to_string(i) + " of '" +
                                        cleanExpr + "'");
        }

        int exp = 1;
        if (i < cleanExpr.length() && cleanExpr[i] == '^') {
            i++;  // saltar el caracter '^'
            size_t startExp = i;
            if (i < cleanExpr.length() && (cleanExpr[i] == '-' || cleanExpr[i] == '+')) {
                i++;
            }
            while (i < cleanExpr.length() && std::isdigit(static_cast<unsigned char>(cleanExpr[i]))) {
                i++;
            }
            try {
                exp = std::stoi(cleanExpr.substr(startExp, i - startExp));
            } catch (...) {
                throw std::invalid_argument("invalid units '" + UnitsExpr + "': invalid exponent");
            }
        }

        auto it = REST_Units_Map.find(base);
        if (it == REST_Units_Map.end()) {
            throw std::invalid_argument("invalid units '" + UnitsExpr + "': unknown unit '" + base + "'");
        }
        const double unitFactor = std::pow(it->second.second, exp);

        if (currentMultiply) {
            totalFactor *= unitFactor;
        } else {
            totalFactor /= unitFactor;
        }
    }

    return totalFactor;
}

std::optional<double> ParseValueWithUnits(const std::string& text) {
    // std::from_chars (locale independent) takes the longest number: "10eV" is 10 and "eV" (an incomplete
    // exponent is not part of the number), which a stream (>> double) fails to read
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) begin++;
    if (begin != end && *begin == '+') begin++;  // from_chars does not accept '+'
    double value = 0;
    const auto [rest, error] = std::from_chars(begin, end, value);
    if (error != std::errc() || rest == begin) return std::nullopt;
    const std::string units = TRestTools::Trim(std::string(rest, end));
    if (!units.empty()) {
        try {
            value *= ParseUnit(units);
        } catch (const std::invalid_argument&) {
            return std::nullopt;
        }
    }
    return value;
}

bool SetGeoManagerDefaultUnits() {
    static const bool done = [] {
        // ROOT locks the default units once geometry elements exist. Its message "Changing system of units
        // to Geant4 units" (Info) is not printed: it is expected, at the start of every REST program
        const Int_t errorLevel = gErrorIgnoreLevel;
        gErrorIgnoreLevel = std::max<Int_t>(errorLevel, kWarning);
        TGeoManager::LockDefaultUnits(kFALSE);
        TGeoManager::SetDefaultUnits(TGeoManager::kG4Units);
        TGeoManager::LockDefaultUnits(kTRUE);
        gErrorIgnoreLevel = errorLevel;
        return true;
    }();
    return done;
}

// Also applied when the library is loaded (e.g. from plain ROOT sessions).
static const bool kGeoManagerDefaultUnitsSet = SetGeoManagerDefaultUnits();

}  // namespace REST_Units
