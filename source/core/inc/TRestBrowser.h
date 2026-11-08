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

/// \class TRestBrowser
/// \brief Event browser of REST files: a ROOT TBrowser with a control panel (entry, event ID and subID,
/// event type, plot option, open file, exit) on the left, the event viewer (TRestEventViewer or a derived
/// viewer) on the right, and a selection box with previous/next buttons at the bottom.
///
/// The file is opened with a TRestRun; the selected event type is the last saved event of the file
/// (TRestRun::GetOutputEvent). The selection is an expression of the analysis tree observables
/// (TTreeFormula) used to jump to the previous/next entry passing it. Opened, e.g., with the macro
/// REST_ViewEvents.
class TRestBrowser {
   protected:
    /// \name GUI widgets
    /// Frames, labels, number entries, text boxes and buttons of the left and bottom control panels, the
    /// default canvas, the shown entry, event ID and subID, the ROOT browser and the run of the file.
    ///@{
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
                                   ///@}

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
    /// \brief Loads the next (step 1) or previous (step -1) entry, the next one passing the selection
    /// (selection box, an expression of the observables of the analysis tree) if it is not empty.
    void SelectEvent(int step);

   public:
    /// \brief Creates the ROOT browser (if not created yet), the control panels, the default canvas and an
    /// empty TRestRun (if not created yet).
    /// \throws std::runtime_error if there is no graphical interface.
    /// \param opt TBrowser option string.
    void Initialize(const std::string& opt = "FI");
    /// \brief Not implemented (only prints its name).
    static void InitFromConfigFile();
    /// \brief Opens a REST file (replacing the previous one), fills the event types and shows the first
    /// entry. A browser created without file gets the control panels (Initialize) first.
    /// \param filename File name or URL.
    /// \return `true` if the file was opened.
    bool OpenFile(const std::string& filename);

    /// \brief Selects an event type of the file and plots the shown entry with it.
    /// \param eventType Event class name.
    void SetEvent(const std::string& eventType);
    /// \brief Sets the title of the control panel window (nothing before the control panels exist).
    inline void SetWindowName(const std::string& windowName) {
        if (frmMain != nullptr) frmMain->SetWindowName(windowName.c_str());
    }

    /// \brief The event viewer, nullptr if none.
    inline TRestEventViewer* GetViewer() const { return fEventViewer; }

    /// \name GUI actions
    /// Slots connected to the widgets of the control panels.
    ///@{
    void LoadFileAction();     ///< Opens a file chosen in a file dialog.
    static void ExitAction();  ///< Exits the application.

    void RowValueChangedAction(long val);  ///< Loads the entry of the entry box.
    void IdValueChangedAction(long val);   ///< Loads the event of the ID and subID boxes.
    void NextEventAction();                ///< Next entry (passing the selection, if any).
    void PreviousEventAction();            ///< Previous entry (passing the selection, if any).

    void EventTypeChangedAction(int id);  ///< Selects the event type of the combo box.

    void NextPlotOptionAction();      ///< Increases a numeric plot option and plots.
    void PreviousPlotOptionAction();  ///< Decreases a numeric plot option and plots.
    void PlotAction();                ///< Plots the event with the plot option.
    ///@}

    /// \brief Browser of the file of the current ROOT directory, if any, with the default viewer;
    /// otherwise a plain ROOT browser, which gets the control panels when a file is opened (OpenFile).
    TRestBrowser();
    /// \brief Browser with a viewer created from its class name (it must contain "Viewer" and derive from
    /// TRestEventViewer, std::invalid_argument otherwise).
    /// \param viewerName Class name of the viewer, e.g. TRestEventViewer.
    /// \param geomScale Factor from hit positions to geometry units.
    TRestBrowser(const std::string& viewerName, double geomScale = 1.0);
    /// Browser with a viewer already created (e.g. a viewer defined in a macro, without dictionary). The
    /// browser takes its ownership (deleted by the destructor).
    TRestBrowser(TRestEventViewer* viewer, double geomScale = 1.0);

    /// \brief Destructor: cleans up the control panel and deletes the viewer and the run (owned).
    ~TRestBrowser();
};
#endif
