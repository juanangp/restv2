#ifndef TRESTGDMLREADOUT_H
#define TRESTGDMLREADOUT_H

#include "TRestDetectorReadout.h"

class TRestGDMLReadoutInfo : public TRestMetadata {
   public:
    std::string fChannelPrefix = "";
    int fMultiplier = 10;
    int fOffset = 0;

    TRestGDMLReadoutInfo();
    TRestGDMLReadoutInfo(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestGDMLReadoutInfo"; }
};

class TRestGDMLReadoutParameters : public TRestMetadata {
   public:
    std::string fGDMLFileName = "";   ///< Path to the input external GDML file
    std::vector<TRestGDMLReadoutInfo> fReadoutInfo;

    TRestGDMLReadoutParameters();
    TRestGDMLReadoutParameters(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestGDMLReadoutParameters"; }
};

/// \class TRestGDMLReadout
/// \brief Detector readout implementation that imports geometry and channels from GDML naming rules.
class TRestGDMLReadout : public TRestDetectorReadout {
   public:

    TRestGDMLReadoutParameters fReadoutParams;

    /// \brief Constructor.
    TRestGDMLReadout();
    /// \brief Builds a GDML readout from an in-memory YAML node section.
    TRestGDMLReadout(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds a GDML readout from file+section metadata references.
    TRestGDMLReadout(const std::string& fileName, const std::string& sectionName);

    virtual std::string GetClassName() const override { return "TRestGDMLReadout"; }
    /// \brief Reads GDML file name and channel parsing options from configuration.
    virtual void LoadConfig() override;
    /// \brief Overrides the abstract geometry builder to import and parse a GDML file.
    /// \param readoutNode YAML node containing `gdml_parameters`.
    virtual void BuildGeometry() override;

    // inline void SetGDMLFile(const std::string& gdlmFile){fGDMLFilePath = gdmlFile;}
};

#endif
