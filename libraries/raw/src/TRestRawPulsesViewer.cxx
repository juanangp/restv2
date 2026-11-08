#include "TRestRawPulsesViewer.h"

#include <TApplication.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TGButton.h>
#include <TGClient.h>
#include <TGFileDialog.h>
#include <TGLabel.h>
#include <TGTextEntry.h>
#include <TGraph.h>
#include <TMultiGraph.h>
#include <TRootEmbeddedCanvas.h>
#include <TSystem.h>
#include <TTree.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>

namespace {
/// Channels of an ASIC, and ASICs of a card, in the signal ID
constexpr int kChannelsPerASIC = 72;
constexpr int kASICsPerCard = 4;
}  // namespace

TRestRawPulsesViewer::TRestRawPulsesViewer() : TGMainFrame(gClient->GetRoot(), 1200, 900) {
    auto hFrame = new TGHorizontalFrame(this);
    auto vFrame = new TGVerticalFrame(hFrame, 150, 900, kFixedWidth);
    auto buttonLayout = new TGLayoutHints(kLHintsExpandX, 5, 5, 5, 5);

    auto btnOpen = new TGTextButton(vFrame, "&Open File");
    btnOpen->Connect("Clicked()", "TRestRawPulsesViewer", this, "OpenFile()");
    vFrame->AddFrame(btnOpen, buttonLayout);

    auto btnPrev = new TGTextButton(vFrame, "&Prev Event");
    btnPrev->Connect("Clicked()", "TRestRawPulsesViewer", this, "Prev()");
    vFrame->AddFrame(btnPrev, buttonLayout);

    auto btnNext = new TGTextButton(vFrame, "&Next Event");
    btnNext->Connect("Clicked()", "TRestRawPulsesViewer", this, "Next()");
    vFrame->AddFrame(btnNext, buttonLayout);

    vFrame->AddFrame(new TGLabel(vFrame, "Jump to Entry:"), new TGLayoutHints(kLHintsCenterX, 5, 5, 10, 0));
    fEntryInput = new TGTextEntry(vFrame);
    fEntryInput->Connect("ReturnPressed()", "TRestRawPulsesViewer", this, "JumpToEvent()");
    vFrame->AddFrame(fEntryInput, new TGLayoutHints(kLHintsExpandX, 5, 5, 2, 5));

    fInfoLabel = new TGLabel(vFrame, "No data");
    vFrame->AddFrame(fInfoLabel, new TGLayoutHints(kLHintsCenterX, 5, 5, 20, 5));

    auto btnSave = new TGTextButton(vFrame, "&Save PDF");
    btnSave->Connect("Clicked()", "TRestRawPulsesViewer", this, "SavePDF()");
    vFrame->AddFrame(btnSave, new TGLayoutHints(kLHintsExpandX, 5, 5, 40, 5));

    auto btnQuit = new TGTextButton(vFrame, "&Quit");
    btnQuit->Connect("Clicked()", "TRestRawPulsesViewer", this, "DoExit()");
    vFrame->AddFrame(btnQuit, buttonLayout);

    hFrame->AddFrame(vFrame, new TGLayoutHints(kLHintsLeft | kLHintsExpandY, 5, 5, 5, 5));

    fCanvas = new TRootEmbeddedCanvas("PulsesCanvas", hFrame, 1000, 850);
    hFrame->AddFrame(fCanvas, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 5, 5, 5, 5));
    AddFrame(hFrame, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    SetWindowName("REST Pulses Viewer");
    MapSubwindows();
    Resize(GetDefaultSize());
    MapWindow();

    Connect("CloseWindow()", "TRestRawPulsesViewer", this, "DoExit()");
    DontCallClose();
}

TRestRawPulsesViewer::~TRestRawPulsesViewer() {
    DeleteGraphs();
    CloseFile();
}

void TRestRawPulsesViewer::CloseFile() {
    delete fReaderSignalsID;
    fReaderSignalsID = nullptr;
    delete fReaderSamples;
    fReaderSamples = nullptr;
    delete fReaderOffsets;
    fReaderOffsets = nullptr;
    delete fReaderEventID;
    fReaderEventID = nullptr;
    delete fReader;
    fReader = nullptr;
    fTree = nullptr;  // Owned by the file
    delete fFile;
    fFile = nullptr;
}

void TRestRawPulsesViewer::DeleteGraphs() {
    for (auto graph : fGraphs) delete graph;  // With its graphs
    fGraphs.clear();
}

