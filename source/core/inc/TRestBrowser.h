#ifndef RestCore_TRestBrowser
#define RestCore_TRestBrowser

#include <TApplication.h>
#include <TBrowser.h>
#include <TCanvas.h>
#include <TGButton.h>
#include <TGComboBox.h>
#include <TGDimension.h>
#include <TGFileDialog.h>
#include <TGFrame.h>
#include <TGLabel.h>
#include <TGNumberEntry.h>
#include <TGTextEntry.h>
#include <TObject.h>
#include <TROOT.h>
#include <TSystem.h>

class TRestEventViewer;
class TRestEveEventViewer;
class TRestRun;

/// Event browser for different input file
class TRestBrowser {
   protected:
    TGMainFrame* frmMain = nullptr;  //!
    TGMainFrame* frmBot = nullptr;   //!

    // Frames and buttons on left//!
    TGVerticalFrame* fVFrame = nullptr;             //! < The main vertical frame for browser controlling
    TGLabel* fEventRowLabel = nullptr;              //!
    TGLabel* fEventIdLabel = nullptr;               //!
    TGLabel* fEventSubIdLabel = nullptr;            //!
    TGNumberEntry* fEventRowNumberBox = nullptr;    //! For row number.
    TGNumberEntry* fEventIdNumberBox = nullptr;     //! For Event number.
    TGNumberEntry* fEventSubIdNumberBox = nullptr;  //! For sub Event number.

    TGLabel* fEventTypeLabel = nullptr;        //!
    TGComboBox* fEventTypeComboBox = nullptr;  //!

    TGLabel* fPlotOptionLabel = nullptr;        //!
    TGTextEntry* fPlotOptionTextBox = nullptr;  //! TextBox for plot options
    TGPictureButton* fButOptPrev = nullptr;     //! Previous plot option
    TGTextButton* fButOptRefresh = nullptr;     //! Refresh plot
    TGPictureButton* fButOptNext = nullptr;     //! Next plot option

    TGPictureButton* fMenuOpen = nullptr;  //! Open file button
    TGPictureButton* fExit = nullptr;      //! Exit button

    // Frames and buttons on bottom
    TGVerticalFrame* fHFrame = nullptr;         //!
    TGLabel* fSelectionTextBoxLabel = nullptr;  //!
    TGTextEntry* fSelectionTextBox = nullptr;   //! TextBox for plot options
    TGTextButton* fButEvePrev = nullptr;        //! Previous plot option
    TGTextButton* fButEveNext = nullptr;        //! Refresh plot

    TCanvas* fCanDefault = nullptr;  //!
    int fEventRow = 0;               //!
    int fEventId = 0;                //!
    int fEventSubId = 0;             //!

    TBrowser* fBrowser = nullptr;  //!
    TRestRun* fRestRun = nullptr;  //!

   private:
    bool pureAnalysis;           //!
    std::string fInputFileName;  //!

    TRestEventViewer* fEventViewer = nullptr;  //!

    /// geomScale: factor from the hit positions (mm) to the geometry ones. The geometry is in mm
    /// (TGeoManager with Geant4 units, REST_Units::SetGeoManagerDefaultUnits), so 1; legacy used 0.1 (cm)
    void SetViewer(TRestEventViewer* eV, double geomScale = 1.0);
    void SetViewer(const std::string& viewerName, double geomScale = 1.0);
    void SetLeftPanelButtons();
    void SetBottomPanelButtons();
    bool LoadEventId(int eventID, int subEventID = -1);
    bool LoadEventEntry(int n);

   public:
    // tool method
    void Initialize(const std::string& opt = "FI");
    static void InitFromConfigFile();
    bool OpenFile(const std::string& filename);

    // setters
    void SetEvent(const std::string& eventType);
    inline void SetWindowName(const std::string& windowName) { frmMain->SetWindowName(windowName.c_str()); }

    // getters
    inline TRestEventViewer* GetViewer() const { return fEventViewer; }

    // actions
    void LoadFileAction();
    static void ExitAction();

    void RowValueChangedAction(long val);
    void IdValueChangedAction(long val);
    void NextEventAction();
    void PreviousEventAction();

    void EventTypeChangedAction(int id);

    void NextPlotOptionAction();
    void PreviousPlotOptionAction();
    void PlotAction();

    // Constructors
    TRestBrowser();
    TRestBrowser(const std::string& viewerName, double geomScale = 1.0);
    /// Browser with a viewer already created (e.g. a viewer defined in a macro, without dictionary). The
    /// browser takes its ownership.
    TRestBrowser(TRestEventViewer* viewer, double geomScale = 1.0);

    // Destructor
    ~TRestBrowser();
};
#endif
