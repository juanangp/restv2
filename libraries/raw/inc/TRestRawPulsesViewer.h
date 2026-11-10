#ifndef TRESTRAWPULSESVIEWER_H
#define TRESTRAWPULSESVIEWER_H

#include <TGFrame.h>
#include <TString.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>

#include <vector>

class TFile;
class TGLabel;
class TGTextEntry;
class TMultiGraph;
class TRootEmbeddedCanvas;
class TTree;

/// \class TRestRawPulsesViewer
/// \brief Event viewer of raw signal files (formerly the DrawPulses macro of FEMDAQ): the signals of each
/// event drawn by ASIC, a pad for each ASIC with signals in the event (signal ID = channel + 72 ASIC + 288
/// card, as written by femclient and TRestRawMultiFEMINOSToSignalProcess), the color by channel.
///
/// It reads the tree TRestRawSignalEvent of a REST file; a file being written is followed (TTree::Refresh
/// at the last event). Buttons: open a file, previous and next event, jump to an entry, save the canvas
/// (PDF), quit. Quit, or closing the window, terminates the ROOT application. Launched with the macro
/// REST_Raw_PulsesViewer(fileName) of restRoot:
/// \code
/// restRoot
/// [0] REST_Raw_PulsesViewer("RawSignals.root")
/// \endcode
///
/// \author JuanAn García <juanangp@unizar.es>
class TRestRawPulsesViewer : public TGMainFrame {
   private:
    TRootEmbeddedCanvas* fCanvas;
    TGTextEntry* fEntryInput;
    TGLabel* fInfoLabel;

    TFile* fFile = nullptr;
    /// Tree TRestRawSignalEvent of the file (owned by it)
    TTree* fTree = nullptr;
    TTreeReader* fReader = nullptr;
    TTreeReaderValue<std::vector<int>>* fReaderSignalsID = nullptr;
    TTreeReaderValue<std::vector<short>>* fReaderSamples = nullptr;
    TTreeReaderValue<std::vector<int>>* fReaderOffsets = nullptr;
    TTreeReaderValue<int>* fReaderEventID = nullptr;

    Long64_t fEntry = 0;
    TString fFileName = "";
    /// Signals of the event drawn, one per ASIC (deleted at the next one)
    std::vector<TMultiGraph*> fGraphs;

    /// \brief Deletes the readers and closes the file
    void CloseFile();
    /// \brief Entries of the tree, with those written since it was read (TTree::Refresh) at the last one
    Long64_t RefreshEntries();
    void DeleteGraphs();

   public:
    /// \brief Creates and maps the window (canvas, buttons and entry box), without a file.
    TRestRawPulsesViewer();
    /// \brief Closes the file and deletes the graphs.
    virtual ~TRestRawPulsesViewer();

    /// \brief Opens a REST file with the tree TRestRawSignalEvent and draws its first entry
    void LoadFile(const char* path);
    /// \brief File dialog to choose a .root file, then LoadFile
    void OpenFile();
    /// \brief Draws the next entry (refreshing the tree at the last one, for a file being written)
    void Next();
    /// \brief Draws the previous entry
    void Prev();
    /// \brief Entry typed in the text box
    void JumpToEvent();
    /// \brief Saves the canvas as Event_<entry>.pdf in the current directory
    void SavePDF();
    /// \brief Terminates the ROOT application
    void DoExit();
    /// \brief Draws the current entry, a pad per ASIC
    void DrawEvent();

    ClassDefOverride(TRestRawPulsesViewer, 0);
};

#endif
