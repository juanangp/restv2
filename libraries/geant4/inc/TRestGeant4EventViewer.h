#ifndef RestCore_TRestGeant4EventViewer
#define RestCore_TRestGeant4EventViewer

#include "TRestEveEventViewer.h"
#include "TRestGeant4Event.h"

class TEveLine;

class TRestGeant4EventViewer : public TRestEveEventViewer {
   private:
    std::vector<TEveLine*> fHitConnectors;

    TRestGeant4Event* fG4Event = nullptr;
    const TRestGeant4Metadata* fG4Metadata = nullptr;

   public:
    void Initialize();
    void DeleteCurrentEvent();
    void AddEvent(TRestEvent* event);

    void NextTrackVertex(Int_t trkID, ROOT::Math::XYZVector to);
    void AddTrack(Int_t trkID, Int_t parentID, ROOT::Math::XYZVector from, TString name);
    void AddParentTrack(Int_t trkID, ROOT::Math::XYZVector from, TString name);

    void AddText(TString text, ROOT::Math::XYZVector at);
    void AddMarker(Int_t trkID, ROOT::Math::XYZVector at, TString name);

    // Constructor
    TRestGeant4EventViewer();
    // Destructor
    ~TRestGeant4EventViewer();

};
#endif

