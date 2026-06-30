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
    /// \brief Builds the Micromegas geometry from a YAML configuration node.
    /// \param readoutNode YAML node containing the `geometry_parameters`
    /// section used to define rows, columns, pitch, position and rotation.
    void BuildGeometry(const YAML::Node& readoutNode) override;
};
