/// \class TRestDetectorLightCollectionProcess
/// Collection of the scintillation photons of the hits of a TRestDetectorHitsEvent (energies in photons,
/// from TRestDetectorScintillationProcess) by the readout channels and their detection by the photosensor of
/// each channel (SiPM, PMT): the photons reaching it give Binomial(N, quantumEfficiency) photoelectrons. The
/// output hits have the photoelectrons of each channel (energies in photoelectrons), one per input hit and
/// channel, at a position that the readout assigns to that channel (so TRestDetectorHitsToSignalProcess puts
/// them in it); the gain of the photosensor and the electronics go into TRestDetectorSignalShapingProcess and
/// TRestDetectorSignalToRawSignalProcess. The way the light reaches the channels depends on the
/// scintillator (`model`):
///
/// - `localCapture`, opaque scintillator (e.g. LiquidO: the light stays near where it is produced and the
///   fibres around it capture it): the channels closer than maxDistance to the hit
///   (TRestDetectorReadout::GetChannelsInRange, e.g. the fibres of a TRestFibreReadout, in the plane
///   transverse to them) capture each photon with probability captureProbability(r), r the distance to the
///   channel; the photons of the hit are distributed among them with a multinomial (a photon is captured by
///   one channel at most; the probabilities of the channels of a position must add up to 1 at most).
///   captureProbability includes the transport in the medium and the trapping and transport of the fibre
///   to its photosensor: a ROOT formula of x = r in mm (e.g. a Gaussian light ball) or a graph or histogram
///   of a ROOT file ("file.root:name", e.g. from an optical simulation of Geant4). The output hits are at
///   the position given by the readout (for a fibre, the projection of the hit on it); their time is that of
///   the hit.
/// - `attenuation`, transparent scintillator read at one end (e.g. a veto with a light guide and a PMT):
///   all the light goes to the channel of the hit (TRestDetectorReadout::GetChannelFromPosition), the
///   photons reaching its readout are Binomial(N, exp(-d / attenuationLength)) and they arrive at
///   t + d / effectiveLightSpeed, with d the distance to the readout of the channel
///   (TRestDetectorReadout::GetDistanceToReadout, e.g. to the readoutVolume of a TRestGDMLReadout) and the
///   `light` response of the medium (attenuationLength 0: no attenuation; effectiveLightSpeed 0: no delay). A
///   channel without distance to its readout is an error. The output hits are at the position of the hit.
///
/// Hits without any channel collecting their light are counted in the observable NHitsWithoutChannel
/// (their photons are lost); NPhotoelectrons is the total of the event. Events without photoelectrons are
/// rejected.
///
/// Parameters: model (localCapture), readout (empty: the data readout of the TRestDetectorReadoutManager),
/// medium (attenuation: name of the TRestDetectorMedium with `light`; empty: the only one),
/// captureProbability and maxDistance (localCapture), quantumEfficiency (1), seed (0: random seed; otherwise
/// the generator is re-seeded at each event from the seed and the event and subevent IDs).
///
/// Examples:
///
///    # LiquidO (TRestFibreReadout): Gaussian light ball of 10 mm, SiPMs
///    lightCollection:
///      class: "TRestDetectorLightCollectionProcess"
///      model: "localCapture"
///      captureProbability: "0.03*exp(-x*x/(2*10*10))"
///      maxDistance: 40 mm
///      quantumEfficiency: 0.4   # SiPMs
///      observables: ["NPhotoelectrons", "NHitsWithoutChannel"]
///
///    # Vetoes (TRestGDMLReadout with readoutVolume), medium with attenuationLength and effectiveLightSpeed
///    lightCollection:
///      class: "TRestDetectorLightCollectionProcess"
///      model: "attenuation"
///      readout: "Veto_Readout"
///      medium: "BC408"
///      quantumEfficiency: 0.25  # PMT
///
/// The attenuation model replaces the legacy TRestDetectorLightAttenuationProcess (energy E exp(-d/λ) of the
/// veto hits), now with photons; its parameters (vetoLightAttenuationLength, vetoEffectiveLightSpeed) are
/// the light response of the medium and the distance is to the readout volume of the channel.

#include "TRestDetectorLightCollectionProcess.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>

#include "TF1.h"
#include "TFile.h"
#include "TGraph.h"
#include "TH1.h"
#include "TRestDetectorHitsEvent.h"
#include "TRestDetectorMedium.h"
#include "TRestDetectorReadout.h"
#include "TRestDetectorReadoutManager.h"
#include "TRestManager.h"
#include "TRestTools.h"

using ROOT::Math::XYZVector;

static const bool TRestDetectorLightCollectionProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorLightCollectionProcess;
    reg.RegisterField<P>("model", &P::fModel);
    reg.RegisterField<P>("readout", &P::fReadoutName);
    reg.RegisterField<P>("medium", &P::fMediumName);
    reg.RegisterField<P>("captureProbability", &P::fCaptureProbability);
    reg.RegisterField<P>("maxDistance", &P::fMaxDistance);
    reg.RegisterField<P>("quantumEfficiency", &P::fQuantumEfficiency);
    reg.RegisterField<P>("seed", &P::fSeed);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorLightCollectionProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorLightCollectionProcess>(instanceName, params);
        });
    return true;
}();

/// Function of the distance (mm) from "file.root:name" (graph or histogram) or a formula of x
std::function<double(double)> MakeCaptureFunction(const std::string& text, double maxDistance,
                                                  const std::string& prefix) {
    const auto separator = text.rfind(':');
    const auto extension = text.find(".root");
    if (separator != std::string::npos && extension != std::string::npos && separator > extension) {
        const std::string fileName = text.substr(0, separator);
        const std::string objectName = text.substr(separator + 1);
        std::unique_ptr<TFile> file(TFile::Open(fileName.c_str()));
        if (!file || file->IsZombie()) throw std::runtime_error(prefix + "cannot open " + fileName);
        TObject* object = file->Get(objectName.c_str());
        if (auto* graph = dynamic_cast<TGraph*>(object)) {
            std::shared_ptr<TGraph> copy(static_cast<TGraph*>(graph->Clone()));
            return [copy](double r) { return copy->Eval(r); };
        }
        if (auto* histogram = dynamic_cast<TH1*>(object)) {
            std::shared_ptr<TH1> copy(static_cast<TH1*>(histogram->Clone()));
            copy->SetDirectory(nullptr);
            return [copy](double r) { return copy->Interpolate(r); };
        }
        throw std::runtime_error(prefix + "no graph or histogram '" + objectName + "' in " + fileName);
    }
    // Not added to the list of functions of ROOT (several processes may have the same formula)
    auto function =
        std::make_shared<TF1>("lightCaptureProbability", text.c_str(), 0, maxDistance, TF1::EAddToList::kNo);
    if (!function->IsValid()) {
        throw std::runtime_error(prefix + "captureProbability '" + text + "' is not a valid formula of x");
    }
    return [function](double r) { return function->Eval(r); };
}
}  // namespace

TRestDetectorLightCollectionProcess::TRestDetectorLightCollectionProcess() : TRestEventProcess() {
    fName = "TRestDetectorLightCollectionProcess";
}

TRestDetectorLightCollectionProcess::TRestDetectorLightCollectionProcess(const std::string& instanceName,
                                                                         const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestDetectorLightCollectionProcess::TRestDetectorLightCollectionProcess(const std::string& fileName,
                                                                         const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorLightCollectionProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestDetectorLightCollectionProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }
    UpdateParamsFromYAML<TRestDetectorLightCollectionProcess>(fNode);
    const std::string prefix = "TRestDetectorLightCollectionProcess: ";
    fModel = TRestTools::NormalizeOption("model", fModel, {"localCapture", "attenuation"});
    if (fQuantumEfficiency < 0 || fQuantumEfficiency > 1) {
        throw std::runtime_error(prefix + "quantumEfficiency must be in [0, 1]");
    }
    fCapture = nullptr;
    if (fModel == "localCapture") {
        if (fMaxDistance <= 0) throw std::runtime_error(prefix + "localCapture needs maxDistance > 0");
        if (fCaptureProbability.empty()) {
            throw std::runtime_error(prefix + "localCapture needs captureProbability");
        }
        fCapture = MakeCaptureFunction(fCaptureProbability, fMaxDistance, prefix);
    }
    UpdateYAMLFromParams<TRestDetectorLightCollectionProcess>(fNode);
}

double TRestDetectorLightCollectionProcess::GetCaptureProbability(double distance) const {
    if (!fCapture || distance > fMaxDistance) return 0;
    return std::max(0.0, fCapture(distance));
}

void TRestDetectorLightCollectionProcess::InitProcess() {
    const std::string user = "TRestDetectorLightCollectionProcess";
    if (!fManager) throw std::runtime_error(user + ": no TRestManager");

    auto* readouts =
        dynamic_cast<TRestDetectorReadoutManager*>(fManager->GetMetadataClass("TRestDetectorReadoutManager"));
    if (!readouts) throw std::runtime_error(user + ": no TRestDetectorReadoutManager found");
    fReadout = readouts->SelectReadout(fReadoutName, user);

    fLight = nullptr;
    if (fModel == "attenuation") {
        const auto* medium = fManager->FindMetadata<TRestDetectorMedium>(fMediumName, user, "medium");
        if (!medium) throw std::runtime_error(user + ": the attenuation model needs a TRestDetectorMedium");
        fLight = &medium->GetLightResponse(user);
        fMediumName = medium->GetName();
    }
    // The stored metadata keeps the readout and medium used
    UpdateYAMLFromParams<TRestDetectorLightCollectionProcess>(fNode);

    // Seed 0: random seed. Otherwise re-seeded at each event in ProcessEvent
    fRandom.SetSeed(fSeed);

    RegisterObservable("NPhotoelectrons", fNPhotoelectrons);
    RegisterObservable("NHitsWithoutChannel", fNHitsWithoutChannel);
}

