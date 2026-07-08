#pragma once

#include "TRestDetectorReadout.h"

/// \class TRestMicromegasReadout
/// \brief Micromegas-specific implementation of a detector readout geometry.
///
/// This class builds a 2D pixelated Micromegas plane from YAML geometry
/// parameters and stores each pixel as a ROOT geometry node with a unique
/// physical identifier.
class TRestMicromegasReadout : public TRestDetectorReadout {
public:
    YAML::Node fReadoutNode;
    std::vector<double> fPositionRelative = {0.0, 0.0, 0.0};
    double fGlobalRotation=0;
    int fNChannels = 0;
    double fPitch = 0;
    double fThickness = 0;

    TRestMicromegasReadout();
    TRestMicromegasReadout(const std::string& instanceName, const YAML::Node& node);
    TRestMicromegasReadout(const std::string& fileName, const std::string& sectionName);

    virtual std::string GetClassName() const override { return "TRestMicromegasReadout"; }
    virtual void LoadConfig() override;

    /// \brief Builds the Micromegas geometry from a YAML configuration node.
    /// \param readoutNode YAML node containing the `geometry_parameters`
    /// section used to define rows, columns, pitch, position and rotation.
    void BuildGeometry() override;
    TVector3 GetPositionFromChannel(int daqID) const override;
};
