#ifndef TRESTSYSTEMOFUNITS_H
#define TRESTSYSTEMOFUNITS_H

#include <TMath.h>
#include <Math/Vector3D.h>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <cmath>
#include <yaml-cpp/yaml.h>

#include "TRestLogManager.h"
#include "TRestTools.h"

namespace REST_Units {

enum Physical_Unit { Energy, Time, Length, Mass, Voltage, MagneticField, Pressure, Angle, NOT_A_UNIT = -1 };

inline std::map<std::string, std::pair<Physical_Unit, double>> REST_Units_Map = {
    // ================== Energy (base = keV) ==================
    {"meV", {Energy, 1e-6}},
    {"eV", {Energy, 1e-3}},
    {"keV", {Energy, 1.}},          // base
    {"MeV", {Energy, 1e3}},
    {"GeV", {Energy, 1e6}},
    {"TeV", {Energy, 1e9}},
    {"J", {Energy, 6.241509e15}},
    {"kJ", {Energy, 6.241509e18}},

    // ================== Time (base = us) ==================
    {"ns", {Time, 1e-3}},
    {"us", {Time, 1.}},          // base
    {"ms", {Time, 1e3}},
    {"s", {Time, 1e6}},
    {"hr", {Time, 3600e6}},
    {"day", {Time, 86400e6}},
    {"yr", {Time, 31557600e6}},

    // ================== Length (base = mm) ==================
    {"nm", {Length, 1e-6}},
    {"um", {Length, 1e-3}},
    {"mm", {Length, 1.}},        // base
    {"cm", {Length, 10.}},
    {"m", {Length, 1000.}},
    {"km", {Length, 1e6}},

    // ================== Mass (base = kg) ==================
    {"mg", {Mass, 1e-6}},
    {"g", {Mass, 1e-3}},
    {"kg", {Mass, 1.}},          // base
    {"ton", {Mass, 1000.}},

    // ================== Voltage (base = V) ==================
    {"mV", {Voltage, 1e-3}},
    {"V", {Voltage, 1.}},        // base
    {"kV", {Voltage, 1000.}},

    // ================== Magnetic field (base = T) ==================
    {"mT", {MagneticField, 1e-3}},
    {"T", {MagneticField, 1.}},  // base
    {"G", {MagneticField, 1e-4}},

    // ================== Pressure (base = bar) ==================
    {"bar", {Pressure, 1.}},     // base
    {"mbar", {Pressure, 1e-3}},
    {"atm", {Pressure, 1.01325}},
    {"Pa", {Pressure, 1e-5}},
    {"kPa", {Pressure, 1e-2}},
    {"MPa", {Pressure, 10.}},
    {"torr", {Pressure, 1.33322e-3}},

    // ================== Angle (base = rad) ==================
    {"rad", {Angle, 1.}},        // base
    {"deg", {Angle, TMath::DegToRad()}},
    {"degree", {Angle, TMath::DegToRad()}},
    {"arcmin", {Angle, TMath::DegToRad() / 60.}},
    {"arcsec", {Angle, TMath::DegToRad() / 3600.}},
};

inline double ParseUnit(const std::string& UnitsExpr) {
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
            i++; // saltar el caracter '^'
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

template <typename T>
inline std::string FormatAs(T value, Physical_Unit quantity) {
    double val = static_cast<double>(value);
    if (val == 0.0) return "0";

    std::string baseUnitSymbol; 
    double factorToBase = 1.0;  

    if (quantity == Energy) {
        baseUnitSymbol = "eV";
        factorToBase = 1.0 / REST_Units_Map["eV"].second; // keV -> eV
    } 
    else if (quantity == Time) {
        baseUnitSymbol = "s";
        factorToBase = 1.0 / REST_Units_Map["s"].second;  // us -> s
    } 
    else if (quantity == Length) {
        baseUnitSymbol = "m";
        factorToBase = 1.0 / REST_Units_Map["m"].second;  // mm -> m
    } 
    else if (quantity == Voltage) {
        baseUnitSymbol = "V";
        factorToBase = 1.0 / REST_Units_Map["V"].second;  // V -> V (1.0)
    } 
    else if (quantity == Mass) {
        baseUnitSymbol = "g"; 
        factorToBase = 1.0 / REST_Units_Map["g"].second;  // kg -> g
    } 
    else if (quantity == MagneticField) {
        baseUnitSymbol = "T";
        factorToBase = 1.0 / REST_Units_Map["T"].second;  // T -> T (1.0)
    } 
    else if (quantity == Pressure) {
        baseUnitSymbol = "bar";
        factorToBase = 1.0 / REST_Units_Map["bar"].second; // bar -> bar (1.0)
    } 
    else if (quantity == Angle) {
        baseUnitSymbol = "rad";
        factorToBase = 1.0 / REST_Units_Map["rad"].second; // rad -> rad (1.0)
    } 
    else {
        return std::to_string(val);
    }

    double valueInSI = val * factorToBase;
    const double absValue = std::abs(valueInSI);

    std::string prefix = "";
    double scaleFactor = 1.0;

    if (absValue < 1e-6)       { prefix = "n"; scaleFactor = 1e9; }
    else if (absValue < 1e-3)  { prefix = "u"; scaleFactor = 1e6; }
    else if (absValue < 1.0)   { prefix = "m"; scaleFactor = 1e3; }
    else if (absValue < 1e3)   { prefix = "";  scaleFactor = 1.0; }
    else if (absValue < 1e6)   { prefix = "k"; scaleFactor = 1e-3; }
    else if (absValue < 1e9)   { prefix = "M"; scaleFactor = 1e-6; }
    else if (absValue < 1e12)  { prefix = "G"; scaleFactor = 1e-9; }
    else                       { prefix = "T"; scaleFactor = 1e-12; }

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << (valueInSI * scaleFactor) << " " << prefix << baseUnitSymbol;
    return ss.str();
}

inline std::string FormatAs(const ROOT::Math::XYZVector& vector, Physical_Unit quantity) {
    std::ostringstream ss;
    ss << "(" 
       << FormatAs(vector.X(), quantity) << ", "
       << FormatAs(vector.Y(), quantity) << ", "
       << FormatAs(vector.Z(), quantity) << ")";
    return ss.str();
}


}  // namespace REST_Units

// ============================================================================
// TRestWithUnits Wrapper (Acts transparently as a double in C++)
// ============================================================================
struct TRestWithUnits {
    double value = 0.0;

    TRestWithUnits() = default;
    TRestWithUnits(double v) : value(v) {}

    operator double&() { return value; }
    operator const double&() const { return value; }
    TRestWithUnits& operator=(double v) { value = v; return *this; }
};

// ============================================================================
// yaml-cpp Specialization for TRestWithUnits
// ============================================================================
namespace YAML {
template <>
struct convert<TRestWithUnits> {
    static Node encode(const TRestWithUnits& rhs) {
        Node node;
        node = rhs.value; // Writes back as a clean plain double to the .root file
        return node;
    }

    static bool decode(const Node& node, TRestWithUnits& rhs) {
        if (!node || !node.IsScalar()) return false;

        std::string raw = node.as<std::string>();
        std::istringstream ss(raw);
        double val = 0.0;
        ss >> val;

        std::string units;
        ss >> units;
        if (!units.empty()) {
            val *= REST_Units::ParseUnit(units);
        }
        rhs.value = val;

        // Mutate the local node to a plain number so it exports clean later
        const_cast<Node&>(node) = val;
        return true;
    }
};
} // namespace YAML

#endif
