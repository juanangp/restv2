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
    std::string fGDMLFileName = "";  ///< Path to the input external GDML file
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
    /// Path of the readout volume of each physical ID (from readoutVolume), stored with the readout
    std::map<int, std::string> fPhysicalIDToReadoutPath;

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
