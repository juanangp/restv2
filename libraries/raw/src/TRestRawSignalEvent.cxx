#include "TRestRawSignalEvent.h"
#include "TRestTools.h"

#include <iostream>

#include <TPad.h>
#include <TStyle.h>
#include <TList.h>
#include <TObjString.h>
#include <TMultiGraph.h>

// ---------------------------------------------------------------------------
// Self-registration in EventRegistry (no macro)
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    EventRegistry::Instance().Register("TRestRawSignalEvent",
                                       []() { return std::make_unique<TRestRawSignalEvent>(); });
    return true;
}();
}  // namespace

void TRestRawSignalEvent::PrintEvent() const {
    TRestEvent::PrintEvent();
    RESTLog << "  Signals: " << GetNumberOfSignals() << RESTendl;

    for (int i = 0; i < GetNumberOfSignals(); ++i) {
        TRestRawSignal sig = GetSignal(i);

        RESTLog << " SignalID " << sig.GetSignalID() << " (nPoints=" << sig.GetNPoints() << "): ";

        for (int j = 0; j < sig.GetNPoints(); ++j) {
            RESTLog << sig.GetPoint(j) << " ";
        }
        RESTLog << RESTendl;
    }
}

TPad* TRestRawSignalEvent::DrawEvent(const TString& option) const {
    const int nSignals = GetNumberOfSignals();
    if (nSignals == 0) {
        std::cout << "Empty event " << std::endl;
        return nullptr;
    }

    if (fPad == nullptr) {
        TString padName = Form("pad_event_%d", this->GetID());
        fPad = new TPad(padName, "REST Event Viewer", 0, 0, 1, 1);
    }

    TVirtualPad* safePad = gPad; 
    
    fPad->cd();
    fPad->Clear();

    std::vector<int> signalIDs;
    bool hasFilter = false;
    int sRangeInit = 0;
    int sRangeEnd = 0;

    TString optStr = option;
    optStr.ToLower();

    if (optStr.Contains("ids[")) {
        int startIdx = optStr.Index("ids[") + 4;
        int endIdx = optStr.Index("]", startIdx);
        if (endIdx > startIdx) {
            TString rangeStr = optStr(startIdx, endIdx - startIdx);
            
            TString sep = "-";
            if (rangeStr.Contains(",")) sep = ",";
            
            TObjArray* tokens = rangeStr.Tokenize(sep);
            if (tokens->GetEntries() >= 2) {
                sRangeInit = ((TObjString*)tokens->At(0))->GetString().Atoi();
                sRangeEnd = ((TObjString*)tokens->At(1))->GetString().Atoi();
                hasFilter = true;
            }
            delete tokens;
        }
    } else if (optStr.Contains("-")) {
        TObjArray* tokens = optStr.Tokenize("-");
        if (tokens->GetEntries() >= 2) {
            sRangeInit = ((TObjString*)tokens->At(0))->GetString().Atoi();
            sRangeEnd = ((TObjString*)tokens->At(1))->GetString().Atoi();
            hasFilter = true;
        }
        delete tokens;
    } else if (!optStr.IsNull() && optStr.IsDigit()) {
        signalIDs.push_back(optStr.Atoi());
        hasFilter = true;
    }

    if (!hasFilter || optStr.IsNull()) {
        for (int n = 0; n < nSignals; n++) {
            signalIDs.push_back(GetSignal(n).GetSignalID());
        }
    } else if (signalIDs.empty() && hasFilter) {
        for (int n = 0; n < nSignals; n++) {
            int sID = GetSignal(n).GetSignalID();
            if (sID >= sRangeInit && sID <= sRangeEnd) {
                signalIDs.push_back(sID);
            }
        }
    }

    if (signalIDs.empty()) {
        fPad->SetTitle("No Signals Found");
        std::cout << "No signals found matching criteria." << std::endl;
        if (safePad) safePad->cd();
        return fPad;
    }

    if (optStr.Contains("printids")) {
        std::cout << "SignalIDs a pintar:";
        for (const auto& id : signalIDs) std::cout << " " << id;
        std::cout << std::endl;
    }

    TMultiGraph* mg = new TMultiGraph();
    TString title = Form("Event ID %d;Time Bin;ADC Channels", this->GetID());
    mg->SetTitle(title.Data());

    for (int sID : signalIDs) {
            const auto& signal = GetSignalByID(sID); 
            TGraph* g = (TGraph*)signal.GetGraph().Clone();
            g->SetLineColor((sID%72)+1);
            mg->Add(g, "L"); 
    }

    mg->Draw("A"); 
    fPad->Modified();
    
    if (safePad) safePad->cd(); 

    return fPad;
}
