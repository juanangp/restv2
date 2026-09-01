//////////////////////////////////////////////////////////////////////////
///
///
/// This class opens input file with TRestRun and shows the plot of each event
/// The plot is shown through TRestEventViewer interface on the right. On the left
/// there is a control bar to switch the events. Plot options can also be given.
///
/// \class TRestBrowser
///
///--------------------------------------------------------------------------
///
/// RESTsoft - Software for Rare Event Searches with TPCs
///
/// History of developments:
///
/// 2014-june: First concept. As part of conceptualization of previous REST
///            code (REST v2)
///            Igor G. Irastorza
///
/// 2017-Aug:  Major change: added for multi-thread capability
///            Kaixiang Ni
///
/// <hr>
//////////////////////////////////////////////////////////////////////////

#include "TRestBrowser.h"
#include <TRestEventViewer.h>
#include <TRestRun.h>
#include <TRestTools.h>

#include <TTreeFormula.h>

using namespace std;
using namespace TRestTools;

TRestBrowser::TRestBrowser() {
    if ((TDirectory*)gDirectory != nullptr && gDirectory->GetFile() != nullptr) {
        Initialize();
        SetViewer("TRestEventViewer");
        OpenFile(gDirectory->GetFile()->GetName());
        cout << "Loaded File : " << fInputFileName << endl;
    } else {
        fBrowser = new TBrowser("Browser", nullptr, "REST Browser");
        fRestRun = new TRestRun();
    }
}

TRestBrowser::TRestBrowser(const string& viewerName, Double_t geomScale) {
    Initialize("I");
    SetViewer(viewerName, geomScale);
}

TRestBrowser::~TRestBrowser() {
    if (frmMain != nullptr) frmMain->Cleanup();
    // delete frmMain;
}

void TRestBrowser::Initialize(const string& opt) {
    pureAnalysis = kFALSE;

    fRestRun = new TRestRun();

    fBrowser = new TBrowser("Browser", 0, "REST Browser", opt.c_str());
    TGMainFrame* fr = fBrowser->GetBrowserImp()->GetMainFrame();
    if (fr == nullptr) {
        RESTWarning << "No x11 interface is available. Cannot call the browser window!" << RESTendl;
        exit(1);
    }
    fr->DontCallClose();

    fBrowser->StartEmbedding(0, -1);
    frmMain = new TGMainFrame(gClient->GetRoot(), 300);
    frmMain->SetCleanup(kDeepCleanup);
    frmMain->SetWindowName("Controller1");
    SetLeftPanelButtons();
    fBrowser->StopEmbedding();

    fBrowser->StartEmbedding(1, -1);
    fCanDefault = new TCanvas();
    fBrowser->StopEmbedding();

    fBrowser->StartEmbedding(2, -1);
    frmBot = new TGMainFrame(gClient->GetRoot(), 300);
    frmBot->SetCleanup(kDeepCleanup);
    frmBot->SetWindowName("Controller2");
    SetBottomPanelButtons();
    fBrowser->StopEmbedding();

    //// frmMain->DontCallClose();
    // frmMain->MapSubwindows();
    //// frmMain->Resize();
    // frmMain->Layout();
    // frmMain->MapWindow();
}

void TRestBrowser::SetViewer(TRestEventViewer* eV, Double_t geomScale) {
    if (fEventViewer != nullptr) {
        cout << "Event viewer has already been set!" << endl;
        return;
    }
    if (eV != nullptr) {
        fEventViewer = eV;
        fEventViewer->SetGeomScale(geomScale);
        // b->StartEmbedding(1, -1);
        eV->Embed(fBrowser);
        // b->StopEmbedding();
    }
}

