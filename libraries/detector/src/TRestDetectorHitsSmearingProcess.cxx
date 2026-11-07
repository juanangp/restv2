/// \class TRestDetectorHitsSmearingProcess
/// Applies the energy resolution of the detector: the energy of all the hits of an event is multiplied by
/// a gain drawn from a gaussian of mean 1, one per event, as legacy. The resolution (FWHM) follows a square
/// root law with the energy of the event E (sum of the hit energies), pivoting on the reference:
///
///     FWHM(E) / E = resolutionReference / 100 * sqrt(energyReference / E)
///
/// With subtractFano, the Fano fluctuation of the number of electrons, already applied by
/// TRestDetectorHitsDriftProcess, is removed from the resolution: only the extra variance
/// sigma^2 - F * W / E (relative) is applied, with the Fano factor F and the W-value of the
/// charge response of the TRestDetectorMedium. Events whose resolution is below the Fano contribution are not
/// smeared (a warning gives their number at the end).
///
/// Differences with legacy: there is no channelType (veto hits are split beforehand, each detector goes in
/// its own pipeline) and the optional Fano subtraction.
///
/// Parameters: energyReference (5.9 keV), resolutionReference (FWHM in %, 15), subtractFano (false),
/// medium (name, optional if there is only one, only with subtractFano), seed (0: random seed; otherwise
/// the generator is re-seeded at each event from the seed and the event and subevent IDs,
/// TRestDetectorHitsEvent::EventSeed, so the result does not depend on the number of threads).
///
/// Example:
///
///    hitsSmearing:
///      class: "TRestDetectorHitsSmearingProcess"
///      energyReference: 5.9 keV
///      resolutionReference: 15
///      subtractFano: true

#include "TRestDetectorHitsSmearingProcess.h"

#include <cmath>
#include <stdexcept>

#include "TRestDetectorHitsEvent.h"
#include "TRestDetectorMedium.h"
#include "TRestManager.h"

static const bool TRestDetectorHitsSmearingProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorHitsSmearingProcess;
    reg.RegisterField<P>("energyReference", &P::fEnergyReference);
    reg.RegisterField<P>("resolutionReference", &P::fResolutionReference);
    reg.RegisterField<P>("subtractFano", &P::fSubtractFano);
    reg.RegisterField<P>("medium", &P::fMediumName);
    reg.RegisterField<P>("seed", &P::fSeed);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorHitsSmearingProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorHitsSmearingProcess>(instanceName, params);
        });
    return true;
}();

/// FWHM of a gaussian in units of sigma
constexpr double kFWHM = 2.35482;
}  // namespace

TRestDetectorHitsSmearingProcess::TRestDetectorHitsSmearingProcess() : TRestEventProcess() {
    fName = "TRestDetectorHitsSmearingProcess";
}

TRestDetectorHitsSmearingProcess::TRestDetectorHitsSmearingProcess(const std::string& instanceName,
                                                                   const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestDetectorHitsSmearingProcess::TRestDetectorHitsSmearingProcess(const std::string& fileName,
                                                                   const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorHitsSmearingProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestDetectorHitsSmearingProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }
    UpdateParamsFromYAML<TRestDetectorHitsSmearingProcess>(fNode);
    UpdateYAMLFromParams<TRestDetectorHitsSmearingProcess>(fNode);
}

void TRestDetectorHitsSmearingProcess::InitProcess() {
    if (fEnergyReference <= 0 || fResolutionReference < 0) {
        throw std::runtime_error(
            "TRestDetectorHitsSmearingProcess: energyReference must be > 0 and resolutionReference >= 0");
    }

    fCharge = nullptr;
    if (fSubtractFano) {
        if (!fManager) throw std::runtime_error("TRestDetectorHitsSmearingProcess: no TRestManager");
        const std::string user = "TRestDetectorHitsSmearingProcess";
        const auto* medium = fManager->FindMetadata<TRestDetectorMedium>(fMediumName, user, "medium");
        if (!medium) throw std::runtime_error(user + ": subtractFano needs a TRestDetectorMedium");
        fCharge = &medium->GetChargeResponse(user);
        if (fCharge->GetWValue() <= 0) {
            throw std::runtime_error(user + ": subtractFano needs the wValue of the charge of '" +
                                     medium->GetName() + "'");
        }
        fMediumName = medium->GetName();
        UpdateYAMLFromParams<TRestDetectorHitsSmearingProcess>(fNode);
    }

    fNBelowFano = 0;
    // Seed 0: random seed. Otherwise re-seeded at each event in ProcessEvent
    fRandom.SetSeed(fSeed);
}

bool TRestDetectorHitsSmearingProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* inputEvent = dynamic_cast<const TRestDetectorHitsEvent*>(&input);
    auto* outputEvent = dynamic_cast<TRestDetectorHitsEvent*>(&output);
    if (!inputEvent || !outputEvent) {
        throw std::runtime_error(
            "TRestDetectorHitsSmearingProcess: input/output is not a TRestDetectorHitsEvent");
    }

    outputEvent->Initialize();
    outputEvent->SetEventInfo(inputEvent);
    if (fSeed != 0) fRandom.SetSeed(TRestDetectorHitsEvent::EventSeed(fSeed, *inputEvent));

    double energy = 0;
    for (size_t n = 0; n < inputEvent->GetNumberOfHits(); n++) {
        if (inputEvent->GetEnergy(n) > 0) energy += inputEvent->GetEnergy(n);
    }

    double gain = 1;
    if (energy > 0) {
        const double sigma = fResolutionReference / 100. / kFWHM * std::sqrt(fEnergyReference / energy);
        double variance = sigma * sigma;
        if (fCharge) variance -= fCharge->GetFanoFactor() * fCharge->GetWValue() / energy;
        if (variance > 0) {
            gain = fRandom.Gaus(1, std::sqrt(variance));
        } else if (fCharge) {
            fNBelowFano++;
        }
    }

    for (size_t n = 0; n < inputEvent->GetNumberOfHits(); n++) {
        outputEvent->AddHit(inputEvent->GetPosition(n), inputEvent->GetEnergy(n) * gain,
                            inputEvent->GetTime(n), inputEvent->GetType(n));
    }

    return outputEvent->GetNumberOfHits() > 0;
}

void TRestDetectorHitsSmearingProcess::EndProcess() {
    if (fNBelowFano > 0) {
        RESTWarning << "TRestDetectorHitsSmearingProcess: " << fNBelowFano
                    << " events with a resolution below the Fano contribution were not smeared" << RESTendl;
    }
}
