#ifndef TRESTSYSTEMOFUNITS_H
#define TRESTSYSTEMOFUNITS_H

#include <Math/Vector3D.h>
#include <TMath.h>
#include <yaml-cpp/yaml.h>

#include <cctype>
#include <cmath>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <string>

#include "TRestLogManager.h"

/// \brief System of units of REST: the values are stored in the REST units keV, us, mm, kg, V, T, bar and
/// rad, and the configuration gives them with units ("300 V", "1 kV/cm", "5.9 keV"), converted with
/// ParseUnit and ParseValueWithUnits (TRestWithUnits for the configuration fields).
///
/// \author Javier Galán <javier.galan@unizar.es>
/// \author Kaixiang Ni
/// \author JuanAn García <juanangp@unizar.es>
namespace REST_Units {

/// Physical quantity of a unit
enum Physical_Unit { Energy, Time, Length, Mass, Voltage, MagneticField, Pressure, Angle, NOT_A_UNIT = -1 };

/// Units known: symbol -> (quantity, factor to the REST unit of the quantity), e.g. "cm" -> (Length, 10)
inline std::map<std::string, std::pair<Physical_Unit, double>> REST_Units_Map = {
    // ================== Energy (base = keV) ==================
    {"meV", {Energy, 1e-6}},
    {"eV", {Energy, 1e-3}},
    {"keV", {Energy, 1.}},  // base
    {"MeV", {Energy, 1e3}},
    {"GeV", {Energy, 1e6}},
    {"TeV", {Energy, 1e9}},
    {"J", {Energy, 6.241509e15}},
    {"kJ", {Energy, 6.241509e18}},

    // ================== Time (base = us) ==================
    {"ns", {Time, 1e-3}},
    {"us", {Time, 1.}},  // base
    {"ms", {Time, 1e3}},
    {"s", {Time, 1e6}},
    {"hr", {Time, 3600e6}},
    {"day", {Time, 86400e6}},
    {"yr", {Time, 31557600e6}},

    // ================== Length (base = mm) ==================
    {"nm", {Length, 1e-6}},
    {"um", {Length, 1e-3}},
    {"mm", {Length, 1.}},  // base
    {"cm", {Length, 10.}},
    {"m", {Length, 1000.}},
    {"km", {Length, 1e6}},

    // ================== Mass (base = kg) ==================
    {"mg", {Mass, 1e-6}},
    {"g", {Mass, 1e-3}},
    {"kg", {Mass, 1.}},  // base
    {"ton", {Mass, 1000.}},

    // ================== Voltage (base = V) ==================
    {"mV", {Voltage, 1e-3}},
    {"V", {Voltage, 1.}},  // base
    {"kV", {Voltage, 1000.}},

    // ================== Magnetic field (base = T) ==================
    {"mT", {MagneticField, 1e-3}},
    {"T", {MagneticField, 1.}},  // base
    {"G", {MagneticField, 1e-4}},

    // ================== Pressure (base = bar) ==================
    {"bar", {Pressure, 1.}},  // base
    {"mbar", {Pressure, 1e-3}},
    {"atm", {Pressure, 1.01325}},
    {"Pa", {Pressure, 1e-5}},
    {"kPa", {Pressure, 1e-2}},
    {"MPa", {Pressure, 10.}},
    {"torr", {Pressure, 1.33322e-3}},

    // ================== Angle (base = rad) ==================
    {"rad", {Angle, 1.}},  // base
    {"deg", {Angle, TMath::DegToRad()}},
    {"degree", {Angle, TMath::DegToRad()}},
    {"arcmin", {Angle, TMath::DegToRad() / 60.}},
    {"arcsec", {Angle, TMath::DegToRad() / 3600.}},
};

/// \brief Factor to REST units of a units expression (e.g. "kV/cm", "keV^2"); 1 if empty. Throws
/// std::invalid_argument if a unit is unknown or the expression is not valid.
double ParseUnit(const std::string& UnitsExpr);

/// \brief A value in REST units written in the SI unit of the quantity (eV, s, m, g, V, T, bar, rad) with
/// an SI prefix (n, u, m, k, M, G, T) and 2 decimals, e.g. FormatAs(5.9, Energy) = "5.90 keV"; "0" for 0
/// and the plain number for NOT_A_UNIT.
template <typename T>
inline std::string FormatAs(T value, Physical_Unit quantity) {
    double val = static_cast<double>(value);
    if (val == 0.0) return "0";

    std::string baseUnitSymbol;
    double factorToBase = 1.0;

    if (quantity == Energy) {
        baseUnitSymbol = "eV";
        factorToBase = 1.0 / REST_Units_Map["eV"].second;  // keV -> eV
    } else if (quantity == Time) {
        baseUnitSymbol = "s";
        factorToBase = 1.0 / REST_Units_Map["s"].second;  // us -> s
    } else if (quantity == Length) {
        baseUnitSymbol = "m";
        factorToBase = 1.0 / REST_Units_Map["m"].second;  // mm -> m
    } else if (quantity == Voltage) {
        baseUnitSymbol = "V";
        factorToBase = 1.0 / REST_Units_Map["V"].second;  // V -> V (1.0)
    } else if (quantity == Mass) {
        baseUnitSymbol = "g";
        factorToBase = 1.0 / REST_Units_Map["g"].second;  // kg -> g
    } else if (quantity == MagneticField) {
        baseUnitSymbol = "T";
        factorToBase = 1.0 / REST_Units_Map["T"].second;  // T -> T (1.0)
    } else if (quantity == Pressure) {
        baseUnitSymbol = "bar";
        factorToBase = 1.0 / REST_Units_Map["bar"].second;  // bar -> bar (1.0)
    } else if (quantity == Angle) {
        baseUnitSymbol = "rad";
        factorToBase = 1.0 / REST_Units_Map["rad"].second;  // rad -> rad (1.0)
    } else {
        return std::to_string(val);
    }

    double valueInSI = val * factorToBase;
    const double absValue = std::abs(valueInSI);

    std::string prefix = "";
    double scaleFactor = 1.0;

    if (absValue < 1e-6) {
        prefix = "n";
        scaleFactor = 1e9;
    } else if (absValue < 1e-3) {
        prefix = "u";
        scaleFactor = 1e6;
    } else if (absValue < 1.0) {
        prefix = "m";
        scaleFactor = 1e3;
    } else if (absValue < 1e3) {
        prefix = "";
        scaleFactor = 1.0;
    } else if (absValue < 1e6) {
        prefix = "k";
        scaleFactor = 1e-3;
    } else if (absValue < 1e9) {
        prefix = "M";
        scaleFactor = 1e-6;
    } else if (absValue < 1e12) {
        prefix = "G";
        scaleFactor = 1e-9;
    } else {
        prefix = "T";
        scaleFactor = 1e-12;
    }

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << (valueInSI * scaleFactor) << " " << prefix << baseUnitSymbol;
    return ss.str();
}

/// \brief A vector in REST units as "(x, y, z)", each component with FormatAs
inline std::string FormatAs(const ROOT::Math::XYZVector& vector, Physical_Unit quantity) {
    std::ostringstream ss;
    ss << "(" << FormatAs(vector.X(), quantity) << ", " << FormatAs(vector.Y(), quantity) << ", "
       << FormatAs(vector.Z(), quantity) << ")";
    return ss.str();
}

/// \brief Sets the TGeoManager default units to the Geant4 ones (TGeoManager::kG4Units: mm, MeV, ns) and
/// locks them; also done when the library is loaded. Safe to call multiple times (done once).
/// \return true
bool SetGeoManagerDefaultUnits();

/// \brief Value of a number with optional units (e.g. "300 V", "1 kV/cm", "10eV", "-3.5e2 keV") in REST
/// units.
/// \return nullopt if the text does not start with a number or the text after it is not valid units
/// (ParseUnit), e.g. "5 apples".
std::optional<double> ParseValueWithUnits(const std::string& text);

}  // namespace REST_Units

