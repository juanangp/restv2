/// \class TRestDetectorScintillationProcess
/// Primary scintillation of the energy deposits of a scintillator (a TRestDetectorHitsEvent, e.g. from
/// TRestGeant4ToDetectorHitsProcess after TRestGeant4QuenchingProcess): each hit of energy E (keV) becomes a
/// hit with the number of scintillation photons produced, Poisson(E * lightYield) with the lightYield of the
/// `light` of the TRestDetectorMedium, at the same position, time and type. Hits without photons are
/// dropped. The energies of the output hits are photons, not keV.
///
/// It is the first step of the light: the collection of the photons by the channels and their conversion
/// into photoelectrons are TRestDetectorLightCollectionProcess (with a model for each kind of
/// scintillator). The decay time of the
/// scintillator is not simulated (the photons have the time of the deposit).
///
/// Observable NPhotons: the photons of the event. Events without photons are rejected.
///
/// Parameters: medium (name of the TRestDetectorMedium with `light` and its lightYield; empty: the only
/// one), seed (0: random seed; otherwise the generator is re-seeded at each event from the seed and the
/// event and subevent IDs).
///
/// Example:
///
///    medium:
///      class: "TRestDetectorMedium"
///      name: "LiquidScintillator"
///      state: "liquid"
///      light:
///        lightYield: 10   # photons per keV
///    ...
///    scintillation:
///      class: "TRestDetectorScintillationProcess"
///      medium: "LiquidScintillator"
///      observables: ["NPhotons"]
///
/// New in restv2 (legacy had no photon statistics).

#include "TRestDetectorScintillationProcess.h"

#include <stdexcept>

#include "TRestDetectorHitsEvent.h"
#include "TRestDetectorMedium.h"
#include "TRestManager.h"

static const bool TRestDetectorScintillationProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorScintillationProcess;
    reg.RegisterField<P>("medium", &P::fMediumName);
    reg.RegisterField<P>("seed", &P::fSeed);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorScintillationProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorScintillationProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestDetectorScintillationProcess::TRestDetectorScintillationProcess() : TRestEventProcess() {
    fName = "TRestDetectorScintillationProcess";
}

TRestDetectorScintillationProcess::TRestDetectorScintillationProcess(const std::string& instanceName,
                                                                     const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestDetectorScintillationProcess::TRestDetectorScintillationProcess(const std::string& fileName,
                                                                     const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorScintillationProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestDetectorScintillationProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }
    UpdateParamsFromYAML<TRestDetectorScintillationProcess>(fNode);
    UpdateYAMLFromParams<TRestDetectorScintillationProcess>(fNode);
}

void TRestDetectorScintillationProcess::InitProcess() {
    const std::string user = "TRestDetectorScintillationProcess";
    if (!fManager) throw std::runtime_error(user + ": no TRestManager");

    const auto* medium = fManager->FindMetadata<TRestDetectorMedium>(fMediumName, user, "medium");
    if (!medium) throw std::runtime_error(user + ": needs a TRestDetectorMedium");
    fLight = &medium->GetLightResponse(user);
    if (fLight->GetLightYield() <= 0) {
        throw std::runtime_error(user + ": the light of the medium '" + medium->GetName() +
                                 "' needs lightYield > 0");
    }
    fMediumName = medium->GetName();
    // The stored metadata keeps the medium used
    UpdateYAMLFromParams<TRestDetectorScintillationProcess>(fNode);

    // Seed 0: random seed. Otherwise re-seeded at each event in ProcessEvent
    fRandom.SetSeed(fSeed);

    RegisterObservable("NPhotons", fNPhotons);
}

bool TRestDetectorScintillationProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* inputEvent = dynamic_cast<const TRestDetectorHitsEvent*>(&input);
    auto* outputEvent = dynamic_cast<TRestDetectorHitsEvent*>(&output);
    if (!inputEvent || !outputEvent) {
        throw std::runtime_error(
            "TRestDetectorScintillationProcess: input/output is not a TRestDetectorHitsEvent");
    }

    outputEvent->Initialize();
    outputEvent->SetEventInfo(inputEvent);
    if (fSeed != 0) fRandom.SetSeed(TRestDetectorHitsEvent::EventSeed(fSeed, *inputEvent));
    fNPhotons = 0;

    const double lightYield = fLight->GetLightYield();
    for (size_t n = 0; n < inputEvent->GetNumberOfHits(); n++) {
        const double energy = inputEvent->GetEnergy(n);
        if (energy <= 0) continue;
        const int photons = fRandom.Poisson(energy * lightYield);
        if (photons <= 0) continue;
        outputEvent->AddHit(inputEvent->GetPosition(n), photons, inputEvent->GetTime(n),
                            inputEvent->GetType(n));
        fNPhotons += photons;
    }

    RESTDebug << "TRestDetectorScintillationProcess: event " << inputEvent->GetID() << ", " << fNPhotons
              << " photons" << RESTendl;
    return outputEvent->GetNumberOfHits() > 0;
}
