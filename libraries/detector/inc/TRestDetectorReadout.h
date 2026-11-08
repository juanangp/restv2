#ifndef TRESTDETECTORREADOUT_H
#define TRESTDETECTORREADOUT_H

#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Math/Vector3D.h"
#include "TGeoMatrix.h"
#include "TRestMetadata.h"

class TFile;
class TGeoManager;
class TGeoMedium;
class TGeoShape;

/// \class TRestDetectorReadout
/// \brief Abstract base class to describe detector readout geometry and decoding.
///
/// The class stores a geometry representation based on ROOT `TGeoManager` and a
/// mapping between geometry physical identifiers and DAQ channel identifiers.
/// Derived classes must implement `BuildGeometry` to define a concrete readout
/// technology (TRestMicromegasReadout, TRestGDMLReadout).
///
/// Life cycle: a readout is built once from its YAML (BuildGeometry, which reads the decoding file) and
/// exported to a ROOT file (Export, e.g. with the macros REST_Detector_GenerateMicromegasReadout and
/// REST_Detector_GenerateGDMLReadout); the pipelines load it from that file with a
/// TRestDetectorReadoutManager (Import, with the decoding stored under a name).
///
/// Common parameters (those of the technology are in its `readoutParameters`):
/// - decodingFile: text file with one `daqChannel physicalID` row per channel (as legacy; `#` starts a
///   comment, a negative physical ID marks an unconnected DAQ channel).
/// - volumeToPhysicalIDMap: geometry path of each channel volume and its physical ID; written by the
///   readouts that identify the channels by path (TRestGDMLReadout), not an input.
///
///    readout:
///      class: "TRestMicromegasReadout"
///      name: "IAXO_D1_Readout"
///      decodingFile: "iaxo_d1_iaxolab.dec"
///      readoutParameters:
///        nChannels: 120
///        pitch: 0.5
///
/// The channels are identified by their physical ID (the geometry) and by their DAQ ID (the signals); the
/// public methods take DAQ IDs. Positions are in mm in the global frame of the readout geometry.
class TRestDetectorReadout : public TRestMetadata {
   protected:
    /// ROOT geometry manager used for geometry navigation and position lookup.
    TGeoManager* fGeoManager = nullptr;

    /// Geometry of the last view drawn (ViewReadoutGeometry, ViewActiveEvent), with the shapes and media of
    /// fGeoManager
    mutable TGeoManager* fViewGeo = nullptr;

    /// Decoding map from geometry physical ID to DAQ channel ID.
    std::map<int, int> fPhysicalToDAQMap;

   public:
    /// \brief Default decoding file name used when none is provided by configuration.
    std::string fDecodingFile = "default_decoding.dec";
    /// Geometry path of each channel volume and its physical ID (`volumeToPhysicalIDMap`). Empty: the
    /// physical ID is the UniqueID of the geometry node (TRestMicromegasReadout)
    std::map<std::string, int> fPathToPhysicalIDMap;

    /// \brief One volume to draw in a view of the readout (GetViewItems, DrawViewItems).
    struct ViewItem {
        TGeoShape* shape = nullptr;           ///< Shape of the volume (owned by the readout geometry)
        TGeoMedium* medium = nullptr;         ///< Medium of the volume
        std::unique_ptr<TGeoHMatrix> matrix;  ///< Global transformation of the volume
        Color_t color = kGray;                ///< Line colour (kRed for active channels)
        char transparency = 0;                ///< Transparency (0 for active channels)
        std::string name;                     ///< Name of the geometry node
        /// Physical ID of the channel of the volume (-1: not a channel)
        int physicalID = -1;
    };

