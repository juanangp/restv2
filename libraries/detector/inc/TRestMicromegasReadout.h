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
    std::array<TRestWithUnits, 3> fPositionRelative = {0.0, 0.0, 0.0};
    TRestWithUnits fGlobalRotation = 0;
    int fNChannels = 0;
    TRestWithUnits fPitch = 0;
    TRestWithUnits fThickness = 0;

    /// \brief Constructor.
    TRestMicromegasReadout();
    /// \brief Builds a Micromegas readout from an in-memory YAML node section.
    TRestMicromegasReadout(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds a Micromegas readout from file+section metadata references.
    TRestMicromegasReadout(const std::string& fileName, const std::string& sectionName);

    virtual std::string GetClassName() const override { return "TRestMicromegasReadout"; }
    /// \brief Loads Micromegas geometry parameters from configuration.
    virtual void LoadConfig() override;

    /// \brief Builds the Micromegas geometry from a YAML configuration node.
    /// \param readoutNode YAML node containing the `geometry_parameters`
    /// section used to define rows, columns, pitch, position and rotation.
    void BuildGeometry() override;
    /// \brief Returns the geometric centroid of a Micromegas DAQ channel.
    ROOT::Math::XYZVector GetPositionFromChannel(int daqID) const override;
};
