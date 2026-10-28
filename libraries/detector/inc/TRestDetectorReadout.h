#ifndef TRESTDETECTORREADOUT_H
#define TRESTDETECTORREADOUT_H

#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
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
/// technology.
class TRestDetectorReadout : public TRestMetadata {
    DECLARE_LOG_CLASS(TRestDetectorReadout)
   protected:
    /// ROOT geometry manager used for geometry navigation and position lookup.
    TGeoManager* fGeoManager = nullptr;

    mutable TGeoManager* fViewGeo = nullptr;

    /// Decoding map from geometry physical ID to DAQ channel ID.
    std::map<int, int> fPhysicalToDAQMap;

   public:
    /// \brief Default decoding file name used when none is provided by configuration.
    std::string fDecodingFile = "default_decoding.dec";
    std::map<std::string, int> fPathToPhysicalIDMap;

    struct ViewItem {
        TGeoShape* shape = nullptr;
        TGeoMedium* medium = nullptr;
        std::unique_ptr<TGeoHMatrix> matrix;
        Color_t color = kGray;
        Char_t transparency = 0;
        std::string name;
    };

    /// \brief Constructs a detector readout metadata object.
    TRestDetectorReadout();
    /// \brief Constructs a readout from an in-memory YAML node section.
    TRestDetectorReadout(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructs a readout from file+section metadata references.
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

    /// \brief Builds the technology-specific readout geometry.
    /// \param readoutNode YAML node with geometry parameters.
    virtual void BuildGeometry() = 0;

    /// \brief Finalizes geometry/readout internals after loading configuration.
    void InitializeReadout();

    /// \brief Opens a graphical window to visualize the readout geometry.
    /// \param option Drawing option passed to ROOT (e.g., "ogl" for OpenGL, "" for default X11/Web).
    void ViewReadoutGeometry(const std::string& option = "ogl") const;
    /// \brief Visualizes the geometry highlighting the provided active DAQ channels.
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
    /// \return The distance, or NaN if the readout does not define one (default).
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

    void GetViewItems(std::vector<ViewItem>& items, const std::vector<int>& activeChannels = {}) const;

    static void DrawViewItems(std::vector<ViewItem>& items, TGeoManager*& viewGeo,
                              const std::string& option = "ogl");

    /// \brief Sets the geometry manager used by this readout.
    /// \param geo Geometry manager pointer.
    void SetGeoManager(TGeoManager* geo);

    /// \brief Returns the current geometry manager.
    /// \return Pointer to the geometry manager.
    TGeoManager* GetGeoManager() const { return fGeoManager; }

   protected:
    virtual void CollectViewItems(const std::set<int>& activeIDs, std::vector<ViewItem>& items) const;

};

#endif
