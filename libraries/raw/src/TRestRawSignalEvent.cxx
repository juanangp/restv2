#include "TRestRawSignalEvent.h"

#include <TMultiGraph.h>
#include <TPad.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>

#include "TRestLogManager.h"
#include "TRestTools.h"

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

TPad* TRestRawSignalEvent::DrawEvent(const std::string& option) const {
    const int nSignals = GetNumberOfSignals();
    if (nSignals == 0) {
        std::cout << "Empty event " << std::endl;
        return nullptr;
    }

    if (fPad == nullptr) {
        const std::string padName = "pad_event_" + std::to_string(GetID());
        fPad = new TPad(padName.c_str(), "REST Event Viewer", 0, 0, 1, 1);
    }

    TVirtualPad* safePad = gPad;

    fPad->cd();
    fPad->Clear();

    std::vector<int> signalIDs;
    bool hasFilter = false;
    int sRangeInit = 0;
    int sRangeEnd = 0;

    const std::string optStr = TRestTools::ToLower(option);
    const auto isNumber = [](const std::string& str) {
        return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
    };
    // A range "first-last" or "first,last" (empty parts are skipped)
    const auto readRange = [&](const std::string& range, char sep) {
        std::vector<std::string> parts;
        for (const auto& part : TRestTools::Split(range, sep)) {
            if (!part.empty()) parts.push_back(part);
        }
        if (parts.size() < 2) return;
        sRangeInit = std::atoi(parts[0].c_str());
        sRangeEnd = std::atoi(parts[1].c_str());
        hasFilter = true;
    };

    if (const size_t idsPos = optStr.find("ids["); idsPos != std::string::npos) {
        const size_t startIdx = idsPos + 4;
        const size_t endIdx = optStr.find(']', startIdx);
        if (endIdx != std::string::npos && endIdx > startIdx) {
            const std::string rangeStr = optStr.substr(startIdx, endIdx - startIdx);
            readRange(rangeStr, rangeStr.find(',') != std::string::npos ? ',' : '-');
        }
    } else if (optStr.find('-') != std::string::npos) {
        readRange(optStr, '-');
    } else if (isNumber(optStr)) {
        signalIDs.push_back(std::atoi(optStr.c_str()));
        hasFilter = true;
    }

    if (!hasFilter || optStr.empty()) {
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

    if (optStr.find("printids") != std::string::npos) {
        std::cout << "Signal IDs to draw:";
        for (const auto& id : signalIDs) std::cout << " " << id;
        std::cout << std::endl;
    }

    TMultiGraph* mg = new TMultiGraph();
    const std::string title = "Event ID " + std::to_string(GetID()) + ";Time Bin;ADC Channels";
    mg->SetTitle(title.c_str());

    for (int sID : signalIDs) {
        const auto& signal = GetSignalByID(sID);
        TGraph* g = (TGraph*)signal.GetGraph().Clone();
        g->SetLineColor((sID % 72) + 1);
        mg->Add(g, "L");
    }

    DrawOwned(mg, "A");
    fPad->Modified();

    if (safePad) safePad->cd();

    return fPad;
}
