#ifndef TRESTDAQVIEWER_H
#define TRESTDAQVIEWER_H

#include <TGFrame.h>
#include <TString.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "TRestDetectorReadoutManager.h"
#include "TRestRawSignalAnalysisProcess.h"

class TFile;
class TTree;
class TGCheckButton;
class TGLabel;
class TGNumberEntry;
class TGTextEntry;
class TGraph;
class TH1F;
class TH1S;
class TH2F;
class TH2Poly;
class THStack;
class TRootEmbeddedCanvas;
class TTimer;

/// \class TRestDAQViewer
/// \brief Online event display of raw signal files (formerly the EventDisplay macro of FEMDAQ): for each
/// readout, the amplitude spectrum, the hit map and the signals of the event, plus the rate.
///
/// It reads the tree TRestRawSignalEvent of a REST file (e.g. written by femclient, package restDAQ) and,
/// in the live mode, follows the file while it is written (TTree::Refresh, it sees the events of the last
/// AutoSave of femclient) and opens the next subrun or run
/// (Run<run>_..._<subrun>.root, first subrun 000) when "Auto-load Next Run" is checked. Launched with the
/// macro REST_DAQViewer(configFile, dataFile) of restRoot.
///
/// The configuration is a YAML of restv2, e.g. the one of the analysis with restManager; the viewer takes
/// from it (at any depth):
///
/// * **TRestDetectorReadoutManager**: the readouts (inputFileName, instanceName, decodingName). Each readout
///   is drawn separately with the signals of its decoding. Its hit map, chosen automatically:
///   - strips on two axes (TRestDetectorReadout::GetPositionFromChannel gives a single coordinate, e.g. x or
///     y of a Micromegas, not necessarily X and Y): the amplitude-weighted mean of the strips of each axis.
///   - otherwise, the volumes of the channels in the geometry of the readout (e.g. the scintillators of a
///     veto readout, TRestGDMLReadout): their projections on the XY, XZ and YZ planes, each volume a bin
///     with the number of events with a signal in its channel (volumes overlapping in a projection are
///     drawn one over the other).
///
///   Without readouts, all the signals in a single view without hit map.
/// * **TRestRawSignalAnalysisProcess**: baselineRange, baselineOption, integralRange, pointsThreshold,
///   signalThreshold, pointsOverThreshold, nPointsFlat and maxOption. The amplitude of a signal is its
///   AmpSgnMaxMethod (a signal with at least 2 points over threshold); the spectrum of a readout is the sum
///   of the amplitudes of its signals in the event. Without it, the default values of the process.
/// * **parameters** (the section `parameters` of restv2, also with --set): the binning of the spectra,
///   `spectrumBins`, `spectrumMin` and `spectrumMax` (default 2048, 0 and 100000), and for a readout
///   `spectrumBins_<instanceName>`, `spectrumMin_<instanceName>` and `spectrumMax_<instanceName>` (the
///   others by default). A parameter not used (e.g. a readout misspelt) is warned.
///
/// The readout files are relative to the current directory. The settings of the window (files, step)
/// are kept in $REST_HOME/.rest/DAQViewer_settings (~/.rest without
/// REST_HOME).
class TRestDAQViewer : public TGMainFrame {
   private:
    /// One readout: its histograms and the signals of the event
    struct ReadoutView {
        std::string name;
        /// nullptr: all the signals (no readout loaded)
        TRestDetectorReadout* readout = nullptr;
        TH1F* spectrum = nullptr;
        /// Readouts with strips on two axes: the hit map, and its axes (0 X, 1 Y, 2 Z)
        TH2F* hitmap = nullptr;
        int stripAxes[2] = {-1, -1};
        /// Other readouts with geometry: projections XY, XZ and YZ of the volumes of the channels
        TH2Poly* projections[3] = {nullptr, nullptr, nullptr};
        THStack* pulses = nullptr;
    };

    TRootEmbeddedCanvas* fMainCanvas;
    TGTextEntry* fDataPathEntry;
    TGTextEntry* fConfigPathEntry;
    TGLabel* fStatusLabel;
    TGCheckButton* fCheckNextRun;
    TGNumberEntry* fStepSize;

    /// File being read (a single subrun) and its tree TRestRawSignalEvent, refreshed (TTree::Refresh) to
    /// read the events written after it was opened
    TFile* fFile = nullptr;
    TTree* fTree = nullptr;
    TTreeReader* fReader = nullptr;
    TTimer* fTimer;
    TTimer* fWatchdogTimer;

