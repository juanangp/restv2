#pragma once

#include "TRestDetectorReadout.h"

class TRestGDMLReadout : public TRestDetectorReadout {
  public:

    YAML::Node fReadoutNode;
    std::string fGDMLFileName="";    ///< Path to the input external GDML file
    std::string fChannelPrefix="";   ///< Prefix token to parse inside volume names (e.g., "strip_")

    TRestGDMLReadout();
    TRestGDMLReadout(const std::string& instanceName, const YAML::Node& node);
    TRestGDMLReadout(const std::string& fileName, const std::string& sectionName);

    virtual std::string GetClassName() const override { return "TRestGDMLReadout"; }
    virtual void LoadConfig() override;
    /// \brief Overrides the abstract geometry builder to import and parse a GDML file.
    /// \param readoutNode YAML node containing `gdml_parameters`.
    virtual void BuildGeometry() override;
    
    //inline void SetGDMLFile(const std::string& gdlmFile){fGDMLFilePath = gdmlFile;}
};
