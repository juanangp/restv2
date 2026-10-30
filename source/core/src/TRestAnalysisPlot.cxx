/// \class TRestAnalysisPlot
/// Combined canvas with the observables of the analysis tree of one or several files (a pattern, e.g.
/// R*_ANA.root): first one pad per panel (text) and then one pad per plot (histograms). The analysis trees
/// are read with an RDataFrame: all the histograms and statistics are filled in a single loop.
/// TRestManager runs it after the pipeline, on its output file, or without pipeline on the inputFileName
/// of the run (`restManager --c plots.yaml --i "R*_ANA.root"`); from restRoot,
/// PlotCombinedCanvas(fileName, manager).
///
/// ### Parameters
/// * cuts: names of the TRestCut (defined in the manager) applied to all the data.
/// * outputFileName: ROOT file of the combined canvas and the histograms, its own (not the one of the
///   TRestRun); [parameter] of the run (e.g. "R[runNumber]_[runTag]_plots.root") and {expressions} are
///   replaced. A data file (with analysis tree, e.g. the output of the pipeline) is updated, keeping its
///   data; otherwise the file is created with the metadata of the manager (the following plots of the
///   same execution update it). Empty (default): not written.
/// * save: image of the combined canvas (pdf, png...; {expressions} are replaced, e.g.
///   "Plots_{run.runTag}.pdf"). Empty: not saved.
/// * previewPlot: if true the canvas is shown, scaled to fit in the screen, and the files are saved
///   when Enter is pressed (changes to the histograms are kept, changes to the pads are not).
/// * canvasSize: size in pixels of the saved files (the preview window keeps its aspect ratio).
/// * paletteStyle (57), canvasSize ([800, 600]), canvasDivisions ([2, 2]), canvasDivisionMargins
///   ([0.01, 0.01]), legendPosition ([x1, y1, x2, y2] = [0.7, 0.75, 0.88, 0.88]).
///
/// ### Panels
/// `panels`: list of panels, with textSize (0.05), precision (significant digits, 3), cuts (TRestCut
/// names, besides the global ones), enabled (true) and `labels`: list of text and position ([x, y] in
/// pad coordinates). In the text, each {expression|format} is replaced by its value; the format is
/// optional, printf style (%.2f) or a number of significant digits. In the expression:
/// * entries, runLength (hours, from the start and end time of each file, not affected by the cuts;
///   without them, from the first and last timeSeconds), meanRate (Hz),
///   startTime and endTime (dates), cuts (expression of the cuts of the panel);
/// * run.key (TRestRun of the first file, e.g. run.runTag, and its runInfo, e.g. run.runInfo.meshVoltage)
///   and instance.key... (any metadata of the first file, e.g. driftMedium.electricField);
/// * mean(obs), sigma(obs), min(obs), max(obs): of an observable with the cuts of the panel (only its
///   finite values: NaN, e.g. a line observable not defined, are ignored);
/// * any arithmetic of them (TFormula), e.g. {entries/runLength|%.1f}.
/// A single reference to a text (e.g. run.runTag, run.runInfo.meshVoltage: 300 V) is written as it is.
///
/// ### Plots
/// `plots`: list of plots, with name, title, xLabel, yLabel, logX/logY/logZ, gridX/gridY, legend,
/// timeDisplay (X axis as dates, e.g. timeSeconds), marginLeft/Right/Top/Bottom, stackOption ("nostack"),
/// normalize (integral of each histogram), scale (each histogram divided by this expression of binSize,
/// entries, integral and runLength, e.g. "binSize*runLength*3600" for a rate), save (file of the pad
/// alone), enabled (true) and `histos`: list of histograms with
/// * name, option (draw option), cuts (TRestCut names), stats (only for plots of a single histogram),
///   lineColor, lineWidth, lineStyle, fillColor, fillStyle (numbers or names: colors as red or kRed+2
///   or #rrggbb, line styles solid, dashed, dotted, dashDotted, fill styles as legacy);
/// * variables: one (1D) or two (2D, X and Y) of {name, range: [min, max], nBins (100)}; without range
///   (or min >= max) the range is automatic.
/// A plot with a single histogram can give its keys directly, without `histos`.
///
/// Example:
///
///    fiducial:
///      class: "TRestCut"
///      conditions:
///        originX: "trackLineAnalysis_OriginX > 20 && trackLineAnalysis_OriginX < 225"
///    upwards:
///      class: "TRestCut"
///      conditions:
///        downwards: "trackLineAnalysis_Downwards < 0"
///    plots:
///      class: "TRestAnalysisPlot"
///      canvasSize: [1200, 1200]
///      canvasDivisions: [2, 2]
///      save: "Plots_{run.runTag}.pdf"
///      cuts: ["fiducial"]
///      panels:
///        - labels:
///            - {text: "Run tag: {run.runTag}", position: [0.1, 0.82]}
///            - {text: "Mesh: {run.runInfo.meshVoltage}", position: [0.1, 0.74]}
///            - {text: "Rate: {entries/runLength|%.1f} per hour", position: [0.1, 0.66]}
///            - {text: "Mean angle: {mean(trackLineAnalysis_Angle)} rad", position: [0.1, 0.58]}
///      plots:
///        - name: "Spectra"
///          xLabel: "Energy (ADC)"
///          legend: true
///          histos:
///            - name: "All"
///              variables: [{name: "trackLineAnalysis_Energy", range: [0, 1e6], nBins: 200}]
///            - name: "Upwards"
///              variables: [{name: "trackLineAnalysis_Energy", range: [0, 1e6], nBins: 200}]
///              cuts: ["upwards"]
///              lineColor: "kRed+2"
///        - name: "Origin"
///          option: "colz"
///          variables:
///            - {name: "trackLineAnalysis_OriginX", range: [0, 250 mm], nBins: 125}
///            - {name: "trackLineAnalysis_OriginY", range: [0, 250 mm], nBins: 125}
///
/// Migrated from legacy TRestDataSetPlot (JuanAn Garcia, Álvaro Ezquerro), without TRestDataSet: it reads
/// the analysis trees of the files. Differences: the cuts are TRestCut of the manager referenced by name;
/// the panel keys variable, metadata, observable and expression are a single label text with
/// {expressions} (observables with mean(...) etc. instead of names with '_', metadata by instance and
/// key, also the runInfo of the run as run.runInfo.key); YAML lists instead of elements; the TApplication
/// preview waits for Enter.