    // Branches of the TRestRawSignalEvent tree
    TTreeReaderValue<std::vector<int>>* fReaderSignalsID = nullptr;
    TTreeReaderValue<std::vector<short>>* fReaderPulses = nullptr;
    TTreeReaderValue<std::vector<int>>* fReaderOffsets = nullptr;
    TTreeReaderValue<int>* fReaderEventID = nullptr;
    TTreeReaderValue<Long64_t>* fReaderSeconds = nullptr;
    TTreeReaderValue<int>* fReaderNanoSeconds = nullptr;

    Long64_t fEntry = 0;
    double fTimeRateStart = -1;
    int fRatePoints = 0;
    bool fIsRunning = false;

    int fCurrentRun = -1;
    int fCurrentSubrun = -1;
    int fCumulativeEntries = 0;
    TString fBaseFileName = "";
    TString fConfigFile = "";

    /// Binning of the amplitude spectrum
    struct SpectrumBinning {
        int bins = 2048;
        double min = 0;
        double max = 100000;
    };
    /// Spectrum of the readouts without their own binning, and the binning of each readout (instanceName),
    /// from the parameters of the configuration
    SpectrumBinning fDefaultSpectrum;
    std::map<std::string, SpectrumBinning> fSpectrumOfReadout;

    /// Readouts of the configuration (owned), and the pulse parameters
    std::unique_ptr<TRestDetectorReadoutManager> fReadoutManager;
    std::unique_ptr<TRestRawSignalAnalysisProcess> fPulseParameters;

    std::vector<ReadoutView> fViews;
    /// View of each signal ID (-1: none), found once
    std::map<int, int> fViewOfSignal;
    /// Strips of the readouts with hit map: (axis, position) of each signal ID
    std::map<int, std::pair<int, double>> fStripPosition;
    /// Bins of the projections of each signal ID (the same in the three), its volumes
    std::map<int, std::vector<int>> fChannelBins;

    TGraph* fRateGraph;
    std::vector<TH1S*> fHistos;

    /// \brief File of the settings of the viewer, in TRestTools::GetRESTUserDirectory()
    static std::string GetSettingsFileName();
    /// \brief First section with `class: className` of the node, at any depth: (key, section)
    static std::pair<std::string, YAML::Node> FindSection(const YAML::Node& node,
                                                          const std::string& className);

    /// \brief Index of the view of a signal ID, -1 if it is in no readout
    int GetViewOfSignal(int signalID);
    /// \brief Amplitude of a signal (AmpSgnMaxMethod of TRestRawSignalAnalysisProcess), 0 if it is not a
    /// signal (less than 2 points over threshold)
    double GetAmplitude(const std::vector<short>& pulse) const;
    /// \brief Entries of the tree, with those written since the last call (TTree::Refresh)
    Long64_t RefreshEntries();
    /// \brief Deletes the readers and closes the file
    void CloseDataFile();
    /// \brief Builds the views of the readouts, empty histograms
    void BuildViews();
    /// \brief Projections of the volumes of the channels of a readout (fChannelBins), for its hit map
    void BuildProjections(ReadoutView& view, int index, const std::map<int, int>& physicalToDAQ);
    void DeleteViews();

   public:
    /// \param configFile YAML with the TRestDetectorReadoutManager and the TRestRawSignalAnalysisProcess
    /// (empty: the last one used)
    /// \param dataFile REST file to open (empty: the last one used)
    TRestDAQViewer(const std::string& configFile = "", const std::string& dataFile = "");
    virtual ~TRestDAQViewer();

    void DoExit();
    void SaveSettings();
    /// \brief Reads the settings of the last session; its data and configuration files are returned
    void LoadSettings(std::string& dataPath, std::string& configPath);
    /// \brief Reads the readouts and the pulse parameters of a configuration file
    void LoadConfigFile(const char* path);
    void LoadDataFile(const char* path, bool newRun = true);
    void UpdateCanvasLayout();
    void DrawCanvas();
    void NextEvent();
    void ManualReset();
    void SavePlots();
    void OpenFile();
    void OpenConfig();
    void ReloadConfig();
    void ReloadRun();
    void ToggleAuto();
    void CheckForNewData();
    void CheckNewFile(int offset = 1);
    void ClearEvent();
    void ParseRunSubrun(TString fn, int& r, int& s);

    ClassDefOverride(TRestDAQViewer, 0);
};

#endif
