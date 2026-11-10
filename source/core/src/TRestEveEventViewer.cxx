///______________________________________________________________________________
///______________________________________________________________________________
///______________________________________________________________________________
///
///
///             RESTSoft : Software for Rare Event Searches with TPCs
///
///             TRestEveEventViewer.cxx
///
///             nov 2015:   First concept
///                 Generic class for visualization of simulated events using
///                 gEveManager JuanAn/Javier Galan
///_______________________________________________________________________________

#include "TRestEveEventViewer.h"

#include <TEveBrowser.h>
#include <TEveEventManager.h>
#include <TEveGeoNode.h>
#include <TEveManager.h>
#include <TEvePointSet.h>
#include <TEveProjectionAxes.h>
#include <TEveProjectionManager.h>
#include <TEveScene.h>
#include <TEveViewer.h>
#include <TEveWindow.h>
#include <TGLRnrCtx.h>
#include <TGLUtil.h>
#include <TGLViewer.h>
#include <TGTab.h>
#include <TGeoManager.h>
#include <TGeoNode.h>
#include <TGeoVolume.h>

#include <algorithm>
#include <map>
#include <string>

#include "TRestLogManager.h"
#include "TRestTools.h"

using namespace std;
using namespace TRestTools;

TRestEveEventViewer::TRestEveEventViewer() {
    Initialize();
    fEnergyDeposits = new TEvePointSet();
    fEnergyDeposits->SetElementName("Energy deposits");
}

TRestEveEventViewer::~TRestEveEventViewer() {
    // DeleteCurrentEvent deletes the energy deposits and creates new empty ones (not added to the scene)
    DeleteCurrentEvent();
    DeleteGeometry();
    delete fEnergyDeposits;
    fEnergyDeposits = nullptr;
}

void InitializePerspectiveView() {
    const std::map<std::string, TGLViewer::ECameraType> projectionsMap = {
        {"Projection XY", TGLViewer::kCameraOrthoXOY},   //
        {"Projection XZ", TGLViewer::kCameraOrthoXnOZ},  //
        {"Projection YZ", TGLViewer::kCameraOrthoZOY}    //
    };
    for (const auto& [name, cameraType] : projectionsMap) {
        auto slot = TEveWindow::CreateWindowInTab(gEve->GetBrowser()->GetTabRight());
        auto pack = slot->MakePack();
        pack->SetElementName(name.c_str());
        pack->SetHorizontal();
        pack->SetShowTitleBar(kFALSE);
        pack->NewSlot()->MakeCurrent();
        auto f3DView = gEve->SpawnNewViewer("3D View", pack->GetName());
        f3DView->AddScene(gEve->GetGlobalScene());
        f3DView->AddScene(gEve->GetEventScene());
        f3DView->GetGLViewer()->SetCurrentCamera(cameraType);
    }
}

void TRestEveEventViewer::Initialize() {
    gEve = TEveManager::Create();
    gEve->GetBrowser()->DontCallClose();

    gEve->AddEvent(new TEveEventManager("Event", "Event"));

    auto singleViewer = gEve->GetDefaultViewer();
    singleViewer->SetElementName("SingleView");

    singleViewer->GetGLViewer()->SetClearColor(kBlack);

    InitializePerspectiveView();
    MultiView();
    DrawTab();

    singleViewer->GetGLViewer()->SetStyle(TGLRnrCtx::kOutline);

    gEve->GetBrowser()->GetTabRight()->SetTab(0);  // select the "SingleView" viewer as active

    singleViewer->GetGLViewer()->CurrentCamera().Reset();
    gEve->FullRedraw3D(kTRUE);
}

void TRestEveEventViewer::SetGeometry(TGeoManager* geo) {
    // Already drawn: not added twice to the global scene
    if (geo == fGeometry) return;

    DeleteGeometry();
    TRestEventViewer::SetGeometry(geo);

    if (fGeometry == nullptr) {
        return;
    }

    // Placements of each logical volume: the volumes placed many times (e.g. thousands of fibres) are hidden,
    // drawing them in every viewer makes the display very slow
    std::map<TGeoVolume*, size_t> copies;
    TGeoIterator next(fGeometry->GetTopVolume());
    while (TGeoNode* node = next()) copies[node->GetVolume()]++;

    const unsigned int nVolumes = fGeometry->GetListOfVolumes()->GetEntries();
    double transparencyLevel = 85;
    if (nVolumes > 30) {
        transparencyLevel = 90;
    }
    for (unsigned int i = 0; i < nVolumes; i++) {
        auto volume = fGeometry->GetVolume(i);
        auto material = volume->GetMaterial();
        bool visible = true;
        if (material->GetDensity() <= 0.01) {
            volume->SetTransparency(99);
            // We consider this vacuum for display purposes
            if (material->GetDensity() <= 0.001) visible = false;
        } else {
            volume->SetTransparency(transparencyLevel);
        }
        const size_t nCopies = copies.count(volume) ? copies.at(volume) : 0;
        if (visible && fMaxVolumeCopies > 0 && nCopies > fMaxVolumeCopies) {
            RESTWarning << "TRestEveEventViewer: volume '" << volume->GetName() << "' not drawn, placed "
                        << nCopies << " times (more than " << fMaxVolumeCopies
                        << ", SetMaxVolumeCopies(0) draws all)" << RESTendl;
            visible = false;
        }
        volume->SetVisibility(visible);
    }

    // From the world (top node, mm as the hits: TGeoManager in Geant4 units) down to the deepest volume
    TEveGeoTopNode* eveGeoTopNode = new TEveGeoTopNode(fGeometry, fGeometry->GetTopNode());
    eveGeoTopNode->SetVisLevel(std::max(5, fGeometry->GetMaxLevel()));

    gEve->AddGlobalElement(eveGeoTopNode);

    if (!fGlobalSceneInViewer3D) {
        viewer3D->AddScene(gEve->GetGlobalScene());
        fGlobalSceneInViewer3D = true;
    }

    // The cameras were reset with the empty scenes (Initialize): fitted now to the world
    gEve->FullRedraw3D(kTRUE);
}

