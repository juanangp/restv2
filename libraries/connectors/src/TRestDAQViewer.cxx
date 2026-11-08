#include "TRestDAQViewer.h"

#include <TApplication.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TGButton.h>
#include <TGClient.h>
#include <TGFileDialog.h>
#include <TGLabel.h>
#include <TGNumberEntry.h>
#include <TGTextEntry.h>
#include <TGeoBBox.h>
#include <TGeoMatrix.h>
#include <TGraph.h>
#include <TH1.h>
#include <TH2.h>
#include <TH2Poly.h>
#include <THStack.h>
#include <TObjArray.h>
#include <TObjString.h>
#include <TPRegexp.h>
#include <TRootEmbeddedCanvas.h>
#include <TSystem.h>
#include <TTimer.h>
#include <TVirtualPad.h>

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <thread>

#include "TRestDetectorReadout.h"
#include "TRestPulseShapeAnalysis.h"
#include "TRestTools.h"
#include "TRestYAMLUtils.h"

using namespace TRestPulseShapeAnalysis;

namespace {
/// Rows of each readout: spectrum, hit map, signals of the event
constexpr int kRows = 3;
/// Names of the axes, and the axes of the projections XY, XZ, YZ
constexpr const char* kAxisNames[3] = {"X", "Y", "Z"};
constexpr int kProjectionAxes[3][2] = {{0, 1}, {0, 2}, {1, 2}};
}  // namespace

std::string TRestDAQViewer::GetSettingsFileName() {
    return TRestTools::GetRESTUserDirectory() + "/DAQViewer_settings";
}

TRestDAQViewer::TRestDAQViewer(const std::string& configFile, const std::string& dataFile)
    : TGMainFrame(gClient->GetRoot(), 1200, 900) {
    fPulseParameters = std::make_unique<TRestRawSignalAnalysisProcess>();

    fRateGraph = new TGraph();
    fRateGraph->SetTitle("Rate;Time [s];Rate [Hz]");
    fRateGraph->SetMarkerStyle(20);
    fRateGraph->SetMarkerSize(0.6);
    fRateGraph->SetLineColor(kAzure + 1);
    fRateGraph->GetXaxis()->SetTimeDisplay(1);

    fTimer = new TTimer();
    fTimer->Connect("Timeout()", "TRestDAQViewer", this, "NextEvent()");
    fWatchdogTimer = new TTimer();
    fWatchdogTimer->Connect("Timeout()", "TRestDAQViewer", this, "CheckForNewData()");

    TGHorizontalFrame* mainFrame = new TGHorizontalFrame(this, 1200, 900);
    AddFrame(mainFrame, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    TGVerticalFrame* leftFrame = new TGVerticalFrame(mainFrame, 240, 900);
    mainFrame->AddFrame(leftFrame, new TGLayoutHints(kLHintsLeft | kLHintsExpandY, 5, 5, 5, 5));

    auto btnL = new TGLayoutHints(kLHintsExpandX, 5, 5, 2, 2);
    auto labL = new TGLayoutHints(kLHintsLeft, 5, 5, 2, 0);

    // UI Controls
    TGTextButton* btnExit = new TGTextButton(leftFrame, "&Exit");
    btnExit->Connect("Clicked()", "TRestDAQViewer", this, "DoExit()");
    leftFrame->AddFrame(btnExit, btnL);

    leftFrame->AddFrame(new TGLabel(leftFrame, "Data File:"), labL);
    fDataPathEntry = new TGTextEntry(leftFrame, "");
    fDataPathEntry->SetEnabled(kFALSE);
    leftFrame->AddFrame(fDataPathEntry, btnL);

    leftFrame->AddFrame(new TGLabel(leftFrame, "Config File:"), labL);
    fConfigPathEntry = new TGTextEntry(leftFrame, "");
    fConfigPathEntry->SetEnabled(kFALSE);
    leftFrame->AddFrame(fConfigPathEntry, btnL);

    fStatusLabel = new TGLabel(leftFrame, "Status: Waiting");
    leftFrame->AddFrame(fStatusLabel, labL);

    leftFrame->AddFrame(new TGLabel(leftFrame, "Step Size"), labL);
    fStepSize = new TGNumberEntry(leftFrame, 100, 5, -1, TGNumberFormat::kNESInteger);
    leftFrame->AddFrame(fStepSize, btnL);

    fCheckNextRun = new TGCheckButton(leftFrame, "Auto-load Next Run");
    leftFrame->AddFrame(fCheckNextRun, labL);

    // --- Action Buttons ---
    TGTextButton* btnOpen = new TGTextButton(leftFrame, "&Open Data");
    btnOpen->Connect("Clicked()", "TRestDAQViewer", this, "OpenFile()");
    leftFrame->AddFrame(btnOpen, btnL);

    TGTextButton* btnConfig = new TGTextButton(leftFrame, "Load &Config");
    btnConfig->Connect("Clicked()", "TRestDAQViewer", this, "OpenConfig()");
    leftFrame->AddFrame(btnConfig, btnL);

    TGTextButton* btnReloadConfig = new TGTextButton(leftFrame, "Reload Con&fig");
    btnReloadConfig->Connect("Clicked()", "TRestDAQViewer", this, "ReloadConfig()");
    leftFrame->AddFrame(btnReloadConfig, btnL);

    TGTextButton* btnReload = new TGTextButton(leftFrame, "&Reload Run");
    btnReload->Connect("Clicked()", "TRestDAQViewer", this, "ReloadRun()");
    leftFrame->AddFrame(btnReload, btnL);

    TGTextButton* btnReset = new TGTextButton(leftFrame, "Reset &Histos");
    btnReset->Connect("Clicked()", "TRestDAQViewer", this, "ManualReset()");
    leftFrame->AddFrame(btnReset, btnL);

    TGTextButton* btnSave = new TGTextButton(leftFrame, "&Save Plots");
    btnSave->Connect("Clicked()", "TRestDAQViewer", this, "SavePlots()");
    leftFrame->AddFrame(btnSave, btnL);

    TGTextButton* btnNext = new TGTextButton(leftFrame, "&Next Event");
    btnNext->Connect("Clicked()", "TRestDAQViewer", this, "NextEvent()");
    leftFrame->AddFrame(btnNext, btnL);

    TGTextButton* btnAuto = new TGTextButton(leftFrame, "&DAQ Live Mode");
    btnAuto->Connect("Clicked()", "TRestDAQViewer", this, "ToggleAuto()");
    leftFrame->AddFrame(btnAuto, btnL);

    fMainCanvas = new TRootEmbeddedCanvas("MainCanvas", mainFrame, 950, 850);
    mainFrame->AddFrame(fMainCanvas, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 0, 5, 5, 5));

    // The files given, otherwise those of the last session; the readouts before the data
    std::string lastData, lastConfig;
    LoadSettings(lastData, lastConfig);
    const std::string config = configFile.empty() ? lastConfig : configFile;
    const std::string data = dataFile.empty() ? lastData : dataFile;
    if (!config.empty()) LoadConfigFile(config.c_str());
    if (fViews.empty()) BuildViews();
    if (!data.empty()) LoadDataFile(data.c_str());

    MapSubwindows();
    Resize(GetDefaultSize());
    MapWindow();

    Connect("CloseWindow()", "TRestDAQViewer", this, "DoExit()");
    DontCallClose();
}

