#include "TRestGeant4PrimaryGeneratorTypes.h"

#include <TAxis.h>
#include <TMath.h>
#include <TF1.h>
#include <TF2.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iostream>

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

// --- SpatialGeneratorTypes Mapping ---

string SpatialGeneratorTypesToString(SpatialGeneratorTypes type) {
    switch (type) {
        case SpatialGeneratorTypes::CUSTOM:
            return "Custom";
        case SpatialGeneratorTypes::VOLUME:
            return "Volume";
        case SpatialGeneratorTypes::SURFACE:
            return "Surface";
        case SpatialGeneratorTypes::POINT:
            return "Point";
        case SpatialGeneratorTypes::COSMIC:
            return "Cosmic";
        case SpatialGeneratorTypes::SOURCE:
            return "Source";
    }
    cout << "SpatialGeneratorTypesToString - Error - Unknown "
            "SpatialGeneratorTypes"
         << endl;
    exit(1);
}

SpatialGeneratorTypes StringToSpatialGeneratorTypes(const string& type) {
    if (CaseInsensitiveCompare(type, SpatialGeneratorTypesToString(SpatialGeneratorTypes::CUSTOM)))
        return SpatialGeneratorTypes::CUSTOM;
    if (CaseInsensitiveCompare(type, SpatialGeneratorTypesToString(SpatialGeneratorTypes::VOLUME)))
        return SpatialGeneratorTypes::VOLUME;
    if (CaseInsensitiveCompare(type, SpatialGeneratorTypesToString(SpatialGeneratorTypes::SURFACE)))
        return SpatialGeneratorTypes::SURFACE;
    if (CaseInsensitiveCompare(type, SpatialGeneratorTypesToString(SpatialGeneratorTypes::POINT)))
        return SpatialGeneratorTypes::POINT;
    if (CaseInsensitiveCompare(type, SpatialGeneratorTypesToString(SpatialGeneratorTypes::COSMIC)))
        return SpatialGeneratorTypes::COSMIC;
    if (CaseInsensitiveCompare(type, SpatialGeneratorTypesToString(SpatialGeneratorTypes::SOURCE)))
        return SpatialGeneratorTypes::SOURCE;

    cout << "StringToSpatialGeneratorTypes - Error - Unknown "
            "SpatialGeneratorTypes: "
         << type << endl;
    exit(1);
}

// --- SpatialGeneratorShapes Mapping ---

string SpatialGeneratorShapesToString(SpatialGeneratorShapes shape) {
    switch (shape) {
        case SpatialGeneratorShapes::GDML:
            return "GDML";
        case SpatialGeneratorShapes::WALL:
            return "Wall";
        case SpatialGeneratorShapes::CIRCLE:
            return "Circle";
        case SpatialGeneratorShapes::BOX:
            return "Box";
        case SpatialGeneratorShapes::SPHERE:
            return "Sphere";
        case SpatialGeneratorShapes::CYLINDER:
            return "Cylinder";
    }
    cout << "SpatialGeneratorShapesToString - Error - Unknown "
            "SpatialGeneratorShapes"
         << endl;
    exit(1);
}

SpatialGeneratorShapes StringToSpatialGeneratorShapes(const string& shape) {
    if (CaseInsensitiveCompare(shape, SpatialGeneratorShapesToString(SpatialGeneratorShapes::GDML)))
        return SpatialGeneratorShapes::GDML;
    if (CaseInsensitiveCompare(shape, SpatialGeneratorShapesToString(SpatialGeneratorShapes::WALL)))
        return SpatialGeneratorShapes::WALL;
    if (CaseInsensitiveCompare(shape, SpatialGeneratorShapesToString(SpatialGeneratorShapes::CIRCLE)))
        return SpatialGeneratorShapes::CIRCLE;
    if (CaseInsensitiveCompare(shape, SpatialGeneratorShapesToString(SpatialGeneratorShapes::BOX)))
        return SpatialGeneratorShapes::BOX;
    if (CaseInsensitiveCompare(shape, SpatialGeneratorShapesToString(SpatialGeneratorShapes::SPHERE)))
        return SpatialGeneratorShapes::SPHERE;
    if (CaseInsensitiveCompare(shape, SpatialGeneratorShapesToString(SpatialGeneratorShapes::CYLINDER)))
        return SpatialGeneratorShapes::CYLINDER;

    cout << "StringToSpatialGeneratorShapes - Error - Unknown "
            "SpatialGeneratorShapes: '"
         << shape << "'" << endl;
    exit(1);
}

