#ifndef REST_GEANT4_VIEWEVENT_C
#define REST_GEANT4_VIEWEVENT_C

#include <TRestBrowser.h>
#include <TRestEveEventViewer.h>
#include <TRestGeant4Event.h>

/// \brief Opens a restG4 file in a TRestBrowser with the TRestGeant4EventViewer (3D TEve view of the
/// tracks and hits of each TRestGeant4Event).
/// \param fName restG4 file with TRestGeant4Event.
/// \param maxVolumeCopies Logical volumes placed more times are not drawn (e.g. thousands of fibres, very
/// slow to draw), 0 draws all (TRestEveEventViewer::SetMaxVolumeCopies).
/// \return 0.
///
///    restRoot
///    REST_Geant4_ViewEvent("Run1_NLDBD_Test.root")
///    REST_Geant4_ViewEvent("Run1_NLDBD_Test.root", 0)  // all the volumes
int REST_Geant4_ViewEvent(const std::string& fName, size_t maxVolumeCopies = 1000) {
    TRestBrowser* browser = new TRestBrowser("TRestGeant4EventViewer");
    if (auto* viewer = dynamic_cast<TRestEveEventViewer*>(browser->GetViewer()))
        viewer->SetMaxVolumeCopies(maxVolumeCopies);

    browser->OpenFile(fName);
    browser->SetEvent("TRestGeant4Event");

    return 0;
}
#endif
