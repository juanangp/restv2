#ifndef REST_TRACK_VIEWTRACKLINE_C
#define REST_TRACK_VIEWTRACKLINE_C

#include <TCanvas.h>
#include <TGraph.h>
#include <TH1.h>
#include <TLine.h>
#include <TList.h>
#include <TMarker.h>
#include <TMultiGraph.h>
#include <TPad.h>
#include <TPolyLine3D.h>
#include <TPolyMarker3D.h>
#include <TTree.h>
#include <TTreeFormula.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "TRestBrowser.h"
#include "TRestDetectorReadout.h"
#include "TRestDetectorReadoutManager.h"
#include "TRestDetectorSignalEvent.h"
#include "TRestEventViewer.h"
#include "TRestRawSignalEvent.h"
#include "TRestRun.h"

/// Event viewer (for TRestBrowser) of the files analysed with TRestTrackLineAnalysisProcess, for the
/// event type selected in the browser:
/// - TRestTrackEvent and TRestDetectorHitsEvent: the event (its DrawEvent with the plot option) with a
///   dashed black line from the origin (open circle) to the end (star) over every view where they are
///   defined. The origin and end are the observables OriginX/Y/Z and EndX/Y/Z of the entry (branches
///   processName_OriginX, ...: of the process given, or if none, of the first process that has them);
///   without them only the event is drawn. With an empty plot option, the hits events are drawn with
///   "markers(colz)": Z bins of the pulse samples and the 3D view as markers colored by energy
///   (TRestDetectorHitsEvent::DrawEvent).
/// - TRestRawSignalEvent and TRestDetectorSignalEvent: with an empty plot option, the signals in one pad
///   per readout axis: X strips (channels with X and no Y), Y strips (Y and no X) and pixels (X and Y),
///   from the readout of the file (TRestDetectorReadoutManager, the first readout if there are several);
///   channels not in the readout are not drawn. With a plot option, or without readout, the event
///   DrawEvent.
/// - Other events: as TRestEventViewer.
class TrackLineViewer : public TRestEventViewer {
   private:
    /// Name of the TRestTrackLineAnalysisProcess of the observables, empty: the first one in the file
    std::string fPrefix;

    /// Pad of the signals split by axis, owned by the viewer (the other pads belong to the events)
    TPad* fSignalsPad = nullptr;

    /// \brief Origin and end of the current entry from the analysis tree of the event run.
    /// \return False if there is no run, analysis tree or line observables.
    bool ReadLine(double origin[3], double end[3]) const;

    /// \brief Draws the line over the view pads under `pad` (recursively), with their axes from the
    /// histogram of each one.
    void DrawLine(TVirtualPad* pad, const double origin[3], const double end[3]) const;

    /// \brief Readout of the run of the event, nullptr if none.
    TRestDetectorReadout* GetReadout() const;

    /// \brief Draws the signals split by readout axis in fSignalsPad.
    /// \return False if the event is not a signal event or there is no readout.
    bool DrawSignals();

   public:
    explicit TrackLineViewer(const std::string& prefix = "") : fPrefix(prefix) {}

    void Plot(const char* option) override;
};

