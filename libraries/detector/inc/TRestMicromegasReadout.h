#ifndef TRESTMICROMEGASREADOUT_H
#define TRESTMICROMEGASREADOUT_H

#include <array>

#include "TRestDetectorReadout.h"

/// \class TRestMicromegasReadoutParameters
/// \brief The `readoutParameters` of a TRestMicromegasReadout: positionRelative, globalRotation,
/// nChannels, pitch, thickness, gasThickness, normal and checkOverlaps.
class TRestMicromegasReadoutParameters : public TRestMetadata {
   public:
    /// Translation of the readout plane (its centre); positionRelative[2] is the Z of the plane
    std::array<TRestWithUnits, 3> fPositionRelative = {0.0, 0.0, 0.0};
    /// Rotation of the plane about Z, applied to the geometry (GetChannelFromPosition works with global
    /// positions). The positions of the channels (GetPositionFromChannel, strips: only X or only Y) are in
    /// the frame of the readout plane (unrotated), so the hits reconstructed from the signals are in that
    /// frame
    TRestWithUnits fGlobalRotation = 0;
    /// Number of strips per axis (nChannels X strips and nChannels Y strips)
    int fNChannels = 0;
    /// Strip pitch
    TRestWithUnits fPitch = 0;
    /// Thickness of the pixels of the strips (only for the geometry)
    TRestWithUnits fThickness = 0;
    /// Thickness of the drift volume (the cathode at that distance from the plane); 0: no drift volume nor
    /// cathode
    TRestWithUnits fGasThickness = 0;
    /// Normal of the readout plane pointing to the cathode (drift volume side). Only +Z or -Z, since
    /// globalRotation is around Z
    std::array<double, 3> fNormal = {0.0, 0.0, 1.0};
    /// Run a sampling overlap check on the strips after building (slow for large nChannels)
    bool fCheckOverlaps = false;

    /// \brief Builds empty parameters.
    TRestMicromegasReadoutParameters();
    /// \brief Builds the parameters from the `readoutParameters` node (LoadConfig).
    TRestMicromegasReadoutParameters(const std::string& name, const YAML::Node& node);
    /// \brief Reads the parameters; throws if normal is not [0, 0, 1] or [0, 0, -1].
    void LoadConfig() override;
    /// \brief Nothing to initialise.
    void Initialize() override {}
    /// \brief Returns the class name, "TRestMicromegasReadoutParameters".
    std::string GetClassName() const override { return "TRestMicromegasReadoutParameters"; }

    /// \brief Side of the readout plane where the cathode is: +1 (+Z) or -1 (-Z).
    int GetNormalSign() const { return fNormal[2] < 0 ? -1 : 1; }
};

/// \class TRestMicromegasReadout
/// \brief Micromegas-specific implementation of a detector readout geometry.
///
/// This class builds a 2D pixelated Micromegas plane from YAML geometry
/// parameters and stores each pixel as a ROOT geometry node with a unique
/// physical identifier.
///
/// The plane has nChannels X strips and nChannels Y strips of interleaved pixels (pitch / sqrt(2), rotated
/// 45 degrees): the Y strips have the physical IDs 0 to nChannels - 1 and the X strips nChannels to
/// 2 nChannels - 1 (the UniqueID of the pixel nodes). The plane is centred at positionRelative and rotated
/// by globalRotation about Z; with gasThickness > 0 a gas volume (the drift volume) is added on the side of
/// the normal, which gives the cathode (GetMaxDistanceToReadout). The positions of the channels are those
/// of the strips (only X or only Y), in the frame of the plane.
///
/// Parameters: decodingFile and, in `readoutParameters`, nChannels, pitch, thickness, gasThickness,
/// positionRelative, globalRotation, normal ([0, 0, 1] or [0, 0, -1]) and checkOverlaps.
///
///    readout:
///      class: "TRestMicromegasReadout"
///      name: "AlphaCAMM_Readout"
///      decodingFile: "AlphaCAMM.dec"
///      readoutParameters:
///        nChannels: 256
///        pitch: 0.96
///        thickness: 1 um
///        gasThickness: 90 mm
///        normal: [0, 0, 1]
///        positionRelative: [0.0, 0.0, 0.0]
///        globalRotation: 0 deg
class TRestMicromegasReadout : public TRestDetectorReadout {
   public:
    TRestMicromegasReadoutParameters fReadoutParams;  ///< `readoutParameters`

    /// \brief Constructor.
    TRestMicromegasReadout();
    /// \brief Builds a Micromegas readout from an in-memory YAML node section.
    TRestMicromegasReadout(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds a Micromegas readout from file+section metadata references.
    TRestMicromegasReadout(const std::string& fileName, const std::string& sectionName);

    /// \brief Returns the class name, "TRestMicromegasReadout".
    virtual std::string GetClassName() const override { return "TRestMicromegasReadout"; }
    /// \brief Loads Micromegas geometry parameters from configuration.
    virtual void LoadConfig() override;

    /// \brief Builds the Micromegas geometry from the `readoutParameters` (strips, gas volume, position and
    /// rotation, see the class description) and loads the decoding file. Throws if the decoding cannot be
    /// loaded.
    void BuildGeometry() override;
    /// \brief Returns the nominal position of a Micromegas DAQ channel: X for the X strips and Y for the Y
    /// strips (the other coordinates NaN), in the frame of the readout plane (without globalRotation).
    ROOT::Math::XYZVector GetPositionFromChannel(int daqID) const override;
    /// \brief With globalRotation, the bounding box in the frame of the readout plane (that of
    /// GetPositionFromChannel and of the reconstructed hits, e.g. for the gain map): the strip positions
    /// extended by half a pitch, Z the plane. Without rotation, that of the geometry (TRestDetectorReadout).
    std::pair<ROOT::Math::XYZVector, ROOT::Math::XYZVector> GetChannelsBoundingBox() const override;
    /// \brief Adjacent strips of a DAQ channel in its plane (Y strips: physical IDs 0 to nChannels - 1, X
    /// strips: nChannels to 2 nChannels - 1): the last Y strip and the first X strip are not neighbours.
    std::pair<int, int> GetAdjacentChannels(int daqID) const override;
    using TRestDetectorReadout::GetDistanceToReadout;
    /// \brief Returns the distance to the readout plane z = positionRelative[2] along its normal (positive
    /// towards the cathode).
    double GetDistanceToReadout(const ROOT::Math::XYZVector& position) const override;
    /// \brief Returns the distance from the readout plane to the cathode (end of the gasThickness volume),
    /// or NaN if gasThickness is not set (no cathode: the drift processes apply no upper cut).
    double GetMaxDistanceToReadout(const ROOT::Math::XYZVector& position) const override;
};

#endif