// --- EnergyDistributionTypes Mapping ---

string EnergyDistributionTypesToString(EnergyDistributionTypes type) {
    switch (type) {
        case EnergyDistributionTypes::TH1D:
            return "TH1D";
        case EnergyDistributionTypes::TH2D:
            return "TH2D";
        case EnergyDistributionTypes::FORMULA:
            return "Formula";
        case EnergyDistributionTypes::FORMULA2:
            return "Formula2";
        case EnergyDistributionTypes::MONO:
            return "Mono";
        case EnergyDistributionTypes::FLAT:
            return "Flat";
        case EnergyDistributionTypes::LOG:
            return "Log";
    }
    cout << "EnergyDistributionTypesToString - Error - Unknown energy "
            "distribution type"
         << endl;
    exit(1);
}

EnergyDistributionTypes StringToEnergyDistributionTypes(
    const string& type) {
    if (CaseInsensitiveCompare(type, EnergyDistributionTypesToString(EnergyDistributionTypes::TH1D)))
        return EnergyDistributionTypes::TH1D;
    if (CaseInsensitiveCompare(type, EnergyDistributionTypesToString(EnergyDistributionTypes::TH2D)))
        return EnergyDistributionTypes::TH2D;
    if (CaseInsensitiveCompare(type, EnergyDistributionTypesToString(EnergyDistributionTypes::FORMULA)))
        return EnergyDistributionTypes::FORMULA;
    if (CaseInsensitiveCompare(type, EnergyDistributionTypesToString(EnergyDistributionTypes::FORMULA2)))
        return EnergyDistributionTypes::FORMULA2;
    if (CaseInsensitiveCompare(type, EnergyDistributionTypesToString(EnergyDistributionTypes::MONO)))
        return EnergyDistributionTypes::MONO;
    if (CaseInsensitiveCompare(type, EnergyDistributionTypesToString(EnergyDistributionTypes::FLAT)))
        return EnergyDistributionTypes::FLAT;
    if (CaseInsensitiveCompare(type, EnergyDistributionTypesToString(EnergyDistributionTypes::LOG)))
        return EnergyDistributionTypes::LOG;

    cout << "StringToEnergyDistributionTypes - Error - Unknown "
            "EnergyDistributionTypes: "
         << type << endl;
    exit(1);
}

// --- EnergyDistributionFormulas Mapping ---

string EnergyDistributionFormulasToString(
    EnergyDistributionFormulas formula) {
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
    cout << "EnergyDistributionFormulasToString - Error - Unknown energy "
            "distribution formula"
         << endl;
    exit(1);
}

EnergyDistributionFormulas StringToEnergyDistributionFormulas(
    const string& type) {
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

    cout << "StringToEnergyDistributionFormulas - Error - Unknown "
            "EnergyDistributionFormulas: "
         << type << endl;
    exit(1);
}

TF1 EnergyDistributionFormulasToRootFormula(
    EnergyDistributionFormulas formula) {
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
            exit(1);
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
    cout << "EnergyDistributionFormulasToRootFormula - Error - Unknown "
            "energy distribution formula"
         << endl;
    exit(1);
}

// --- AngularDistributionTypes Mapping ---

string AngularDistributionTypesToString(AngularDistributionTypes type) {
    switch (type) {
        case AngularDistributionTypes::TH1D:
            return "TH1D";
        case AngularDistributionTypes::TH2D:
            return "TH2D";
        case AngularDistributionTypes::FORMULA:
            return "Formula";
        case AngularDistributionTypes::FORMULA2:
            return "Formula2";
        case AngularDistributionTypes::ISOTROPIC:
            return "Isotropic";
        case AngularDistributionTypes::FLUX:
            return "Flux";
        case AngularDistributionTypes::BACK_TO_BACK:
            return "Back to back";
    }
    cout << "AngularDistributionTypesToString - Error - Unknown "
            "AngularDistributionTypes"
         << endl;
    exit(1);
}

