#include "TRestSystemOfUnits.h"

#include <TGeoManager.h>

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
            RESTError << "ERROR: Invalid format in position " << i << " in: " << UnitsExpr << RESTendl;
            break;
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
                RESTError << "ERROR: invalid exponent: " << UnitsExpr << RESTendl;
            }
        }

        auto it = REST_Units_Map.find(base);
        double unitFactor = 1.0;
        if (it == REST_Units_Map.end()) {
            RESTError << "ERROR: Unit '" << base << "' not found" << RESTendl;
        } else {
            unitFactor = std::pow(it->second.second, exp);
        }

        if (currentMultiply) {
            totalFactor *= unitFactor;
        } else {
            totalFactor /= unitFactor;
        }
    }

    return totalFactor;
}

std::optional<double> ParseValueWithUnits(const std::string& text) {
    std::istringstream stream(text);
    double value = 0;
    if (!(stream >> value)) return std::nullopt;
    std::string units;
    std::getline(stream, units);
    units = TRestTools::Trim(units);
    if (!units.empty()) value *= ParseUnit(units);
    return value;
}

bool SetGeoManagerDefaultUnits() {
    static const bool done = [] {
        // ROOT locks the default units once geometry elements exist
        TGeoManager::LockDefaultUnits(kFALSE);
        TGeoManager::SetDefaultUnits(TGeoManager::kG4Units);
        TGeoManager::LockDefaultUnits(kTRUE);
        return true;
    }();
    return done;
}

// Also applied when the library is loaded (e.g. from plain ROOT sessions).
static const bool kGeoManagerDefaultUnitsSet = SetGeoManagerDefaultUnits();

}  // namespace REST_Units
