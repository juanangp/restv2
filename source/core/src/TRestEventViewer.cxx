///______________________________________________________________________________
///______________________________________________________________________________
///______________________________________________________________________________
///
///
///             RESTSoft : Software for Rare Event Searches with TPCs
///
///             TRestEventViewer.cxx
///
///             A geometry class to store detector geometry
///
///             jul 2015:   First concept
///                 J. Galan
///		nov 2015: Generic class for event visualization
///		    JuanAn Garcia
///
///_______________________________________________________________________________

#include "TRestEventViewer.h"

#include <TBrowser.h>
#include <TCanvas.h>
#include <TPad.h>

#include "TRestEvent.h"

using namespace std;

TRestEventViewer::TRestEventViewer() {
    // TRestEventViewer default constructor
    // Initialize();
    fPad = nullptr;
    fCanvas = nullptr;
}

TRestEventViewer::~TRestEventViewer() {
    // TRestEventViewer destructor
    // DeleteCurrentEvent(  );
}

void TRestEventViewer::Initialize() {
    fPad = nullptr;

    if (fCanvas != nullptr) delete fCanvas;
    fCanvas = new TCanvas("Event Viewer", "Event Viewer");

    fCanvas->SetWindowPosition(350, 10);
}

void TRestEventViewer::Embed(TBrowser* b) {
    if (b != nullptr) b->StartEmbedding(1, -1);

    Initialize();

    if (b != nullptr) b->StopEmbedding();
}

void TRestEventViewer::AddEvent(TRestEvent* ev) { fEvent = ev; }

void TRestEventViewer::Plot(const char* option) {
    // The pad is the one of the event (DrawEvent), owned by it: the viewer does not create nor delete it
    if (fEvent != nullptr) {
        fPad = fEvent->DrawEvent(option);
        fCanvas->cd();
        if (fPad != nullptr) {
            fPad->Draw();
            fPad->Update();
        }
        fCanvas->Update();
    } else {
        // Without event the pad of the previous one may be deleted: only the canvas is cleared
        fPad = nullptr;
        fCanvas->cd();
        fCanvas->Clear();
        fCanvas->Update();
    }
}

void TRestEventViewer::DeleteCurrentEvent() { delete fEvent; }