void TRestEveEventViewer::SetMaxVolumeCopies(size_t maxCopies) {
    fMaxVolumeCopies = maxCopies;
    // Drawn again with the new limit
    if (TGeoManager* geometry = fGeometry) {
        DeleteGeometry();
        SetGeometry(geometry);
    }
}

void TRestEveEventViewer::DeleteCurrentEvent() {
    cout << "Removing event" << endl;
    delete fEnergyDeposits;
    fEnergyDeposits = new TEvePointSet();
    fEnergyDeposits->SetElementName("Energy deposits");
    gEve->GetViewers()->DeleteAnnotations();
    gEve->GetCurrentEvent()->DestroyElements();
}

void TRestEveEventViewer::DeleteGeometry() {
    gEve->GetGlobalScene()->DestroyElements();
    fGeometry = nullptr;
}

void TRestEveEventViewer::MultiView() {
    slot = TEveWindow::CreateWindowInTab(gEve->GetBrowser()->GetTabRight());

    pack = slot->MakePack();
    pack->SetElementName("MultiView");
    pack->SetHorizontal();
    pack->SetShowTitleBar(kFALSE);
    pack->NewSlot()->MakeCurrent();

    viewer3D = gEve->SpawnNewViewer("3DView");
    viewer3D->AddScene(gEve->GetEventScene());

    pack = pack->NewSlot()->MakePack();
    pack->SetShowTitleBar(kFALSE);
    pack->NewSlot()->MakeCurrent();
    rphiViewer = gEve->SpawnNewViewer("RPhi View");
    rphiViewer->GetGLViewer()->SetCurrentCamera(TGLViewer::kCameraOrthoXOY);

    rphiScene = gEve->SpawnNewScene("RPhi");
    rphiViewer->AddScene(rphiScene);

    rphi = new TEveProjectionManager(TEveProjection::kPT_RPhi);
    rphiScene->AddElement(rphi);

    rphiAxes = new TEveProjectionAxes(rphi);
    rphiScene->AddElement(rphiAxes);

    pack = pack->NewSlot()->MakePack();
    pack->SetShowTitleBar(kFALSE);
    pack->NewSlot()->MakeCurrent();
    rhozViewer = gEve->SpawnNewViewer("RhoZ View");
    rhozViewer->GetGLViewer()->SetCurrentCamera(TGLViewer::kCameraOrthoXOY);

    rhozScene = gEve->SpawnNewScene("RhoZ");
    rhozViewer->AddScene(rhozScene);

    rhoz = new TEveProjectionManager(TEveProjection::kPT_RhoZ);
    rhozScene->AddElement(rhoz);

    rhozAxes = new TEveProjectionAxes(rhoz);
    rhozScene->AddElement(rhozAxes);
}

void TRestEveEventViewer::DrawTab() {
    TGLViewer* v2 = viewer3D->GetGLViewer();
    v2->SetGuideState(TGLUtil::kAxesEdge, kTRUE, kFALSE, nullptr);
    v2->SetStyle(TGLRnrCtx::kOutline);
}

void TRestEveEventViewer::Update() {
    gEve->AddElement(fEnergyDeposits);
    rphi->ImportElements(gEve->GetCurrentEvent());
    rhoz->ImportElements(gEve->GetCurrentEvent());

    rphiViewer->Redraw(kTRUE);
    rhozViewer->Redraw(kTRUE);
}

void TRestEveEventViewer::AddSphericalHit(double x, double y, double z, double radius, double en) {
    fEnergyDeposits->SetOwnIds(kTRUE);
    fEnergyDeposits->SetNextPoint(x * fGeomScale, y * fGeomScale, z * fGeomScale);
    fEnergyDeposits->SetMarkerColor(kYellow);
    fEnergyDeposits->SetMarkerSize(Size_t(radius));
    fEnergyDeposits->SetMarkerStyle(4);
}
