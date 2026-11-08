#include "TRestGeant4PrimaryGeneratorTypes.h"

#include <TAxis.h>
#include <TF1.h>
#include <TF2.h>
#include <TFile.h>
#include <TH1.h>
#include <TH2.h>
#include <TMath.h>
#include <TRandom.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <vector>

#include "TRestTools.h"

using namespace std;

namespace {
bool CaseInsensitiveCompare(const std::string& str1, const std::string& str2) {
    if (str1.size() != str2.size()) return false;
    return std::equal(str1.begin(), str1.end(), str2.begin(), [](unsigned char c1, unsigned char c2) {
        return std::tolower(c1) == std::tolower(c2);
    });
}
}  // namespace

namespace TRestGeant4PrimaryGeneratorTypes {

// --- EnergyDistributionFormulas Mapping ---

string EnergyDistributionFormulasToString(EnergyDistributionFormulas formula) {
    switch (formula) {
        case EnergyDistributionFormulas::COSMIC_NEUTRONS:
            return "CosmicNeutrons";
        case EnergyDistributionFormulas::COSMIC_GAMMAS:
            return "CosmicGammas";
        case EnergyDistributionFormulas::FISSION_NEUTRONS_U238:
            return "FissionNeutronsU238";
        case EnergyDistributionFormulas::ENVIRONMENTAL_GAMMAS:
            return "EnvironmentalGammas";
        case EnergyDistributionFormulas::ENVIRONMENTAL_NEUTRONS:
            return "EnvironmentalNeutrons";
    }
    throw invalid_argument("EnergyDistributionFormulasToString: Unknown energy distribution formula");
}

EnergyDistributionFormulas StringToEnergyDistributionFormulas(const string& type) {
    if (CaseInsensitiveCompare(
            type, EnergyDistributionFormulasToString(EnergyDistributionFormulas::COSMIC_NEUTRONS)))
        return EnergyDistributionFormulas::COSMIC_NEUTRONS;
    if (CaseInsensitiveCompare(type,
                               EnergyDistributionFormulasToString(EnergyDistributionFormulas::COSMIC_GAMMAS)))
        return EnergyDistributionFormulas::COSMIC_GAMMAS;
    if (CaseInsensitiveCompare(
            type, EnergyDistributionFormulasToString(EnergyDistributionFormulas::FISSION_NEUTRONS_U238)))
        return EnergyDistributionFormulas::FISSION_NEUTRONS_U238;
    if (CaseInsensitiveCompare(
            type, EnergyDistributionFormulasToString(EnergyDistributionFormulas::ENVIRONMENTAL_GAMMAS)))
        return EnergyDistributionFormulas::ENVIRONMENTAL_GAMMAS;
    if (CaseInsensitiveCompare(
            type, EnergyDistributionFormulasToString(EnergyDistributionFormulas::ENVIRONMENTAL_NEUTRONS)))
        return EnergyDistributionFormulas::ENVIRONMENTAL_NEUTRONS;

    throw invalid_argument("StringToEnergyDistributionFormulas: Unknown EnergyDistributionFormulas: " + type);
}

TF1 EnergyDistributionFormulasToRootFormula(EnergyDistributionFormulas formula) {
    switch (formula) {
        case EnergyDistributionFormulas::COSMIC_NEUTRONS: {
            const char* title = "Cosmic Neutrons at Sea Level";
            auto cosmicNeutrons = [](double* xs, double*) {
                const double logE = std::log(xs[0] * 1E-3);
                return 1.006E-6 * std::exp(-0.3500 * logE * logE + 2.1451 * logE) +
                       1.011E-3 * std::exp(-0.4106 * logE * logE - 0.6670 * logE);
            };
            auto distribution = TF1(title, cosmicNeutrons, 1.0E2, 1.0E7, 0);
            distribution.SetNormalized(true);
            distribution.SetTitle(title);
            distribution.GetXaxis()->SetTitle("Energy (keV)");
            return distribution;
        }
        case EnergyDistributionFormulas::COSMIC_GAMMAS: {
            throw invalid_argument(
                "EnergyDistributionFormulasToRootFormula: "
                "CosmicGammas is not implemented");
        }
        case EnergyDistributionFormulas::FISSION_NEUTRONS_U238: {
            const char* title = "Watt formula: Neutrons from U238 fission";
            auto watt = [](double* xs, double*) {
                return std::exp(-xs[0] / 712.4) * std::sinh(std::sqrt(0.0056405 * xs[0]));
            };
            auto distribution = TF1(title, watt, 0.0, 10000.0, 0);
            distribution.SetNormalized(true);
            distribution.SetTitle(title);
            distribution.GetXaxis()->SetTitle("Energy (keV)");
            return distribution;
        }
        case EnergyDistributionFormulas::ENVIRONMENTAL_GAMMAS: {
            const char* title = "Environmental Gammas";
            auto gammas = [](double* xs, double*) {
                return (1.0 / std::pow(xs[0], 1.5)) * std::exp(-xs[0] / 1500.0);
            };
            auto distribution = TF1(title, gammas, 250.0, 10000.0, 0);
            distribution.SetNormalized(true);
            distribution.SetTitle(title);
            distribution.GetXaxis()->SetTitle("Energy (keV)");
            return distribution;
        }
        case EnergyDistributionFormulas::ENVIRONMENTAL_NEUTRONS: {
            const char* title = "Environmental Neutrons";
            auto neutrons = [](double* xs, double*) { return std::exp(-xs[0] * 0.0005); };
            auto distribution = TF1(title, neutrons, 0.0, 10000.0, 0);
            distribution.SetNormalized(true);
            distribution.SetTitle(title);
            distribution.GetXaxis()->SetTitle("Energy (keV)");
            return distribution;
        }
    }
    throw invalid_argument("EnergyDistributionFormulasToRootFormula: Unknown energy distribution formula");
}

// --- AngularDistributionFormulas Mapping ---

string AngularDistributionFormulasToString(AngularDistributionFormulas type) {
    switch (type) {
        case AngularDistributionFormulas::COS2:
            return "Cos2";
        case AngularDistributionFormulas::COS3:
            return "Cos3";
        case AngularDistributionFormulas::SIN_2THETA:
            return "Sin2theta";
    }
    throw invalid_argument("AngularDistributionFormulasToString: Unknown angular distribution formula");
}

AngularDistributionFormulas StringToAngularDistributionFormulas(const string& type) {
    if (CaseInsensitiveCompare(type, AngularDistributionFormulasToString(AngularDistributionFormulas::COS2)))
        return AngularDistributionFormulas::COS2;
    if (CaseInsensitiveCompare(type, AngularDistributionFormulasToString(AngularDistributionFormulas::COS3)))
        return AngularDistributionFormulas::COS3;
    if (CaseInsensitiveCompare(type,
                               AngularDistributionFormulasToString(AngularDistributionFormulas::SIN_2THETA)))
        return AngularDistributionFormulas::SIN_2THETA;

    throw invalid_argument(
        "StringToAngularDistributionFormulas: "
        "Unknown AngularDistributionFormulas: " +
        type);
}

TF1 AngularDistributionFormulasToRootFormula(AngularDistributionFormulas type) {
    switch (type) {
        case AngularDistributionFormulas::COS2: {
            auto cos2 = [](double* xs, double*) {
                if (xs[0] >= 0.0 && xs[0] <= TMath::Pi() / 2.0) {
                    return TMath::Power(std::cos(xs[0]), 2.0);
                }
                return 0.0;
            };
            const char* title = "AngularDistribution: Cos2";
            auto f = TF1(title, cos2, 0.0, TMath::Pi(), 0);
            f.SetTitle(title);
            return f;
        }
        case AngularDistributionFormulas::COS3: {
            auto cos3 = [](double* xs, double*) {
                if (xs[0] >= 0.0 && xs[0] <= TMath::Pi() / 2.0) {
                    return TMath::Power(std::cos(xs[0]), 3.0);
                }
                return 0.0;
            };
            const char* title = "AngularDistribution: Cos3";
            auto f = TF1(title, cos3, 0.0, TMath::Pi(), 0);
            f.SetTitle(title);
            return f;
        }
        case AngularDistributionFormulas::SIN_2THETA: {
            auto sin2theta = [](double* xs, double*) {
                if (xs[0] >= 0.0 && xs[0] <= TMath::Pi() / 2.0) {
                    return std::sin(2.0 * xs[0]);
                }
                return 0.0;
            };
            const char* title = "AngularDistribution: Sin2theta";
            auto f = TF1(title, sin2theta, 0.0, TMath::Pi(), 0);
            f.SetTitle(title);
            return f;
        }
    }
    throw invalid_argument("AngularDistributionFormulasToRootFormula: Unknown angular distribution formula");
}

// --- EnergyAndAngularDistributionFormulas Mapping & Guan Parametrization ---

string EnergyAndAngularDistributionFormulasToString(EnergyAndAngularDistributionFormulas type) {
    if (type == EnergyAndAngularDistributionFormulas::COSMIC_MUONS) {
        return "CosmicMuons";
    }
    throw invalid_argument(
        "EnergyAndAngularDistributionFormulasToString: "
        "Unknown energy/angular distribution formula");
}

EnergyAndAngularDistributionFormulas StringToEnergyAndAngularDistributionFormulas(const string& type) {
    if (CaseInsensitiveCompare(type, EnergyAndAngularDistributionFormulasToString(
                                         EnergyAndAngularDistributionFormulas::COSMIC_MUONS))) {
        return EnergyAndAngularDistributionFormulas::COSMIC_MUONS;
    } else {
        throw invalid_argument(
            "StringToEnergyAndAngularDistributionFormulas: "
            "Unknown AngularDistributionFormulas: " +
            type);
    }
}

TF2 EnergyAndAngularDistributionFormulasToRootFormula(EnergyAndAngularDistributionFormulas type) {
    if (type == EnergyAndAngularDistributionFormulas::COSMIC_MUONS) {
        // Guan modified Gaisser parametrization formula for sea-level cosmic-ray muon flux
        // Source document: arXiv:1509.06176 (Chinese Physics C Vol. 39, No. 6)
        // Input parameter x: muon kinetic energy (processed inside simulation in units of keV)
        // Input parameter y: zenith angle theta (in radians)
        const char* title = "Cosmic Muons Energy and Angular";

        // Compiled (not a formula string): no interpreter needed, e.g. in the generator of each thread
        auto guan = [](double* xs, double*) {
            const double energy = xs[0] * 1.0E-6;  // GeV
            const double c = std::cos(xs[1]);
            if (c < 0) return 0.0;
            const double cosStar = std::sqrt(
                (c * c + 0.0105212 - 0.068287 * std::pow(c, 0.958633) + 0.0407253 * std::pow(c, 0.817285)) /
                0.982960);
            return 2.0 * TMath::Pi() * 1.0E-6 * 0.14 *
                   std::pow(energy * (1.0 + 3.64 / (energy * std::pow(cosStar, 1.29))), -2.7) *
                   (1.0 / (1.0 + 1.1 * energy * cosStar / 115.0) +
                    0.054 / (1.0 + 1.1 * energy * cosStar / 850.0)) *
                   (2.0 * std::sin(xs[1]) * TMath::Pi());
        };
        auto f = TF2(title, guan, 2.0E5, 5.0E9, 0.0, TMath::Pi() / 2.0, 0);

        f.SetTitle(title);
        return f;
    }
    throw invalid_argument(
        "EnergyAndAngularDistributionFormulasToRootFormula: "
        "Unknown energy and angular distribution formula");
}

// --- Helpers of the generator classes ---

std::unique_ptr<TH1> ReadHistogram(const string& fileName, const string& name) {
    unique_ptr<TFile> file(TFile::Open(TRestTools::GetFullPath(fileName).c_str(), "READ"));
    if (!file || file->IsZombie()) {
        throw invalid_argument("ReadHistogram: cannot open the file '" + fileName + "'");
    }
    const auto* histogram = file->Get<TH1>(name.c_str());
    if (!histogram) {
        throw invalid_argument("ReadHistogram: histogram '" + name + "' not found in '" + fileName + "'");
    }
    unique_ptr<TH1> copy(static_cast<TH1*>(histogram->Clone()));
    copy->SetDirectory(nullptr);
    return copy;
}

namespace {
/// Bin edges of an axis (variable or fixed bins)
std::vector<double> BinEdges(const TAxis& axis) {
    std::vector<double> edges(axis.GetNbins() + 1);
    for (int b = 1; b <= axis.GetNbins() + 1; b++) edges[b - 1] = axis.GetBinLowEdge(b);
    return edges;
}
}  // namespace

std::unique_ptr<TH1D> ReadHistogram1D(const string& fileName, const string& name) {
    auto histogram = ReadHistogram(fileName, name);
    if (histogram->GetDimension() != 1) {
        throw invalid_argument("ReadHistogram1D: '" + name + "' of '" + fileName + "' is not a 1D histogram");
    }
    if (auto* h = dynamic_cast<TH1D*>(histogram.get())) {
        histogram.release();
        return std::unique_ptr<TH1D>(h);
    }
    const auto edges = BinEdges(*histogram->GetXaxis());
    auto out = std::make_unique<TH1D>(histogram->GetName(), histogram->GetTitle(), histogram->GetNbinsX(),
                                      edges.data());
    out->SetDirectory(nullptr);
    // The axis titles give the units (e.g. "Energy (GeV)")
    out->GetXaxis()->SetTitle(histogram->GetXaxis()->GetTitle());
    out->GetYaxis()->SetTitle(histogram->GetYaxis()->GetTitle());
    for (int b = 0; b <= histogram->GetNbinsX() + 1; b++) {
        out->SetBinContent(b, histogram->GetBinContent(b));
        out->SetBinError(b, histogram->GetBinError(b));
    }
    out->SetEntries(histogram->GetEntries());
    return out;
}

std::unique_ptr<TH2D> ReadHistogram2D(const string& fileName, const string& name) {
    auto histogram = ReadHistogram(fileName, name);
    if (histogram->GetDimension() != 2) {
        throw invalid_argument("ReadHistogram2D: '" + name + "' of '" + fileName + "' is not a 2D histogram");
    }
    if (auto* h = dynamic_cast<TH2D*>(histogram.get())) {
        histogram.release();
        return std::unique_ptr<TH2D>(h);
    }
    const auto xEdges = BinEdges(*histogram->GetXaxis());
    const auto yEdges = BinEdges(*histogram->GetYaxis());
    auto out = std::make_unique<TH2D>(histogram->GetName(), histogram->GetTitle(), histogram->GetNbinsX(),
                                      xEdges.data(), histogram->GetNbinsY(), yEdges.data());
    out->SetDirectory(nullptr);
    out->GetXaxis()->SetTitle(histogram->GetXaxis()->GetTitle());
    out->GetYaxis()->SetTitle(histogram->GetYaxis()->GetTitle());
    out->GetZaxis()->SetTitle(histogram->GetZaxis()->GetTitle());
    for (int bx = 0; bx <= histogram->GetNbinsX() + 1; bx++) {
        for (int by = 0; by <= histogram->GetNbinsY() + 1; by++) {
            out->SetBinContent(bx, by, histogram->GetBinContent(bx, by));
            out->SetBinError(bx, by, histogram->GetBinError(bx, by));
        }
    }
    out->SetEntries(histogram->GetEntries());
    return out;
}

ROOT::Math::XYZVector IsotropicDirection(TRandom& random) {
    const double cosTheta = random.Uniform(-1, 1);
    const double sinTheta = std::sqrt(1 - cosTheta * cosTheta);
    const double phi = random.Uniform(0, 2 * M_PI);
    return {sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta};
}

ROOT::Math::XYZVector DirectionFromAxis(TRandom& random, const ROOT::Math::XYZVector& axis, double angle) {
    // Two unit vectors normal to the axis and to each other
    const ROOT::Math::XYZVector reference =
        std::abs(axis.Z()) < 0.9 ? ROOT::Math::XYZVector(0, 0, 1) : ROOT::Math::XYZVector(1, 0, 0);
    const auto v = axis.Cross(reference).Unit();
    const auto w = axis.Cross(v);
    const double phi = random.Uniform(0, 2 * M_PI);
    return std::cos(angle) * axis + std::sin(angle) * (std::cos(phi) * v + std::sin(phi) * w);
}

}  // namespace TRestGeant4PrimaryGeneratorTypes
