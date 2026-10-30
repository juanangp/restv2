#include "TRestGeant4PrimaryGeneratorTypes.h"

#include <TAxis.h>
#include <TF1.h>
#include <TF2.h>
#include <TFile.h>
#include <TH1.h>
#include <TMath.h>
#include <TRandom.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

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
            auto distribution = TF1(title,
                                    "1.006E-6 * TMath::Exp(-0.3500 * TMath::Power(TMath::Log(x * 1E-3), 2.0) "
                                    "+ 2.1451 * TMath::Log(x * 1E-3)) + "
                                    "1.011E-3 * TMath::Exp(-0.4106 * TMath::Power(TMath::Log(x * 1E-3), 2.0) "
                                    "- 0.6670 * TMath::Log(x * 1E-3))",
                                    1.0E2, 1.0E7);
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
            auto distribution =
                TF1(title, "TMath::Exp(-x/712.4) * TMath::SinH(TMath::Sqrt(0.0056405*x))", 0.0, 10000.0);
            distribution.SetNormalized(true);
            distribution.SetTitle(title);
            distribution.GetXaxis()->SetTitle("Energy (keV)");
            return distribution;
        }
        case EnergyDistributionFormulas::ENVIRONMENTAL_GAMMAS: {
            const char* title = "Environmental Gammas";
            auto distribution =
                TF1(title, "(1.0 / TMath::Power(x, 1.5)) * TMath::Exp(-x / 1500.0)", 250.0, 10000.0);
            distribution.SetNormalized(true);
            distribution.SetTitle(title);
            distribution.GetXaxis()->SetTitle("Energy (keV)");
            return distribution;
        }
        case EnergyDistributionFormulas::ENVIRONMENTAL_NEUTRONS: {
            const char* title = "Environmental Neutrons";
            auto distribution = TF1(title, "TMath::Exp(-x * 0.0005)", 0.0, 10000.0);
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

        auto f = TF2(title,
                     "2.0*TMath::Pi()*1.0E-6*0.14*TMath::Power(x*1.0E-6*(1.0+3.64/"
                     "(x*1.0E-6*TMath::Power((TMath::Power(TMath::Cos(y),2.0)+0.0105212-0.068287*"
                     "TMath::Power(TMath::Cos(y),0.958633)+0.0407253*TMath::Power(TMath::Cos(y),0.817285)"
                     ")/(0.982960),0.5),1.29))),-2.7)*(1.0/"
                     "(1.0+(1.1*x*1.0E-6*TMath::Power((TMath::Power(TMath::Cos(y),2.0)+0.0105212-0.068287*"
                     "TMath::Power(TMath::Cos(y),0.958633)+0.0407253*TMath::Power(TMath::Cos(y),0.817285))/"
                     "(0.982960),0.5))/115.0)+0.054/"
                     "(1.0+(1.1*x*1.0E-6*TMath::Power((TMath::Power(TMath::Cos(y),2.0)+0.0105212-0.068287*"
                     "TMath::Power(TMath::Cos(y),0.958633)+0.0407253*TMath::Power(TMath::Cos(y),0.817285))/"
                     "(0.982960),0.5))/850.0))*(2.0*TMath::Sin(y)*TMath::Pi())",
                     2.0E5, 5.0E9, 0.0, TMath::Pi() / 2.0);

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