// ============================================================================
// TRestWithUnits Wrapper (Acts transparently as a double in C++)
// ============================================================================
/// \brief A number read from the configuration with optional units, e.g. `energy: "5.9 keV"` or
/// `voltage: 300`, stored in REST units (REST_Units::ParseValueWithUnits); it converts to and from double.
/// The YAML written back (UpdateYAMLFromParams) is the plain number in REST units. Reading a text that is
/// not a number with valid units throws (YAML::TypedBadConversion).
struct TRestWithUnits {
    double value = 0.0;  ///< Value in REST units

    TRestWithUnits() = default;
    /// \brief A value already in REST units
    TRestWithUnits(double v) : value(v) {}

    /// \brief The value, usable as a double
    operator double&() { return value; }
    /// \brief The value, usable as a double
    operator const double&() const { return value; }
    /// \brief Sets the value (in REST units)
    TRestWithUnits& operator=(double v) {
        value = v;
        return *this;
    }
};

// ============================================================================
// yaml-cpp Specialization for TRestWithUnits
// ============================================================================
namespace YAML {
/// \brief yaml-cpp conversion of TRestWithUnits: decode parses the units and replaces the node by the plain
/// number in REST units; encode writes the plain number
template <>
struct convert<TRestWithUnits> {
    /// \brief Node with the value in REST units
    static Node encode(const TRestWithUnits& rhs) {
        Node node;
        node = rhs.value;  // Writes back as a clean plain double to the .root file
        return node;
    }

    /// \brief Value of a scalar with optional units; false (yaml-cpp throws) if it is not a number with
    /// valid units
    static bool decode(const Node& node, TRestWithUnits& rhs) {
        if (!node || !node.IsScalar()) return false;

        // Not a number with valid units: false, yaml-cpp throws (TypedBadConversion)
        const auto parsed = REST_Units::ParseValueWithUnits(node.as<std::string>());
        if (!parsed) return false;
        const double val = *parsed;
        rhs.value = val;

        // Mutate the local node to a plain number so it exports clean later
        const_cast<Node&>(node) = val;
        return true;
    }
};
}  // namespace YAML

#endif