#include "TRestAnalysisPlot.h"

#include <TApplication.h>
#include <TAttFill.h>
#include <TAttLine.h>
#include <TChain.h>
#include <TColor.h>
#include <TFile.h>
#include <TFormula.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TKey.h>
#include <TROOT.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TTree.h>
#include <TVirtualX.h>

#include <ROOT/RDataFrame.hxx>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "TRestCut.h"
#include "TRestManager.h"
#include "TRestRun.h"
#include "TRestSystemOfUnits.h"
#include "TRestTools.h"

using TRestTools::CheckYAMLKeys;
using TRestTools::GetYAMLList;
using TRestTools::GetYAMLParam;
using TRestTools::Trim;

namespace {
const bool kFieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestAnalysisPlot;
    reg.RegisterField<P>("cuts", &P::fCutNames);
    reg.RegisterField<P>("save", &P::fSave);
    reg.RegisterField<P>("outputFileName", &P::fOutputFileName);
    reg.RegisterField<P>("previewPlot", &P::fPreviewPlot);
    reg.RegisterField<P>("paletteStyle", &P::fPaletteStyle);
    reg.RegisterField<P>("canvasSize", &P::fCanvasSize);
    reg.RegisterField<P>("canvasDivisions", &P::fCanvasDivisions);
    reg.RegisterField<P>("canvasDivisionMargins", &P::fCanvasDivisionMargins);
    reg.RegisterField<P>("legendPosition", &P::fLegendPosition);
    reg.RegisterField<P>("panels", &P::fPanelsConfig);
    reg.RegisterField<P>("plots", &P::fPlotsConfig);
    return true;
}();

const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestAnalysisPlot", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestAnalysisPlot>(instanceName, params);
        });
    return true;
}();

/// EColor of Rtypes.h
const std::map<std::string, int> kColors = {
    {"white", kWhite},   {"black", kBlack},   {"gray", kGray},       {"red", kRed},       {"green", kGreen},
    {"blue", kBlue},     {"yellow", kYellow}, {"magenta", kMagenta}, {"cyan", kCyan},     {"orange", kOrange},
    {"spring", kSpring}, {"teal", kTeal},     {"azure", kAzure},     {"violet", kViolet}, {"pink", kPink}};

/// ELineStyle of TAttLine.h
const std::map<std::string, int> kLineStyles = {
    {"solid", kSolid}, {"dashed", kDashed}, {"dotted", kDotted}, {"dashDotted", kDashDotted}};

/// EFillStyle of TAttFill.h
const std::map<std::string, int> kFillStyles = {
    {"empty", kFEmpty},       {"solid", kFSolid},           {"dotted", kFDotted1},    {"dotted1", kFDotted1},
    {"dotted2", kFDotted2},   {"dotted3", kFDotted3},       {"hatched1", kFHatched1}, {"hatched2", kHatched2},
    {"hatched3", kFHatched3}, {"hatched4", kFHatched4},     {"wicker", kFWicker},     {"scales", kFScales},
    {"bricks", kFBricks},     {"snowflakes", kFSnowflakes}, {"circles", kFCircles},   {"tiles", kFTiles},
    {"mondrian", kFMondrian}, {"diamonds", kFDiamonds},     {"waves1", kFWaves1},     {"dashed", kFDashed1},
    {"dashed1", kFDashed1},   {"dashed2", kFDashed2},       {"alhambra", kFAlhambra}, {"waves2", kFWaves2},
    {"stars1", kFStars1},     {"stars2", kFStars2},         {"pyramids", kFPyramids}, {"frieze", kFFrieze},
    {"metopes", kFMetopes}};

/// Identifier, possibly with dots (metadata reference, e.g. run.runTag)
const std::string kReferencePattern = R"([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*)";
const std::regex kReference(kReferencePattern);
/// Statistic of an observable, e.g. mean(trackLineAnalysis_Angle)
const std::regex kStatistic(R"((mean|sigma|min|max)\s*\(\s*([A-Za-z_]\w*)\s*\))");

/// Number, name of the map or default (with a warning)
int ParseStyle(const std::string& value, const std::map<std::string, int>& names, int defaultValue) {
    if (value.empty()) return defaultValue;
    if (TRestTools::isANumber(value)) return std::stoi(value);
    const auto it = names.find(value);
    if (it != names.end()) return it->second;
    RESTWarning << "TRestAnalysisPlot: unknown style " << value << RESTendl;
    return defaultValue;
}

