#include "TRestGeant4EventViewer.h"

#include <TEveLine.h>
#include <TEveManager.h>
#include <TEvePointSet.h>
#include <TEveStraightLineSet.h>
#include <TEveText.h>

#include "TRestGeant4Event.h"
#include "TRestGeant4Metadata.h"
#include "TRestGeant4Track.h"
#include "TRestTools.h"

using namespace std;
using XYZVector = ROOT::Math::XYZVector;

TRestGeant4EventViewer::TRestGeant4EventViewer() { Initialize(); }

TRestGeant4EventViewer::~TRestGeant4EventViewer() {
    // TRestGeant4EventViewer destructor
}

void TRestGeant4EventViewer::Initialize() {
    // The event is not owned: it is the one given to AddEvent
    fG4Event = nullptr;
    fHitConnectors.clear();
}

void TRestGeant4EventViewer::DeleteCurrentEvent() {
    TRestEveEventViewer::DeleteCurrentEvent();
    fG4Event = nullptr;
    fG4Metadata = nullptr;

    fHitConnectors.clear();
}

TEveLine* TRestGeant4EventViewer::GetHitConnector(int trkID) const {
    const auto it = fHitConnectors.find(trkID);
    if (it == fHitConnectors.end()) {
        RESTWarning << "TRestGeant4EventViewer: no line for the track ID " << trkID
                    << " (AddTrack or AddParentTrack)" << RESTendl;
        return nullptr;
    }
    return it->second;
}

struct TrackVisualConfiguration {
    Color_t fColor = kWhite;
    Style_t fLineStyle = 1;
    Width_t fLineWidth = 4.0;
};

TrackVisualConfiguration GetTrackVisualConfiguration(const TRestGeant4Track& track) {
    auto config = TrackVisualConfiguration();

    const auto particleName = track.GetParticleName();

    if (particleName == "geantino") {
        config.fColor = kRed;
        config.fLineStyle = 9;
        config.fLineWidth = 2;
        return config;
    }

    if (particleName.find('-') != std::string::npos) {
        config.fColor = kMagenta;
    } else if (particleName.find('+') != std::string::npos) {
        config.fColor = kAzure;
    }

    if (particleName == "neutron") {
        config.fColor = kOrange;
    } else if (particleName == "gamma") {
        config.fColor = kGreen;
    } else if (particleName == "e-") {
        config.fColor = kRed;
    } else if (particleName == "mu-") {
        config.fColor = kGray;
    } else if (particleName == "alpha") {
        config.fColor = kYellow;
    }

    Width_t width = TMath::Log10(track.GetInitialEnergy() / 10);
    width = (width > 10 ? 10 : width);
    width = (width < 1 ? 1 : width);

    // At least 1, a track below 10 keV had a width <= 0 and was not drawn
    config.fLineWidth = width;

    if (track.GetCreatorProcess() == "nCapture") {
        config.fLineStyle = 2;
    }

    return config;
}

TEveStraightLineSet* GetTrackEveDrawable(const TRestGeant4Track& track, double geomScale) {
    auto lineSet = new TEveStraightLineSet(
        TRestTools::Format("ID %d | %s | Created by %s | KE: %s", track.GetTrackID(),
                           track.GetParticleName().c_str(), track.GetCreatorProcess().c_str(),
                           REST_Units::FormatAs(track.GetInitialEnergy(), REST_Units::Energy).c_str())
            .c_str());

    const size_t nHits = track.GetNumberOfHits();

    if (nHits > 1) {
        for (size_t i = 0; i < nHits - 1; ++i) {
            const auto posCurrent = track.GetHitPosition(i);
            const auto posNext = track.GetHitPosition(i + 1);

            lineSet->AddLine(
                static_cast<float>(geomScale * posCurrent.X()),
                static_cast<float>(geomScale * posCurrent.Y()),
                static_cast<float>(geomScale * posCurrent.Z()), static_cast<float>(geomScale * posNext.X()),
                static_cast<float>(geomScale * posNext.Y()), static_cast<float>(geomScale * posNext.Z()));
        }
    }

    const auto config = GetTrackVisualConfiguration(track);

    lineSet->SetMainColor(config.fColor);
    lineSet->SetLineColor(config.fColor);
    lineSet->SetLineStyle(config.fLineStyle);
    lineSet->SetLineWidth(config.fLineWidth);

    return lineSet;
}

