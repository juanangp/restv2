///______________________________________________________________________________
///______________________________________________________________________________
///______________________________________________________________________________
///
///
///             RESTSoft : Software for Rare Event Searches with TPCs
///
///             TRestEveEventViewer.h inherited from TRestEventViewer
///
///             nov 2015:   First concept
///                 Generic class for visualization of simulated events using
///                 gEveManager JuanAn/Javier Galan
///_______________________________________________________________________________

#ifndef RestCore_TRestEveEventViewer
#define RestCore_TRestEveEventViewer

#include <TObject.h>
#include <TVector3.h>
#include "TRestEventViewer.h"

#define GEOM_SCALE 0.1

class TEveManager;
class TEveWindowSlot;
class TEveWindowPack;
class TEveViewer;
class TEveScene;
class TEveProjectionManager;
class TEveProjectionAxes;
class TEvePointSet;
class TGeoManager;
class TBrowser;
class TPad; 

class TRestEveEventViewer : public TRestEventViewer {
   protected:
    TEveManager* gEve;

    TEveWindowSlot* slot;
    TEveWindowPack* pack;

    TEveViewer* viewer3D;
    TEveViewer* rphiViewer;
    TEveViewer* rhozViewer;

    TEveScene* rphiScene;
    TEveScene* rhozScene;

    TEveProjectionManager* rphi;
    TEveProjectionManager* rhoz;

    TEveProjectionAxes* rphiAxes;
    TEveProjectionAxes* rhozAxes;

    TEvePointSet* fEnergyDeposits;

    char pointName[256];

    Double_t fMinRadius = 0.2;
    Double_t fMaxRadius = 3.0;

   public:
    virtual void Initialize();

    virtual void Embed(TBrowser* b) {}

    virtual void DeleteCurrentEvent();
    void DeleteGeometry();

    virtual void AddEvent(TRestEvent* ev) = 0;

    virtual void Plot(const char* option) {}

    void AddSphericalHit(double x, double y, double z, double radius, double en);

    void MultiView();
    void DrawTab();
    void SetGeometry(TGeoManager* geo);
    void Update();

    void SetMinRadius(Double_t rmin) { fMinRadius = rmin; }
    void SetMaxRadius(Double_t rmax) { fMaxRadius = rmax; }

    // Constructor
    TRestEveEventViewer();
    // Destructor
    virtual ~TRestEveEventViewer();

};
#endif