void TRestRawPulsesViewer::LoadFile(const char* path) {
    CloseFile();
    fFile = TFile::Open(path, "READ");
    if (fFile && !fFile->IsZombie()) fFile->GetObject("TRestRawSignalEvent", fTree);
    if (!fTree) {
        std::cout << "TRestRawPulsesViewer: no TRestRawSignalEvent tree in " << path << std::endl;
        CloseFile();
        fInfoLabel->SetText("No data");
        return;
    }
    // No read cache: its baskets would not follow the refreshes of a file being written
    fTree->SetCacheSize(0);

    fReader = new TTreeReader(fTree);
    fReaderSignalsID = new TTreeReaderValue<std::vector<int>>(*fReader, "fSigIDs");
    fReaderSamples = new TTreeReaderValue<std::vector<short>>(*fReader, "fSigSamples");
    fReaderOffsets = new TTreeReaderValue<std::vector<int>>(*fReader, "fSigOffsets");
    fReaderEventID = new TTreeReaderValue<int>(*fReader, "eventID");

    fFileName = path;
    fEntry = 0;
    SetWindowName(gSystem->BaseName(path));
    DrawEvent();
}

Long64_t TRestRawPulsesViewer::RefreshEntries() {
    if (!fTree) return 0;
    if (fEntry >= fTree->GetEntries() - 1) fTree->Refresh();
    return fTree->GetEntries();
}

void TRestRawPulsesViewer::OpenFile() {
    TGFileInfo fileInfo;
    const char* fileTypes[] = {"ROOT", "*.root", nullptr, nullptr};
    fileInfo.fFileTypes = fileTypes;
    new TGFileDialog(gClient->GetRoot(), this, kFDOpen, &fileInfo);
    if (fileInfo.fFilename) LoadFile(fileInfo.fFilename);
}

void TRestRawPulsesViewer::Next() {
    if (fTree && fEntry + 1 < RefreshEntries()) {
        fEntry++;
        DrawEvent();
    }
}

void TRestRawPulsesViewer::Prev() {
    if (fTree && fEntry > 0) {
        fEntry--;
        DrawEvent();
    }
}

void TRestRawPulsesViewer::JumpToEvent() {
    if (!fTree) return;
    const Long64_t entry = std::atoll(fEntryInput->GetText());
    if (entry >= 0 && entry < fTree->GetEntries()) {
        fEntry = entry;
        DrawEvent();
    } else {
        fEntryInput->SetText(Form("%lld", fEntry));  // Not valid: the current one
    }
}

void TRestRawPulsesViewer::SavePDF() {
    if (fTree) fCanvas->GetCanvas()->SaveAs(Form("Event_%lld.pdf", fEntry));
}

void TRestRawPulsesViewer::DoExit() { gApplication->Terminate(0); }

void TRestRawPulsesViewer::DrawEvent() {
    if (!fReader || fReader->SetEntry(fEntry) != TTreeReader::kEntryValid) return;

    const int eventID = **fReaderEventID;
    const std::vector<int>& signalIDs = **fReaderSignalsID;
    const std::vector<short>& samples = **fReaderSamples;
    const std::vector<int>& offsets = **fReaderOffsets;

    // The signals of each ASIC (signal ID / 72) of the event
    std::map<int, TMultiGraph*> asics;
    for (size_t s = 0; s < signalIDs.size() && s < offsets.size(); s++) {
        const int id = signalIDs[s];
        // Samples of the signal s: from its offset to the next one
        const int first = offsets[s];
        const int last = s + 1 < offsets.size() ? offsets[s + 1] : static_cast<int>(samples.size());
        if (first < 0 || last > static_cast<int>(samples.size()) || first >= last) continue;

        auto graph = new TGraph(last - first);
        for (int p = first; p < last; p++) graph->SetPoint(p - first, p - first, samples[p]);
        graph->SetLineColor(id % kChannelsPerASIC + 1);

        const int asic = id / kChannelsPerASIC;
        auto& multiGraph = asics[asic];
        if (!multiGraph) {
            multiGraph = new TMultiGraph();
            multiGraph->SetTitle(Form("Card %d ASIC %d, event %d;Time bin;ADC", asic / kASICsPerCard,
                                      asic % kASICsPerCard, eventID));
        }
        multiGraph->Add(graph, "L");
    }

    // The previous event is removed from the canvas before its graphs are deleted
    TCanvas* canvas = fCanvas->GetCanvas();
    canvas->Clear();
    DeleteGraphs();

    const int nPads = asics.size();
    if (nPads > 0) {
        const int nColumns = std::ceil(std::sqrt(nPads));
        const int nRows = (nPads + nColumns - 1) / nColumns;
        canvas->Divide(nColumns, nRows);
        int pad = 1;
        for (const auto& [asic, multiGraph] : asics) {
            canvas->cd(pad++);
            multiGraph->Draw("A");
            fGraphs.push_back(multiGraph);
        }
    }
    canvas->Update();

    fInfoLabel->SetText(Form("Entry %lld / %lld, event %d", fEntry, fTree->GetEntries() - 1, eventID));
    fInfoLabel->Resize(fInfoLabel->GetDefaultSize());
    fEntryInput->SetText(Form("%lld", fEntry));
}