struct HitsVisualConfiguration {
    Color_t fColor = kBlack;
    Style_t fMarkerStyle = 1;
    Size_t fMarkerSize = 1.0;
};

HitsVisualConfiguration GetHitsVisualConfiguration(const std::string& processName,
                                                   const std::string& processType) {
    auto config = HitsVisualConfiguration();

    // By process type (Geant4 type name), then by process name
    if (processType == "Electromagnetic") {
        config.fColor = kRed;
    }

    if (processName == "Transportation") {
        config.fColor = kWhite;
    } else if (processName == "Init") {
        config.fColor = kWhite;
    } else if (processName == "hadElastic") {
        config.fColor = kOrange;
    } else if (processName == "neutronInelastic") {
        config.fColor = kGreen;
        config.fMarkerStyle = 4;
    } else if (processName == "nCapture") {
        config.fColor = kBlue;
        config.fMarkerStyle = 2;
        config.fMarkerSize = 4.0;
    }

    return config;
}

void TRestGeant4EventViewer::AddEvent(TRestEvent* event) {
    DeleteCurrentEvent();

    if (!event) {
        RESTError << "TRestGeant4EventViewer::AddEvent, received event is NULL!!!" << RESTendl;
        return;
    }

    fG4Event = static_cast<TRestGeant4Event*>(event);
    fEvent = fG4Event;
    fG4Metadata = fG4Event->GetGeant4Metadata();

    if (fG4Metadata == nullptr) {
        RESTError << "TRestGeant4EventViewer::AddEvent. No TRestGeant4Metadata found." << RESTendl;
        return;
    }

    const int nTracks = fG4Event->GetNumberOfTracks();

    size_t trackCounter = 0;
    size_t hitsCounter = 0;

    map<int, TEveStraightLineSet*> linesSet;
    map<std::string, TEvePointSet*> hitsPoints;
    map<std::string, TEveElementList*> hitsType;

    auto trackList = new TEveElementList("Tracks");
    gEve->AddElement(trackList);

    auto hitsList = new TEveElementList("Hits");
    gEve->AddElement(hitsList);

    const auto& physicsInfo = fG4Metadata->GetGeant4PhysicsInfo();

    for (int i = 0; i < nTracks; ++i) {
        const auto track = fG4Event->GetTrack(i);

        if (track.GetInitialEnergy() < 1.0 || track.GetLength() < 0.1) {
            continue;
        }

        auto line = GetTrackEveDrawable(track, fGeomScale);
        linesSet[track.GetTrackID()] = line;

        TEveElement* parentLine = trackList;

        if (linesSet.count(track.GetParentID())) {
            parentLine = linesSet.at(track.GetParentID());
        }

        gEve->AddElement(line, parentLine);
        trackCounter++;

        const size_t nTrackHits = track.GetNumberOfHits();

        for (size_t j = 0; j < nTrackHits; ++j) {
            const auto processName = physicsInfo.GetProcessName(track.GetHitProcess(j));

            const auto processType = physicsInfo.GetProcessType(processName);

            const XYZVector position = track.GetHitPosition(j);

            if (hitsType.count(processType) == 0) {
                hitsType[processType] = new TEveElementList(processType.c_str());

                gEve->AddElement(hitsType[processType], hitsList);
            }

            if (hitsPoints.count(processName) == 0) {
                hitsPoints[processName] = new TEvePointSet(processName.c_str());

                auto hitPoints = hitsPoints.at(processName);

                auto hitsVisualConfig = GetHitsVisualConfiguration(processName, processType);

                hitPoints->SetMarkerColor(hitsVisualConfig.fColor);
                hitPoints->SetMarkerStyle(hitsVisualConfig.fMarkerStyle);
                hitPoints->SetMarkerSize(hitsVisualConfig.fMarkerSize);

                gEve->AddElement(hitPoints, hitsType[processType]);
            }

            hitsPoints.at(processName)
                ->SetNextPoint(fGeomScale * position.X(), fGeomScale * position.Y(),
                               fGeomScale * position.Z());

            hitsCounter++;
        }
    }

    if (nTracks > 0) {
        const auto firstTrack = fG4Event->GetTrack(0);

        const XYZVector position = firstTrack.GetInitialPosition();

        const std::string subID =
            fG4Event->GetSubID() > 0 ? TRestTools::Format(" (SubID: %d)", fG4Event->GetSubID()) : "";
        AddText(
            TRestTools::Format("Event ID: %d%s | Primary origin: (%4.2lf, %4.2lf, %4.2lf) mm",
                               fG4Event->GetID(), subID.c_str(), position.X(), position.Y(), position.Z()),
            position);
    } else {
        AddText(TRestTools::Format("Event ID: %d | No tracks found", fG4Event->GetID()), XYZVector(0, 0, 0));
    }

    Update();
}

