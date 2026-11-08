#ifndef TRESTANALYSISPLOT_H
#define TRESTANALYSISPLOT_H

#include <TCanvas.h>
#include <TH1.h>
#include <THStack.h>
#include <TLatex.h>
#include <TLegend.h>

#include <ROOT/RDataFrame.hxx>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "TRestMetadata.h"

class TRestCut;
class TRestDataSet;
class TRestManager;

/// \class TRestAnalysisPlot
/// \brief Combined canvas of panels (text with run information and observable statistics) and plots
/// (1D and 2D histograms of the analysis tree observables) of one or several files, with TRestCut.
///
/// See the class documentation in TRestAnalysisPlot.cxx for all the keys of panels, plots and histograms
/// and the {expressions} of the labels. Short example:
///
/// \code{.yaml}
/// plots:
///   class: TRestAnalysisPlot
///   inputFileName: "R*_ANA.root"       # empty: the output of the pipeline of the manager
///   cuts: ["fiducial"]
///   previewPlot: false
///   save: "Plots_{run.runTag}.pdf"
///   outputFileName: "R[runNumber]_plots.root"
///   paletteStyle: 57
///   canvasSize: [1200, 600]
///   canvasDivisions: [2, 1]
///   canvasDivisionMargins: [0.01, 0.01]
///   legendPosition: [0.7, 0.75, 0.88, 0.88]
///   panels:
///     - labels:
///         - {text: "Rate: {entries/runLength|%.1f} per hour", position: [0.1, 0.8]}
///   plots:
///     - name: "Spectrum"
///       xLabel: "Energy (ADC)"
///       variables: [{name: "trackLineAnalysis_Energy", range: [0, 1e6], nBins: 200}]
/// \endcode
class TRestAnalysisPlot : public TRestMetadata {
   public:
    /// Observable of a histogram axis
    struct Variable {
        std::string name;  ///< Observable or expression of observables.
        int nBins = 100;   ///< Number of bins.
        /// Range, automatic if min >= max
        double min = 0, max = 0;
    };

    /// A histogram of a plot
    struct Histo {
        std::string name;                 ///< Name (legend and object name in the output file).
        std::string drawOption;           ///< ROOT draw option (`option`).
        std::vector<Variable> variables;  ///< 1 (1D) or 2 (2D: X, Y)
        std::vector<std::string> cuts;    ///< TRestCut names applied to this histogram.
        int lineColor = kBlue + 2, lineWidth = 1, lineStyle = 1;  ///< Line attributes.
        int fillColor = 0, fillStyle = 1001;                      ///< Fill attributes.
        bool stats = false;                                       ///< Show the stats box.
        /// Filled histogram
        std::unique_ptr<TH1> histo;  //!
    };

    /// A pad with histograms
    struct Plot {
        std::string name, title, xLabel, yLabel;  ///< Name, pad title and axis labels.
        bool logX = false, logY = false, logZ = false, gridX = false, gridY = false;  ///< Axis options.
        bool legend = false, timeDisplay = false;  ///< Draw a legend; X axis as dates.
        /// Integral of each histogram after normalization (0: none)
        double normalize = 0;
        /// Expression the histograms are divided by (binSize, entries, integral, runLength)
        std::string scale;
        std::string stackOption = "nostack";  ///< THStack draw option.
        double marginLeft = 0.15, marginRight = 0.1, marginTop = 0.07, marginBottom = 0.15;  ///< Pad margins.
        /// File the pad is saved to alone (empty: none)
        std::string save;
        std::vector<Histo> histos;  ///< Histograms of the pad.
        /// Stack of the histograms
        std::unique_ptr<THStack> stack;  //!
        /// Legend of the pad
        std::unique_ptr<TLegend> legendBox;  //!
    };

    /// Text of a panel at (x, y) (pad coordinates); {expression|format} are replaced (see the class doc)
    struct Label {
        std::string text;         ///< Text with {expression|format}.
        double x = 0.1, y = 0.9;  ///< Position (`position: [x, y]`).
    };

