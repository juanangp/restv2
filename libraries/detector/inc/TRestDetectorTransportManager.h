#ifndef TRESTDETECTORTRANSPORTMANAGER_H
#define TRESTDETECTORTRANSPORTMANAGER_H

#include <memory>
#include <vector>

#include "TRestDetectorMedium.h"
#include "TRestMetadata.h"

/// \class TRestDetectorTransportManager
/// \brief Top section of a restGarfield configuration (as TRestGeant4Manager for restG4): a medium
/// (TRestDetectorMedium) and the simulations to run for it, in declaration order (e.g.
/// TRestDetectorTransport). The metadata do not depend on Garfield++; restGarfield runs them.
///
///    transportManager:
///      class: "TRestDetectorTransportManager"
///      medium:
///        class: "TRestDetectorMedium"
///        name: "ArIso"
///        components: {Ar: 99, iC4H10: 1}
///      gasTable:
///        class: "TRestDetectorTransport"
///        pressures: [1 bar]
class TRestDetectorTransportManager : public TRestMetadata {
   public:
    std::unique_ptr<TRestDetectorMedium> fMedium;              //!
    std::vector<std::unique_ptr<TRestMetadata>> fSimulations;  //!

    TRestDetectorTransportManager(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorTransportManager(const std::string& fileName, const std::string& sectionName);
    ~TRestDetectorTransportManager() override;

    std::string GetClassName() const override { return "TRestDetectorTransportManager"; }
    /// \brief Creates the medium and the simulations declared in the section. Throws if there is not
    /// exactly one medium
    void LoadConfig() override;
    void Initialize() override {}
    void PrintMetadata() const override;
};

#endif
