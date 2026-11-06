/// \class TRestDetectorLightAttenuationProcess
/// Propagates the scintillation light of the hits of a TRestDetectorHitsEvent to the readout of their
/// channel (e.g. the PMT of a veto), before TRestDetectorHitsToSignalProcess:
/// - the channel of each hit is the one of its position (TRestDetectorReadout::GetChannelFromPosition);
/// - d is the distance to the readout of that channel (TRestDetectorReadout::GetDistanceToReadout, e.g. to
///   the readoutVolume of a TRestGDMLReadout channel);
/// - energy E exp(-d / attenuationLength) and time t + d / effectiveLightSpeed, with the `light` response
///   of the medium (TRestDetectorLightResponse).
/// Hits without channel are kept as they are (TRestDetectorHitsToSignalProcess rejects them) and counted in
/// the observable NHitsWithoutChannel. A channel without distance to its readout (e.g. a TRestGDMLReadout
/// entry without readoutVolume) is an error. Events without hits are rejected.
///
/// Parameters: readout (empty: the data readout of the TRestDetectorReadoutManager), medium (name of the
/// TRestDetectorMedium with `light`; empty: the only one).
///
/// Example (vetoes):
///
///    medium:
///      class: "TRestDetectorMedium"
///      name: "BC408"
///      state: "solid"
///      light:
///        attenuationLength: 3 m
///        effectiveLightSpeed: 150 mm/ns
///    ...
///    lightAttenuation:
///      class: "TRestDetectorLightAttenuationProcess"
///      readout: "Veto_Readout"
///      medium: "BC408"
///      observables: ["NHitsWithoutChannel"]
///
/// Differences with legacy: the parameters (vetoLightAttenuationLength, vetoEffectiveLightSpeed) are the
/// light response of the medium; the distance is to the readout volume of the channel (legacy: to the
/// readout plane of the veto) and the channel type is not checked (the veto hits go in their own pipeline);
/// a hit without channel is kept instead of ending the program.

#include "TRestDetectorLightAttenuationProcess.h"

#include <cmath>
#include <stdexcept>

#include "TRestDetectorHitsEvent.h"
#include "TRestDetectorMedium.h"
#include "TRestDetectorReadout.h"
#include "TRestDetectorReadoutManager.h"
#include "TRestManager.h"
#include "TRestRun.h"

using ROOT::Math::XYZVector;

static const bool TRestDetectorLightAttenuationProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorLightAttenuationProcess;
    reg.RegisterField<P>("readout", &P::fReadoutName);
    reg.RegisterField<P>("medium", &P::fMediumName);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorLightAttenuationProcess",
        [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorLightAttenuationProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestDetectorLightAttenuationProcess::TRestDetectorLightAttenuationProcess() : TRestEventProcess() {
    fName = "TRestDetectorLightAttenuationProcess";
}

TRestDetectorLightAttenuationProcess::TRestDetectorLightAttenuationProcess(const std::string& instanceName,
                                                                           const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestDetectorLightAttenuationProcess::TRestDetectorLightAttenuationProcess(const std::string& fileName,
                                                                           const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorLightAttenuationProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestDetectorLightAttenuationProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }
    UpdateParamsFromYAML<TRestDetectorLightAttenuationProcess>(fNode);
    UpdateYAMLFromParams<TRestDetectorLightAttenuationProcess>(fNode);
}

void TRestDetectorLightAttenuationProcess::InitProcess() {
    const std::string user = "TRestDetectorLightAttenuationProcess";
    if (!fManager) throw std::runtime_error(user + ": no TRestManager");

    auto* readouts =
        dynamic_cast<TRestDetectorReadoutManager*>(fManager->GetMetadataClass("TRestDetectorReadoutManager"));
    if (!readouts) throw std::runtime_error(user + ": no TRestDetectorReadoutManager found");
    fReadout = readouts->SelectReadout(fReadoutName, user);

    const auto* medium = fManager->FindMetadata<TRestDetectorMedium>(fMediumName, user, "medium");
    if (!medium) throw std::runtime_error(user + ": needs a TRestDetectorMedium");
    fLight = &medium->GetLightResponse(user);
    if (fLight->GetAttenuationLength() <= 0 || fLight->GetEffectiveLightSpeed() <= 0) {
        throw std::runtime_error(user + ": the light of the medium '" + medium->GetName() +
                                 "' needs attenuationLength > 0 and effectiveLightSpeed > 0");
    }
    fMediumName = medium->GetName();
    // The stored metadata keeps the medium used
    UpdateYAMLFromParams<TRestDetectorLightAttenuationProcess>(fNode);

    RegisterObservable("NHitsWithoutChannel", fNHitsWithoutChannel);
}

bool TRestDetectorLightAttenuationProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* inputEvent = dynamic_cast<const TRestDetectorHitsEvent*>(&input);
    auto* outputEvent = dynamic_cast<TRestDetectorHitsEvent*>(&output);
    if (!inputEvent || !outputEvent) {
        throw std::runtime_error(
            "TRestDetectorLightAttenuationProcess: input/output is not a TRestDetectorHitsEvent");
    }

    outputEvent->Initialize();
    outputEvent->SetEventInfo(inputEvent);
    fNHitsWithoutChannel = 0;

    const TRestHits& hits = *inputEvent->GetHits();
    if (hits.GetNumberOfHits() == 0) return false;

    const double attenuationLength = fLight->GetAttenuationLength();
    const double lightSpeed = fLight->GetEffectiveLightSpeed();
    for (size_t n = 0; n < hits.GetNumberOfHits(); n++) {
        const XYZVector position(hits.GetX(n), hits.GetY(n), hits.GetZ(n));
        double energy = hits.GetEnergy(n);
        double time = hits.GetTime(n);

        const int channel = fReadout->GetChannelFromPosition(position.X(), position.Y(), position.Z());
        if (channel < 0) {
            fNHitsWithoutChannel++;
        } else {
            const double distance = fReadout->GetDistanceToReadout(channel, position);
            if (std::isnan(distance)) {
                throw std::runtime_error("TRestDetectorLightAttenuationProcess: the channel " +
                                         std::to_string(channel) + " of readout " + fReadout->GetName() +
                                         " has no distance to its readout (e.g. no readoutVolume)");
            }
            energy *= std::exp(-distance / attenuationLength);
            time += distance / lightSpeed;
        }
        outputEvent->AddHit(position, energy, time, hits.GetType(n));
    }

    RESTDebug << "TRestDetectorLightAttenuationProcess: event " << inputEvent->GetID() << ", "
              << fNHitsWithoutChannel << " hits without channel" << RESTendl;
    return true;
}