void TRestDAQViewer::DoExit() {
    SaveSettings();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    gApplication->Terminate(0);
}

void TRestDAQViewer::SaveSettings() {
    std::ofstream cfg(GetSettingsFileName(), std::ios::out | std::ios::trunc);
    if (cfg.is_open()) {
        cfg << (fCheckNextRun->GetState() == kButtonDown ? 1 : 0) << "\n";
        cfg << fBaseFileName.Data() << "\n" << fConfigFile.Data() << "\n";
        cfg << (int)fStepSize->GetNumber() << "\n";
    }
}

void TRestDAQViewer::LoadSettings(std::string& dataPath, std::string& configPath) {
    std::ifstream cfg(GetSettingsFileName());
    if (!cfg.is_open()) return;

    std::string line;
    if (std::getline(cfg, line) && !line.empty())
        fCheckNextRun->SetState(std::stoi(line) ? kButtonDown : kButtonUp);

    std::getline(cfg, dataPath);
    std::getline(cfg, configPath);

    if (std::getline(cfg, line) && !line.empty()) fStepSize->SetNumber(std::stoi(line));
}

std::pair<std::string, YAML::Node> TRestDAQViewer::FindSection(const YAML::Node& node,
                                                               const std::string& className) {
    if (!node.IsMap()) return {};
    for (const auto& element : node) {
        const YAML::Node value = element.second;
        if (!value.IsMap()) continue;
        if (value["class"] && value["class"].IsScalar() && value["class"].Scalar() == className)
            return {element.first.as<std::string>(), value};
        auto found = FindSection(value, className);
        if (found.second) return found;
    }
    return {};
}

