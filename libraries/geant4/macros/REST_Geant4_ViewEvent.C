#ifndef REST_GEANT4_VIEWEVENT_C
#define REST_GEANT4_VIEWEVENT_C

#include <TRestBrowser.h>
#include <TRestGeant4Event.h>

/// \brief Opens a restG4 file in a TRestBrowser with the TRestGeant4EventViewer (3D TEve view of the
/// tracks and hits of each TRestGeant4Event).
/// \param fName restG4 file with TRestGeant4Event.
/// \return 0.
///
///    restRoot
///    REST_Geant4_ViewEvent("Run1_NLDBD_Test.root")
int REST_Geant4_ViewEvent(const std::string& fName) {
    TRestBrowser* browser = new TRestBrowser("TRestGeant4EventViewer");

    browser->OpenFile(fName);
    browser->SetEvent("TRestGeant4Event");

    return 0;
}
#endif
