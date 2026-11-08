#include "TRestBrowser.h"
#include "TRestEventViewer.h"
#include "TRestLogManager.h"

/// \brief Opens a REST run file in the event browser (TRestBrowser with a TRestEventViewer).
///
/// The browser window stays open after the macro returns, to navigate the events of the file.
///
/// \param fName Run file to open (TRestBrowser::OpenFile).
/// \param eventType Event type (class name) to display (TRestBrowser::SetEvent); if empty, the browser
/// default (the event of the file) is used.
/// \return 0 on success, 1 if the file cannot be opened (TRestBrowser::OpenFile returns false).
///
///    restRoot
///    REST_ViewEvents("R01234_ANA.root")
///    REST_ViewEvents("R01234_ANA.root", "TRestDetectorHitsEvent")
int REST_ViewEvents(const std::string& fName, const std::string& eventType = "") {
    TRestBrowser* browser = new TRestBrowser("TRestEventViewer");

    if (!browser->OpenFile(fName)) {
        RESTError << "REST_ViewEvents: could not open " << fName << RESTendl;
        return 1;
    }
    if (!eventType.empty()) browser->SetEvent(eventType);

    return 0;
}
