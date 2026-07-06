#include <limits>
#include <cmath>

namespace TRestConstants {

  //MATH
  // Not a number
  inline constexpr double REST_nan = std::numeric_limits<double>::quiet_NaN();
  inline constexpr float REST_nan_f = std::numeric_limits<float>::quiet_NaN();

  //Physics
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
  /// Planck constant in J*s
  inline constexpr double hPlanck = 1.054E-34;

  /// A meter in eV
  inline constexpr double PhMeterIneV = 5067731.236453719;
  /// A second in eV (using natural units)
  inline constexpr double secondIneV = 1519225802531030.2;
  /// Electron charge in natural units
  inline constexpr double naturalElectron = 0.302822120214353;
  /// A kelvin in eV
  inline constexpr double kelvinToeV = 86.172809e-6;

  /// Average Sun-Earth distance in m
  inline constexpr double AU = 1.49597870691E11;

  // Solar radius in m
  inline constexpr double solarRadius = 6.95700E8;
}
