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

#include <iostream>
#include <map>

#include "TRestTools.h"
#include "TRestEventViewer.h"
#include "TRestEveEventViewer.h"
#include "TRestBrowser.h"
#include "TRestLogManager.h"

#include <TString.h>
#include <TPad.h>
#include <TCanvas.h>
#include <TBrowser.h>
#include <TGeoManager.h>

#include <TEveManager.h>
#include <TEveBrowser.h>
#include <TEveWindow.h>
#include <TEveEventManager.h>
#include <TEveViewer.h>
#include <TEveScene.h>
#include <TEveProjectionManager.h>
#include <TEvePointSet.h>
#include <TEveGeoNode.h>

#include <TGLViewer.h>
#include <TGLRnrCtx.h>
#include <TGLUtil.h>


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
    if (fPad == nullptr) fPad = new TPad();
    if (fEvent != nullptr) {
        fPad = fEvent->DrawEvent(option);
        fCanvas->cd();
        fPad->Draw();
        fPad->Update();
        fCanvas->Update();
    } else {
        fCanvas->cd();
        fPad->Clear();
        fPad->Update();
        fCanvas->Update();
    }
}

void TRestEventViewer::DeleteCurrentEvent() { delete fEvent; }
