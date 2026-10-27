#ifndef TRESTDETECTORMEDIUM_H
#define TRESTDETECTORMEDIUM_H

#include <map>
#include <set>
#include <string>

#include "TRestMetadata.h"

/// \class TRestDetectorMedium
/// \brief Definition of a medium (section of a TRestDetectorTransportManager): its name (the medium file
/// `<name>.root`, see TRestDetectorTransport), state and components. It describes the material, not the
/// detector technology (TPC, scintillator, bolometer...): what is measured in it is given by the readout
/// and the response models.
///
/// - state: gas, liquid or solid. Only gas tables scale with the pressure.
/// - components: names and molar fractions in percent (for gases, volume fractions or partial
///   pressures), e.g. {Ar: 99, iC4H10: 1} or {Si: 100}.
///
/// restGarfield computes gases (Magboltz, Garfield++ gas names, up to 6 components) and solids of a single
/// component known by Garfield++ (Si, GaAs, GaN, CdTe, diamond); other media need their tables from other
/// sources.
///
///    medium:
///      class: "TRestDetectorMedium"
///      name: "ArIso"
///      state: "gas"
///      components: {Ar: 99, iC4H10: 1}
class TRestDetectorMedium : public TRestMetadata {
   public:
    inline static const std::set<std::string> kStates = {"gas", "liquid", "solid"};

    std::string fState = "gas";
    std::map<std::string, double> fComponents;

    TRestDetectorMedium();
    TRestDetectorMedium(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorMedium(const std::string& fileName, const std::string& sectionName);

    std::string GetClassName() const override { return "TRestDetectorMedium"; }
    void LoadConfig() override;
    void Initialize() override {}

    inline bool IsGas() const { return fState == "gas"; }

    /// \brief YAML of the medium stored in its file (name, state, components)
    YAML::Node GetDefinition() const;
};

#endif
