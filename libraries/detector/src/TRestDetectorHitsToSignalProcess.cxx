/// \class TRestDetectorHitsToSignalProcess
/// Converts a TRestDetectorHitsEvent into a TRestDetectorSignalEvent: the energy of each hit is added to the
/// signal of the readout channel that collects it, at its arrival time.
///
/// The drift is given by the readout (TRestDetectorReadout::GetDistanceToReadout, e.g. the Micromegas plane
/// and its normal): the hit is collected at its projection on the readout (ProjectToReadout), which gives
/// the channel (GetChannelFromPosition), and arrives at time + distance / driftVelocity. The drift velocity
/// is the one of the TRestDetectorDrift, unless driftVelocity is given. Hits outside the drift volume
/// (distance < 0 or beyond the cathode, GetMaxDistanceToReadout) or without channel are rejected
/// (fiducialization), and counted in the observable NHitsRejected. For readouts without a distance (e.g. a
/// PMT) the channel is the one at the hit position and the time is the hit time.
///
/// With sampling > 0 the arrival times are rounded down to multiples of it (fewer points per signal);
/// otherwise the times are exact. Events without signals are rejected.
///
/// Differences with legacy: the drift velocity comes from the drift medium, there is no light-speed time
/// for veto channels (they go in their own pipeline, with a readout without distance), no readout planes
/// loop (the readout gives the distance) and the sampling is optional.
///
/// Parameters: readout and driftMedium (names, optional if there is only one), driftVelocity (0: the one
/// of the medium), sampling (0).
///
/// Example:
///
///    hitsToSignal:
///      class: "TRestDetectorHitsToSignalProcess"
///      observables:
///        - "NHitsRejected"

#include "TRestDetectorHitsToSignalProcess.h"

#include <cmath>
#include <map>
#include <stdexcept>

#include "TRestDetectorDrift.h"
#include "TRestDetectorHitsEvent.h"
#include "TRestDetectorReadout.h"
#include "TRestDetectorReadoutManager.h"
#include "TRestDetectorSignalEvent.h"
#include "TRestManager.h"
#include "TRestRun.h"

using ROOT::Math::XYZVector;

static const bool TRestDetectorHitsToSignalProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorHitsToSignalProcess;
    reg.RegisterField<P>("readout", &P::fReadoutName);
    reg.RegisterField<P>("driftMedium", &P::fDriftMediumName);
    reg.RegisterField<P>("driftVelocity", &P::fDriftVelocity);
    reg.RegisterField<P>("sampling", &P::fSampling);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorHitsToSignalProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorHitsToSignalProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestDetectorHitsToSignalProcess::TRestDetectorHitsToSignalProcess() : TRestEventProcess() {
    fName = "TRestDetectorHitsToSignalProcess";
}

TRestDetectorHitsToSignalProcess::TRestDetectorHitsToSignalProcess(const std::string& instanceName,
                                                                   const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestDetectorHitsToSignalProcess::TRestDetectorHitsToSignalProcess(const std::string& fileName,
                                                                   const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorHitsToSignalProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestDetectorHitsToSignalProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }
    UpdateParamsFromYAML<TRestDetectorHitsToSignalProcess>(fNode);
    UpdateYAMLFromParams<TRestDetectorHitsToSignalProcess>(fNode);
}

void TRestDetectorHitsToSignalProcess::InitProcess() {
    if (fSampling < 0) throw std::runtime_error("TRestDetectorHitsToSignalProcess: sampling must be >= 0");
    if (!fManager) throw std::runtime_error("TRestDetectorHitsToSignalProcess: no TRestManager");

    auto* readouts =
        dynamic_cast<TRestDetectorReadoutManager*>(fManager->GetMetadataClass("TRestDetectorReadoutManager"));
    if (!readouts) {
        throw std::runtime_error("TRestDetectorHitsToSignalProcess: no TRestDetectorReadoutManager found");
    }
    if (fReadoutName.empty() && readouts->fReadoutMap.size() == 1) {
        fReadoutName = readouts->fReadoutMap.begin()->first;
    }
    fReadout = readouts->GetReadout(fReadoutName);
    if (!fReadout) {
        throw std::runtime_error("TRestDetectorHitsToSignalProcess: readout '" + fReadoutName +
                                 "' not found. Set the 'readout' parameter");
    }

    // The drift velocity is only needed by readouts with a distance: checked with the first hit that drifts
    fVelocity = fDriftVelocity;
    if (fVelocity <= 0) {
        const auto media = fManager->GetMetadataOfType<TRestDetectorDrift>();
        const TRestDetectorDrift* medium = nullptr;
        if (fDriftMediumName.empty() && media.size() == 1) medium = media.front();
        for (const auto* m : media) {
            if (m->GetName() == fDriftMediumName) medium = m;
        }
        if (medium) fVelocity = medium->GetDriftVelocity();
    }
    // The stored metadata keeps the readout and drift velocity used, not the defaults of the YAML
    fDriftVelocity = fVelocity;
    UpdateYAMLFromParams<TRestDetectorHitsToSignalProcess>(fNode);

    RegisterObservable("NHitsRejected", fNHitsRejected);
}

bool TRestDetectorHitsToSignalProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* hitsEvent = dynamic_cast<const TRestDetectorHitsEvent*>(&input);
    auto* signalEvent = dynamic_cast<TRestDetectorSignalEvent*>(&output);
    if (!hitsEvent || !signalEvent) {
        throw std::runtime_error(
            "TRestDetectorHitsToSignalProcess: input/output is not a TRestDetectorHitsEvent/"
            "TRestDetectorSignalEvent");
    }

    signalEvent->Initialize();
    signalEvent->SetEventInfo(hitsEvent);

    // Charge of each channel versus the arrival time, sorted
    std::map<int, std::map<double, double>> charges;
    fNHitsRejected = 0;
    for (size_t n = 0; n < hitsEvent->GetNumberOfHits(); n++) {
        const XYZVector position = hitsEvent->GetPosition(n);
        double time = hitsEvent->GetTime(n);
        XYZVector collected = position;

        const double distance = fReadout->GetDistanceToReadout(position);
        if (!std::isnan(distance)) {
            if (distance < 0 || distance > fReadout->GetMaxDistanceToReadout(position)) {
                fNHitsRejected++;
                continue;
            }
            if (fVelocity <= 0) {
                throw std::runtime_error(
                    "TRestDetectorHitsToSignalProcess: no drift velocity. Define a TRestDetectorDrift (set "
                    "'driftMedium' if there is more than one) or set 'driftVelocity'");
            }
            collected = fReadout->ProjectToReadout(position);
            time += distance / fVelocity;
        }

        const int daqID = fReadout->GetChannelFromPosition(collected.X(), collected.Y(), collected.Z());
        if (daqID < 0) {
            fNHitsRejected++;
            continue;
        }
        if (fSampling > 0) time = std::floor(time / fSampling) * fSampling;
        charges[daqID][time] += hitsEvent->GetEnergy(n);
    }

    for (const auto& [daqID, points] : charges) {
        signalEvent->AddSignal(daqID, std::vector<std::pair<double, double>>(points.begin(), points.end()));
    }

    RESTDebug << "TRestDetectorHitsToSignalProcess: " << signalEvent->GetNumberOfSignals() << " signals, "
              << fNHitsRejected << " hits rejected" << RESTendl;
    return signalEvent->GetNumberOfSignals() > 0;
}
