///______________________________________________________________________________
///______________________________________________________________________________
///______________________________________________________________________________
///
///
///             RESTSoft : Software for Rare Event Searches with TPCs
///
///             TRestEveEventViewer.h inherited from TRestEventViewer
///
///             nov 2015:   First concept
///                 Generic class for visualization of simulated events using
///                 gEveManager JuanAn/Javier Galan
///_______________________________________________________________________________

#ifndef RestCore_TRestEveEventViewer
#define RestCore_TRestEveEventViewer

#include <TObject.h>

#include "TRestEventViewer.h"

class TEveManager;
class TEveWindowSlot;
class TEveWindowPack;
class TEveViewer;
class TEveScene;
class TEveProjectionManager;
class TEveProjectionAxes;
class TEvePointSet;
class TGeoManager;
class TBrowser;
class TPad;

/// \class TRestEveEventViewer
/// \brief Base of the 3D event viewers built on ROOT Eve (TEveManager): a single 3D view, a multi view
/// (3D, R-Phi and Rho-Z projections) and orthogonal XY, XZ and YZ projections, with the geometry and the
/// energy deposits of the event as points.
///
/// Derived classes implement AddEvent, adding the hits of their event with AddSphericalHit and calling
/// Update. The Eve windows are created in the constructor (Initialize), not embedded in TRestBrowser.
class TRestEveEventViewer : public TRestEventViewer {
   protected:
    TEveManager* gEve;  ///< Eve manager (TEveManager::Create), shadows the ROOT global of the same name.

    TEveWindowSlot* slot;  ///< Window slot of the multi view.
    TEveWindowPack* pack;  ///< Window pack of the multi view.

    TEveViewer* viewer3D;    ///< 3D viewer of the multi view.
    TEveViewer* rphiViewer;  ///< R-Phi projection viewer.
    TEveViewer* rhozViewer;  ///< Rho-Z projection viewer.

    TEveScene* rphiScene;  ///< Scene of the R-Phi projection.
    TEveScene* rhozScene;  ///< Scene of the Rho-Z projection.

    TEveProjectionManager* rphi;  ///< R-Phi projection manager.
    TEveProjectionManager* rhoz;  ///< Rho-Z projection manager.

    TEveProjectionAxes* rphiAxes;  ///< Axes of the R-Phi projection.
    TEveProjectionAxes* rhozAxes;  ///< Axes of the Rho-Z projection.

    TEvePointSet* fEnergyDeposits;  ///< Points of the energy deposits of the current event.

    char pointName[256];  ///< Buffer for point names of derived viewers.

    double fMinRadius = 0.2;  ///< Minimum marker radius of a hit, for derived viewers.
    double fMaxRadius = 3.0;  ///< Maximum marker radius of a hit, for derived viewers.

    /// Logical volumes placed more times than this are not drawn (0 for no limit), see SetMaxVolumeCopies
    size_t fMaxVolumeCopies = 1000;

    bool fGlobalSceneInViewer3D = false;  ///< The global scene (geometry) was added to the multi view.

   public:
    /// \brief Creates the Eve manager, the viewers and the projections.
    void Initialize() override;

    /// \brief Nothing: the Eve windows are not embedded in the browser.
    void Embed(TBrowser* b) override {}

    /// \brief Removes the energy deposits and annotations of the current event from the scenes.
    void DeleteCurrentEvent() override;
    /// \brief Removes the geometry from the global scene (it is not deleted, it is not owned).
    void DeleteGeometry();

    /// \brief Adds the event to the scenes (implemented by each viewer).
    void AddEvent(TRestEvent* ev) override = 0;

    /// \brief Nothing: the event is drawn by AddEvent and Update.
    void Plot(const char* option) override {}

    /// \brief Adds a hit as a point (scaled by the geometry scale) to the energy deposits.
    /// \param x X position.
    /// \param y Y position.
    /// \param z Z position.
    /// \param radius Marker size.
    /// \param en Energy of the hit (not used by the base class).
    void AddSphericalHit(double x, double y, double z, double radius, double en);

    /// \brief Creates the multi view tab: 3D, R-Phi and Rho-Z.
    void MultiView();
    /// \brief Sets the guides and style of the 3D viewer.
    void DrawTab();
    /// \brief Draws the geometry (not owned) in the global scene from the world down to its deepest volume
    /// (light materials transparent, vacuum and volumes placed more than SetMaxVolumeCopies times hidden),
    /// replacing a previous one, and fits the cameras to the world. Positions in mm, as the hits (TGeoManager
    /// in Geant4 units, TRestSystemOfUnits).
    void SetGeometry(TGeoManager* geo) override;
    /// \brief Adds the energy deposits to the event and redraws the projections.
    void Update();

    /// \brief Logical volumes placed more than `maxCopies` times (e.g. thousands of fibres) are not drawn,
    /// they make the display very slow; 0 draws all (default 1000). The geometry already drawn is redrawn.
    void SetMaxVolumeCopies(size_t maxCopies);
    /// \brief Limit of placements of a drawn logical volume (0 for no limit).
    size_t GetMaxVolumeCopies() const { return fMaxVolumeCopies; }

    /// \brief Sets the minimum marker radius.
    void SetMinRadius(double rmin) { fMinRadius = rmin; }
    /// \brief Sets the maximum marker radius.
    void SetMaxRadius(double rmax) { fMaxRadius = rmax; }

    /// \brief Constructor: creates the Eve windows (Initialize).
    TRestEveEventViewer();
    /// \brief Destructor: removes the event and the geometry from the scenes.
    ~TRestEveEventViewer() override;
};
#endif
