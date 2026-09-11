#ifndef REST_GEANT4_VIEWEVENT_C
#define REST_GEANT4_VIEWEVENT_C

#include<TRestBrowser.h>
#include<TRestGeant4Event.h>

int REST_Geant4_ViewEvent(const std::string& fName) {
    TRestBrowser* browser = new TRestBrowser("TRestGeant4EventViewer");

    browser->OpenFile(fName);
    browser->SetInputEvent("TRestGeant4Event");

    return 0;
}
#endif