namespace {
const Style_t kOriginMarker = 24;
const Style_t kEndMarker = 29;
const double kMarkerSize = 2.5;

const char* kLineObservables[6] = {"OriginX", "OriginY", "OriginZ", "EndX", "EndY", "EndZ"};

// Branches of the line observables (processName_observable) of the process `prefix`, or if empty of the
// first process with all of them; empty if they are not in the tree
std::vector<std::string> GetLineBranches(TTree* tree, const std::string& prefix) {
    if (!prefix.empty()) {
        std::vector<std::string> names;
        for (const char* observable : kLineObservables) {
            const std::string name = prefix + "_" + observable;
            if (!tree->GetBranch(name.c_str())) return {};
            names.push_back(name);
        }
        return names;
    }
    const std::string suffix = std::string("_") + kLineObservables[0];
    TObjArray* branches = tree->GetListOfBranches();
    for (int i = 0; i < branches->GetEntries(); i++) {
        const std::string name = branches->At(i)->GetName();
        if (name.size() <= suffix.size()) continue;
        if (name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
        const std::vector<std::string> names =
            GetLineBranches(tree, name.substr(0, name.size() - suffix.size()));
        if (!names.empty()) return names;
    }
    return {};
}

// Axis (0, 1, 2) from a title starting with X, Y or Z (TRestHitsData::kAxisTitles), -1 otherwise (energy)
int GetAxis(const char* title) {
    if (!title) return -1;
    switch (title[0]) {
        case 'X':
            return 0;
        case 'Y':
            return 1;
        case 'Z':
            return 2;
        default:
            return -1;
    }
}

template <typename T>
void SetLineStyle(T* line) {
    line->SetLineColor(kBlack);
    line->SetLineStyle(2);
    line->SetLineWidth(2);
    line->SetBit(TObject::kCanDelete);
}

template <typename T>
void SetMarkerStyle(T* marker) {
    marker->SetMarkerColor(kBlack);
    marker->SetMarkerSize(kMarkerSize);
    marker->SetBit(TObject::kCanDelete);
}

// Signals of `event` in one pad of `pad` per readout axis: X strips, Y strips and pixels (X and Y). The
// channels not in the readout (no X nor Y) are not drawn
template <typename Event>
void DrawSplitSignals(const Event& event, const TRestDetectorReadout& readout, TPad* pad, const char* xTitle,
                      const char* yTitle) {
    const char* names[3] = {"X strips", "Y strips", "pixels"};
    std::vector<int> groups[3];
    for (int n = 0; n < event.GetNumberOfSignals(); n++) {
        const auto position = readout.GetPositionFromChannel(event.GetSignal(n).GetSignalID());
        const bool x = !std::isnan(position.X()), y = !std::isnan(position.Y());
        if (!x && !y) continue;
        groups[x && !y ? 0 : y && !x ? 1 : 2].push_back(n);
    }

    int nPads = 0;
    for (const auto& group : groups) nPads += !group.empty();
    if (nPads == 0) {
        std::cout << "TrackLineViewer: no signal of the event is in the readout" << std::endl;
        return;
    }
    pad->Divide(nPads, 1);

    int padIndex = 1;
    for (int g = 0; g < 3; g++) {
        if (groups[g].empty()) continue;
        pad->cd(padIndex++);
        TRestEvent::SetPadMargins(gPad);
        auto* graphs = new TMultiGraph();
        graphs->SetTitle(Form("Event ID %d, %s", event.GetID(), names[g]));
        for (int n : groups[g]) {
            const auto& signal = event.GetSignal(n);
            auto* graph = new TGraph(signal.GetGraph());
            graph->SetLineColor((signal.GetSignalID() % 72) + 1);
            graphs->Add(graph, "L");
        }
        TRestEvent::DrawOwned(graphs, "A", xTitle, yTitle);
    }
}
}  // namespace

bool TrackLineViewer::ReadLine(double origin[3], double end[3]) const {
    TRestRun* run = fEvent ? fEvent->GetRestRun() : nullptr;
    TTree* tree = run ? run->fAnalysisTree : nullptr;
    if (!tree) return false;

    // Formulas, so the branch addresses set by others are kept
    std::vector<std::unique_ptr<TTreeFormula>> formulas;
    const std::vector<std::string> names = GetLineBranches(tree, fPrefix);
    if (names.empty()) return false;
    for (const auto& name : names) {
        formulas.push_back(std::make_unique<TTreeFormula>(name.c_str(), name.c_str(), tree));
    }
    const Long64_t entry = run->GetCurrentEntry();
    if (tree->GetEntry(entry) <= 0) return false;
    for (int i = 0; i < 3; i++) {
        origin[i] = formulas[i]->EvalInstance();
        end[i] = formulas[i + 3]->EvalInstance();
    }
    return true;
}

void TrackLineViewer::DrawLine(TVirtualPad* pad, const double origin[3], const double end[3]) const {
    TH1* frame = nullptr;
    for (auto* object : *pad->GetListOfPrimitives()) {
        if (auto* subPad = dynamic_cast<TVirtualPad*>(object)) {
            DrawLine(subPad, origin, end);
        } else if (!frame) {
            frame = dynamic_cast<TH1*>(object);
        }
    }
    if (!frame) return;

    TVirtualPad* safePad = gPad;
    pad->cd();
    if (frame->GetDimension() == 3) {
        for (int a = 0; a < 3; a++) {
            if (std::isnan(origin[a]) || std::isnan(end[a])) {
                if (safePad) safePad->cd();
                return;
            }
        }
        auto* line = new TPolyLine3D(2);
        line->SetPoint(0, origin[0], origin[1], origin[2]);
        line->SetPoint(1, end[0], end[1], end[2]);
        SetLineStyle(line);
        line->Draw();
        for (int i = 0; i < 2; i++) {
            const double* p = i == 0 ? origin : end;
            auto* marker = new TPolyMarker3D(1, i == 0 ? kOriginMarker : kEndMarker);
            marker->SetPoint(0, p[0], p[1], p[2]);
            SetMarkerStyle(marker);
            marker->Draw();
        }
    } else {
        // Views of the energy against a coordinate have no second axis
        const int u = GetAxis(frame->GetXaxis()->GetTitle());
        const int v = GetAxis(frame->GetYaxis()->GetTitle());
        if (u >= 0 && v >= 0 && !std::isnan(origin[u]) && !std::isnan(origin[v]) && !std::isnan(end[u]) &&
            !std::isnan(end[v])) {
            auto* line = new TLine(origin[u], origin[v], end[u], end[v]);
            SetLineStyle(line);
            line->Draw();
            auto* originMarker = new TMarker(origin[u], origin[v], kOriginMarker);
            SetMarkerStyle(originMarker);
            originMarker->Draw();
            auto* endMarker = new TMarker(end[u], end[v], kEndMarker);
            SetMarkerStyle(endMarker);
            endMarker->Draw();
        }
    }
    pad->Modified();
    if (safePad) safePad->cd();
}

TRestDetectorReadout* TrackLineViewer::GetReadout() const {
    TRestRun* run = fEvent ? fEvent->GetRestRun() : nullptr;
    auto* manager =
        run ? dynamic_cast<TRestDetectorReadoutManager*>(run->GetMetadataClass("TRestDetectorReadoutManager"))
            : nullptr;
    if (!manager || manager->fReadoutMap.empty()) return nullptr;
    if (manager->fReadoutMap.size() > 1) {
        std::cout << "TrackLineViewer: several readouts, using " << manager->fReadoutMap.begin()->first
                  << std::endl;
    }
    return manager->fReadoutMap.begin()->second;
}

bool TrackLineViewer::DrawSignals() {
    const auto* rawEvent = dynamic_cast<const TRestRawSignalEvent*>(fEvent);
    const auto* signalEvent = dynamic_cast<const TRestDetectorSignalEvent*>(fEvent);
    if (!rawEvent && !signalEvent) return false;
    if ((rawEvent ? rawEvent->GetNumberOfSignals() : signalEvent->GetNumberOfSignals()) == 0) return false;
    const TRestDetectorReadout* readout = GetReadout();
    if (!readout) {
        std::cout << "TrackLineViewer: no readout in the file, the signals are not split by axis"
                  << std::endl;
        return false;
    }

    fCanvas->cd();
    fSignalsPad = new TPad("signalsPad", "Signals", 0, 0, 1, 1);
    fSignalsPad->Draw();
    if (rawEvent) {
        DrawSplitSignals(*rawEvent, *readout, fSignalsPad, "Time Bin", "ADC Channels");
    } else {
        DrawSplitSignals(*signalEvent, *readout, fSignalsPad, "Time", "Amplitude / Charge");
    }
    fSignalsPad->Modified();
    return true;
}

void TrackLineViewer::Plot(const char* option) {
    // Only this plot in the canvas: the pads of the events are removed, not deleted (each event owns its
    // pad), the signals pad is deleted
    fCanvas->cd();
    fCanvas->GetListOfPrimitives()->Clear("nodelete");
    delete fSignalsPad;
    fSignalsPad = nullptr;
    fPad = nullptr;
    if (!fEvent) {
        fCanvas->Update();
        return;
    }

    const std::string drawOption = option ? option : "";
    if (drawOption.empty() && DrawSignals()) {
        fCanvas->cd();
        fCanvas->Modified();
        fCanvas->Update();
        return;
    }

    const std::string className = fEvent->GetClassName();
    const std::string eventOption =
        drawOption.empty() && className == "TRestDetectorHitsEvent" ? "markers(colz)" : drawOption;
    fCanvas->cd();
    fPad = fEvent->DrawEvent(eventOption.c_str());
    if (!fPad) {
        fCanvas->Update();
        return;
    }
    // Some events draw their pad in DrawEvent, others only build it
    fCanvas->cd();
    if (!fCanvas->GetListOfPrimitives()->FindObject(fPad)) fPad->Draw();

    if (className == "TRestTrackEvent" || className == "TRestDetectorHitsEvent") {
        double origin[3], end[3];
        if (ReadLine(origin, end)) {
            DrawLine(fPad, origin, end);
            std::cout << "Line origin (" << origin[0] << ", " << origin[1] << ", " << origin[2] << "), end ("
                      << end[0] << ", " << end[1] << ", " << end[2] << ")" << std::endl;
        } else {
            std::cout << "TrackLineViewer: no line observables (TRestTrackLineAnalysisProcess) for this "
                         "entry, drawing only the event"
                      << std::endl;
        }
    }
    fPad->Modified();
    fCanvas->cd();
    fCanvas->Modified();
    fCanvas->Update();
}

/// Browses the events of a file analysed with TRestTrackLineAnalysisProcess (TRestBrowser: entry,
/// event ID, event type, selection with observables, previous/next event) with TrackLineViewer: the
/// track and hits events with the origin and end over them, the raw and detector signal events split
/// in X and Y strips (readout of the file). The event type is chosen in the browser, at start the last
/// saved event type of the file (TRestRun::GetOutputEvent, e.g. TRestTrackEvent). `prefix` is the name
/// of the TRestTrackLineAnalysisProcess whose observables are drawn (branches prefix_OriginX, ...); if
/// empty, the first process of the file with them.
/// The plot option is the DrawEvent option of the event (for signals, any option draws them without
/// splitting).
///
///    restRoot
///    [0] REST_Track_ViewTrackLine("R01234_LINEAR.root")
///    [1] REST_Track_ViewTrackLine("R01234_LINEAR.root", "trackLineAnalysis")
///
/// e.g. selection "trackLineAnalysis_Energy > 100000 && trackLineAnalysis_TrackBalance > 0.95" to go only
/// through those events (branches processName_observable).
int REST_Track_ViewTrackLine(const std::string& fileName, const std::string& prefix = "") {
    auto* browser = new TRestBrowser(new TrackLineViewer(prefix));
    browser->OpenFile(fileName);
    return 0;
}
#endif
