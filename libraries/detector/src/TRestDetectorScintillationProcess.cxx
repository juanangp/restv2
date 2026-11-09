/// \class TRestDetectorScintillationProcess
/// Converts the energy of the hits of a scintillator (a TRestDetectorHitsEvent, e.g. from
/// TRestGeant4ToDetectorHitsProcess after TRestGeant4QuenchingProcess) into the photoelectrons of the
/// channels that collect its light. For each hit of energy E (keV):
/// - the scintillation photons are E * lightYield on average (`light` of the TRestDetectorMedium);
/// - the readout gives the channels that collect the light produced at the hit and the probability that a
///   photon gives a photoelectron in each one (TRestDetectorReadout::GetLightCollection): for a
///   TRestFibreReadout, the fibres closer than maxDistance with captureProbability(r) * quantumEfficiency;
///   by default, the channel at the position with probability 1;
/// - the photoelectrons of each channel are Poisson(E * lightYield * probability) (the production of the
///   photons is Poisson and their collection binomial, so the counts of the channels are independent).
///
/// Each channel with photoelectrons gives a hit with energy the number of photoelectrons, at the time of the
/// deposit and at the position of the channel given by the readout (for a fibre, the projection of the
/// deposit on it, so TRestDetectorHitsToSignalProcess assigns it to that fibre). The energies of the output
/// hits are photoelectrons, not keV. Hits without any channel collecting their light are counted in the
/// observable NHitsWithoutLight; NPhotoelectrons is the total of the event. Events without photoelectrons
/// are rejected.
///
/// The decay time of the scintillator and the propagation along the fibres are not simulated (the time is
/// that of the deposit).
///
/// Parameters: readout (empty: the data readout of the TRestDetectorReadoutManager), medium (name of the
/// TRestDetectorMedium with `light` and its lightYield; empty: the only one), seed (0: random seed;
/// otherwise the generator is re-seeded at each event from the seed and the event and subevent IDs).
///
/// Example (LiquidO, see TRestFibreReadout):
///
///    medium:
///      class: "TRestDetectorMedium"
///      name: "LiquidScintillator"
///      state: "liquid"
///      light:
///        lightYield: 10
///    ...
///    scintillation:
///      class: "TRestDetectorScintillationProcess"
///      readout: "LiquidO_Readout"
///      medium: "LiquidScintillator"
///      observables: ["NPhotoelectrons", "NHitsWithoutLight"]
///
/// New in restv2 (legacy had no scintillation with photoelectrons): it replaces the light emulation of the
/// LiquidO macro Geant4ToSignal.C.

#include "TRestDetectorScintillationProcess.h"

#include <stdexcept>

#include "TRestDetectorHitsEvent.h"
#include "TRestDetectorMedium.h"
#include "TRestDetectorReadout.h"
#include "TRestDetectorReadoutManager.h"
#include "TRestManager.h"
#include "TRestRun.h"

using ROOT::Math::XYZVector;

static const bool TRestDetectorScintillationProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorScintillationProcess;
    reg.RegisterField<P>("readout", &P::fReadoutName);
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

    auto* readouts =
        dynamic_cast<TRestDetectorReadoutManager*>(fManager->GetMetadataClass("TRestDetectorReadoutManager"));
    if (!readouts) throw std::runtime_error(user + ": no TRestDetectorReadoutManager found");
    fReadout = readouts->SelectReadout(fReadoutName, user);

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

    RegisterObservable("NPhotoelectrons", fNPhotoelectrons);
    RegisterObservable("NHitsWithoutLight", fNHitsWithoutLight);
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
    fNPhotoelectrons = 0;
    fNHitsWithoutLight = 0;

    const double lightYield = fLight->GetLightYield();
    for (size_t n = 0; n < inputEvent->GetNumberOfHits(); n++) {
        const double energy = inputEvent->GetEnergy(n);
        if (energy <= 0) continue;
        const XYZVector position = inputEvent->GetPosition(n);
        const auto collections = fReadout->GetLightCollection(position);
        if (collections.empty()) {
            fNHitsWithoutLight++;
            continue;
        }
        const double photons = energy * lightYield;
        for (const auto& collection : collections) {
            const int photoelectrons = fRandom.Poisson(photons * collection.probability);
            if (photoelectrons <= 0) continue;
            outputEvent->AddHit(collection.position, photoelectrons, inputEvent->GetTime(n));
            fNPhotoelectrons += photoelectrons;
        }
    }

    RESTDebug << "TRestDetectorScintillationProcess: event " << inputEvent->GetID() << ", "
              << fNPhotoelectrons << " photoelectrons, " << fNHitsWithoutLight << " hits without light"
              << RESTendl;
    return outputEvent->GetNumberOfHits() > 0;
}