/// Number, #rrggbb or name (red, kRed) with an optional offset (kRed+2)
int ParseColor(const std::string& value, int defaultValue) {
    if (value.empty()) return defaultValue;
    if (value[0] == '#') return TColor::GetColor(value.c_str());
    if (TRestTools::isANumber(value)) return std::stoi(value);
    const size_t sign = value.find_first_of("+-");
    std::string name = TRestTools::ToLower(Trim(value.substr(0, sign)));
    if (name.size() > 1 && name[0] == 'k') name = name.substr(1);
    const auto it = kColors.find(name);
    if (it == kColors.end()) {
        RESTWarning << "TRestAnalysisPlot: unknown color " << value << RESTendl;
        return defaultValue;
    }
    if (sign == std::string::npos) return it->second;
    return it->second + std::stoi(value.substr(sign));
}

/// Values of the {expression|format} of the labels, output file names and scales
class Evaluator {
   public:
    /// Metadata of the first file by instance name, and its TRestRun
    const std::map<std::string, YAML::Node>* metadata = nullptr;
    YAML::Node run;
    std::map<std::string, double> numbers;
    std::map<std::string, std::string> texts;
    /// Statistics of the observables, e.g. "mean(trackLineAnalysis_Angle)"
    std::map<std::string, double> statistics;
    int precision = 3;

    /// \brief The text with its {expression|format} replaced.
    std::string Replace(const std::string& text) const {
        return TRestTools::ReplaceDelimited(text, '{', '}', [this](const std::string& content) {
            std::string expression = content;
            std::string format;
            // The last '|' that is not part of ||
            const size_t bar = expression.rfind('|');
            if (bar != std::string::npos && (bar == 0 || expression[bar - 1] != '|')) {
                format = Trim(expression.substr(bar + 1));
                expression = expression.substr(0, bar);
            }
            return std::optional<std::string>(Value(Trim(expression), format));
        });
    }

    /// \brief Numerical value of an expression, nullopt if it cannot be evaluated.
    std::optional<double> Evaluate(const std::string& expression) const {
        // Statistics first: their observable is not a reference
        std::string withStatistics;
        size_t last = 0;
        for (auto it = std::sregex_iterator(expression.begin(), expression.end(), kStatistic);
             it != std::sregex_iterator(); ++it) {
            const auto value = statistics.find((*it)[1].str() + "(" + (*it)[2].str() + ")");
            if (value == statistics.end()) return std::nullopt;
            withStatistics += expression.substr(last, it->position() - last) + Number(value->second);
            last = it->position() + it->length();
        }
        withStatistics += expression.substr(last);

        // References with a value; the rest (functions, e.g. sqrt or TMath::Pi) are left to TFormula
        bool valid = true;
        const std::string formula =
            TRestTools::ReplaceIdentifiers(withStatistics, kReferencePattern,
                                           [&](const std::string& reference) -> std::optional<std::string> {
                                               const auto number = numbers.find(reference);
                                               if (number != numbers.end()) return Number(number->second);
                                               const auto text = Lookup(reference);
                                               if (!text) return std::nullopt;
                                               const auto value = REST_Units::ParseValueWithUnits(*text);
                                               if (!value) valid = false;
                                               return value ? Number(*value) : *text;
                                           });
        if (!valid) return std::nullopt;

        TFormula function("TRestAnalysisPlot", formula.c_str(), false);
        if (!function.IsValid()) return std::nullopt;
        return function.Eval(0);
    }

   private:
    static std::string Number(double value) {
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "(%.17g)", value);
        return buffer;
    }

    /// Text of a variable or a metadata reference (instance.key...), nullopt if none
    std::optional<std::string> Lookup(const std::string& reference) const {
        const auto text = texts.find(reference);
        if (text != texts.end()) return text->second;
        const size_t dot = reference.find('.');
        if (dot == std::string::npos) return std::nullopt;

        // The run of the file whatever its instance name (run.runInfo.key: its runInfo)
        const std::string instance = reference.substr(0, dot);
        YAML::Node node;
        if (instance == "run") {
            node.reset(run);
        } else if (metadata && metadata->count(instance)) {
            node.reset(metadata->at(instance));
        }
        if (!node) return std::nullopt;
        node.reset(TRestTools::GetNodeAtPath(node, reference.substr(dot + 1)));
        if (!node || !node.IsScalar()) return std::nullopt;
        return node.as<std::string>();
    }

    std::string Format(double value, const std::string& format) const {
        char buffer[128];
        if (format.find('%') != std::string::npos) {
            std::snprintf(buffer, sizeof(buffer), format.c_str(), value);
        } else if (format.empty() && value == std::round(value) && std::abs(value) < 1e15) {
            // Integers (e.g. entries) without exponent
            std::snprintf(buffer, sizeof(buffer), "%.0f", value);
        } else {
            int digits = precision;
            if (!format.empty()) {
                if (TRestTools::isANumber(format)) {
                    digits = std::stoi(format);
                } else {
                    RESTWarning << "TRestAnalysisPlot: unknown format " << format << RESTendl;
                }
            }
            std::snprintf(buffer, sizeof(buffer), "%.*g", digits, value);
        }
        return buffer;
    }

    std::string Value(const std::string& expression, const std::string& format) const {
        // A single reference to a text is written as it is (formatted if it is a number and has format)
        if (std::regex_match(expression, kReference) && !numbers.count(expression)) {
            if (const auto text = Lookup(expression)) {
                const auto number = format.empty() ? std::nullopt : REST_Units::ParseValueWithUnits(*text);
                return number ? Format(*number, format) : *text;
            }
        }
        if (const auto value = Evaluate(expression)) return Format(*value, format);
        RESTWarning << "TRestAnalysisPlot: cannot evaluate {" << expression << "}" << RESTendl;
        return "?";
    }
};