void TRestBrowser::SetViewer(const std::string& viewerName, Double_t geomScale) {
    if (CountString((string)viewerName, "Viewer") > 0) {
        TClass* cl = TClass::GetClass(viewerName.c_str());
          if (!cl) {
            throw std::runtime_error("Error: La clase '" + viewerName + "' no está registrada en ROOT.");
          }

        std::unique_ptr<TRestEventViewer> viewer(static_cast<TRestEventViewer*>(cl->New()));
        viewer->SetGeomScale(geomScale);
        if (viewer != nullptr) {
            SetViewer(viewer.release(), geomScale);
        } else {
            RESTError << viewerName << " not recognized! Did you install the corresponding library?"
                      << RESTendl;
            RESTError << "Also check EVE feature is turned on in REST for 3d event viewing." << RESTendl;
            RESTWarning << "Using default event viewer" << RESTendl;
        }
    } else {
        cout << "illegal viewer : " << viewerName << endl;
        exit(0);
    }
}

void TRestBrowser::SetLeftPanelButtons() {
    fVFrame = new TGVerticalFrame(frmMain);
    fVFrame->Resize(300, 200);

    // row in the tree
    fEventRowLabel = new TGLabel(fVFrame, "Entry:");
    fVFrame->AddFrame(fEventRowLabel, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    fEventRowNumberBox = new TGNumberEntry(fVFrame, fEventRow);
    fEventRowNumberBox->Connect("ValueSet(Long_t)", "TRestBrowser", this, "RowValueChangedAction(Long_t)");
    fVFrame->AddFrame(fEventRowNumberBox, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    // event id and sub event id
    auto labelBar = new TGHorizontalFrame(fVFrame);
    {
        fEventIdLabel = new TGLabel(labelBar, "Event ID:");
        labelBar->AddFrame(fEventIdLabel, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

        fEventSubIdLabel = new TGLabel(labelBar, "Sub ID:");
        labelBar->AddFrame(fEventSubIdLabel, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
    }
    fVFrame->AddFrame(labelBar, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    auto numberBoxBar = new TGHorizontalFrame(fVFrame);
    {
        fEventIdNumberBox = new TGNumberEntry(numberBoxBar, fEventId);
        fEventIdNumberBox->Connect("ValueSet(Long_t)", "TRestBrowser", this, "IdValueChangedAction(Long_t)");
        numberBoxBar->AddFrame(fEventIdNumberBox, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

        fEventSubIdNumberBox = new TGNumberEntry(numberBoxBar, fEventSubId);
        fEventSubIdNumberBox->Connect("ValueSet(Long_t)", "TRestBrowser", this,
                                      "IdValueChangedAction(Long_t)");
        numberBoxBar->AddFrame(fEventSubIdNumberBox, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
    }
    fVFrame->AddFrame(numberBoxBar, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    // event type to choose
    fEventTypeLabel = new TGLabel(fVFrame, "Event Type:");
    fVFrame->AddFrame(fEventTypeLabel, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    fEventTypeComboBox = new TGComboBox(fVFrame);
    fEventTypeComboBox->Connect("Selected(Int_t)", "TRestBrowser", this, "EventTypeChangedAction(Int_t)");
    fVFrame->AddFrame(fEventTypeComboBox, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    // plot option buttons
    fPlotOptionLabel = new TGLabel(fVFrame, "Plot Options:");
    fVFrame->AddFrame(fPlotOptionLabel, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    fPlotOptionTextBox = new TGTextEntry(fVFrame, "");
    fPlotOptionTextBox->SetText("");
    fPlotOptionTextBox->Connect("ReturnPressed()", "TRestBrowser", this, "PlotAction()");
    fVFrame->AddFrame(fPlotOptionTextBox, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    auto switchButtonBar = new TGHorizontalFrame(fVFrame);
    {
        fButOptPrev = new TGPictureButton(switchButtonBar, gClient->GetPicture("bld_undo.png"));
        fButOptPrev->Connect("Clicked()", "TRestBrowser", this, "PreviousPlotOptionAction()");
        switchButtonBar->AddFrame(fButOptPrev, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

        fButOptRefresh = new TGTextButton(switchButtonBar, "Plot");
        fButOptRefresh->Connect("Clicked()", "TRestBrowser", this, "PlotAction()");
        switchButtonBar->AddFrame(fButOptRefresh, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

        fButOptNext = new TGPictureButton(switchButtonBar, gClient->GetPicture("bld_redo.png"));

        fButOptNext->Connect("Clicked()", "TRestBrowser", this, "NextPlotOptionAction()");
        switchButtonBar->AddFrame(fButOptNext, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
    }
    fVFrame->AddFrame(switchButtonBar, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    // tool buttons
    fMenuOpen = new TGPictureButton(fVFrame, gClient->GetPicture("bld_open.png"));
    fMenuOpen->Connect("Clicked()", "TRestBrowser", this, "LoadFileAction()");
    fVFrame->AddFrame(fMenuOpen, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    fExit = new TGPictureButton(fVFrame, gClient->GetPicture("bld_exit.png"));

    fExit->Connect("Clicked()", "TRestBrowser", this, "ExitAction()");
    fVFrame->AddFrame(fExit, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    frmMain->Resize(TGDimension(300, frmMain->GetHeight() + fVFrame->GetHeight()));

    frmMain->AddFrame(fVFrame, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
    // frmMain->DontCallClose();
    frmMain->MapSubwindows();
    // frmMain->Resize();
    frmMain->Layout();
    frmMain->MapWindow();
}

void TRestBrowser::SetBottomPanelButtons() {
    fHFrame = new TGVerticalFrame(frmBot);
    fHFrame->Resize(300, 100);

    fSelectionTextBoxLabel = new TGLabel(fHFrame, "Selection:");
    fHFrame->AddFrame(fSelectionTextBoxLabel, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    fSelectionTextBox = new TGTextEntry(fHFrame, "");
    fHFrame->AddFrame(fSelectionTextBox, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    auto bottomBar = new TGHorizontalFrame(fHFrame);
    {
        fButEveNext = new TGTextButton(bottomBar, "Previous Event");  ///< Exit button
        fButEveNext->Connect("Clicked()", "TRestBrowser", this, "PreviousEventAction()");
        bottomBar->AddFrame(fButEveNext, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

        fButEvePrev = new TGTextButton(bottomBar, "Next Event");  ///< Exit button
        fButEvePrev->Connect("Clicked()", "TRestBrowser", this, "NextEventAction()");
        bottomBar->AddFrame(fButEvePrev, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
    }
    fHFrame->AddFrame(bottomBar, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    frmBot->AddFrame(fHFrame, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));
    frmBot->MapSubwindows();
    frmBot->Layout();
    frmBot->MapWindow();
}

void TRestBrowser::InitFromConfigFile() { cout << __PRETTY_FUNCTION__ << endl; }


Bool_t TRestBrowser::LoadEventEntry(Int_t n) {
    if (fRestRun->fInputFile == nullptr || fRestRun->fInputFile->IsZombie()) {
        RESTWarning << "TRestBrowser::LoadEventEntry. No File..." << RESTendl;
        return kFALSE;
    }
    if (pureAnalysis) {
        RESTWarning << "TRestBrowser::LoadEventEntry. This is a pure analysis file..." << RESTendl;
        return kFALSE;
    }

    if (fRestRun->fAnalysisTree != nullptr && n < fRestRun->fAnalysisTree->GetEntries() && n >= 0) {
        fRestRun->GetEntry(n);
        TRestEvent* ev = fRestRun->GetInputEvent();
        if (!ev) {
            RESTError << "internal error!" << RESTendl;
            return kFALSE;
        } else {
            fEventRow = fRestRun->GetCurrentEntry();
            fEventId = ev->GetID();
            fEventSubId = ev->GetSubID();

            fEventRowNumberBox->SetIntNumber(fEventRow);
            fEventIdNumberBox->SetIntNumber(fEventId);
            fEventSubIdNumberBox->SetIntNumber(fEventSubId);
            fRestRun->PrintObservables();
        }
    } else {
        RESTWarning << "TRestBrowser::LoadEventEntry. Event out of limits" << RESTendl;
        return kFALSE;
    }

    if (fEventViewer != nullptr) {
        TRestEvent* inputEvent = (fRestRun->GetInputEvent());
        inputEvent->SetRestRun(fRestRun);
        fEventViewer->AddEvent(inputEvent);
        fEventViewer->Plot(fPlotOptionTextBox->GetText());
        cout << endl;
    }

    fCanDefault->cd();
    return kTRUE;
}

Bool_t TRestBrowser::LoadEventId(Int_t eventID, Int_t subEventID) {
    if (fRestRun->fInputFile == nullptr || fRestRun->fInputFile->IsZombie()) {
        RESTWarning << "TRestBrowser::LoadEventEntry. No File..." << RESTendl;
        return kFALSE;
    }
    if (pureAnalysis) {
        cout << "" << endl;
        RESTWarning << "TRestBrowser::LoadEventEntry. This is a pure analysis file..." << RESTendl;
        return kFALSE;
    }

    if (fRestRun->fAnalysisTree != nullptr && fRestRun->GetEntries() > 0) {
        auto entry = fRestRun->GetEntryWithID(eventID, subEventID);
        if (entry < 0) {
            RESTWarning << "Event ID : " << eventID << " with sub ID : " << subEventID << " not found!"
                        << RESTendl;
            return kFALSE;
        } else {
            fEventRow = entry;
            fEventId = eventID;
            fEventSubId = subEventID;

            fEventRowNumberBox->SetIntNumber(fEventRow);
            fEventIdNumberBox->SetIntNumber(fEventId);
            fEventSubIdNumberBox->SetIntNumber(fEventSubId);
            fRestRun->PrintObservables();
        }
    } else {
        RESTWarning << "TRestBrowser::LoadEventEntry. Event out of limits" << RESTendl;
        return kFALSE;
    }

    if (fEventViewer != nullptr) {
        TRestEvent* inputEvent = (fRestRun->GetInputEvent());
        inputEvent->SetRestRun(fRestRun);
        fEventViewer->AddEvent(inputEvent);
        fEventViewer->Plot(fPlotOptionTextBox->GetText());
        cout << endl;
    }

    fCanDefault->cd();
    return kTRUE;
}

Bool_t TRestBrowser::OpenFile(const std::string& filename) {
    if (filename.find("http") != std::string::npos || TRestTools::fileExists(filename)) {
        fInputFileName = filename;

        fRestRun->OpenInputFile(fInputFileName);
        fRestRun->fInputFile->cd();

        TGeoManager* geometry = gGeoManager;

        auto eventMap = fRestRun->GetInputEventMap();
        if (!eventMap.empty()) {
            for (auto& [eventType, eventObj] : eventMap){
              fEventTypeComboBox->AddEntry(eventType.c_str(), fEventTypeComboBox->GetNumberOfEntries());
              fEventTypeComboBox->Select(fEventTypeComboBox->GetNumberOfEntries() - 1, false);
            }

            // init viewer
            pureAnalysis = kFALSE;
            if (fEventViewer == nullptr) SetViewer("TRestEventViewer");
            if (geometry != nullptr && fEventViewer != nullptr) fEventViewer->SetGeometry(geometry);
            RowValueChangedAction(0);
        } else {
            pureAnalysis = kTRUE;
        }

        TRestEvent* ev = fRestRun->GetInputEvent();
        if (!ev) {
            RESTError << "internal error!" << RESTendl;
        } else {
            fEventRowNumberBox->SetIntNumber(fRestRun->GetCurrentEntry());
            fEventIdNumberBox->SetIntNumber(ev->GetID());
            fEventSubIdNumberBox->SetIntNumber(ev->GetSubID());
        }
        return true;
    } else {
        RESTError << "file: " << filename << " does not exist!" << RESTendl;
    }
    return false;
}

void TRestBrowser::SetInputEvent(const std::string &eventType) {
    if (fRestRun != nullptr) {
        fRestRun->SetInputEvent(eventType);
    }
}


void TRestBrowser::NextPlotOptionAction() {
    string text = fPlotOptionTextBox->GetText();
    if (text.empty()) {
        text = "0";
    } else if (isANumber(text)) {
        text = std::to_string(StringToInteger(text) + 1);
    }

    fPlotOptionTextBox->SetText(text.c_str());
    PlotAction();
}

void TRestBrowser::PreviousPlotOptionAction() {
    string text = fPlotOptionTextBox->GetText();
    if (text.empty()) {
        text = "0";
    } else if (isANumber(text)) {
        text = std::to_string(StringToInteger(text) - 1);
    }

    fPlotOptionTextBox->SetText(text.c_str());
    PlotAction();
}

void TRestBrowser::PlotAction() {
    if (fEventViewer != nullptr) {
        fEventViewer->Plot(fPlotOptionTextBox->GetText());
    }
}

void TRestBrowser::RowValueChangedAction(Long_t val) {
    int eventRow = fEventRow;
    fEventRow = (Int_t)fEventRowNumberBox->GetNumber();

    RESTDebug << "TRestBrowser::LoadEventAction. Entry:" << fEventRow << RESTendl;

    bool success = LoadEventEntry(fEventRow);

    if (!success) {
        fEventRow = eventRow;
        fEventRowNumberBox->SetIntNumber(fEventRow);
    }
}

void TRestBrowser::EventTypeChangedAction(Int_t id) {
    string eventType = fEventTypeComboBox->GetSelectedEntry()->GetTitle();

    if (!eventType.empty()) {
        fRestRun->SetInputEvent(eventType);
        RowValueChangedAction(0);
    }
}

void TRestBrowser::IdValueChangedAction(Long_t val) {
    int eventID = fEventId;
    int subEventID = fEventSubId;

    fEventId = (Int_t)fEventIdNumberBox->GetNumber();
    fEventSubId = (Int_t)fEventSubIdNumberBox->GetNumber();

    RESTDebug << "TRestBrowser::LoadEventAction. Event ID: " << fEventId << ", Sub ID: " << fEventSubId
              << RESTendl;

    bool success = LoadEventId(fEventId, fEventSubId);

    if (!success) {
        fEventId = eventID;
        fEventSubId = subEventID;
        fEventIdNumberBox->SetIntNumber(fEventId);
        fEventSubIdNumberBox->SetIntNumber(fEventSubId);
    }
}

void TRestBrowser::NextEventAction() {
    string sel = (string)fSelectionTextBox->GetText();
    if (sel.empty()) {
        fEventRow++;
        LoadEventEntry(fEventRow);
    } else {
        auto tree = fRestRun->fAnalysisTree;
        TTreeFormula formula("Selection", sel.c_str(), tree);
        if (formula.GetNdim() > 0) {  // valid expression
            fEventRow++;
            while (true) {
                tree->GetEntry(fEventRow);
                if (formula.EvalInstance(fEventRow) == 1) {
                    LoadEventEntry(fEventRow);
                    break;
                } else {
                    cout << fEventRow << endl;
                    fEventRow++;
                }
                if (fEventRow >= fRestRun->fAnalysisTree->GetEntries()) {
                    LoadEventEntry(fEventRow);
                    break;
                }
            }
        } else {
            cout << "invalid selection!" << endl;
        }
    }
}

void TRestBrowser::PreviousEventAction() {
    string sel = (string)fSelectionTextBox->GetText();
    if (sel.empty()) {
        fEventRow--;
        LoadEventEntry(fEventRow);
    } else {
       auto tree = fRestRun->fAnalysisTree;
        TTreeFormula formula("Selection", sel.c_str(), tree);
        if (formula.GetNdim() > 0) {  // valid expression
            fEventRow--;
            while (true) {
                tree->GetEntry(fEventRow);
                if (formula.EvalInstance(fEventRow) == 1) {
                    LoadEventEntry(fEventRow);
                    break;
                } else {
                    cout << fEventRow << endl;
                    fEventRow--;
                }
                if (fEventRow < 0) {
                    LoadEventEntry(fEventRow);
                    break;
                }
            }
        } else {
            cout << "invalid selection!" << endl;
        }
    }
}

void TRestBrowser::LoadFileAction() {
    TGFileInfo fi;
    new TGFileDialog(gClient->GetRoot(), gClient->GetRoot(), kFDOpen, &fi);

    TString dir = fi.fFilename;

    cout << "Opening " << dir.Data() << endl;

    OpenFile(dir.Data());
}

void TRestBrowser::ExitAction() { gSystem->Exit(0); }
