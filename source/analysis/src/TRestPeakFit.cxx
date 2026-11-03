#include "TRestPeakFit.h"

#include <RooAddPdf.h>
#include <RooArgList.h>
#include <RooCBShape.h>
#include <RooChebychev.h>
#include <RooDataHist.h>
#include <RooExtendPdf.h>
#include <RooFitResult.h>
#include <RooGaussian.h>
#include <RooMsgService.h>
#include <RooRealVar.h>
#include <TF1.h>
#include <TGraph.h>
#include <TH1.h>
#include <TH1D.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

namespace TRestPeakFit {

namespace {
/// Removes (and deletes) the object `name` of the list of functions of a histogram
void RemoveFunction(TH1& histogram, const std::string& name) {
    if (TObject* previous = histogram.GetListOfFunctions()->FindObject(name.c_str())) {
        histogram.GetListOfFunctions()->Remove(previous);
        delete previous;
    }
}
}  // namespace

Model ModelFromString(const std::string& name) {
    if (name == "gaussian") return Model::Gaussian;
    if (name == "crystalBall") return Model::CrystalBall;
    throw std::runtime_error("TRestPeakFit: unknown peak model '" + name + "' (gaussian or crystalBall)");
}

Background BackgroundFromString(const std::string& name) {
    if (name == "none") return Background::None;
    if (name == "constant") return Background::Constant;
    if (name == "linear") return Background::Linear;
    throw std::runtime_error("TRestPeakFit: unknown background '" + name + "' (none, constant or linear)");
}

Peak FitPeak(TH1& histogram, double start, double end, const std::string& name, Model model,
             Background background) {
    Peak peak;
    RemoveFunction(histogram, name);

    // The bins with their center in [start, end]
    const TAxis* axis = histogram.GetXaxis();
    int first = std::max(axis->FindFixBin(start), 1);
    int last = std::min(axis->FindFixBin(end), axis->GetNbins());
    if (axis->GetBinCenter(first) < start) first++;
    if (axis->GetBinCenter(last) > end) last--;
    if (last - first < 2) return peak;
    const double low = axis->GetBinLowEdge(first), high = axis->GetBinUpEdge(last);
    const int nBins = last - first + 1;
    TH1D window("window", "", nBins, low, high);
    window.SetDirectory(nullptr);
    int maximumBin = first;
    for (int bin = first; bin <= last; bin++) {
        window.SetBinContent(bin - first + 1, histogram.GetBinContent(bin));
        window.SetBinError(bin - first + 1, histogram.GetBinError(bin));
        if (histogram.GetBinContent(bin) > histogram.GetBinContent(maximumBin)) maximumBin = bin;
    }
    const double total = window.Integral();
    if (total <= 0) return peak;
    const double binWidth = (high - low) / nBins;

    // The model: the peak (gaussian or Crystal Ball) and the background (Chebychev polynomial: flat or
    // linear), extended
    RooRealVar x("x", "x", low, high);
    x.setBins(nBins);
    RooDataHist data("data", "", RooArgList(x), RooFit::Import(window));
    RooRealVar mean("mean", "", axis->GetBinCenter(maximumBin), low, high);
    RooRealVar sigma("sigma", "", (high - low) / 6, binWidth / 10, high - low);
    RooRealVar alpha("alpha", "", 1.5, 0.1, 10);
    RooRealVar n("n", "", 3, 1.01, 100);
    std::unique_ptr<RooAbsPdf> signalPdf;
    if (model == Model::CrystalBall) {
        signalPdf = std::make_unique<RooCBShape>("signal", "", x, mean, sigma, alpha, n);
    } else {
        signalPdf = std::make_unique<RooGaussian>("signal", "", x, mean, sigma);
    }
    RooRealVar slope("backgroundSlope", "", 0, -1, 1);
    std::unique_ptr<RooAbsPdf> backgroundPdf;
    if (background == Background::Constant) {
        backgroundPdf = std::make_unique<RooChebychev>("background", "", x, RooArgList());
    } else if (background == Background::Linear) {
        backgroundPdf = std::make_unique<RooChebychev>("background", "", x, RooArgList(slope));
    }
    RooRealVar nSignal("signalCounts", "", backgroundPdf ? 0.8 * total : total, 0, 2 * total);
    RooRealVar nBackground("backgroundCounts", "", 0.2 * total, 0, 2 * total);
    std::unique_ptr<RooAbsPdf> fitPdf;
    if (backgroundPdf) {
        fitPdf = std::make_unique<RooAddPdf>("model", "", RooArgList(*signalPdf, *backgroundPdf),
                                             RooArgList(nSignal, nBackground));
    } else {
        fitPdf = std::make_unique<RooExtendPdf>("model", "", *signalPdf, nSignal);
    }

    // Quiet fit
    RooMsgService& messages = RooMsgService::instance();
    const RooFit::MsgLevel level = messages.globalKillBelow();
    messages.setGlobalKillBelow(RooFit::WARNING);
    std::unique_ptr<RooFitResult> result(fitPdf->fitTo(data, RooFit::Save(), RooFit::PrintLevel(-1),
                                                       RooFit::Extended(true), RooFit::Warnings(false)));
    messages.setGlobalKillBelow(level);

    peak.valid = result && result->status() == 0;
    peak.mean = mean.getVal();
    peak.sigma = sigma.getVal();
    peak.meanError = mean.getError();
    peak.sigmaError = sigma.getError();
    peak.signal = nSignal.getVal();
    peak.background = backgroundPdf ? nBackground.getVal() : 0;
    if (result) {
        peak.status = result->status();
        for (const auto* parameter : result->floatParsFinal()) {
            if (const auto* variable = dynamic_cast<const RooRealVar*>(parameter))
                peak.parameters[variable->GetName()] = {variable->getVal(), variable->getError()};
        }
    }

    // The fit as a TF1 with the scale of the histogram (counts per bin), kept in it
    std::string formula = "[Constant]*exp(-0.5*((x-[Mean])/[Sigma])^2)";
    if (model == Model::CrystalBall) {
        formula =
            "[Constant]*(((x-[Mean])/[Sigma] > -[Alpha]) ? exp(-0.5*((x-[Mean])/[Sigma])^2) : "
            "pow([N]/[Alpha],[N])*exp(-0.5*[Alpha]*[Alpha])*pow([N]/[Alpha]-[Alpha]-(x-[Mean])/"
            "[Sigma],-[N]))";
    }
    if (backgroundPdf) formula += "+[Background]*(1+[Slope]*(2*x-[Low]-[High])/([High]-[Low]))";
    auto* fit = new TF1(name.c_str(), formula.c_str(), low, high, TF1::EAddToList::kNo);
    fit->SetParameter("Constant", 1);
    fit->SetParameter("Mean", peak.mean);
    fit->SetParameter("Sigma", peak.sigma);
    fit->SetParError(fit->GetParNumber("Mean"), peak.meanError);
    fit->SetParError(fit->GetParNumber("Sigma"), peak.sigmaError);
    if (model == Model::CrystalBall) {
        fit->SetParameter("Alpha", alpha.getVal());
        fit->SetParameter("N", n.getVal());
    }
    if (backgroundPdf) {
        fit->SetParameter("Low", low);
        fit->SetParameter("High", high);
        fit->SetParameter("Slope", background == Background::Linear ? slope.getVal() : 0);
        fit->SetParameter("Background", 0);
    }
    // Normalized as RooFit, in the fit range
    const double shapeIntegral = fit->Integral(low, high);
    fit->SetParameter("Constant", shapeIntegral > 0 ? peak.signal * binWidth / shapeIntegral : 0);
    if (backgroundPdf) fit->SetParameter("Background", peak.background * binWidth / (high - low));
    histogram.GetListOfFunctions()->Add(fit);
    return peak;
}

double GetPeakMean(const TH1& histogram, const std::string& name) {
    const TF1* fit = histogram.GetFunction(name.c_str());
    const int parameter = fit ? fit->GetParNumber("Mean") : -1;
    return parameter >= 0 ? fit->GetParameter(parameter) : std::numeric_limits<double>::quiet_NaN();
}

std::optional<double> FindPeak(const TH1& histogram, double start, double end, double smoothing,
                               double significance) {
    // The bins with their center in [start, end]
    const TAxis* axis = histogram.GetXaxis();
    std::vector<double> counts, centers;
    for (int bin = 1; bin <= axis->GetNbins(); bin++) {
        const double center = axis->GetBinCenter(bin);
        if (center < start || center > end) continue;
        counts.push_back(histogram.GetBinContent(bin));
        centers.push_back(center);
    }
    const int n = static_cast<int>(counts.size());
    if (n < 3) return std::nullopt;

    // Gaussian smoothing (kernel up to 3 sigma, normalized with the bins inside the range)
    std::vector<double> smoothed = counts;
    if (smoothing > 0) {
        const int reach = static_cast<int>(std::ceil(3 * smoothing));
        for (int i = 0; i < n; i++) {
            double sum = 0, weights = 0;
            for (int j = std::max(0, i - reach); j <= std::min(n - 1, i + reach); j++) {
                const double weight = std::exp(-0.5 * (i - j) * (i - j) / (smoothing * smoothing));
                sum += weight * counts[j];
                weights += weight;
            }
            smoothed[i] = sum / weights;
        }
    }

    // The most prominent significant local maximum
    std::optional<double> position;
    double bestProminence = 0;
    for (int i = 1; i < n - 1; i++) {
        if (!(smoothed[i] > smoothed[i - 1] && smoothed[i] >= smoothed[i + 1])) continue;
        // Lowest value on each side before a higher maximum (or the edge)
        double left = smoothed[i], right = smoothed[i];
        for (int j = i - 1; j >= 0 && smoothed[j] <= smoothed[i]; j--) left = std::min(left, smoothed[j]);
        for (int j = i + 1; j < n && smoothed[j] <= smoothed[i]; j++) right = std::min(right, smoothed[j]);
        const double prominence = smoothed[i] - std::max(left, right);
        if (prominence < significance * std::sqrt(std::max(smoothed[i], 1.))) continue;
        if (prominence > bestProminence) {
            bestProminence = prominence;
            position = centers[i];
        }
    }
    return position;
}

std::optional<std::pair<double, double>> FindPeakWindow(const TH1& histogram,
                                                        const std::pair<double, double>& zone,
                                                        double fraction) {
    const auto peak = FindPeak(histogram, zone.first, zone.second);
    if (!peak) return std::nullopt;
    return std::make_pair(std::max(zone.first, *peak * (1 - fraction)),
                          std::min(zone.second, *peak * (1 + fraction)));
}

std::pair<double, double> EstimateWindow(const TH1& histogram, double maximumEnergy, double energy,
                                         double fraction) {
    const double maximum = histogram.GetBinCenter(histogram.GetMaximumBin());
    const double position = energy * maximum / maximumEnergy;
    return {position * (1 - fraction), position * (1 + fraction)};
}

std::pair<double, double> FitLine(TGraph& graph) {
    while (graph.GetN() < 2) graph.SetPoint(graph.GetN(), 0, 0);
    TF1 line("linearFit", "pol1", 0, 1, TF1::EAddToList::kNo);
    graph.Fit(&line, "SQ");
    return {line.GetParameter(0), line.GetParameter(1)};
}

}  // namespace TRestPeakFit