int TRestDetectorLightCollectionProcess::Detect(int photons) {
    if (photons <= 0) return 0;
    if (fQuantumEfficiency >= 1) return photons;
    return static_cast<int>(fRandom.Binomial(photons, fQuantumEfficiency));
}

void TRestDetectorLightCollectionProcess::CaptureLocally(const XYZVector& position, int photons, double time,
                                                         TRestDetectorHitsEvent& output) {
    const auto channels = fReadout->GetChannelsInRange(position, fMaxDistance);
    // Multinomial: each channel takes a binomial of the photons left, with its probability relative to
    // what is left of the total
    int left = photons;
    double probabilityLeft = 1;
    bool collected = false;
    for (const auto& channel : channels) {
        const double probability = GetCaptureProbability(channel.distance);
        if (probability <= 0) continue;
        collected = true;
        if (probability > probabilityLeft + 1e-9) {
            throw std::runtime_error(
                "TRestDetectorLightCollectionProcess: the capture probabilities of the channels around a "
                "position add up to more than 1 (captureProbability too large for the distance between "
                "channels)");
        }
        if (left <= 0) {
            probabilityLeft -= probability;
            continue;
        }
        const int captured = probability >= probabilityLeft
                                 ? left
                                 : static_cast<int>(fRandom.Binomial(left, probability / probabilityLeft));
        probabilityLeft -= probability;
        left -= captured;
        const int photoelectrons = Detect(captured);
        if (photoelectrons <= 0) continue;
        output.AddHit(channel.position, photoelectrons, time);
        fNPhotoelectrons += photoelectrons;
    }
    if (!collected) fNHitsWithoutChannel++;
}

void TRestDetectorLightCollectionProcess::Attenuate(const XYZVector& position, int photons, double time,
                                                    TRestDetectorHitsEvent& output) {
    const int channel = fReadout->GetChannelFromPosition(position.X(), position.Y(), position.Z());
    if (channel < 0) {
        fNHitsWithoutChannel++;
        return;
    }
    const double distance = fReadout->GetDistanceToReadout(channel, position);
    if (std::isnan(distance)) {
        throw std::runtime_error("TRestDetectorLightCollectionProcess: the channel " +
                                 std::to_string(channel) + " of readout " + fReadout->GetName() +
                                 " has no distance to its readout (e.g. no readoutVolume)");
    }
    const double attenuationLength = fLight->GetAttenuationLength();
    const double lightSpeed = fLight->GetEffectiveLightSpeed();
    const int arriving =
        attenuationLength > 0
            ? static_cast<int>(fRandom.Binomial(photons, std::exp(-distance / attenuationLength)))
            : photons;
    const int photoelectrons = Detect(arriving);
    if (photoelectrons <= 0) return;
    output.AddHit(position, photoelectrons, lightSpeed > 0 ? time + distance / lightSpeed : time);
    fNPhotoelectrons += photoelectrons;
}

bool TRestDetectorLightCollectionProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* inputEvent = dynamic_cast<const TRestDetectorHitsEvent*>(&input);
    auto* outputEvent = dynamic_cast<TRestDetectorHitsEvent*>(&output);
    if (!inputEvent || !outputEvent) {
        throw std::runtime_error(
            "TRestDetectorLightCollectionProcess: input/output is not a TRestDetectorHitsEvent");
    }

    outputEvent->Initialize();
    outputEvent->SetEventInfo(inputEvent);
    if (fSeed != 0) fRandom.SetSeed(TRestDetectorHitsEvent::EventSeed(fSeed, *inputEvent));
    fNPhotoelectrons = 0;
    fNHitsWithoutChannel = 0;

    for (size_t n = 0; n < inputEvent->GetNumberOfHits(); n++) {
        const int photons = static_cast<int>(std::lround(inputEvent->GetEnergy(n)));
        if (photons <= 0) continue;
        if (fModel == "localCapture") {
            CaptureLocally(inputEvent->GetPosition(n), photons, inputEvent->GetTime(n), *outputEvent);
        } else {
            Attenuate(inputEvent->GetPosition(n), photons, inputEvent->GetTime(n), *outputEvent);
        }
    }

    RESTDebug << "TRestDetectorLightCollectionProcess: event " << inputEvent->GetID() << ", "
              << fNPhotoelectrons << " photoelectrons, " << fNHitsWithoutChannel << " hits without channel"
              << RESTendl;
    return outputEvent->GetNumberOfHits() > 0;
}
