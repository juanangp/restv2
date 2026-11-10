#ifndef TRESTCONSTANTS_H
#define TRESTCONSTANTS_H

#include <cmath>
#include <limits>

/// \brief Mathematical and physical constants used across REST (SI units unless stated otherwise).
///
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
namespace TRestConstants {

// MATH
//  Not a number
/// Quiet NaN (double), e.g. the value of an undefined coordinate or observable
inline constexpr double REST_nan = std::numeric_limits<double>::quiet_NaN();
/// Quiet NaN (float)
inline constexpr float REST_nan_f = std::numeric_limits<float>::quiet_NaN();

// Physics
/// Vacuum permitivity in F/m
inline constexpr double vacuumPermitivity = 8.854E-12;
/// Vacuum permeability in H/m
inline constexpr double vacuumPermeability = 4E-7 * 3.141592653589793;
/// Speed of light in m/s
inline constexpr double lightSpeed = 2.99792458E8;
/// Electron charge in C
inline constexpr double qElectron = 1.602E-19;
/// Electron mass in Kg
inline constexpr double mElectron = 9.107E-31;
/// Boltzman  constant in J/K
inline constexpr double kBoltzman = 1.380E-23;
/// Reduced Planck constant (h-bar, h / 2pi) in J*s
inline constexpr double hBar = 1.054E-34;

/// A meter in natural units (1/eV, hbar = c = 1)
inline constexpr double PhMeterIneV = 5067731.236453719;
/// A second in natural units (1/eV, hbar = 1)
inline constexpr double secondIneV = 1519225802531030.2;
/// Electron charge in natural units
inline constexpr double naturalElectron = 0.302822120214353;
/// A kelvin in eV
inline constexpr double kelvinToeV = 86.172809e-6;

/// Average Sun-Earth distance in m
inline constexpr double AU = 1.49597870691E11;

/// Solar radius in m
inline constexpr double solarRadius = 6.95700E8;
}  // namespace TRestConstants

#endif
