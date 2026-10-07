#include "TRestBrowser.h"
#include "TRestEventViewer.h"

int REST_ViewEvents(const std::string& fName, const std::string& eventType = "") {

    TRestBrowser* browser = new TRestBrowser("TRestEventViewer");

    browser->OpenFile(fName);
    if(!eventType.empty())browser->SetInputEvent(eventType);

    return 0;
}
