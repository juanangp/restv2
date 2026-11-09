#ifndef REST_DETECTOR_TIMEWALK_C
#define REST_DETECTOR_TIMEWALK_C

#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH2D.h>
#include <TProfile.h>
#include <TStyle.h>
#include <TTree.h>
#include <TTreeFormula.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "TRestDetectorReadout.h"
#include "TRestDetectorReadoutManager.h"
#include "TRestDetectorSignalEvent.h"
#include "TRestPulseShapeAnalysis.h"
#include "TRestRun.h"

/// \brief Diagnostic of the time walk of the pulses of a TRestDetectorSignalEvent: whether the time of the
/// small pulses is delayed with respect to the large ones, for point-like events (e.g. X-rays), where all the
/// pulses of an event should have about the same time (the electrons arrive together, up to the
/// diffusion).
///
/// The time and amplitude of each pulse are those of TRestDetectorSignalToHitsProcess with the same
/// `method` (TRestPulseShapeAnalysis::GetMax: gaussFit, tripleMaxAverage, ...); pulses whose fit fails
/// are counted and skipped. The methods without a fit (all but gaussFit, landauFit and agetFit) take the
/// maximum of the signal smoothed with a moving average of `smoothingPoints` points
/// (TRestPulseShapeAnalysis::GetSignalSmoothed), so the noise does not spread the times, as
/// TRestDetectorSignalToHitsProcess does with its smoothingPoints for onlyMax, tripleMax and
/// tripleMaxAverage. The reference
/// time of each event is the amplitude weighted mean time of the pulses with an amplitude of at least
/// `refFraction` of the largest one, so the small pulses do not pull it. For every pulse, its delay t - t_ref
/// is plotted against:
/// 1. the relative amplitude A / Amax (Amax: largest pulse of the event);
/// 2. the amplitude A;
/// 3. the distance of its strip to the strip of the largest pulse of the same axis (X strips with X strips,
///    Y with Y, from the readout of the file; not drawn without readout).
/// The profiles (mean delay per bin) are drawn over the 2D histograms, and the mean delay per relative
/// amplitude bin is printed (e.g. for a correction). Delays are in the time units of the signals.
///
/// Time walk shows as a delay depending on the amplitude, the same at any distance. A delay depending on
/// the distance instead points to the charge spread (the strips far from the center see the tail of the
/// electron cloud) or to signals induced in the neighbour strips.
///
/// \param fileName File with a TRestDetectorSignalEvent tree.
/// \param method Time and amplitude of the pulses, as the method of TRestDetectorSignalToHitsProcess
/// (those of TRestPulseShapeAnalysis::GetMax).
/// \param selection If not empty, only the entries passing this expression of the analysis tree branches
/// (e.g. "hitsAnalysis_Energy > 1000 && hitsAnalysis_SigmaZ < 2" to keep point-like events).
/// \param nEntries Number of entries to read, 0 for all.
/// \param refFraction Minimum A / Amax of the pulses used for the reference time.
/// \param smoothingPoints Points of the moving average before the maximum for the methods without a fit
/// (odd, 1 or less: no smoothing).
/// \param outputFileName If not empty, ROOT file where the histograms and the canvas are saved.
///
/// The strip positions come from the data readout (GetDataReadout: `readout`, or the only readout loaded)
/// of the TRestDetectorReadoutManager of the file; events with fewer than two pulses are skipped.
///
/// Example:
///
///    restRoot
///    [0] REST_Detector_TimeWalk("R01234_SIGNALS.root", "gaussFit", "", 0, 0.3, 5, "timewalk.root")
void REST_Detector_TimeWalk(const std::string& fileName, const std::string& method = "gaussFit",
                            const std::string& selection = "", long long nEntries = 0,
                            double refFraction = 0.3, int smoothingPoints = 5,
                            const std::string& outputFileName = "") {
    TRestRun run(fileName);
    if (!run.HasEvent("TRestDetectorSignalEvent")) {
        std::cerr << "REST_Detector_TimeWalk: " << fileName << " has no TRestDetectorSignalEvent tree"
                  << std::endl;
        return;
    }
    auto& event = run.GetEvent<TRestDetectorSignalEvent>("TRestDetectorSignalEvent");

    // Data readout of the file, for the strip positions (none if there are several without `readout`)
    TRestDetectorReadout* readout = nullptr;
    if (auto* manager =
            dynamic_cast<TRestDetectorReadoutManager*>(run.GetMetadataClass("TRestDetectorReadoutManager"))) {
        readout = manager->GetDataReadout();
    }
    if (!readout)
        std::cout << "REST_Detector_TimeWalk: no data readout in the file, no distances" << std::endl;

    std::unique_ptr<TTreeFormula> formula;
    if (!selection.empty()) {
        if (!run.fAnalysisTree) {
            std::cerr << "REST_Detector_TimeWalk: no analysis tree for the selection" << std::endl;
            return;
        }
        formula = std::make_unique<TTreeFormula>("selection", selection.c_str(), run.fAnalysisTree);
        if (formula->GetNdim() == 0) {
            std::cerr << "REST_Detector_TimeWalk: wrong selection " << selection << std::endl;
            return;
        }
    }

    const bool isFit = method == "gaussFit" || method == "landauFit" || method == "agetFit";

    struct Pulse {
        double time, amplitude;
        int axis;  ///< 0: X strip, 1: Y strip, -1: other or no readout
        double position;
    };
    struct Result {
        double relative, amplitude, delay, distance;
    };
    std::vector<Result> results;
    double maxAmplitude = 0, maxDelay = 0, maxDistance = 0;

    const long long entries = nEntries > 0 ? std::min(nEntries, run.GetEntries()) : run.GetEntries();
    long nEvents = 0, nPulses = 0, nFailed = 0;
    for (long long entry = 0; entry < entries; entry++) {
        if (formula) {
            run.fAnalysisTree->LoadTree(entry);
            formula->GetNdata();
            if (formula->EvalInstance() == 0) continue;
        }
        if (!run.GetEntry(entry)) continue;

        std::vector<Pulse> pulses;
        for (int s = 0; s < event.GetNumberOfSignals(); s++) {
            const auto& signal = event.GetSignal(s);
            TGraph graph = signal.GetGraph();
            if (!isFit && smoothingPoints > 1) {
                const auto smoothed =
                    TRestPulseShapeAnalysis::GetSignalSmoothed(signal.GetCharge(), smoothingPoints);
                for (int j = 0; j < graph.GetN(); j++) graph.SetPointY(j, smoothed[j]);
            }
            const auto [time, amplitude] = TRestPulseShapeAnalysis::GetMax(graph, method);
            nPulses++;
            if (time < 0 || amplitude <= 0) {  // fits return -1 on failure
                nFailed++;
                continue;
            }
            Pulse pulse{time, amplitude, -1, 0};
            if (readout) {
                const auto p = readout->GetPositionFromChannel(signal.GetSignalID());
                if (!std::isnan(p.X()) && std::isnan(p.Y())) pulse = {time, amplitude, 0, p.X()};
                if (std::isnan(p.X()) && !std::isnan(p.Y())) pulse = {time, amplitude, 1, p.Y()};
            }
            pulses.push_back(pulse);
        }
        if (pulses.size() < 2) continue;

        double aMax = 0;
        for (const auto& pulse : pulses) aMax = std::max(aMax, pulse.amplitude);
        double sum = 0, weight = 0;
        for (const auto& pulse : pulses) {
            if (pulse.amplitude < refFraction * aMax) continue;
            sum += pulse.amplitude * pulse.time;
            weight += pulse.amplitude;
        }
        const double reference = sum / weight;

        // Strip of the largest pulse of each axis
        double center[2] = {0, 0}, centerAmplitude[2] = {0, 0};
        for (const auto& pulse : pulses) {
            if (pulse.axis < 0 || pulse.amplitude <= centerAmplitude[pulse.axis]) continue;
            centerAmplitude[pulse.axis] = pulse.amplitude;
            center[pulse.axis] = pulse.position;
        }

        nEvents++;
        for (const auto& pulse : pulses) {
            const double delay = pulse.time - reference;
            const double distance = pulse.axis < 0 ? -1 : std::abs(pulse.position - center[pulse.axis]);
            results.push_back({pulse.amplitude / aMax, pulse.amplitude, delay, distance});
            maxAmplitude = std::max(maxAmplitude, pulse.amplitude);
            maxDelay = std::max(maxDelay, std::abs(delay));
            maxDistance = std::max(maxDistance, distance);
        }
    }
    std::cout << "REST_Detector_TimeWalk: " << nEvents << " events with 2 or more pulses, " << nPulses
              << " pulses, " << nFailed << " failed (" << method << ")" << std::endl;
    if (results.empty()) return;

    // Ranges from the data: delays symmetric, up to the largest one
    const double delayRange = maxDelay > 0 ? 1.05 * maxDelay : 1;
    std::vector<TH2D*> histograms = {
        new TH2D("hDelayVsRelativeAmplitude", "Delay vs relative amplitude;A / A_{max};t - t_{ref}", 50, 0,
                 1.0001, 200, -delayRange, delayRange),
        new TH2D("hDelayVsAmplitude", "Delay vs amplitude;A;t - t_{ref}", 100, 0, 1.01 * maxAmplitude, 200,
                 -delayRange, delayRange)};
    if (readout) {
        histograms.push_back(new TH2D(
            "hDelayVsDistance", "Delay vs distance to the largest pulse;strip distance (mm);t - t_{ref}", 100,
            0, 1.01 * maxDistance + 1, 200, -delayRange, delayRange));
    }
    for (const auto& r : results) {
        histograms[0]->Fill(r.relative, r.delay);
        histograms[1]->Fill(r.amplitude, r.delay);
        if (histograms.size() > 2 && r.distance >= 0) histograms[2]->Fill(r.distance, r.delay);
    }

    // Not attached to the input file, closed at the end of the macro
    std::vector<TProfile*> profiles;
    for (auto* h : histograms) {
        h->SetDirectory(nullptr);
        auto* profile = h->ProfileX((std::string(h->GetName()) + "_profile").c_str());
        profile->SetDirectory(nullptr);
        profile->SetLineColor(kRed);
        profile->SetMarkerColor(kRed);
        profile->SetMarkerStyle(20);
        profile->SetMarkerSize(0.6);
        profiles.push_back(profile);
    }

    gStyle->SetOptStat(0);
    auto canvas = new TCanvas("cTimeWalk", "Time walk", 500 * histograms.size(), 500);
    canvas->Divide(histograms.size(), 1);
    for (size_t i = 0; i < histograms.size(); i++) {
        canvas->cd(i + 1);
        gPad->SetLogz();
        histograms[i]->Draw("colz");
        profiles[i]->Draw("same");
    }
    canvas->Update();

    std::cout << "Mean delay vs relative amplitude (A / Amax: mean delay +- error, pulses)" << std::endl;
    for (int bin = 1; bin <= profiles[0]->GetNbinsX(); bin++) {
        if (profiles[0]->GetBinEntries(bin) == 0) continue;
        std::cout << "  " << profiles[0]->GetXaxis()->GetBinLowEdge(bin) << " - "
                  << profiles[0]->GetXaxis()->GetBinUpEdge(bin) << ": " << profiles[0]->GetBinContent(bin)
                  << " +- " << profiles[0]->GetBinError(bin) << " (" << profiles[0]->GetBinEntries(bin) << ")"
                  << std::endl;
    }

    if (!outputFileName.empty()) {
        TFile output(outputFileName.c_str(), "RECREATE");
        output.cd();
        for (auto* h : histograms) h->Write();
        for (auto* p : profiles) p->Write();
        canvas->Write();
        std::cout << "REST_Detector_TimeWalk: histograms saved in " << outputFileName << std::endl;
    }
}

#endif
