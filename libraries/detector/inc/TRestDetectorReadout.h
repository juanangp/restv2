#pragma once

#include "TRestMetadata.h"
#include "TRestTools.h"
#include "TGeoManager.h"
#include "TGeoVolume.h"
#include "TVector3.h"
#include <map>
#include <string>

class TFile;

/// \class TRestDetectorReadout
/// \brief Abstract base class to describe detector readout geometry and decoding.
///
/// The class stores a geometry representation based on ROOT `TGeoManager` and a
/// mapping between geometry physical identifiers and DAQ channel identifiers.
/// Derived classes must implement `BuildGeometry` to define a concrete readout
/// technology.
class TRestDetectorReadout : public TRestMetadata {
DECLARE_LOG_CLASS(TRestDetectorReadout)
protected:
    /// ROOT geometry manager used for geometry navigation and position lookup.
    TGeoManager* fGeoManager = nullptr;

    /// Top-level logical assembly that owns all readout nodes.
    TGeoVolumeAssembly* fTopAssembly = nullptr;

    /// Decoding map from geometry physical ID to DAQ channel ID.
    std::map<int, int> fPhysicalToDAQMap;

public:

    /// Decoding fileName
    std::string fDecodingFile = "default_decoding.dec";

    /// \brief Constructs a detector readout metadata object.
    TRestDetectorReadout();
    TRestDetectorReadout(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorReadout(const std::string& fileName, const std::string& sectionName);

    /// \brief Destructor.
    virtual ~TRestDetectorReadout();

    /// \brief Returns the class name used by REST metadata introspection.
    /// \return The literal class name.
    std::string GetClassName() const override { return "TRestDetectorReadout"; }

    /// \brief Loads the current YAML node into the internal geometry model.
    void LoadConfig() override;

    /// \brief Metadata initialization hook.
    void Initialize() override {}

    /// \brief Prints basic metadata information.
    void PrintMetadata() override;

    /// \brief Builds the technology-specific readout geometry.
    /// \param readoutNode YAML node with geometry parameters.
    virtual void BuildGeometry() = 0;

    void InitializeReadout();

    /// \brief Opens a graphical window to visualize the readout geometry.
    /// \param option Drawing option passed to ROOT (e.g., "ogl" for OpenGL, "" for default X11/Web).
    void ViewReadoutGeometry(const std::string& option = "ogl") const;
    void ViewActiveEvent(const std::vector<int>& activeChannels) const;

    /// \brief Loads a decoding file mapping physical IDs to DAQ channels.
    /// \param decFilename Path to decoding file.
    /// \return `true` if the decoding is loaded successfully.
    bool LoadDecoding(const std::string& decFilename);

    /// \brief Serializes the current decoding table into text format.
    /// \return Decoding table as a multiline string.
    std::string GetDecodingAsString() const;

    /// \brief Imports only geometry information from a ROOT file.
    /// \param fIn Input ROOT file.
    /// \param geometryName Name of geometry object to import.
    /// \return `true` if geometry was imported.
    bool ImportGeometry(TFile* fIn, const std::string& geometryName);

    /// \brief Imports geometry and decoding from a ROOT file.
    /// \param fIn Input ROOT file.
    /// \param geometryName Geometry name to import.
    /// \param decodingName Decoding object name inside the geometry decoding folder.
    /// \return `true` if import completed successfully.
    bool Import(TFile* fIn, const std::string& geometryName, const std::string& decodingName = "default");

    /// \brief Exports geometry and decoding into a ROOT file.
    /// \param fOut Output ROOT file.
    /// \param geometryName Geometry object name.
    /// \param decodingName Decoding object name.
    /// \return `true` if export completed successfully.
    bool Export(TFile* fOut, const std::string& geometryName, const std::string& decodingName = "default") const;

    /// \brief Returns DAQ channel corresponding to a spatial position.
    /// \param x X coordinate in detector reference frame.
    /// \param y Y coordinate in detector reference frame.
    /// \param z Z coordinate in detector reference frame.
    /// \return DAQ channel, or `-1` when no channel is found.
    int GetChannelFromPosition(double x, double y, double z) const;

    /// \brief Returns the centroid position of a given DAQ channel.
    /// \param daqID DAQ channel identifier.
    /// \return Channel centroid in detector coordinates.
    virtual TVector3 GetPositionFromChannel(int daqID) const;

    /// \brief Checks whether a DAQ channel is present in the decoding table.
    /// \param daqID DAQ channel identifier.
    /// \return `true` if the channel is connected.
    bool IsChannelConnected(int daqID) const {
        for (const auto& [physicalID, channelID] : fPhysicalToDAQMap) {
            if (channelID == daqID) return true;
        }
        return false;
    }

    /// \brief Sets the geometry manager used by this readout.
    /// \param geo Geometry manager pointer.
    void SetGeoManager(TGeoManager* geo);

    /// \brief Returns the current geometry manager.
    /// \return Pointer to the geometry manager.
    TGeoManager* GetGeoManager() const { return fGeoManager; }
};
