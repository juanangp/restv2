#ifndef TRESTRAWDAQVIEWER_H
#define TRESTRAWDAQVIEWER_H

#include <TGFrame.h>
#include <TString.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>

#include <map>
#include <string>
#include <vector>

class TChain;
class TGCheckButton;
class TGLabel;
class TGNumberEntry;
class TGTextEntry;
class TGraph;
class TH1F;
class TH1S;
class TH2F;
class THStack;
class TRootEmbeddedCanvas;
class TTimer;

/// \class TRestRawDAQViewer
/// \brief Online event display of the raw signal files (formerly the EventDisplay macro of FEMDAQ): the
/// signals of each event, the amplitude spectrum, the hit map of each readout and the rate.
///
/// It reads the tree TRestRawSignalEvent of a REST file (e.g. written by restDAQ) and, in the live mode,
/// follows the file while it is written and opens the next subrun or run
/// (Run<run>_..._<subrun>.root, first subrun 000) when "Auto-load Next Run" is checked. Launched with the
/// macro REST_Raw_DAQViewer() of restRoot.
///
/// The amplitude of a signal is its maximum (in the pulse range) if it is above the baseline (baseline
/// range) plus the sigma threshold times the baseline sigma. The hit map needs a decoding file with the
/// position of each channel, one line per channel: `daqID axis position`, with axis X or Y followed by the
/// readout number (e.g. X2; X: readout 1). The settings are kept in ~/.restdaq_viewer.
class TRestRawDAQViewer : public TGMainFrame {
   private:
    TRootEmbeddedCanvas* fMainCanvas;
    TGTextEntry* fDataPathEntry;
    TGTextEntry* fDecoPathEntry;
    TGLabel* fStatusLabel;
    TGCheckButton* fCheckNextRun;

    // Pulse Parameter Entries
    TGNumberEntry *fBaseStart, *fBaseEnd;
    TGNumberEntry *fPulseStart, *fPulseEnd;
    TGNumberEntry* fSigmaThr;
    TGNumberEntry* fStepSize;
    TGNumberEntry* fSpecMaxEntry;

    TChain* fChain = nullptr;
    TTreeReader* fReader = nullptr;
    std::map<int, std::pair<std::string, double>> fDecodingMap;
    TTimer* fTimer;
    TTimer* fWatchdogTimer;

    // Branches of the TRestRawSignalEvent tree
    TTreeReaderValue<std::vector<int>>* fReaderSignalsID = nullptr;
    TTreeReaderValue<std::vector<short>>* fReaderPulses = nullptr;
    TTreeReaderValue<std::vector<int>>* fReaderOffsets = nullptr;
    TTreeReaderValue<int>* fReaderEventID = nullptr;
    TTreeReaderValue<Long64_t>* fReaderSeconds = nullptr;
    TTreeReaderValue<int>* fReaderNanoSeconds = nullptr;

    int fEventID;
    double fTimestamp;
    std::vector<int>* fSignalsID = nullptr;
    std::vector<short>* fPulses = nullptr;

    Long64_t fEntry = 0;
    int fNChannels = 1;
    double fTimeRateStart = -1;
    int fRatePoints = 0;
    bool fIsRunning = false;

    int fCurrentRun = -1;
    int fCurrentSubrun = -1;
    int fCumulativeEntries = 0;
    TString fBaseFileName = "";
    TString fLastDecoFile = "";

    // Default Pulse Params
    int fBLS = 10, fBLE = 110, fPS = 100, fPE = 500;
    double fSThr = 7.0;
    int fSSz = 100;

    int fNReadouts = 1;

    std::map<int, TH1F*> fSpectraMap;
    std::map<int, TH2F*> fHitMapMap;
    std::map<int, THStack*> fStacksMap;
    TGraph* fRateGraph;
    std::vector<TH1S*> fHistos;

    /// \brief File of the settings of the viewer (~/.restdaq_viewer)
    static std::string GetConfigFileName();

   public:
    TRestRawDAQViewer();
    virtual ~TRestRawDAQViewer();

    void DoExit();
    void SaveConfig();
    void LoadConfig();
    void GetParamsFromPulse(const std::vector<short>& p, double& amp, double& area, int& mP);
    void LoadDataFile(const char* path, bool newRun = true);
    void LoadDecoding(const char* path);
    void UpdateCanvasLayout();
    void DrawCanvas();
    void NextEvent();
    void UpdateRange(Long_t);
    void ManualReset();
    void SavePlots();
    void OpenFile();
    void ReloadRun();
    void OpenDecoding();
    void ToggleAuto();
    void CheckForNewData();
    void CheckNewFile(int offset = 1);
    void ClearEvent();
    void ParseRunSubrun(TString fn, int& r, int& s);
    static std::map<int, std::pair<std::string, double>> ReadDecoding(const std::string& filedec);

    ClassDefOverride(TRestRawDAQViewer, 0);
};

#endif