void TRestDAQViewer::LoadConfigFile(const char* path) {
    if (gSystem->AccessPathName(path)) {
        std::cout << "TRestDAQViewer: configuration file " << path << " not found" << std::endl;
        return;
    }

    try {
        const YAML::Node config = TRestYAMLUtils::OpenConfigFile(path);

        // Binning of the spectra: parameters spectrumBins/Min/Max, and with _<readout> for a readout
        const auto parameters = TRestYAMLUtils::GetParameters();
        auto readBinning = [&](const std::string& suffix, const SpectrumBinning& defaults) {
            SpectrumBinning binning = defaults;
            auto read = [&](const std::string& name, double& value) {
                const auto it = parameters.find(name + suffix);
                if (it == parameters.end()) return;
                TRestYAMLUtils::MarkParameterUsed(it->first);
                value = std::stod(it->second);
            };
            double bins = binning.bins;
            read("spectrumBins", bins);
            read("spectrumMin", binning.min);
            read("spectrumMax", binning.max);
            binning.bins = static_cast<int>(bins);
            if (binning.bins <= 0 || binning.max <= binning.min)
                throw std::runtime_error("spectrum" + suffix + ": bins > 0 and max > min needed");
            return binning;
        };
        const SpectrumBinning defaultSpectrum = readBinning("", SpectrumBinning());
        std::map<std::string, SpectrumBinning> spectrumOfReadout;

        // Pulse parameters, the defaults of the process if not given
        const auto analysis = FindSection(config, "TRestRawSignalAnalysisProcess");
        if (analysis.second)
            fPulseParameters =
                std::make_unique<TRestRawSignalAnalysisProcess>(analysis.first, analysis.second);
        else
            fPulseParameters = std::make_unique<TRestRawSignalAnalysisProcess>();

        fReadoutManager.reset();
        const auto readouts = FindSection(config, "TRestDetectorReadoutManager");
        if (readouts.second) {
            fReadoutManager = std::make_unique<TRestDetectorReadoutManager>(readouts.first, readouts.second);
            fReadoutManager->LoadReadout();
            for (const auto& info : fReadoutManager->fReadoutInfo)
                spectrumOfReadout[info.fInstanceName] =
                    readBinning("_" + info.fInstanceName, defaultSpectrum);
        }
        fDefaultSpectrum = defaultSpectrum;
        fSpectrumOfReadout = spectrumOfReadout;

        // A parameter not used, e.g. the name of a readout misspelt: warned, the configuration is used
        try {
            TRestYAMLUtils::CheckParametersUsed();
        } catch (const std::exception& e) {
            std::cout << "TRestDAQViewer: WARNING " << e.what() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "TRestDAQViewer: error reading " << path << ": " << e.what() << std::endl;
        return;
    }

    fConfigFile = path;
    fConfigPathEntry->SetText(gSystem->BaseName(path));
    fConfigPathEntry->SetToolTipText(path);
    std::cout << "TRestDAQViewer: pulse parameters: baselineRange [" << fPulseParameters->fBaselineRange.first
              << ", " << fPulseParameters->fBaselineRange.second << "], integralRange ["
              << fPulseParameters->fIntegralRange.first << ", " << fPulseParameters->fIntegralRange.second
              << "], pointsThreshold " << fPulseParameters->fPointsThreshold << ", signalThreshold "
              << fPulseParameters->fSignalThreshold << ", pointsOverThreshold "
              << fPulseParameters->fPointsOverThreshold << std::endl;

    BuildViews();
}

void TRestDAQViewer::DeleteViews() {
    ClearEvent();
    for (auto& view : fViews) {
        delete view.spectrum;
        delete view.hitmap;
        for (auto projection : view.projections) delete projection;
    }
    fViews.clear();
    fViewOfSignal.clear();
    fStripPosition.clear();
    fChannelBins.clear();
}

void TRestDAQViewer::BuildViews() {
    DeleteViews();

    auto addView = [&](const std::string& name, TRestDetectorReadout* readout) {
        ReadoutView view;
        view.name = name;
        view.readout = readout;
        const int n = fViews.size();
        const auto binning = fSpectrumOfReadout.find(name);
        const SpectrumBinning& spectrum =
            binning == fSpectrumOfReadout.end() ? fDefaultSpectrum : binning->second;
        view.spectrum = new TH1F(Form("hSpec%d", n), Form("%s;Amplitude (sum);Counts", name.c_str()),
                                 spectrum.bins, spectrum.min, spectrum.max);
        view.spectrum->SetDirectory(nullptr);
        fViews.push_back(view);
    };

    if (fReadoutManager) {
        for (const auto& info : fReadoutManager->fReadoutInfo) {
            auto readout = fReadoutManager->GetReadout(info.fInstanceName);
            if (readout) addView(info.fInstanceName, readout);
        }
    }
    // No readout: all the signals together
    if (fViews.empty()) addView("All signals", nullptr);

    // Hit map of each readout, from the channels of its decoding: strips on two axes (a single coordinate
    // of GetPositionFromChannel), otherwise the projections of the volumes of the channels
    for (size_t n = 0; n < fViews.size(); n++) {
        auto& view = fViews[n];
        if (!view.readout) continue;
        std::map<int, int> physicalToDAQ;
        std::map<int, std::pair<int, double>> strips;
        std::set<double> stripPositions[3];
        std::istringstream decoding(view.readout->GetDecodingAsString());
        std::string line;
        while (std::getline(decoding, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream fields(line);
            int daqID, physicalID;
            if (!(fields >> daqID >> physicalID)) continue;
            fViewOfSignal[daqID] = n;
            physicalToDAQ[physicalID] = daqID;
            const auto position = view.readout->GetPositionFromChannel(daqID);
            const double coordinates[3] = {position.X(), position.Y(), position.Z()};
            int axis = -1, nFinite = 0;
            for (int a = 0; a < 3; a++)
                if (std::isfinite(coordinates[a])) {
                    axis = a;
                    nFinite++;
                }
            if (nFinite != 1) continue;  // Not a strip
            strips[daqID] = {axis, coordinates[axis]};
            stripPositions[axis].insert(coordinates[axis]);
        }

        // The (first) two axes with strips
        int nAxes = 0;
        for (int a = 0; a < 3 && nAxes < 2; a++)
            if (!stripPositions[a].empty()) view.stripAxes[nAxes++] = a;
        if (nAxes < 2) {
            view.stripAxes[0] = view.stripAxes[1] = -1;
            BuildProjections(view, n, physicalToDAQ);
            continue;
        }

        // Bins centred on the strips
        auto range = [](const std::set<double>& values) {
            const double pitch =
                values.size() > 1 ? (*values.rbegin() - *values.begin()) / (values.size() - 1) : 1;
            return std::make_pair(*values.begin() - pitch / 2, *values.rbegin() + pitch / 2);
        };
        const auto& positionsA = stripPositions[view.stripAxes[0]];
        const auto& positionsB = stripPositions[view.stripAxes[1]];
        const auto [minA, maxA] = range(positionsA);
        const auto [minB, maxB] = range(positionsB);
        view.hitmap = new TH2F(Form("hHit%zu", n),
                               Form("%s hit map;%s [mm];%s [mm]", view.name.c_str(),
                                    kAxisNames[view.stripAxes[0]], kAxisNames[view.stripAxes[1]]),
                               positionsA.size(), minA, maxA, positionsB.size(), minB, maxB);
        view.hitmap->SetDirectory(nullptr);
        for (const auto& [daqID, strip] : strips)
            if (strip.first == view.stripAxes[0] || strip.first == view.stripAxes[1])
                fStripPosition[daqID] = strip;
    }

    UpdateCanvasLayout();
}

void TRestDAQViewer::BuildProjections(ReadoutView& view, int index, const std::map<int, int>& physicalToDAQ) {
    std::vector<TRestDetectorReadout::ViewItem> items;
    view.readout->GetViewItems(items);

    // Bounding box (global frame) of each volume of a channel of the decoding
    struct Box {
        int daqID;
        double minimum[3], maximum[3];
    };
    std::vector<Box> boxes;
    double minimum[3] = {INFINITY, INFINITY, INFINITY}, maximum[3] = {-INFINITY, -INFINITY, -INFINITY};
    for (const auto& item : items) {
        const auto channel = physicalToDAQ.find(item.physicalID);
        if (channel == physicalToDAQ.end()) continue;
        auto* bbox = dynamic_cast<TGeoBBox*>(item.shape);
        if (!bbox || !item.matrix) continue;
        Box box{channel->second, {INFINITY, INFINITY, INFINITY}, {-INFINITY, -INFINITY, -INFINITY}};
        const double* origin = bbox->GetOrigin();
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2)
                for (int sz = -1; sz <= 1; sz += 2) {
                    const double local[3] = {origin[0] + sx * bbox->GetDX(), origin[1] + sy * bbox->GetDY(),
                                             origin[2] + sz * bbox->GetDZ()};
                    double global[3];
                    item.matrix->LocalToMaster(local, global);
                    for (int a = 0; a < 3; a++) {
                        box.minimum[a] = std::min(box.minimum[a], global[a]);
                        box.maximum[a] = std::max(box.maximum[a], global[a]);
                    }
                }
        for (int a = 0; a < 3; a++) {
            minimum[a] = std::min(minimum[a], box.minimum[a]);
            maximum[a] = std::max(maximum[a], box.maximum[a]);
        }
        boxes.push_back(box);
    }
    if (boxes.empty()) return;

    // The same bins (one per volume) in the three projections
    for (int p = 0; p < 3; p++) {
        const int a = kProjectionAxes[p][0], b = kProjectionAxes[p][1];
        const double marginA = 0.05 * (maximum[a] - minimum[a]), marginB = 0.05 * (maximum[b] - minimum[b]);
        auto projection = new TH2Poly(Form("hProj%d_%d", index, p),
                                      Form("%s %s%s;%s [mm];%s [mm]", view.name.c_str(), kAxisNames[a],
                                           kAxisNames[b], kAxisNames[a], kAxisNames[b]),
                                      minimum[a] - marginA, maximum[a] + marginA, minimum[b] - marginB,
                                      maximum[b] + marginB);
        projection->SetDirectory(nullptr);
        projection->SetStats(false);
        for (const auto& box : boxes) {
            const int bin =
                projection->AddBin(box.minimum[a], box.minimum[b], box.maximum[a], box.maximum[b]);
            if (p == 0) fChannelBins[box.daqID].push_back(bin);
        }
        view.projections[p] = projection;
    }
}

int TRestDAQViewer::GetViewOfSignal(int signalID) {
    // Without readouts, a single view with all the signals
    if (fViews.size() == 1 && !fViews[0].readout) return 0;
    const auto it = fViewOfSignal.find(signalID);
    return it == fViewOfSignal.end() ? -1 : it->second;
}

double TRestDAQViewer::GetAmplitude(const std::vector<short>& pulse) const {
    const auto& p = *fPulseParameters;
    double baseline = 0, baselineSigma = 0;
    const auto data = CalculateBaselineAndSigma(pulse, p.fBaselineRange.first, p.fBaselineRange.second,
                                                baseline, baselineSigma, p.fBaselineOption);
    auto range = p.fIntegralRange;
    const auto points = GetPointsOverThreshold(data, range, {p.fPointsThreshold, p.fSignalThreshold},
                                               p.fPointsOverThreshold, p.fNPointsFlat, baselineSigma);
    // A signal: at least 2 points over threshold (NGoodSignals of TRestRawSignalAnalysisProcess)
    if (points.size() < 2) return 0;
    auto graph = GetGraphPair(points);
    const auto maxPeak = GetMax(graph, p.fMaxOption);
    if (maxPeak.first < 0 || maxPeak.first >= pulse.size()) return 0;
    return maxPeak.second;
}

void TRestDAQViewer::LoadDataFile(const char* path, bool newRun) {
    if (gSystem->AccessPathName(path)) return;
    fBaseFileName = path;
    ParseRunSubrun(fBaseFileName, fCurrentRun, fCurrentSubrun);

    std::cout << "Opening file " << path << std::endl;

    fDataPathEntry->SetText(gSystem->BaseName(path));
    fDataPathEntry->SetToolTipText(path);

    CloseDataFile();

    // A single file (not a TChain, whose number of entries is fixed when it is opened): its tree is
    // refreshed to read the events written after it was opened
    fFile = TFile::Open(fBaseFileName, "READ");
    if (fFile && !fFile->IsZombie()) fFile->GetObject("TRestRawSignalEvent", fTree);
    if (!fTree) {
        // The tree is in the file after the first AutoSave of the DAQ
        std::cout << "No TRestRawSignalEvent tree in the file yet, please reload the file... "
                  << fBaseFileName << std::endl;
        CloseDataFile();
        fStatusLabel->SetText("Status: Waiting for DAQ initialization...");
        fDataPathEntry->SetText("");
        return;
    }
    // No read cache: its baskets would not follow the refreshes
    fTree->SetCacheSize(0);

    fEntry = 0;

    if (newRun) {
        ManualReset();
    }

    fReader = new TTreeReader(fTree);
    fReaderSignalsID = new TTreeReaderValue<std::vector<int>>(*fReader, "fSigIDs");
    fReaderPulses = new TTreeReaderValue<std::vector<short>>(*fReader, "fSigSamples");
    fReaderOffsets = new TTreeReaderValue<std::vector<int>>(*fReader, "fSigOffsets");
    fReaderEventID = new TTreeReaderValue<int>(*fReader, "eventID");
    fReaderSeconds = new TTreeReaderValue<Long64_t>(*fReader, "timeSeconds");
    fReaderNanoSeconds = new TTreeReaderValue<int>(*fReader, "timeNanoSecs");

    SetWindowName(Form("DAQ Viewer - %s", gSystem->BaseName(path)));
    NextEvent();
}

void TRestDAQViewer::UpdateCanvasLayout() {
    TCanvas* c = fMainCanvas->GetCanvas();
    c->Clear();
    // A column per readout, plus the rate
    c->Divide(fViews.size() + 1, kRows);
    // The hit map of the readouts with projections: a pad for each one
    for (size_t n = 0; n < fViews.size(); n++) {
        if (!fViews[n].projections[0]) continue;
        c->cd((fViews.size() + 1) + n + 1);
        gPad->Divide(3, 1);
    }
    DrawCanvas();
}

void TRestDAQViewer::DrawCanvas() {
    TCanvas* c = fMainCanvas->GetCanvas();
    const int nCols = fViews.size() + 1;
    auto pad = [&](int row, int col) { return row * nCols + col + 1; };

    for (size_t n = 0; n < fViews.size(); n++) {
        const auto& view = fViews[n];
        c->cd(pad(0, n));
        view.spectrum->Draw("HIST");
        c->cd(pad(1, n));
        if (view.hitmap) view.hitmap->Draw("COLZ");
        if (view.projections[0]) {
            TVirtualPad* hitPad = gPad;
            for (int p = 0; p < 3; p++) {
                hitPad->cd(p + 1);
                view.projections[p]->Draw("COLZ");
            }
        }
        c->cd(pad(2, n));
        if (view.pulses) view.pulses->Draw("nostack,l");
    }

    c->cd(pad(0, nCols - 1));
    if (fRateGraph->GetN() > 0) fRateGraph->Draw("ALP");

    c->Modified();
    c->Update();
}

void TRestDAQViewer::CloseDataFile() {
    delete fReaderSignalsID;
    fReaderSignalsID = nullptr;
    delete fReaderPulses;
    fReaderPulses = nullptr;
    delete fReaderOffsets;
    fReaderOffsets = nullptr;
    delete fReaderEventID;
    fReaderEventID = nullptr;
    delete fReaderSeconds;
    fReaderSeconds = nullptr;
    delete fReaderNanoSeconds;
    fReaderNanoSeconds = nullptr;
    delete fReader;
    fReader = nullptr;
    fTree = nullptr;  // Owned by the file
    delete fFile;
    fFile = nullptr;
}

Long64_t TRestDAQViewer::RefreshEntries() {
    if (!fTree) return 0;
    // Only at the end of the entries known: the header of the tree is read from the file again
    if (fEntry >= fTree->GetEntries()) fTree->Refresh();
    return fTree->GetEntries();
}

void TRestDAQViewer::NextEvent() {
    if (!fReader || !fTree) return;

    const Long64_t totalEntries = RefreshEntries();

    if (fEntry >= totalEntries) {
        if (fIsRunning) {
            fStatusLabel->SetText(Form("Status: Waiting DAQ... (Processed: %lld/%lld)",
                                       fCumulativeEntries + fEntry, fCumulativeEntries + totalEntries));
        } else {
            fStatusLabel->SetText(Form("Status: End of File (%lld/%lld)", fCumulativeEntries + fEntry,
                                       fCumulativeEntries + totalEntries));
        }
        return;
    }

    const int stepSize = fStepSize->GetNumber();

    int eventsToProcess = (fIsRunning) ? stepSize : 1;
    if (fEntry + eventsToProcess >= totalEntries) eventsToProcess = totalEntries - fEntry;

    // Per view in an event: total amplitude, and the amplitude and weighted position of the strips of
    // each axis (0 X, 1 Y, 2 Z)
    struct ViewSums {
        double total = 0, amplitude[3] = {0, 0, 0}, position[3] = {0, 0, 0};
    };
    std::vector<ViewSums> sums(fViews.size());
    std::vector<short> pulse;

    for (int i = 0; i < eventsToProcess; ++i) {
        // Check if we reached the end of the available data
        if (fEntry >= totalEntries) break;

        if (fReader->SetEntry(fEntry) != TTreeReader::kEntryValid) break;

        const int eventID = **fReaderEventID;
        const double timestamp = **fReaderSeconds + **fReaderNanoSeconds * 1e-9;
        const std::vector<int>& signalIDs = **fReaderSignalsID;
        const std::vector<short>& samples = **fReaderPulses;
        const std::vector<int>& offsets = **fReaderOffsets;

        // Rate logic
        if (fEntry == 0) {
            fTimeRateStart = timestamp;
            fRateGraph->SetPoint(fRatePoints, timestamp, 0.);
        } else if (fEntry % stepSize == 0) {
            double dt = timestamp - fTimeRateStart;
            if (dt > 0) fRateGraph->SetPoint(fRatePoints++, (timestamp + fTimeRateStart) / 2., stepSize / dt);
            fTimeRateStart = timestamp;
        }

        const bool shouldDraw = (i == eventsToProcess - 1);
        if (shouldDraw) {
            ClearEvent();
            fStatusLabel->SetText(Form("Status: Running (Ev: %lld / %lld)", fCumulativeEntries + fEntry,
                                       fCumulativeEntries + totalEntries));
        }

        std::fill(sums.begin(), sums.end(), ViewSums());
        for (size_t s = 0; s < signalIDs.size() && s < offsets.size(); s++) {
            const int sID = signalIDs[s];
            const int view = GetViewOfSignal(sID);
            if (view < 0) continue;

            // Samples of the signal s: from its offset to the next one
            const int first = offsets[s];
            const int last = s + 1 < offsets.size() ? offsets[s + 1] : (int)samples.size();
            if (first < 0 || last > (int)samples.size() || first >= last) continue;
            pulse.assign(samples.begin() + first, samples.begin() + last);

            if (shouldDraw) {
                auto& stack = fViews[view].pulses;
                if (!stack)
                    stack = new THStack(Form("hs%d", view),
                                        Form("%s, event %d", fViews[view].name.c_str(), eventID));
                const int nPoints = pulse.size();
                TH1S* h = new TH1S(Form("h_s%d", sID), "", nPoints, 0, nPoints);
                h->SetDirectory(nullptr);
                for (int p = 0; p < nPoints; p++) h->SetBinContent(p + 1, pulse[p]);
                h->SetLineColor((sID % 72) + 1);
                fHistos.push_back(h);
                stack->Add(h);
            }

            const double amplitude = GetAmplitude(pulse);
            if (amplitude <= 0) continue;
            auto& sum = sums[view];
            sum.total += amplitude;
            const auto strip = fStripPosition.find(sID);
            if (strip != fStripPosition.end()) {
                sum.position[strip->second.first] += amplitude * strip->second.second;
                sum.amplitude[strip->second.first] += amplitude;
            }
            // Projections: one more event with a signal in the volumes of the channel
            const auto bins = fChannelBins.find(sID);
            if (bins != fChannelBins.end()) {
                for (auto projection : fViews[view].projections)
                    for (int bin : bins->second)
                        projection->SetBinContent(bin, projection->GetBinContent(bin) + 1);
            }
        }

        for (size_t n = 0; n < fViews.size(); n++) {
            const auto& sum = sums[n];
            const auto& view = fViews[n];
            if (sum.total > 0) view.spectrum->Fill(sum.total);
            if (!view.hitmap) continue;
            const int a = view.stripAxes[0], b = view.stripAxes[1];
            if (sum.amplitude[a] > 0 && sum.amplitude[b] > 0)
                view.hitmap->Fill(sum.position[a] / sum.amplitude[a], sum.position[b] / sum.amplitude[b]);
        }

        fEntry++;
    }

    // Update the canvas to show the latest processed data
    DrawCanvas();
}

void TRestDAQViewer::ManualReset() {
    for (auto& view : fViews) {
        view.spectrum->Reset();
        if (view.hitmap) view.hitmap->Reset();
        for (auto projection : view.projections)
            if (projection) projection->ClearBinContents();
    }

    fRateGraph->Set(0);
    fRateGraph->SetPoint(0, 0, 0);
    fRatePoints = 0;
    fTimeRateStart = -1;
    fMainCanvas->GetCanvas()->Update();
}

void TRestDAQViewer::SavePlots() {
    TString outName = Form("Plots_Run%05d_Ev%lld.pdf", fCurrentRun, fCumulativeEntries + fEntry);
    fMainCanvas->GetCanvas()->SaveAs(outName);
}

void TRestDAQViewer::OpenFile() {
    TGFileInfo fi;
    const char* ft[] = {"ROOT", "*.root", 0, 0};
    fi.fFileTypes = ft;
    new TGFileDialog(gClient->GetRoot(), this, kFDOpen, &fi);
    if (fi.fFilename) {
        fCumulativeEntries = 0;
        LoadDataFile(fi.fFilename);
    }
}

void TRestDAQViewer::OpenConfig() {
    TGFileInfo fi;
    const char* ft[] = {"YAML", "*.yaml", "YAML", "*.yml", "All", "*", 0, 0};
    fi.fFileTypes = ft;
    new TGFileDialog(gClient->GetRoot(), this, kFDOpen, &fi);
    if (fi.fFilename) LoadConfigFile(fi.fFilename);
}

void TRestDAQViewer::ReloadConfig() {
    if (fConfigFile == "") return;
    LoadConfigFile(TString(fConfigFile).Data());
}

void TRestDAQViewer::ReloadRun() {
    if (fBaseFileName == "") return;
    CheckNewFile(0);
}

void TRestDAQViewer::ToggleAuto() {
    if (!fIsRunning) {
        fIsRunning = true;
        fTimer->Start(100, kFALSE);
        fWatchdogTimer->Start(5000, kFALSE);
        ((TGTextButton*)gTQSender)->SetText("Stop Monitor");
    } else {
        fIsRunning = false;
        fTimer->Stop();
        fWatchdogTimer->Stop();
        ((TGTextButton*)gTQSender)->SetText("&DAQ Live Mode");
    }
}

void TRestDAQViewer::CheckForNewData() {
    if (!fTree || !fIsRunning || fCheckNextRun->GetState() != kButtonDown) return;

    // Events of the current file not read yet
    if (fEntry < RefreshEntries()) return;

    CheckNewFile();
}

void TRestDAQViewer::CheckNewFile(int offset) {
    // Extract directory and build search patterns
    TString dirName = gSystem->DirName(fBaseFileName);
    void* dir = gSystem->OpenDirectory(dirName);
    if (!dir) return;

    TString nextSubrunPattern;
    nextSubrunPattern.Form("Run%05d_.*_%03d.root", fCurrentRun, fCurrentSubrun + 1);

    TString nextRunPattern;
    nextRunPattern.Form("Run%05d_.*_000.root", fCurrentRun + offset);

    const char* entry;
    TString foundNextSubrun = "";
    TString foundNextRun = "";

    // Scan directory for matches
    TPRegexp reSub(nextSubrunPattern);
    TPRegexp reRun(nextRunPattern);

    while ((entry = gSystem->GetDirEntry(dir))) {
        TString fileName = entry;

        // TPRegexp::Match returns the number of matches found
        if (reSub.Match(fileName) > 0) {
            foundNextSubrun = dirName + "/" + fileName;
            break;  // Subrun takes priority
        }
        if (reRun.Match(fileName) > 0) {
            foundNextRun = dirName + "/" + fileName;
        }
    }
    gSystem->FreeDirectory(dir);

    // Load found files
    if (foundNextSubrun != "") {
        std::cout << ">>> New Subrun found: " << std::endl;
        fCurrentSubrun++;
        fBaseFileName = foundNextSubrun;
        fCumulativeEntries += fTree ? fTree->GetEntries() : 0;
        LoadDataFile(foundNextSubrun, false);
        fDataPathEntry->SetText(gSystem->BaseName(fBaseFileName));
        fDataPathEntry->SetToolTipText(fBaseFileName);

    } else if (foundNextRun != "") {
        std::cout << ">>> New Run detected: " << std::endl;
        ManualReset();  // Reset histograms for new main Run
        fCurrentRun += offset;
        fCumulativeEntries = 0;
        fCurrentSubrun = 0;
        fBaseFileName = foundNextRun;
        LoadDataFile(foundNextRun);
    }
}

void TRestDAQViewer::ClearEvent() {
    for (auto& view : fViews) {
        delete view.pulses;
        view.pulses = nullptr;
    }
    for (auto h : fHistos) delete h;
    fHistos.clear();
}

void TRestDAQViewer::ParseRunSubrun(TString fn, int& r, int& s) {
    // Matches "Run" + digits + anything + "_" + digits + ".root"
    TObjArray* sa = TPRegexp("Run(\\d+).+_(\\d+)\\.root$").MatchS(gSystem->BaseName(fn));
    if (sa && sa->GetEntries() > 2) {
        r = ((TObjString*)sa->At(1))->GetString().Atoi();
        s = ((TObjString*)sa->At(2))->GetString().Atoi();
    }
    delete sa;
}

TRestDAQViewer::~TRestDAQViewer() {
    DeleteViews();
    CloseDataFile();
    delete fTimer;
    delete fWatchdogTimer;
    delete fRateGraph;
}