/// Pad style, histograms (or stack), axis titles and legend of a plot in `pad`
void DrawPlot(TVirtualPad* pad, TRestAnalysisPlot::Plot& plot) {
    pad->cd();
    pad->SetLogx(plot.logX);
    pad->SetLogy(plot.logY);
    pad->SetLogz(plot.logZ);
    pad->SetGridx(plot.gridX);
    pad->SetGridy(plot.gridY);
    pad->SetLeftMargin(plot.marginLeft);
    pad->SetRightMargin(plot.marginRight);
    pad->SetTopMargin(plot.marginTop);
    pad->SetBottomMargin(plot.marginBottom);

    TAxis* xAxis = nullptr;
    TAxis* yAxis = nullptr;
    // A single histogram is drawn directly (statistics box and 2D options), several in a stack (all empty:
    // the first one, for the frame)
    if (plot.stack && plot.stack->GetNhists() > 0) {
        plot.stack->Draw(plot.stackOption.c_str());
        pad->Update();  // the stack has axes once painted
        xAxis = plot.stack->GetXaxis();
        yAxis = plot.stack->GetYaxis();
    } else if (!plot.histos.empty() && plot.histos.front().histo) {
        auto& histo = plot.histos.front();
        histo.histo->Draw(histo.drawOption.c_str());
        xAxis = histo.histo->GetXaxis();
        yAxis = histo.histo->GetYaxis();
    }
    if (xAxis && yAxis) {
        xAxis->SetTitle(plot.xLabel.c_str());
        yAxis->SetTitle(plot.yLabel.c_str());
        if (plot.timeDisplay) {
            xAxis->SetTimeDisplay(1);
            xAxis->SetTimeFormat("%d/%m %H:%M%F1970-01-01 00:00:00");
        }
    }
    if (plot.legendBox) plot.legendBox->Draw();
    pad->Modified();
    pad->Update();
}

/// Processes the GUI events (the canvas can be modified) until Enter is pressed in the terminal
void WaitForEnter() {
    std::cout << "TRestAnalysisPlot: press Enter to continue" << std::endl;
    std::atomic<bool> pressed{false};
    std::thread reader([&pressed] {
        std::cin.get();
        pressed = true;
    });
    while (!pressed) {
        gSystem->ProcessEvents();
        gSystem->Sleep(20);
    }
    reader.join();
}
}  // namespace

TRestAnalysisPlot::TRestAnalysisPlot() : TRestMetadata() { fName = "TRestAnalysisPlot"; }

