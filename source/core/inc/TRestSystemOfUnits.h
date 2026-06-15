#pragma once

#include <TMath.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "TRestLogManager.h"

/// This namespace defines the unit conversion for different units which are understood by REST.
namespace REST_Units {

// We use more common physics units instead of SI unit
enum Physical_Unit { Energy, Time, Length, Mass, Voltage, MagneticField, Pressure, Angle, NOT_A_UNIT = -1 };

static std::map<std::string, std::pair<Physical_Unit, double> > REST_Units_Map = {
    // ================== Energy (base = keV) ==================
    {"meV", {Energy, 1e-6}},        // 1 eV = 0.000001 keV
    {"eV", {Energy, 1e-3}},         // 1 eV = 0.001 keV
    {"keV", {Energy, 1.}},          // base
    {"MeV", {Energy, 1e3}},         // 1 MeV = 1000 keV
    {"GeV", {Energy, 1e6}},         // 1 GeV = 1,000,000 keV
    {"J", {Energy, 6.241509e15}},   // 1 J = 6.24×10^15 keV
    {"kJ", {Energy, 6.241509e18}},  // 1 kJ = 6.24×10^18 keV

    // ================== Time (base = us) ==================
    {"ns", {Time, 1e-3}},        // 1 ns = 0.001 us
    {"us", {Time, 1.}},          // base
    {"ms", {Time, 1e3}},         // 1 ms = 1000 us
    {"s", {Time, 1e6}},          // 1 s = 1,000,000 us
    {"hr", {Time, 3600e6}},      // 1 h = 3600 s = 3.6e9 us
    {"day", {Time, 86400e6}},    // 1 day = 8.64e10 us
    {"yr", {Time, 31557600e6}},  // 1 yr = 3.15576e13 us

    // ================== Length (base = mm) ==================
    {"nm", {Length, 1e-6}},  // 1 nm = 1e-6 mm
    {"um", {Length, 1e-3}},  // 1 µm = 0.001 mm
    {"mm", {Length, 1.}},    // base
    {"cm", {Length, 10.}},   // 1 cm = 10 mm
    {"m", {Length, 1000.}},  // 1 m = 1000 mm
    {"km", {Length, 1e6}},   // 1 km = 1,000,000 mm

    // ================== Mass (base = kg) ==================
    {"mg", {Mass, 1e-6}},    // 1 mg = 1e-6 kg
    {"g", {Mass, 1e-3}},     // 1 g = 0.001 kg
    {"kg", {Mass, 1.}},      // base
    {"ton", {Mass, 1000.}},  // 1 ton (métrica) = 1000 kg

    // ================== Voltage (base = V) ==================
    {"mV", {Voltage, 1e-3}},   // 1 mV = 0.001 V
    {"V", {Voltage, 1.}},      // base
    {"kV", {Voltage, 1000.}},  // 1 kV = 1000 V

    // ================== Magnetic field (base = T) ==================
    {"mT", {MagneticField, 1e-3}},  // 1 mT = 0.001 T
    {"T", {MagneticField, 1.}},     // base
    {"G", {MagneticField, 1e-4}},   // 1 Gauss = 1e-4 T

    // ================== Pressure (base = bar) ==================
    {"bar", {Pressure, 1.}},           // base
    {"mbar", {Pressure, 1e-3}},        // 1 mbar = 0.001 bar
    {"atm", {Pressure, 1.01325}},      // 1 atm = 1.01325 bar
    {"Pa", {Pressure, 1e-5}},          // 1 Pa = 1e-5 bar
    {"kPa", {Pressure, 1e-2}},         // 1 kPa = 0.01 bar
    {"MPa", {Pressure, 10.}},          // 1 MPa = 10 bar
    {"torr", {Pressure, 1.33322e-3}},  // 1 Torr = 1.33322e-3 bar

    // ================== Angle (base = rad) ==================
    {"rad", {Angle, 1.}},                 // base
    {"deg", {Angle, TMath::DegToRad()}},  // 1° = p/180 rad
    {"degree", {Angle, TMath::DegToRad()}},
    {"arcmin", {Angle, TMath::DegToRad() / 60.}},    // 1' = (p/180)/60 rad
    {"arcsec", {Angle, TMath::DegToRad() / 3600.}},  // 1" = (p/180)/3600 rad
};

static double ParseUnit(const std::string& UnitsExpr) {
    std::istringstream ss(UnitsExpr);
    double factor = 1.0;
    bool divide = false;

    std::string token;
    while (std::getline(ss, token, '/')) {
        std::istringstream mult(token);
        std::string unit;
        while (std::getline(mult, unit, '*')) {
            if (unit.empty()) continue;

            // Remove spaces
            unit.erase(remove_if(unit.begin(), unit.end(), ::isspace), unit.end());

            // Separate name and exp
            std::string base = unit;
            int exp = 1;
            auto pos = unit.find('^');
            if (pos != std::string::npos) {
                base = unit.substr(0, pos);
                exp = std::stoi(unit.substr(pos + 1));
            }

            auto it = REST_Units::REST_Units_Map.find(base);
            double f = 1;
            if (it == REST_Units::REST_Units_Map.end()) {
                RESTError << "ERROR: Unit " << base << " not found in map " << RESTendl;
            } else {
                f = std::pow(it->second.second, exp);
            }
            if (divide)
                factor /= f;
            else
                factor *= f;
        }
        divide = true;
    }

    return factor;
}

static void AddUnit(std::string& name, Physical_Unit type, double scale) {
    REST_Units::REST_Units_Map[name] = std::make_pair(type, scale);
}

}  // namespace REST_Units
