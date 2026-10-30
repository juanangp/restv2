#include "TRestBrowser.h"
#include "TRestEventViewer.h"

int REST_ViewEvents(const std::string& fName, const std::string& eventType = "") {
    TRestBrowser* browser = new TRestBrowser("TRestEventViewer");

    browser->OpenFile(fName);
    if (!eventType.empty()) browser->SetEvent(eventType);

    return 0;
}