TRestAnalysisPlot::TRestAnalysisPlot(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestAnalysisPlot::TRestAnalysisPlot(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

TRestAnalysisPlot::~TRestAnalysisPlot() {
    if (fCanvas && gROOT->GetListOfCanvases()->FindObject(fCanvas)) delete fCanvas;
}

void TRestAnalysisPlot::LoadConfig() {
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestAnalysisPlot::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }
    UpdateParamsFromYAML<TRestAnalysisPlot>(fNode);
    if (fCanvasSize.size() != 2 || fCanvasDivisions.size() != 2 || fCanvasDivisionMargins.size() != 2 ||
        fLegendPosition.size() != 4) {
        throw std::runtime_error("TRestAnalysisPlot '" + fName +
                                 "': canvasSize, canvasDivisions and canvasDivisionMargins need 2 values, "
                                 "legendPosition 4");
    }
    ReadPanels();
    ReadPlots();
    UpdateYAMLFromParams<TRestAnalysisPlot>(fNode);
}

void TRestAnalysisPlot::ReadPanels() {
    fPanels.clear();
    if (!fPanelsConfig || fPanelsConfig.IsNull()) return;
    const std::string context = "TRestAnalysisPlot '" + fName + "', panel";
    for (const auto& node : fPanelsConfig) {
        CheckYAMLKeys(node, {"enabled", "textSize", "precision", "cuts", "labels"}, context);
        if (!GetYAMLParam<bool>(node, "enabled", true)) continue;
        Panel panel;
        panel.textSize = GetYAMLParam<TRestWithUnits>(node, "textSize", panel.textSize);
        panel.precision = GetYAMLParam<int>(node, "precision", panel.precision);
        panel.cuts = GetYAMLList(node, "cuts");
        if (node["labels"]) {
            for (const auto& labelNode : node["labels"]) {
                CheckYAMLKeys(labelNode, {"text", "position"}, context + " label");
                Label label;
                label.text = GetYAMLParam<std::string>(labelNode, "text");
                if (labelNode["position"]) {
                    const auto position = labelNode["position"].as<std::vector<double>>();
                    if (position.size() != 2) {
                        throw std::runtime_error("TRestAnalysisPlot '" + fName +
                                                 "': label position needs [x, y]");
                    }
                    label.x = position[0];
                    label.y = position[1];
                }
                panel.labels.push_back(label);
            }
        }
        fPanels.push_back(std::move(panel));
    }
}

void TRestAnalysisPlot::ReadPlots() {
    fPlots.clear();
    if (!fPlotsConfig || fPlotsConfig.IsNull()) return;
    const std::set<std::string> plotKeys = {
        "enabled",     "name",       "title",       "xLabel",    "yLabel",       "logX",      "logY",
        "logZ",        "gridX",      "gridY",       "legend",    "timeDisplay",  "normalize", "scale",
        "stackOption", "marginLeft", "marginRight", "marginTop", "marginBottom", "save",      "histos"};
    const std::set<std::string> histoKeys = {"name",      "option",    "cuts",      "variables", "lineColor",
                                             "lineWidth", "lineStyle", "fillColor", "fillStyle", "stats"};
    for (const auto& node : fPlotsConfig) {
        // Without histos the plot is also its single histogram
        std::set<std::string> keys = plotKeys;
        if (!node["histos"]) keys.insert(histoKeys.begin(), histoKeys.end());
        CheckYAMLKeys(node, keys, "TRestAnalysisPlot '" + fName + "', plot");
        if (!GetYAMLParam<bool>(node, "enabled", true)) continue;
        Plot plot;
        plot.name = GetYAMLParam<std::string>(node, "name", "plot_" + std::to_string(fPlots.size()));
        plot.title = GetYAMLParam<std::string>(node, "title", plot.name);
        plot.xLabel = GetYAMLParam<std::string>(node, "xLabel");
        plot.yLabel = GetYAMLParam<std::string>(node, "yLabel");
        plot.logX = GetYAMLParam<bool>(node, "logX", plot.logX);
        plot.logY = GetYAMLParam<bool>(node, "logY", plot.logY);
        plot.logZ = GetYAMLParam<bool>(node, "logZ", plot.logZ);
        plot.gridX = GetYAMLParam<bool>(node, "gridX", plot.gridX);
        plot.gridY = GetYAMLParam<bool>(node, "gridY", plot.gridY);
        plot.legend = GetYAMLParam<bool>(node, "legend", plot.legend);
        plot.timeDisplay = GetYAMLParam<bool>(node, "timeDisplay", plot.timeDisplay);
        plot.normalize = GetYAMLParam<TRestWithUnits>(node, "normalize", plot.normalize);
        plot.scale = GetYAMLParam<std::string>(node, "scale");
        plot.stackOption = GetYAMLParam<std::string>(node, "stackOption", plot.stackOption);
        plot.marginLeft = GetYAMLParam<TRestWithUnits>(node, "marginLeft", plot.marginLeft);
        plot.marginRight = GetYAMLParam<TRestWithUnits>(node, "marginRight", plot.marginRight);
        plot.marginTop = GetYAMLParam<TRestWithUnits>(node, "marginTop", plot.marginTop);
        plot.marginBottom = GetYAMLParam<TRestWithUnits>(node, "marginBottom", plot.marginBottom);
        plot.save = GetYAMLParam<std::string>(node, "save");

        auto readHisto = [&](const YAML::Node& histoNode) {
            if (node["histos"]) {
                CheckYAMLKeys(histoNode, histoKeys, "TRestAnalysisPlot '" + fName + "', plot " + plot.name);
            }
            Histo histo;
            histo.name = GetYAMLParam<std::string>(histoNode, "name", plot.name);
            histo.drawOption = GetYAMLParam<std::string>(histoNode, "option");
            histo.cuts = GetYAMLList(histoNode, "cuts");
            if (histoNode["variables"]) {
                for (const auto& variableNode : histoNode["variables"]) {
                    CheckYAMLKeys(variableNode, {"name", "nBins", "range"},
                                  "TRestAnalysisPlot '" + fName + "', plot " + plot.name + " variable");
                    Variable variable;
                    variable.name = GetYAMLParam<std::string>(variableNode, "name");
                    variable.nBins = GetYAMLParam<int>(variableNode, "nBins", variable.nBins);
                    if (variableNode["range"]) {
                        const auto range = variableNode["range"];
                        if (!range.IsSequence() || range.size() != 2) {
                            throw std::runtime_error("TRestAnalysisPlot '" + fName +
                                                     "': range needs [min, max]");
                        }
                        variable.min = range[0].as<TRestWithUnits>();
                        variable.max = range[1].as<TRestWithUnits>();
                    }
                    histo.variables.push_back(variable);
                }
            }
            if (histo.variables.empty() || histo.variables.size() > 2) {
                throw std::runtime_error("TRestAnalysisPlot '" + fName + "': histogram " + histo.name +
                                         " needs one (1D) or two (2D) variables");
            }
            for (const auto& variable : histo.variables) {
                if (variable.name.empty() || variable.nBins <= 0) {
                    throw std::runtime_error("TRestAnalysisPlot '" + fName + "': histogram " + histo.name +
                                             " has a variable without name or bins");
                }
            }
            histo.lineColor = ParseColor(GetYAMLParam<std::string>(histoNode, "lineColor"), histo.lineColor);
            histo.lineWidth = GetYAMLParam<int>(histoNode, "lineWidth", histo.lineWidth);
            histo.lineStyle =
                ParseStyle(GetYAMLParam<std::string>(histoNode, "lineStyle"), kLineStyles, histo.lineStyle);
            histo.fillColor = ParseColor(GetYAMLParam<std::string>(histoNode, "fillColor"), histo.fillColor);
            histo.fillStyle =
                ParseStyle(GetYAMLParam<std::string>(histoNode, "fillStyle"), kFillStyles, histo.fillStyle);
            histo.stats = GetYAMLParam<bool>(histoNode, "stats", histo.stats);
            plot.histos.push_back(std::move(histo));
        };
        if (node["histos"]) {
            for (const auto& histoNode : node["histos"]) readHisto(histoNode);
        } else {
            readHisto(node);
        }
        fPlots.push_back(std::move(plot));
    }
}

void TRestAnalysisPlot::PlotCombinedCanvas(const std::string& fileName, const TRestManager* manager) {
    TChain chain("AnalysisTree");
    if (chain.Add(fileName.c_str()) <= 0) {
        throw std::runtime_error("TRestAnalysisPlot '" + fName + "': no files " + fileName);
    }

    // Metadata of the first file and start and end times of all of them
    std::map<std::string, YAML::Node> metadata;
    YAML::Node run;
    double startTime = 0, endTime = 0, duration = 0;
    bool firstFile = true;
    for (const auto* element : *chain.GetListOfFiles()) {
        std::unique_ptr<TFile> file(TFile::Open(element->GetTitle()));
        if (!file || file->IsZombie()) {
            throw std::runtime_error("TRestAnalysisPlot '" + fName + "': cannot open " + element->GetTitle());
        }
        YAML::Node fileRun;
        if (auto* directory = file->GetDirectory("RESTMetadataStore")) {
            for (const auto* key : *directory->GetListOfKeys()) {
                const std::string instance = key->GetName();
                YAML::Node node = TRestMetadata::ReadMetadata(file.get(), instance);
                if (!node || !node["class"]) continue;
                if (node["class"].as<std::string>() == "TRestRun") fileRun.reset(node);
                if (firstFile) metadata[instance] = node;
            }
        }
        if (fileRun) {
            const double start = GetYAMLParam<double>(fileRun, "startTime", 0);
            const double end = GetYAMLParam<double>(fileRun, "endTime", 0);
            if (firstFile || start < startTime) startTime = start;
            if (firstFile || end > endTime) endTime = end;
            duration += end - start;
        } else {
            RESTWarning << "TRestAnalysisPlot: no TRestRun in " << element->GetTitle() << RESTendl;
        }
        if (firstFile) run.reset(fileRun);
        firstFile = false;
    }

    auto cutExpression = [&](const std::vector<std::string>& names) {
        std::string expression;
        for (const auto& name : names) {
            const TRestCut* cut = nullptr;
            if (manager) {
                for (const auto* c : manager->GetMetadataOfType<TRestCut>()) {
                    if (c->GetName() == name) cut = c;
                }
            }
            if (!cut) {
                throw std::runtime_error("TRestAnalysisPlot '" + fName + "': TRestCut '" + name +
                                         "' not found in the manager");
            }
            const std::string condition = cut->GetExpression();
            if (condition.empty()) continue;
            if (!expression.empty()) expression += " && ";
            expression += "(" + condition + ")";
        }
        return expression;
    };

    // All the results are booked before the loop, run once when the first one is read
    ROOT::RDataFrame frame(chain);
    const std::string globalCuts = cutExpression(fCutNames);
    ROOT::RDF::RNode all(frame);
    if (!globalCuts.empty()) all = all.Filter(globalCuts);
    auto withCuts = [&](const std::vector<std::string>& names) -> ROOT::RDF::RNode {
        const std::string expression = cutExpression(names);
        if (expression.empty()) return all;
        return all.Filter(expression);
    };
    auto totalEntries = all.Count();
    // Files without start and end times (e.g. not from raw data): time of the first and last event
    const bool eventTimes = duration <= 0 && frame.HasColumn("timeSeconds");
    ROOT::RDF::RResultPtr<double> firstEventTime, lastEventTime;
    if (eventTimes) {
        firstEventTime = frame.Min("timeSeconds");
        lastEventTime = frame.Max("timeSeconds");
    }

    struct PanelResults {
        ROOT::RDF::RResultPtr<ULong64_t> entries;
        std::map<std::string, ROOT::RDF::RResultPtr<double>> statistics;
        std::string cuts;
    };
    std::vector<PanelResults> panelResults;
    for (const auto& panel : fPanels) {
        auto node = withCuts(panel.cuts);
        PanelResults results;
        results.entries = node.Count();
        std::vector<std::string> cuts = fCutNames;
        cuts.insert(cuts.end(), panel.cuts.begin(), panel.cuts.end());
        results.cuts = cutExpression(cuts);
        for (const auto& label : panel.labels) {
            for (auto it = std::sregex_iterator(label.text.begin(), label.text.end(), kStatistic);
                 it != std::sregex_iterator(); ++it) {
                const std::string function = (*it)[1].str();
                const std::string column = (*it)[2].str();
                const std::string key = function + "(" + column + ")";
                if (results.statistics.count(key)) continue;
                try {
                    // Only the defined values (e.g. the line observables are NaN when not defined)
                    auto finite = node.Filter("std::isfinite(" + column + ")");
                    if (function == "mean") results.statistics[key] = finite.Mean(column);
                    if (function == "sigma") results.statistics[key] = finite.StdDev(column);
                    if (function == "min") results.statistics[key] = finite.Min(column);
                    if (function == "max") results.statistics[key] = finite.Max(column);
                } catch (const std::exception& error) {
                    RESTWarning << "TRestAnalysisPlot: " << key << ": " << error.what() << RESTendl;
                }
            }
        }
        panelResults.push_back(std::move(results));
    }

    struct HistoResult {
        ROOT::RDF::RResultPtr<TH1D> histo1D;
        ROOT::RDF::RResultPtr<TH2D> histo2D;
    };
    std::vector<std::vector<HistoResult>> histoResults;
    for (const auto& plot : fPlots) {
        auto& results = histoResults.emplace_back();
        for (const auto& histo : plot.histos) {
            auto node = withCuts(histo.cuts);
            const auto& x = histo.variables.front();
            HistoResult result;
            if (histo.variables.size() == 1) {
                const ROOT::RDF::TH1DModel model(histo.name.c_str(), plot.title.c_str(), x.nBins, x.min,
                                                 x.max);
                result.histo1D = node.Histo1D(model, x.name);
            } else {
                const auto& y = histo.variables.back();
                const ROOT::RDF::TH2DModel model(histo.name.c_str(), plot.title.c_str(), x.nBins, x.min,
                                                 x.max, y.nBins, y.min, y.max);
                result.histo2D = node.Histo2D(model, x.name, y.name);
            }
            results.push_back(result);
        }
    }

    RESTInfo << "TRestAnalysisPlot '" << fName << "': " << *totalEntries << " entries"
             << (globalCuts.empty() ? "" : " after the cuts " + globalCuts) << RESTendl;

    if (eventTimes && *lastEventTime > *firstEventTime) {
        startTime = *firstEventTime;
        endTime = *lastEventTime;
        duration = endTime - startTime;
        RESTWarning << "TRestAnalysisPlot: no start and end times in the files, runLength from timeSeconds"
                    << RESTendl;
    }

    // Values of the expressions without panel (output file names, scales)
    Evaluator global;
    global.metadata = &metadata;
    global.run.reset(run);
    global.numbers["entries"] = *totalEntries;
    global.numbers["runLength"] = duration / 3600.;
    global.numbers["meanRate"] = duration > 0 ? *totalEntries / duration : 0;
    global.texts["startTime"] = TRestTools::GetTimeStampFromUnixTime(startTime);
    global.texts["endTime"] = TRestTools::GetTimeStampFromUnixTime(endTime);
    global.texts["cuts"] = globalCuts;

    const int nPads = fCanvasDivisions[0] * fCanvasDivisions[1];
    if (fPanels.size() + fPlots.size() > static_cast<size_t>(nPads)) {
        RESTWarning << "TRestAnalysisPlot '" << fName << "': more panels and plots than pads, the last ones "
                    << "are not drawn" << RESTendl;
    }

    for (size_t p = 0; p < fPanels.size(); p++) {
        auto& panel = fPanels[p];
        Evaluator evaluator = global;
        evaluator.precision = panel.precision;
        const double entries = *panelResults[p].entries;
        evaluator.numbers["entries"] = entries;
        evaluator.numbers["meanRate"] = duration > 0 ? entries / duration : 0;
        evaluator.texts["cuts"] = panelResults[p].cuts;
        for (auto& [key, value] : panelResults[p].statistics) evaluator.statistics[key] = *value;

        panel.texts.clear();
        for (const auto& label : panel.labels) {
            auto text = std::make_unique<TLatex>(label.x, label.y, evaluator.Replace(label.text).c_str());
            text->SetTextSize(panel.textSize);
            panel.texts.push_back(std::move(text));
        }
    }

    for (size_t p = 0; p < fPlots.size(); p++) {
        auto& plot = fPlots[p];
        plot.stack.reset();
        if (plot.histos.size() > 1) {
            plot.stack = std::make_unique<THStack>(plot.name.c_str(), plot.title.c_str());
        }
        plot.legendBox.reset();
        if (plot.legend) {
            plot.legendBox = std::make_unique<TLegend>(fLegendPosition[0], fLegendPosition[1],
                                                       fLegendPosition[2], fLegendPosition[3]);
        }
        for (size_t h = 0; h < plot.histos.size(); h++) {
            auto& histo = plot.histos[h];
            auto& result = histoResults[p][h];
            TH1* source = histo.variables.size() == 1 ? static_cast<TH1*>(result.histo1D.GetPtr())
                                                      : static_cast<TH1*>(result.histo2D.GetPtr());
            histo.histo.reset(static_cast<TH1*>(source->Clone(histo.name.c_str())));
            histo.histo->SetDirectory(nullptr);
            // With automatic range the entries are in the buffer until the axis limits are computed
            histo.histo->BufferEmpty(1);
            histo.histo->SetLineColor(histo.lineColor);
            histo.histo->SetLineWidth(histo.lineWidth);
            histo.histo->SetLineStyle(histo.lineStyle);
            histo.histo->SetFillColor(histo.fillColor);
            histo.histo->SetFillStyle(histo.fillStyle);
            histo.histo->SetStats(histo.stats);

            // Empty (e.g. exclusive cuts): nothing to scale, and without range its axis has no width (it
            // stays in the legend)
            const bool empty = histo.histo->GetEntries() == 0;
            if (empty) {
                RESTWarning << "TRestAnalysisPlot: plot " << plot.name << ", histogram " << histo.name
                            << " has no entries after the cuts" << RESTendl;
            }
            if (plot.normalize > 0) {
                const double integral = histo.histo->Integral();
                if (integral > 0) histo.histo->Scale(plot.normalize / integral);
            }
            if (!plot.scale.empty() && !empty) {
                Evaluator evaluator = global;
                evaluator.numbers["binSize"] = histo.histo->GetXaxis()->GetBinWidth(1);
                evaluator.numbers["entries"] = histo.histo->GetEntries();
                evaluator.numbers["integral"] = histo.histo->Integral("width");
                const auto scale = evaluator.Evaluate(plot.scale);
                if (scale && *scale != 0) {
                    histo.histo->Scale(1. / *scale);
                } else {
                    RESTWarning << "TRestAnalysisPlot: plot " << plot.name << ", cannot evaluate scale "
                                << plot.scale << RESTendl;
                }
            }
            // Not in the stack if empty: without range its axis is [0, 0], and the axis of the stack (union
            // of those of its histograms) would hide the others (e.g. timeSeconds from 0)
            if (plot.stack && !empty) plot.stack->Add(histo.histo.get(), histo.drawOption.c_str());
            if (plot.legendBox) plot.legendBox->AddEntry(histo.histo.get(), histo.name.c_str(), "lf");
        }
        if (plot.histos.size() > 1 && std::any_of(plot.histos.begin(), plot.histos.end(),
                                                  [](const Histo& histo) { return histo.stats; })) {
            RESTWarning << "TRestAnalysisPlot: plot " << plot.name
                        << ", stats only for plots of a single histogram" << RESTendl;
        }
    }

    // Panels first, then plots, one per pad
    gStyle->SetPalette(fPaletteStyle);
    auto drawCanvas = [this, nPads](TCanvas* canvas) {
        canvas->Divide(fCanvasDivisions[0], fCanvasDivisions[1], fCanvasDivisionMargins[0],
                       fCanvasDivisionMargins[1]);
        int padIndex = 1;
        for (auto& panel : fPanels) {
            if (padIndex > nPads) break;
            canvas->cd(padIndex++);
            for (auto& text : panel.texts) text->Draw();
        }
        for (auto& plot : fPlots) {
            if (padIndex > nPads) break;
            DrawPlot(canvas->cd(padIndex++), plot);
        }
        canvas->cd();
        canvas->Modified();
        canvas->Update();
    };

    const bool batch = gROOT->IsBatch();
    if (fCanvas && gROOT->GetListOfCanvases()->FindObject(fCanvas)) delete fCanvas;
    fCanvas = nullptr;
    if (fPreviewPlot && !batch) {
        if (!gApplication) new TApplication("TRestAnalysisPlot", nullptr, nullptr);
        // The window is scaled (same aspect ratio) to fit in the screen, so all the pads are visible
        double factor = 1;
        if (gVirtualX) {
            Int_t screenX = 0, screenY = 0;
            UInt_t screenWidth = 0, screenHeight = 0;
            gVirtualX->GetGeometry(-1, screenX, screenY, screenWidth, screenHeight);
            if (screenWidth > 0 && screenHeight > 0) {
                factor =
                    std::min({1., 0.9 * screenWidth / fCanvasSize[0], 0.9 * screenHeight / fCanvasSize[1]});
            }
        }
        fCanvas = new TCanvas(fName.c_str(), fName.c_str(), static_cast<int>(factor * fCanvasSize[0]),
                              static_cast<int>(factor * fCanvasSize[1]));
        drawCanvas(fCanvas);
        WaitForEnter();
    }

    // Files are drawn without window, at canvasSize
    gROOT->SetBatch(true);
    for (auto& plot : fPlots) {
        if (plot.save.empty()) continue;
        TCanvas canvas(("save_" + plot.name).c_str(), plot.title.c_str(), fCanvasSize[0], fCanvasSize[1]);
        DrawPlot(&canvas, plot);
        canvas.Print(global.Replace(plot.save).c_str());
    }
    std::string outputFileName = fOutputFileName;
    if (!outputFileName.empty()) {
        if (fRunInfo) outputFileName = fRunInfo->ResolveFilePattern(outputFileName);
        outputFileName = global.Replace(outputFileName);
        if (fRunInfo) outputFileName = fRunInfo->PrefixMainDataPath(outputFileName);
    }
    if (!fSave.empty() || !outputFileName.empty()) {
        TCanvas canvas(("save_" + fName).c_str(), fName.c_str(), fCanvasSize[0], fCanvasSize[1]);
        canvas.SetName(fName.c_str());  // name in a root file; no clash with the preview canvas when built
        drawCanvas(&canvas);
        if (!fSave.empty()) {
            const std::string saveFileName = global.Replace(fSave);
            canvas.Print(saveFileName.c_str());
            RESTInfo << "TRestAnalysisPlot '" << fName << "': saved " << saveFileName << RESTendl;
        }
        // A data file (e.g. the output of the pipeline or the input file) or a file already written by a
        // plot of this execution is updated; otherwise a new file with the metadata of the manager
        if (!outputFileName.empty()) {
            static std::set<std::string> createdFiles;
            bool update = createdFiles.count(outputFileName) > 0;
            if (!update && !gSystem->AccessPathName(outputFileName.c_str())) {
                std::unique_ptr<TFile> existing(TFile::Open(outputFileName.c_str()));
                update = existing && !existing->IsZombie() && existing->Get<TTree>("AnalysisTree");
            }
            std::unique_ptr<TFile> file(TFile::Open(outputFileName.c_str(), update ? "UPDATE" : "RECREATE"));
            if (!file || file->IsZombie()) {
                throw std::runtime_error("TRestAnalysisPlot '" + fName + "': cannot open " + outputFileName);
            }
            if (!update) {
                createdFiles.insert(outputFileName);
                if (manager) {
                    for (auto* meta : manager->GetMetadataOfType<TRestMetadata>()) {
                        meta->WriteMetadata(file.get());
                    }
                }
            }
            file->cd();
            canvas.Write("", TObject::kOverwrite);
            for (auto& plot : fPlots) {
                for (auto& histo : plot.histos) histo.histo->Write("", TObject::kOverwrite);
            }
            RESTInfo << "TRestAnalysisPlot '" << fName << "': written in " << outputFileName << RESTendl;
        }
    }
    gROOT->SetBatch(batch);
}
