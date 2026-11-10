/// Unit tests of the system of units (TRestSystemOfUnits.h): REST_Units::ParseUnit,
/// REST_Units::ParseValueWithUnits, REST_Units::FormatAs, the TRestWithUnits configuration type (yaml-cpp
/// conversion) and the constants of TRestConstants.h. Everything in memory.

#include <Math/Vector3D.h>
#include <TMath.h>
#include <TRestConstants.h>
#include <TRestSystemOfUnits.h>
#include <TRestYAMLUtils.h>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <cmath>
#include <stdexcept>
#include <string>

using namespace REST_Units;

TEST(Units, ParseUnitFactors) {
    // REST units: keV, us, mm, kg, V, T, bar, rad
    EXPECT_DOUBLE_EQ(ParseUnit("keV"), 1.);
    EXPECT_DOUBLE_EQ(ParseUnit("MeV"), 1e3);
    EXPECT_DOUBLE_EQ(ParseUnit("eV"), 1e-3);
    EXPECT_DOUBLE_EQ(ParseUnit("cm"), 10.);
    EXPECT_DOUBLE_EQ(ParseUnit("m"), 1000.);
    EXPECT_DOUBLE_EQ(ParseUnit("ns"), 1e-3);
    EXPECT_DOUBLE_EQ(ParseUnit("s"), 1e6);
    EXPECT_DOUBLE_EQ(ParseUnit("mbar"), 1e-3);
    EXPECT_DOUBLE_EQ(ParseUnit("G"), 1e-4);
    EXPECT_DOUBLE_EQ(ParseUnit("deg"), TMath::DegToRad());
    // Empty: no units
    EXPECT_DOUBLE_EQ(ParseUnit(""), 1.);
}

TEST(Units, ParseUnitExpressions) {
    EXPECT_DOUBLE_EQ(ParseUnit("kV/cm"), 100.);
    EXPECT_DOUBLE_EQ(ParseUnit("V / cm"), 0.1);  // whitespace ignored
    EXPECT_DOUBLE_EQ(ParseUnit("m^2"), 1e6);
    EXPECT_DOUBLE_EQ(ParseUnit("cm^-1"), 0.1);
    EXPECT_DOUBLE_EQ(ParseUnit("keV*us"), 1.);
    EXPECT_DOUBLE_EQ(ParseUnit("mm/us"), 1.);
}

TEST(Units, ParseUnitInvalid) {
    EXPECT_THROW(ParseUnit("apples"), std::invalid_argument);
    EXPECT_THROW(ParseUnit("2cm"), std::invalid_argument);   // a number is not a unit
    EXPECT_THROW(ParseUnit("cm^x"), std::invalid_argument);  // invalid exponent
    EXPECT_THROW(ParseUnit("kev"), std::invalid_argument);   // case sensitive
}

TEST(Units, ParseValueWithUnits) {
    auto value = [](const std::string& text) {
        const auto parsed = ParseValueWithUnits(text);
        EXPECT_TRUE(parsed.has_value()) << text;
        return parsed.value_or(std::nan(""));
    };
    EXPECT_DOUBLE_EQ(value("5.3 MeV"), 5300.);
    EXPECT_DOUBLE_EQ(value("2 cm"), 20.);
    EXPECT_DOUBLE_EQ(value("30 deg"), 30. * TMath::DegToRad());
    EXPECT_DOUBLE_EQ(value("300 V"), 300.);
    EXPECT_DOUBLE_EQ(value("1 kV/cm"), 100.);
    EXPECT_DOUBLE_EQ(value("10eV"), 0.01);  // "e" of eV is not an exponent
    EXPECT_DOUBLE_EQ(value("-3.5e2 keV"), -350.);
    EXPECT_DOUBLE_EQ(value("+2 mm"), 2.);
    EXPECT_DOUBLE_EQ(value("  7  "), 7.);  // no units: the number as it is
    EXPECT_DOUBLE_EQ(value("1.5 bar"), 1.5);
    EXPECT_DOUBLE_EQ(value("5 um"), 0.005);

    EXPECT_FALSE(ParseValueWithUnits("5 apples").has_value());
    EXPECT_FALSE(ParseValueWithUnits("abc").has_value());
    EXPECT_FALSE(ParseValueWithUnits("").has_value());
    EXPECT_FALSE(ParseValueWithUnits("MeV 5").has_value());
}

TEST(Units, FormatAs) {
    EXPECT_EQ(FormatAs(5.9, Energy), "5.90 keV");
    EXPECT_EQ(FormatAs(5900., Energy), "5.90 MeV");
    EXPECT_EQ(FormatAs(0., Length), "0");
    EXPECT_EQ(FormatAs(1500., Length), "1.50 m");
    EXPECT_EQ(FormatAs(0.5, Time), "500.00 ns");
    EXPECT_EQ(FormatAs(2., Voltage), "2.00 V");
    EXPECT_EQ(FormatAs(ROOT::Math::XYZVector(1, 2, 3), Length), "(1.00 mm, 2.00 mm, 3.00 mm)");
    // Not a unit: the plain number
    EXPECT_EQ(FormatAs(2.5, NOT_A_UNIT), std::to_string(2.5));
}

TEST(Units, WithUnitsYAML) {
    const YAML::Node node = YAML::Load(
        "energy: 5.9 keV\n"
        "voltage: 1 kV\n"
        "plain: 42\n"
        "wrong: 5 apples\n");
    EXPECT_DOUBLE_EQ(node["energy"].as<TRestWithUnits>(), 5.9);
    EXPECT_DOUBLE_EQ(TRestYAMLUtils::GetParam<TRestWithUnits>(node, "voltage"), 1000.);
    EXPECT_DOUBLE_EQ(node["plain"].as<TRestWithUnits>(), 42.);
    EXPECT_DOUBLE_EQ(TRestYAMLUtils::GetParam<TRestWithUnits>(node, "missing", TRestWithUnits(3.)), 3.);
    EXPECT_THROW(node["wrong"].as<TRestWithUnits>(), YAML::BadConversion);

    // Written back as the plain number in REST units
    YAML::Node out;
    out["value"] = TRestWithUnits(2.5);
    EXPECT_DOUBLE_EQ(out["value"].as<double>(), 2.5);

    // Usable as a double
    TRestWithUnits withUnits;
    withUnits = 4.;
    const double doubled = withUnits * 2;
    EXPECT_DOUBLE_EQ(doubled, 8.);
}

TEST(Units, Constants) {
    EXPECT_TRUE(std::isnan(TRestConstants::REST_nan));
    EXPECT_TRUE(std::isnan(TRestConstants::REST_nan_f));
    EXPECT_DOUBLE_EQ(TRestConstants::lightSpeed, 2.99792458E8);
    EXPECT_GT(TRestConstants::hBar, 0.);
}