void TRestGeant4EventViewer::AddText(const std::string& text, XYZVector at) {
    TEveText* evText = new TEveText(text.c_str());

    evText->SetName("Event title");
    evText->SetFontSize(12);

    evText->RefMainTrans().SetPos((at.X() + 15) * fGeomScale, (at.Y() + 15) * fGeomScale,
                                  (at.Z() + 15) * fGeomScale);

    gEve->AddElement(evText);
}

void TRestGeant4EventViewer::AddMarker(int trkID, XYZVector at, const std::string& name) {
    auto* line = GetHitConnector(trkID);
    if (line == nullptr) return;

    TEvePointSet* marker = new TEvePointSet(1);

    marker->SetName(name.c_str());
    marker->SetMarkerColor(kMagenta);
    marker->SetMarkerStyle(3);

    marker->SetPoint(0, at.X() * fGeomScale, at.Y() * fGeomScale, at.Z() * fGeomScale);

    marker->SetMarkerSize(0.4);

    line->AddElement(marker);
}

void TRestGeant4EventViewer::NextTrackVertex(int trkID, XYZVector to) {
    auto* line = GetHitConnector(trkID);
    if (line == nullptr) return;
    line->SetNextPoint(to.X() * fGeomScale, to.Y() * fGeomScale, to.Z() * fGeomScale);
}

void TRestGeant4EventViewer::AddTrack(int trkID, int parentID, XYZVector from, const std::string& name) {
    TEveLine* evLine = new TEveLine();

    evLine->SetName(name.c_str());
    if (fHitConnectors.count(trkID) > 0) {
        RESTWarning << "TRestGeant4EventViewer::AddTrack: track ID " << trkID << " added twice" << RESTendl;
    }
    fHitConnectors[trkID] = evLine;

    evLine->SetMainColor(kWhite);
    evLine->SetLineWidth(4);

    if (name.find("gamma") != std::string::npos) evLine->SetMainColor(kGreen);

    if (name.find("e-") != std::string::npos) evLine->SetMainColor(kRed);

    if (name.find("mu-") != std::string::npos) evLine->SetMainColor(kGray);

    if (name.find("alpha") != std::string::npos) evLine->SetMainColor(kYellow);

    if (name.find("neutron") != std::string::npos) evLine->SetMainColor(kBlue);

    evLine->SetNextPoint(from.X() * fGeomScale, from.Y() * fGeomScale, from.Z() * fGeomScale);

    // A primary (parent ID 0) or a track whose parent has no line is drawn at the top level
    const auto parent = fHitConnectors.find(parentID);
    if (parentID != 0 && parent != fHitConnectors.end() && parent->second != nullptr) {
        parent->second->AddElement(evLine);
        return;
    }
    if (parentID != 0) {
        RESTWarning << "Parent ID: " << parentID << " of track " << trkID << " was not found!" << RESTendl;

        RESTWarning << "This might be solved by enabling registerEmptyTracks in the TRestGeant4Metadata"
                    << RESTendl;
    }
    gEve->AddElement(evLine);
}

void TRestGeant4EventViewer::AddParentTrack(int trkID, XYZVector from, const std::string& name) {
    TEveLine* evLine = new TEveLine();

    evLine->SetName(name.c_str());
    fHitConnectors[trkID] = evLine;

    evLine->SetMainColor(kWhite);
    evLine->SetLineWidth(4);

    evLine->SetNextPoint(from.X() * fGeomScale, from.Y() * fGeomScale, from.Z() * fGeomScale);

    gEve->AddElement(evLine);
}
