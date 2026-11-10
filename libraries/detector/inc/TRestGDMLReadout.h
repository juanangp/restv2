#ifndef TRESTGDMLREADOUT_H
#define TRESTGDMLREADOUT_H

#include "TRestDetectorReadout.h"

/// \class TRestGDMLReadoutInfo
/// \brief A group of channels of a TRestGDMLReadout: the sensitive volumes (the medium where the energy is
/// deposited, e.g. the scintillators) and, optionally, where their signal is collected.
class TRestGDMLReadoutInfo : public TRestMetadata {
   public:
    /// Regular expression of the paths of the sensitive volumes, one channel each; its captured numbers,
    /// multiplier and offset give the physical ID
    std::string fVolume = "";
    /// Regular expression of the volume where the signal of each channel is collected (e.g. the light guide
    /// or the PMT of a scintillator): exactly one in the same parent volume (assembly copy) as the
    /// sensitive volume. Empty: no readout position (GetReadoutPosition and GetDistanceToReadout NaN)
    std::string fReadoutVolume = "";
    /// With two captured numbers n1 and n2 in volume: physical ID = offset + n1 * multiplier + n2
    int fMultiplier = 10;
    /// Physical ID of the first channel: offset + the captured number (one captured number), or offset
    /// incremented for each channel found (none)
    int fOffset = 0;

    /// \brief Builds an empty entry.
    TRestGDMLReadoutInfo();
    /// \brief Builds the entry from its YAML node (LoadConfig).
    TRestGDMLReadoutInfo(const std::string& name, const YAML::Node& node);
    /// \brief Reads the entry keys (volume, readoutVolume, offset, multiplier).
    void LoadConfig() override;
    /// \brief Nothing to initialise.
    void Initialize() override {}
    /// \brief Returns the class name, "TRestGDMLReadoutInfo".
    std::string GetClassName() const override { return "TRestGDMLReadoutInfo"; }
};

/// \class TRestGDMLReadoutParameters
/// \brief The `readoutParameters` of a TRestGDMLReadout: the GDML file (`gdmlFileName`) and the groups of
/// channels (`readoutInfo`, a list of TRestGDMLReadoutInfo).
class TRestGDMLReadoutParameters : public TRestMetadata {
   public:
    std::string fGDMLFileName = "";                  ///< Path to the input external GDML file
    std::vector<TRestGDMLReadoutInfo> fReadoutInfo;  ///< Groups of channels (`readoutInfo`)

    /// \brief Builds empty parameters.
    TRestGDMLReadoutParameters();
    /// \brief Builds the parameters from the `readoutParameters` node (LoadConfig).
    TRestGDMLReadoutParameters(const std::string& name, const YAML::Node& node);
    /// \brief Reads gdmlFileName and readoutInfo.
    void LoadConfig() override;
    /// \brief Nothing to initialise.
    void Initialize() override {}
    /// \brief Returns the class name, "TRestGDMLReadoutParameters".
    std::string GetClassName() const override { return "TRestGDMLReadoutParameters"; }
};

/// \class TRestGDMLReadout
/// \brief Detector readout implementation that imports geometry and channels from GDML naming rules.
///
/// The geometry is the GDML file of the simulation (e.g. the vetoes of restG4, read with
/// TRestGDMLParser, so its entities and parameters are resolved as restG4 does). Each entry of
/// `readoutInfo` selects the sensitive volumes whose geometry path matches the regular expression
/// `volume` (one channel each, the first entry matching a path wins); their physical ID comes from the
/// numbers captured by the expression:
/// - two captured numbers n1, n2: offset + n1 * multiplier + n2 (multiplier 10 by default);
/// - one captured number n: offset + n;
/// - none: offset, incremented for each channel found (in the order of the geometry).
///
/// `readoutVolume` (optional) is the regular expression of the volume where the signal of each channel is
/// collected (e.g. the light guide of a scintillator): exactly one in the same parent volume as the
/// channel, otherwise BuildGeometry throws. It gives GetReadoutPosition and GetDistanceToReadout (used by
/// the attenuation model of TRestDetectorLightCollectionProcess). The hits are not drifted:
/// GetDistanceToReadout without channel is NaN.
///
/// Parameters: decodingFile and, in `readoutParameters`, gdmlFileName and readoutInfo (volume,
/// readoutVolume, offset, multiplier). volumeToPhysicalIDMap and physicalIDToReadoutPath are written by
/// BuildGeometry (not inputs).
///
///    readout:
///      class: "TRestGDMLReadout"
///      name: "Veto_Readout"
///      decodingFile: "veto.dec"
///      readoutParameters:
///        gdmlFileName: "setup.gdml"
///        readoutInfo:
///          - volume: .*scintillatorVolume-*
///            readoutVolume: scintillatorLightGuideVolume
///            offset: 288
///
/// \author JuanAn García <juanangp@unizar.es>
class TRestGDMLReadout : public TRestDetectorReadout {
   private:
    /// Global position of the readout volume of each physical ID, computed at the first GetReadoutPosition
    /// (navigating the geometry once, not for each hit); each thread has its own readouts
    mutable std::map<int, ROOT::Math::XYZVector> fReadoutPositions;  //!

   public:
    TRestGDMLReadoutParameters fReadoutParams;  ///< `readoutParameters`
    /// Path of the readout volume of each physical ID (from readoutVolume), stored with the readout
    std::map<int, std::string> fPhysicalIDToReadoutPath;

    /// \brief Constructor.
    TRestGDMLReadout();
    /// \brief Builds a GDML readout from an in-memory YAML node section.
    TRestGDMLReadout(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds a GDML readout from file+section metadata references.
    TRestGDMLReadout(const std::string& fileName, const std::string& sectionName);

    /// \brief Returns the class name, "TRestGDMLReadout".
    virtual std::string GetClassName() const override { return "TRestGDMLReadout"; }
    /// \brief Reads GDML file name and channel parsing options from configuration.
    virtual void LoadConfig() override;
    /// \brief Overrides the abstract geometry builder to import and parse a GDML file: loads the decoding
    /// file, imports gdmlFileName and finds the channels and their readout volumes (see the class
    /// description); the volumes that are not channels nor readout volumes are hidden. Throws if the
    /// decoding or the GDML cannot be loaded, a regular expression is invalid or a channel has not exactly
    /// one readout volume.
    virtual void BuildGeometry() override;

    /// \brief Origin of the readout volume of the channel (its readoutVolume) in the global frame, NaN
    /// without one.
    ROOT::Math::XYZVector GetReadoutPosition(int daqID) const override;

    using TRestDetectorReadout::GetDistanceToReadout;
    /// \brief Distance from a position to the readout position of the channel (e.g. from an energy deposit
    /// in a scintillator to its PMT, for the light attenuation), NaN without readout volume. Without a
    /// channel it stays NaN: the hits are not drifted nor projected.
    double GetDistanceToReadout(int daqID, const ROOT::Math::XYZVector& position) const override;

    // inline void SetGDMLFile(const std::string& gdlmFile){fGDMLFilePath = gdmlFile;}
};

#endif