    /// \brief Constructs a detector readout metadata object.
    TRestDetectorReadout();
    /// \brief Constructs a readout from an in-memory YAML node section.
    TRestDetectorReadout(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructs a readout from file+section metadata references.
    TRestDetectorReadout(const std::string& fileName, const std::string& sectionName);

    /// \brief Destructor: deletes the geometry, owned by the readout (kept if a view was drawn, see
    /// ReleaseGeometry).
    virtual ~TRestDetectorReadout();

    /// Not copyable: the readout owns its geometry
    TRestDetectorReadout(const TRestDetectorReadout&) = delete;
    TRestDetectorReadout& operator=(const TRestDetectorReadout&) = delete;

    /// \brief Returns the class name used by REST metadata introspection.
    /// \return The literal class name.
    std::string GetClassName() const override { return "TRestDetectorReadout"; }

    /// \brief Reads the common parameters (decodingFile, volumeToPhysicalIDMap) from the YAML node. The
    /// decoding file is only read by BuildGeometry: a readout imported from a ROOT file takes the decoding
    /// stored there.
    void LoadConfig() override;

    /// \brief Metadata initialization hook.
    void Initialize() override {}

    /// \brief Builds the technology-specific readout geometry from the parameters of the YAML and loads the
    /// decoding file (decodingFile). Throws if the decoding or the geometry cannot be loaded.
    virtual void BuildGeometry() = 0;

    /// \brief Replaces the geometry by a new empty TGeoManager named as the readout, with an assembly
    /// READOUT_TOP as top volume (called by the BuildGeometry of the derived classes that build their
    /// volumes).
    void InitializeReadout();

    /// \brief Opens a graphical window to visualize the readout geometry.
    /// \param option Drawing option passed to ROOT (e.g., "ogl" for OpenGL, "" for default X11/Web).
    void ViewReadoutGeometry(const std::string& option = "ogl") const;
    /// \brief Visualizes the geometry highlighting the provided active DAQ channels (in red).
    /// \param activeChannels DAQ IDs of the channels to highlight.
    void ViewActiveEvent(const std::vector<int>& activeChannels) const;

    /// \brief Loads a decoding file mapping physical IDs to DAQ channels (rows `daqChannel physicalID`,
    /// see the class description), replacing the current decoding.
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
    /// \param decodingName Decoding object name inside the geometry decoding folder, as given to Export.
    /// \return `true` if import completed successfully.
    bool Import(TFile* fIn, const std::string& geometryName, const std::string& decodingName);

    /// \brief Exports geometry and decoding into a ROOT file, as Readouts/<geometryName>/Geometry and
    /// Readouts/<geometryName>/Decodings/<decodingName> (the readout YAML goes to RESTMetadataStore).
    /// \param fOut Output ROOT file.
    /// \param geometryName Geometry object name.
    /// \param decodingName Name under which the decoding is stored (what Import must be given).
    /// \return `true` if export completed successfully.
    bool Export(TFile* fOut, const std::string& geometryName, const std::string& decodingName) const;

    /// \brief Names of the decodings stored for a geometry in a ROOT file.
    static std::vector<std::string> GetDecodingNames(TFile* fIn, const std::string& geometryName);

    /// \brief Returns DAQ channel corresponding to a spatial position.
    /// \param x X coordinate in detector reference frame.
    /// \param y Y coordinate in detector reference frame.
    /// \param z Z coordinate in detector reference frame.
    /// \return DAQ channel, or `-1` when no channel is found.
    int GetChannelFromPosition(double x, double y, double z) const;

    /// \brief Returns the centroid position of a given DAQ channel.
    /// \param daqID DAQ channel identifier.
    /// \return Channel centroid in detector coordinates.
    virtual ROOT::Math::XYZVector GetPositionFromChannel(int daqID) const;

    /// \brief Where the signal of a DAQ channel is collected, e.g. the photocathode of the PMT of a
    /// scintillator (TRestGDMLReadout `readoutVolume`), unlike GetPositionFromChannel (the channel volume).
    /// \return The position in the global frame, NaN if the readout does not define one (default).
    virtual ROOT::Math::XYZVector GetReadoutPosition(int daqID) const {
        (void)daqID;
        const double nan = std::numeric_limits<double>::quiet_NaN();
        return {nan, nan, nan};
    }

    /// \brief Bounding box of the channels (the volumes with a DAQ channel) in the global frame, e.g. the
    /// extent of the readout for a gain map (TRestDataSetGainMap).
    /// \return (minimum, maximum) corners; NaN if there are no channels. A readout whose channel positions
    /// (GetPositionFromChannel) are in another frame gives it in that frame (TRestMicromegasReadout rotated).
    virtual std::pair<ROOT::Math::XYZVector, ROOT::Math::XYZVector> GetChannelsBoundingBox() const;

    /// \brief Returns the signed distance from a position to the readout of a DAQ channel, projected on the
    /// readout normal (as legacy TRestDetectorReadoutPlane::GetDistanceTo: (pos - plane) · normal).
    /// \return The distance, or NaN if the readout does not define one or the channel is unknown.
    virtual double GetDistanceToReadout(int daqID, const ROOT::Math::XYZVector& position) const {
        if (!IsChannelConnected(daqID)) return std::numeric_limits<double>::quiet_NaN();
        return GetDistanceToReadout(position);
    }

    /// \brief Signed distance from a position to the readout, for queries without a channel (e.g. the drift
    /// of simulated hits, see TRestDetectorHitsDriftProcess).
    /// \return The distance, or NaN if the readout does not define one (default).
    virtual double GetDistanceToReadout(const ROOT::Math::XYZVector& position) const {
        (void)position;
        return std::numeric_limits<double>::quiet_NaN();
    }

    /// \brief Maximum distance to the readout inside its drift volume (the cathode, as the legacy readout
    /// plane height).
    /// \return The distance, or NaN if the readout does not define one (default): no cathode, the drift
    /// processes apply no upper cut.
    virtual double GetMaxDistanceToReadout(const ROOT::Math::XYZVector& position) const {
        (void)position;
        return std::numeric_limits<double>::quiet_NaN();
    }

    /// \brief Unit normal of the readout (pointing to the cathode) at a position: the gradient of
    /// GetDistanceToReadout, a projection on the normal. A null vector if the readout has no distance.
    ROOT::Math::XYZVector GetReadoutNormal(const ROOT::Math::XYZVector& position) const {
        const double distance = GetDistanceToReadout(position);
        if (std::isnan(distance)) return {0, 0, 0};
        using V = ROOT::Math::XYZVector;
        const V normal(GetDistanceToReadout(position + V(1, 0, 0)) - distance,
                       GetDistanceToReadout(position + V(0, 1, 0)) - distance,
                       GetDistanceToReadout(position + V(0, 0, 1)) - distance);
        return normal.R() > 0 ? normal.Unit() : normal;
    }

    /// \brief Position on the readout of a point of its drift volume: the point moved along the normal by its
    /// distance to the readout (where its charge is collected). The point itself if the readout has no
    /// distance.
    ROOT::Math::XYZVector ProjectToReadout(const ROOT::Math::XYZVector& position) const {
        const double distance = GetDistanceToReadout(position);
        if (std::isnan(distance)) return position;
        return position - distance * GetReadoutNormal(position);
    }

    /// \brief Checks whether a DAQ channel is present in the decoding table.
    /// \param daqID DAQ channel identifier.
    /// \return `true` if the channel is connected.
    bool IsChannelConnected(int daqID) const {
        for (const auto& [physicalID, channelID] : fPhysicalToDAQMap) {
            if (channelID == daqID) return true;
        }
        return false;
    }

    /// \brief Returns the physical ID of a DAQ channel, or -1 if it is not in the decoding table.
    int GetPhysicalID(int daqID) const {
        for (const auto& [physicalID, channelID] : fPhysicalToDAQMap) {
            if (channelID == daqID) return physicalID;
        }
        return -1;
    }

    /// \brief Returns the DAQ channels adjacent to a DAQ channel, {previous, next}: those of the physical
    /// IDs - 1 and + 1, -1 if not connected (or the channel is not in the decoding table). Readouts whose
    /// consecutive physical IDs are not always neighbours override it.
    virtual std::pair<int, int> GetAdjacentChannels(int daqID) const;

    /// \brief Appends the volumes of the channels of the readout to items, those of activeChannels (DAQ
    /// IDs) highlighted. Used to draw several readouts together (TRestDetectorReadoutManager).
    void GetViewItems(std::vector<ViewItem>& items, const std::vector<int>& activeChannels = {}) const;

    /// \brief Draws the items in a new geometry (stored in viewGeo, replacing the previous view) with
    /// the ROOT drawing option (e.g. "ogl").
    static void DrawViewItems(std::vector<ViewItem>& items, TGeoManager*& viewGeo,
                              const std::string& option = "ogl");

    /// \brief Sets the geometry manager used by this readout, which owns it from now on (the previous one is
    /// deleted, see ReleaseGeometry).
    /// \param geo Geometry manager pointer.
    void SetGeoManager(TGeoManager* geo);

    /// \brief Returns the current geometry manager.
    /// \return Pointer to the geometry manager.
    TGeoManager* GetGeoManager() const { return fGeoManager; }

   protected:
    /// \brief Deletes fGeoManager (TRestTools::DeleteGeometry), unless a view was drawn: the view uses its
    /// shapes and may still be displayed, so both are then left to ROOT. Resets both pointers.
    void ReleaseGeometry();

    /// \brief Appends to items the channel volumes of the geometry (by path with fPathToPhysicalIDMap,
    /// otherwise by node UniqueID), those of activeIDs (physical IDs) highlighted.
    virtual void CollectViewItems(const std::set<int>& activeIDs, std::vector<ViewItem>& items) const;
};

#endif
