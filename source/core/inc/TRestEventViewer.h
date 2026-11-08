///______________________________________________________________________________
///______________________________________________________________________________
///______________________________________________________________________________
///
///
///             RESTSoft : Software for Rare Event Searches with TPCs
///
///             TRestEventViewer.h
///
///             Base class of the event viewers of TRestBrowser
///
///             jul 2015:   First concept
///                 J. Galan
///		nov 2015: Generic class for event visualization
///		    JuanAn Garcia
///
///_______________________________________________________________________________

#ifndef RestCore_TRestEventViewer
#define RestCore_TRestEventViewer

#include <TGButton.h>
#include <TGLabel.h>
#include <TGTextEntry.h>
#include <TObject.h>

class TRestEvent;

class TGeoManager;
class TRestBrowser;
class TPad;
class TCanvas;
class TBrowser;

/// \class TRestEventViewer
/// \brief Generic event viewer: draws the current event (TRestEvent::DrawEvent with the plot option of
/// the browser) in a canvas embedded in TRestBrowser.
///
/// Derived viewers (e.g. 3D viewers based on TRestEveEventViewer) override AddEvent and Plot. They are
/// created by TRestBrowser from their class name (ROOT dictionary) or given to it already created.
class TRestEventViewer {
   protected:
    /// Geometry shown with the events (not owned), nullptr if none
    TGeoManager* fGeometry = nullptr;  //!
    /// Current event (not owned: usually the event of the TRestRun of the browser)
    TRestEvent* fEvent = nullptr;  //!

    /// Factor from the hit positions (mm) to the geometry units, 1 for a geometry in mm
    double fGeomScale = 1.0;

    /// Pad of the current event, returned by its DrawEvent (owned by the event)
    TPad* fPad = nullptr;  //!
    /// Canvas of the viewer, created by Initialize
    TCanvas* fCanvas = nullptr;  //!

    /// \brief Creates the canvas of the viewer (deleting a previous one).
    virtual void Initialize();

   public:
    /// \brief Creates the canvas embedded in the right tab of the browser.
    /// \param b ROOT browser, or nullptr for a standalone canvas.
    virtual void Embed(TBrowser* b);

    /// \brief Sets the current event (not owned).
    /// \param ev Event to show.
    virtual void AddEvent(TRestEvent* ev);

    /// \brief Draws the current event with a DrawEvent option, or clears the canvas without event.
    /// \param option Draw option of the event.
    virtual void Plot(const char* option);

    /// \brief Forgets the current event without deleting it (it is not owned, usually it is the event of
    /// the TRestRun of the browser). Derived viewers override it to delete the graphics they own.
    virtual void DeleteCurrentEvent();
    /// \brief Sets the geometry shown with the events (not owned).
    virtual void SetGeometry(TGeoManager* geo) { fGeometry = geo; }
    /// \brief Sets the current event (not owned) without plotting it.
    void SetEvent(TRestEvent* event) { fEvent = event; }
    /// \brief The geometry, nullptr if none.
    TGeoManager* GetGeometry() { return fGeometry; }
    /// \brief The current event, nullptr if none.
    TRestEvent* GetEvent() { return fEvent; }

    /// \brief Sets the factor from hit positions to geometry units.
    void SetGeomScale(double scale) { fGeomScale = scale; }
    /// \brief Factor from hit positions to geometry units.
    double GetGeomScale() const { return fGeomScale; }

    /// \brief Constructor (the canvas is created by Embed or Initialize).
    TRestEventViewer();
    /// \brief Destructor (the event is not deleted).
    virtual ~TRestEventViewer();
};
#endif