    /// A pad with labels
    struct Panel {
        double textSize = 0.05;  ///< Text size.
        /// Significant digits of the values without format
        int precision = 3;
        std::vector<std::string> cuts;  ///< TRestCut names (besides the global ones).
        std::vector<Label> labels;      ///< Texts of the panel.
        /// Drawn texts
        std::vector<std::unique_ptr<TLatex>> texts;  //!
    };

   private:
    std::vector<Panel> fPanels;  ///< Panels read from `panels`
    std::vector<Plot> fPlots;    ///< Plots read from `plots`
    /// Combined canvas. Not a unique_ptr: ROOT deletes it when its window is closed
    TCanvas* fCanvas = nullptr;  //!

    void ReadPanels();
    void ReadPlots();

    /// \brief Draws (and saves) the combined canvas with the frame. metadata: instance -> configuration
    /// ({instance.key}), run: TRestRun ({run.key}), times of the runs (seconds; duration 0: from the first
    /// and last timeSeconds)
    void DrawFrame(ROOT::RDF::RNode frame, const std::map<std::string, YAML::Node>& metadata,
                   const YAML::Node& run, double startTime, double endTime, double duration,
                   const TRestManager* manager);

   public:
    /// Input: a run file, a dataset or a pattern of them (e.g. R*_ANA.root). Empty: the output of the
    /// pipeline of the manager
    std::string fInputFileName = "";
    /// Names of the TRestCut (in the manager) applied to all the data
    std::vector<std::string> fCutNames = {};
    /// Image of the combined canvas (pdf, png...), {expression} are replaced. Empty: not saved
    std::string fSave = "";
    /// ROOT file of the canvas and the histograms, [parameter] of the run and {expression} are replaced.
    /// A data file (with analysis tree) is updated, otherwise it is created. Empty: not written
    std::string fOutputFileName = "";
    /// If true the canvas is shown, and saved when Enter is pressed (it can be modified before)
    bool fPreviewPlot = true;
    int fPaletteStyle = 57;                                     ///< ROOT palette (`paletteStyle`).
    std::vector<int> fCanvasSize = {800, 600};                  ///< Canvas size in pixels.
    std::vector<int> fCanvasDivisions = {2, 2};                 ///< Pads in X and Y.
    std::vector<double> fCanvasDivisionMargins = {0.01, 0.01};  ///< Margins between pads.
    /// Legend box (x1, y1, x2, y2) in pad coordinates
    std::vector<double> fLegendPosition = {0.7, 0.75, 0.88, 0.88};
    /// `panels` and `plots` as written, read by ReadPanels and ReadPlots
    YAML::Node fPanelsConfig;
    YAML::Node fPlotsConfig;  ///< `plots` as written

    TRestAnalysisPlot();  ///< Default constructor.
    /// \brief Constructs from a YAML node (LoadConfig).
    TRestAnalysisPlot(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructs from a section of a YAML file (LoadConfig).
    TRestAnalysisPlot(const std::string& fileName, const std::string& sectionName);
    ~TRestAnalysisPlot() override;  ///< Destructor.

    /// \brief Returns "TRestAnalysisPlot".
    std::string GetClassName() const override { return "TRestAnalysisPlot"; }

    /// \brief Reads the fields and the panels and plots.
    void LoadConfig() override;
    /// \brief Nothing to initialize.
    void Initialize() override {}

    /// \brief Draws (and saves) the combined canvas with the analysis trees of the files: a TRestDataSet is
    /// generated in memory from them (run files or datasets) and drawn as PlotCombinedCanvas(dataSet).
    /// \param fileName ROOT file or pattern of files (e.g. R*_ANA.root).
    /// \param manager Manager with the TRestCut used (needed only if there are cuts); its metadata are
    /// written in a new outputFileName. The [parameter] of outputFileName are those of the run of the
    /// pipeline (fRunInfo), otherwise those of the first run of the input.
    void PlotCombinedCanvas(const std::string& fileName, const TRestManager* manager = nullptr);

    /// \brief Draws (and saves) the combined canvas with the data frame of a dataset (generated or opened).
    void PlotCombinedCanvas(TRestDataSet& dataSet, const TRestManager* manager = nullptr);

    /// \brief The combined canvas of the last PlotCombinedCanvas, nullptr if none.
    TCanvas* GetCanvas() const { return fCanvas; }
};

#endif
