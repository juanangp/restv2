#ifndef RestCore_TRestGeant4EventViewer
#define RestCore_TRestGeant4EventViewer

#include <Math/Vector3D.h>

#include <map>

#include "TRestEveEventViewer.h"
class TRestGeant4Event;
class TRestGeant4Metadata;

class TEveLine;

/// \class TRestGeant4EventViewer
/// \brief 3D TEve viewer of a TRestGeant4Event, used by TRestBrowser (see REST_Geant4_ViewEvent).
///
/// AddEvent draws each track with initial kinetic energy >= 1 keV and length >= 0.1 mm as a line through
/// its hits, nested under its parent track, coloured by particle (gamma green, e- red, neutron orange,
/// alpha yellow, mu- gray, other negative magenta and positive azure, geantino dashed red), with a width
/// growing with the initial energy and dashed if created by nCapture; and the hits as points grouped by
/// process type and process name (Electromagnetic processes red, Transportation and Init white, hadElastic
/// orange, neutronInelastic green, nCapture blue). It needs the TRestGeant4Metadata of the event
/// (GetGeant4Metadata) for the names of the IDs.
///
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
class TRestGeant4EventViewer : public TRestEveEventViewer {
   private:
    /// The lines of the tracks added with AddTrack or AddParentTrack, by track ID (owned by TEve)
    std::map<int, TEveLine*> fHitConnectors;

    /// \brief The line of a track ID (nullptr and a warning if it was not added)
    TEveLine* GetHitConnector(int trkID) const;

    TRestGeant4Event* fG4Event = nullptr;
    const TRestGeant4Metadata* fG4Metadata = nullptr;

   public:
    /// \brief Resets the event (not owned, the one of AddEvent) and the track lines.
    void Initialize() override;
    /// \brief Removes the drawn event, the metadata and the track lines.
    void DeleteCurrentEvent() override;
    /// \brief Draws a TRestGeant4Event: its tracks, hits and a text with the event ID and the primary
    /// origin (an error is printed if the event or its metadata is missing).
    void AddEvent(TRestEvent* event) override;

    /// \brief Adds a point (mm) to the line of the track `trkID` (a track ID added with AddTrack or
    /// AddParentTrack; a warning if not).
    void NextTrackVertex(int trkID, ROOT::Math::XYZVector to);
    /// \brief Adds a line for the track `trkID` starting at `from` (mm), under the line of `parentID`,
    /// coloured by the particle in `name`. A primary (parentID 0) or a track whose parent was not added
    /// (with a warning) is added at the top level.
    void AddTrack(int trkID, int parentID, ROOT::Math::XYZVector from, const std::string& name);
    /// \brief Adds a top-level line for the primary track `trkID` starting at `from` (mm).
    void AddParentTrack(int trkID, ROOT::Math::XYZVector from, const std::string& name);

    /// \brief Adds a text near the position `at` (mm).
    void AddText(const std::string& text, ROOT::Math::XYZVector at);
    /// \brief Adds a marker at `at` (mm) to the line of the track `trkID`.
    void AddMarker(int trkID, ROOT::Math::XYZVector at, const std::string& name);

    // Constructor
    /// \brief Viewer without event (Initialize).
    TRestGeant4EventViewer();
    // Destructor
    ~TRestGeant4EventViewer() override;
};
#endif
