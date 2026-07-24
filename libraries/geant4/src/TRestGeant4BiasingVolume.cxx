#include "TRestGeant4BiasingVolume.h"

#include "TRestMetadata.h"

// Modern REST v3 field registry reflection hook
static const bool TRestGeant4BiasingVolume_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestGeant4BiasingVolume>("position", &TRestGeant4BiasingVolume::fVolumePosition);
    reg.RegisterField<TRestGeant4BiasingVolume>("size", &TRestGeant4BiasingVolume::fVolumeSize);
    reg.RegisterField<TRestGeant4BiasingVolume>("biasingFactor", &TRestGeant4BiasingVolume::fBiasingFactor);
    reg.RegisterField<TRestGeant4BiasingVolume>("energyRange", &TRestGeant4BiasingVolume::fEnergyRange);
    reg.RegisterField<TRestGeant4BiasingVolume>("type", &TRestGeant4BiasingVolume::fVolumeType);
    return true;
}();

/// \brief Default constructor.
TRestGeant4BiasingVolume::TRestGeant4BiasingVolume() {
    fVolumeType = "virtualBox";
    fBiasingVolumeType = "virtualBox";
}

/// \brief Destructor.
TRestGeant4BiasingVolume::~TRestGeant4BiasingVolume() {}

/// \brief Outputs the biasing fields configuration data summary.
void TRestGeant4BiasingVolume::PrintBiasingVolume() const {
    std::cout << "-----------------------------" << std::endl;
    std::cout << "Biasing volume" << std::endl;
    std::cout << "-----------------------------" << std::endl;
    std::cout << "volume size : " << GetBiasingVolumeSize() << " mm" << std::endl;
    std::cout << "volume type : " << GetBiasingVolumeType() << std::endl;
    std::cout << "volume factor : " << GetBiasingFactor() << std::endl;
    std::cout << "volume position : ( " << fVolumePosition[0] << " , " << fVolumePosition[1] << " , "
              << fVolumePosition[2] << " ) mm" << std::endl;
    std::cout << "Energy range : ( " << GetMinEnergy() << " , " << GetMaxEnergy() << " ) keV" << std::endl;
    std::cout << "-----------------------------" << std::endl;
}