AngularDistributionTypes StringToAngularDistributionTypes(
    const string& type) {
    if (CaseInsensitiveCompare(type, AngularDistributionTypesToString(AngularDistributionTypes::TH1D)))
        return AngularDistributionTypes::TH1D;
    if (CaseInsensitiveCompare(type, AngularDistributionTypesToString(AngularDistributionTypes::TH2D)))
        return AngularDistributionTypes::TH2D;
    if (CaseInsensitiveCompare(type, AngularDistributionTypesToString(AngularDistributionTypes::FORMULA)))
        return AngularDistributionTypes::FORMULA;
    if (CaseInsensitiveCompare(type, AngularDistributionTypesToString(AngularDistributionTypes::FORMULA2)))
        return AngularDistributionTypes::FORMULA2;
    if (CaseInsensitiveCompare(type, AngularDistributionTypesToString(AngularDistributionTypes::ISOTROPIC)))
        return AngularDistributionTypes::ISOTROPIC;
    if (CaseInsensitiveCompare(type, AngularDistributionTypesToString(AngularDistributionTypes::FLUX)))
        return AngularDistributionTypes::FLUX;
    if (CaseInsensitiveCompare(type,
                               AngularDistributionTypesToString(AngularDistributionTypes::BACK_TO_BACK)))
        return AngularDistributionTypes::BACK_TO_BACK;

    cout << "StringToAngularDistributionTypes - Error - Unknown "
            "AngularDistributionTypes: "
         << type << endl;
    exit(1);
}

// --- AngularDistributionFormulas Mapping ---

string AngularDistributionFormulasToString(
    AngularDistributionFormulas type) {
    switch (type) {
        case AngularDistributionFormulas::COS2:
            return "Cos2";
        case AngularDistributionFormulas::COS3:
            return "Cos3";
        case AngularDistributionFormulas::SIN_2THETA:
            return "Sin2theta";
    }
    cout << "AngularDistributionFormulasToString - Error - Unknown angular "
            "distribution formula"
         << endl;
    exit(1);
}

AngularDistributionFormulas StringToAngularDistributionFormulas(
    const string& type) {
    if (CaseInsensitiveCompare(type, AngularDistributionFormulasToString(AngularDistributionFormulas::COS2)))
        return AngularDistributionFormulas::COS2;
    if (CaseInsensitiveCompare(type, AngularDistributionFormulasToString(AngularDistributionFormulas::COS3)))
        return AngularDistributionFormulas::COS3;
    if (CaseInsensitiveCompare(type,
                               AngularDistributionFormulasToString(AngularDistributionFormulas::SIN_2THETA)))
        return AngularDistributionFormulas::SIN_2THETA;

    cout << "StringToAngularDistributionFormulas - Error - Unknown "
            "AngularDistributionFormulas: "
         << type << endl;
    exit(1);
}

TF1 AngularDistributionFormulasToRootFormula(
    AngularDistributionFormulas type) {
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
    cout << "AngularDistributionFormulasToRootFormula - Error - Unknown "
            "angular distribution formula"
         << endl;
    exit(1);
}

// --- EnergyAndAngularDistributionFormulas Mapping & Guan Parametrization ---

string EnergyAndAngularDistributionFormulasToString(
    EnergyAndAngularDistributionFormulas type) {
    if (type == EnergyAndAngularDistributionFormulas::COSMIC_MUONS) {
        return "CosmicMuons";
    }
    cout << "EnergyAndAngularDistributionFormulasToString - Error - "
            "Unknown energy/angular distribution formula"
         << endl;
    exit(1);
}

EnergyAndAngularDistributionFormulas
StringToEnergyAndAngularDistributionFormulas(const string& type) {
    if (CaseInsensitiveCompare(type, EnergyAndAngularDistributionFormulasToString(
                                         EnergyAndAngularDistributionFormulas::COSMIC_MUONS))) {
        return EnergyAndAngularDistributionFormulas::COSMIC_MUONS;
    } else {
        cout << "StringToEnergyAndAngularDistributionFormulas - Error - "
                "Unknown AngularDistributionFormulas: "
             << type << endl;
        exit(1);
    }
}

TF2 EnergyAndAngularDistributionFormulasToRootFormula(
    EnergyAndAngularDistributionFormulas type) {
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
    cout << "EnergyAndAngularDistributionFormulasToRootFormula - Error - "
            "Unknown energy and angular distribution formula"
         << endl;
    exit(1);
}

}  // namespace TRestGeant4PrimaryGeneratorTypes
