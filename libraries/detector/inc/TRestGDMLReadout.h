#pragma once

#include "TRestDetectorReadout.h"

class TRestGDMLReadout : public TRestDetectorReadout {
  public:

    std::string fGDMLFilePath;    ///< Path to the input external GDML file
    std::string fChannelPrefix;   ///< Prefix token to parse inside volume names (e.g., "strip_")

    using TRestDetectorReadout::TRestDetectorReadout;

    virtual std::string GetClassName() const override { return "TRestGDMLReadout"; }
    /// \brief Overrides the abstract geometry builder to import and parse a GDML file.
    /// \param readoutNode YAML node containing `gdml_parameters`.
    virtual void BuildGeometry() override;
    
    //inline void SetGDMLFile(const std::string& gdlmFile){fGDMLFilePath = gdmlFile;}
};
