#ifndef TRESTANALYSISPLOT_H
#define TRESTANALYSISPLOT_H

#include <TCanvas.h>
#include <TH1.h>
#include <THStack.h>
#include <TLatex.h>
#include <TLegend.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "TRestMetadata.h"

class TRestCut;
class TRestManager;

/// \class TRestAnalysisPlot
/// \brief Combined canvas of panels (text with run information and observable statistics) and plots
/// (1D and 2D histograms of the analysis tree observables) of one or several files, with TRestCut.
class TRestAnalysisPlot : public TRestMetadata {
   public:
    /// Observable of a histogram axis
    struct Variable {
        std::string name;
        int nBins = 100;
        /// Range, automatic if min >= max
        double min = 0, max = 0;
    };

    struct Histo {
        std::string name;
        std::string drawOption;
        std::vector<Variable> variables;  ///< 1 (1D) or 2 (2D: X, Y)
        std::vector<std::string> cuts;
        int lineColor = kBlue + 2, lineWidth = 1, lineStyle = 1;
        int fillColor = 0, fillStyle = 1001;
        bool stats = false;
        std::unique_ptr<TH1> histo;  //!
    };

    /// A pad with histograms
    struct Plot {
        std::string name, title, xLabel, yLabel;
        bool logX = false, logY = false, logZ = false, gridX = false, gridY = false;
        bool legend = false, timeDisplay = false;
        /// Integral of each histogram after normalization (0: none)
        double normalize = 0;
        /// Expression the histograms are divided by (binSize, entries, integral, runLength)
        std::string scale;
        std::string stackOption = "nostack";
        double marginLeft = 0.15, marginRight = 0.1, marginTop = 0.07, marginBottom = 0.15;
        /// File the pad is saved to alone (empty: none)
        std::string save;
        std::vector<Histo> histos;
        std::unique_ptr<THStack> stack;     //!
        std::unique_ptr<TLegend> legendBox;  //!
    };

    /// Text of a panel at (x, y) (pad coordinates); {expression|format} are replaced (see the class doc)
    struct Label {
        std::string text;
        double x = 0.1, y = 0.9;
    };

    /// A pad with labels
    struct Panel {
        double textSize = 0.05;
        /// Significant digits of the values without format
        int precision = 3;
        std::vector<std::string> cuts;
        std::vector<Label> labels;
        std::vector<std::unique_ptr<TLatex>> texts;  //!
    };

   private:
    std::vector<Panel> fPanels;
    std::vector<Plot> fPlots;
    /// Combined canvas. Not a unique_ptr: ROOT deletes it when its window is closed
    TCanvas* fCanvas = nullptr;  //!

    void ReadPanels();
    void ReadPlots();

   public:
    /// Names of the TRestCut (in the manager) applied to all the data
    std::vector<std::string> fCutNames = {};
    /// Image of the combined canvas (pdf, png...), {expression} are replaced. Empty: not saved
    std::string fSave = "";
    /// If true the canvas is shown, and saved when Enter is pressed (it can be modified before)
    bool fPreviewPlot = true;
    int fPaletteStyle = 57;
    std::vector<int> fCanvasSize = {800, 600};
    std::vector<int> fCanvasDivisions = {2, 2};
    std::vector<double> fCanvasDivisionMargins = {0.01, 0.01};
    /// Legend box (x1, y1, x2, y2) in pad coordinates
    std::vector<double> fLegendPosition = {0.7, 0.75, 0.88, 0.88};

    TRestAnalysisPlot();
    TRestAnalysisPlot(const std::string& instanceName, const YAML::Node& node);
    TRestAnalysisPlot(const std::string& fileName, const std::string& sectionName);
    ~TRestAnalysisPlot() override;

    std::string GetClassName() const override { return "TRestAnalysisPlot"; }

    void LoadConfig() override;
    void Initialize() override {}

    /// \brief Draws (and saves) the combined canvas with the analysis trees of the files.
    /// \param fileName ROOT file or pattern of files (e.g. R*_ANA.root).
    /// \param outputFileName ROOT file (the output of the run) where the canvas and the histograms are
    /// written. Empty: not written.
    /// \param manager Manager with the TRestCut used (needed only if there are cuts).
    void PlotCombinedCanvas(const std::string& fileName, const std::string& outputFileName = "",
                            const TRestManager* manager = nullptr);

    TCanvas* GetCanvas() const { return fCanvas; }
};

#endif
