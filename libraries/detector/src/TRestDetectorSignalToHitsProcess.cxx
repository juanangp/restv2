/// \class TRestDetectorSignalToHitsProcess
/// Creates the hits of an event from its signals. The channel of each signal gives the coordinates
/// defined by the readout (e.g. only X or only Y for strips; undefined ones are NaN) and, if
/// driftVelocity > 0, the coordinate along driftAxis where the distance to the readout is
/// driftVelocity * time (as legacy: plane position + normal * distance). If the readout defines
/// TRestDetectorReadout::GetDistanceToReadout (e.g. the Micromegas plane, positive towards its cathode
/// as set by its normal), the plane and the drift sense come from the readout and the sign of driftAxis
/// is ignored. Otherwise the plane is at 0 and the sense is the sign of driftAxis ("-Z" puts the drift
/// volume on the -Z side). The drift only fills a
/// coordinate the readout leaves undefined: channels whose readout already gives it (e.g. the centre of
/// a scintillator) keep their position, so detectors with and without drift can share the process.
/// The hit type (X, Y, XY, ...) is deduced from the coordinates that are defined.
///
/// Methods (energy, time of each hit):
/// - onlyMax, tripleMax, tripleMaxAverage, gaussFit, landauFit, agetFit: one hit at the pulse maximum.
/// - integral: one hit with the charge integrated in integralRange (whole signal if empty) at the time
///   of the maximum in that range. For pulses without drift (scintillators, solid state, bolometers).
/// - qCenter: one hit at the charge-weighted mean time, energy = mean charge per point (as legacy).
/// - all: one hit per signal point.
/// - intWindow: one hit per integration window of intWindow, if the mean charge is over threshold.
///
/// Parameters: readout (name in TRestDetectorReadoutManager, optional if only one), driftVelocity (0:
/// from the TRestDetectorDrift driftMedium, optional if only one, or no drift if there is none),
/// driftMedium (""), driftAxis ("Z"), method ("tripleMaxAverage"), intWindow (5), threshold (100),
/// integralRange ([0, 0]).
///
/// Example:
///
///    signalToHits:
///      class: "TRestDetectorSignalToHitsProcess"
///      driftVelocity: 1.2
///      method: "tripleMaxAverage"

#include "TRestDetectorSignalToHitsProcess.h"

#include <cmath>
#include <stdexcept>

#include "TRestConstants.h"
#include "TRestDetectorHitsEvent.h"
#include "TRestDetectorDrift.h"
#include "TRestDetectorReadout.h"
#include "TRestDetectorSignalEvent.h"
#include "TRestDetectorReadoutManager.h"
#include "TRestManager.h"
#include "TRestPulseShapeAnalysis.h"

using namespace TRestPulseShapeAnalysis;

static const bool TRestDetectorSignalToHitsProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorSignalToHitsProcess;
    reg.RegisterField<P>("readout", &P::fReadoutName);
    reg.RegisterField<P>("driftVelocity", &P::fDriftVelocity);
    reg.RegisterField<P>("driftMedium", &P::fDriftMediumName);
    reg.RegisterField<P>("driftAxis", &P::fDriftAxis);
    reg.RegisterField<P>("method", &P::fMethod);
    reg.RegisterField<P>("intWindow", &P::fIntWindow);
    reg.RegisterField<P>("threshold", &P::fThreshold);
    reg.RegisterField<P>("integralRange", &P::fIntegralRange);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorSignalToHitsProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorSignalToHitsProcess>(instanceName, params);
        });
    return true;
}();

/// Sets the coordinate along the drift axis, only if the channel position leaves it undefined
void SetDriftCoordinate(ROOT::Math::XYZVector& p, char axis, double value) {
    if (axis == 'X' && std::isnan(p.X())) p.SetX(value);
    if (axis == 'Y' && std::isnan(p.Y())) p.SetY(value);
    if (axis == 'Z' && std::isnan(p.Z())) p.SetZ(value);
}

/// Drift coordinate as {offset, slope}: coordinate along the axis = offset + slope * distance, where
/// distance is the one to the readout of a channel. TRestDetectorReadout::GetDistanceToReadout is a
/// projection on the plane normal, so it is affine along the axis and two evaluations give the plane
/// (offset) and the sense (slope). {0, direction} if the readout defines no distance along the axis.
std::pair<double, double> GetDriftLine(const TRestDetectorReadout& readout, int daqID,
                                       ROOT::Math::XYZVector p, char axis, int direction) {
    if (std::isnan(p.X())) p.SetX(0);
    if (std::isnan(p.Y())) p.SetY(0);
    if (std::isnan(p.Z())) p.SetZ(0);
    const ROOT::Math::XYZVector step(axis == 'X', axis == 'Y', axis == 'Z');
    p -= step * (axis == 'X' ? p.X() : axis == 'Y' ? p.Y() : p.Z());  // coordinate 0 on the axis
    const double d0 = readout.GetDistanceToReadout(daqID, p);
    const double d1 = readout.GetDistanceToReadout(daqID, p + step);
    if (std::isnan(d0) || std::isnan(d1) || d1 == d0) return {0, direction};
    return {-d0 / (d1 - d0), 1 / (d1 - d0)};
}

/// Hit type from the coordinates that are defined
TRestHitsData::REST_HitType GetHitType(const ROOT::Math::XYZVector& p) {
    const bool x = !std::isnan(p.X()), y = !std::isnan(p.Y()), z = !std::isnan(p.Z());
    if (x && y && z) return TRestHitsData::XYZ;
    if (x && y) return TRestHitsData::XY;
    if (x && z) return TRestHitsData::XZ;
    if (y && z) return TRestHitsData::YZ;
    if (x) return TRestHitsData::X;
    if (y) return TRestHitsData::Y;
    if (z) return TRestHitsData::Z;
    return TRestHitsData::unknown;
}
}  // namespace

TRestDetectorSignalToHitsProcess::TRestDetectorSignalToHitsProcess() : TRestEventProcess() {
    fName = "TRestDetectorSignalToHitsProcess";
}

TRestDetectorSignalToHitsProcess::TRestDetectorSignalToHitsProcess(const std::string& instanceName,
                                                                   const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestDetectorSignalToHitsProcess::TRestDetectorSignalToHitsProcess(const std::string& fileName,
                                                                   const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorSignalToHitsProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestDetectorSignalToHitsProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestDetectorSignalToHitsProcess>(fNode);
    UpdateYAMLFromParams<TRestDetectorSignalToHitsProcess>(fNode);
}

void TRestDetectorSignalToHitsProcess::InitProcess() {
    if (kMethods.count(fMethod) == 0) {
        throw std::runtime_error("TRestDetectorSignalToHitsProcess: unknown method '" + fMethod + "'");
    }
    const std::string axis = !fDriftAxis.empty() && (fDriftAxis[0] == '+' || fDriftAxis[0] == '-')
                                 ? fDriftAxis.substr(1)
                                 : fDriftAxis;
    if (axis != "X" && axis != "Y" && axis != "Z") {
        throw std::runtime_error("TRestDetectorSignalToHitsProcess: driftAxis must be [+-]X, Y or Z, not '" +
                                 fDriftAxis + "'");
    }
    fAxis = axis[0];
    fDriftDirection = fDriftAxis[0] == '-' ? -1 : 1;

    auto* readouts = fManager ? dynamic_cast<TRestDetectorReadoutManager*>(
                                    fManager->GetMetadataClass("TRestDetectorReadoutManager"))
                              : nullptr;
    if (!readouts) {
        throw std::runtime_error("TRestDetectorSignalToHitsProcess: no TRestDetectorReadoutManager found");
    }
    if (fReadoutName.empty() && readouts->fReadoutMap.size() == 1) {
        fReadoutName = readouts->fReadoutMap.begin()->first;
    }
    fReadout = readouts->GetReadout(fReadoutName);
    if (!fReadout) {
        throw std::runtime_error("TRestDetectorSignalToHitsProcess: readout '" + fReadoutName +
                                 "' not found. Set the 'readout' parameter");
    }

    if (fDriftVelocity <= 0) {
        const auto media = fManager->GetMetadataOfType<TRestDetectorDrift>();
        const TRestDetectorDrift* medium = nullptr;
        if (fDriftMediumName.empty() && media.size() == 1) medium = media.front();
        for (const auto* m : media) {
            if (m->GetName() == fDriftMediumName) medium = m;
        }
        if (!medium && !fDriftMediumName.empty()) {
            throw std::runtime_error("TRestDetectorSignalToHitsProcess: drift medium '" + fDriftMediumName +
                                     "' not found");
        }
        if (medium && medium->GetDriftVelocity() <= 0 && !fDriftMediumName.empty()) {
            throw std::runtime_error("TRestDetectorSignalToHitsProcess: drift medium '" + fDriftMediumName +
                                     "' has no driftVelocity");
        }
        if (medium) fDriftVelocity = medium->GetDriftVelocity();
    }
}

bool TRestDetectorSignalToHitsProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* signalEvent = dynamic_cast<const TRestDetectorSignalEvent*>(&input);
    auto* hitsEvent = dynamic_cast<TRestDetectorHitsEvent*>(&output);
    if (!signalEvent || !hitsEvent) {
        throw std::runtime_error(
            "TRestDetectorSignalToHitsProcess: input/output is not a TRestDetectorSignalEvent/"
            "TRestDetectorHitsEvent");
    }

    hitsEvent->Initialize();
    hitsEvent->SetEventInfo(&input);

    for (int s = 0; s < signalEvent->GetNumberOfSignals(); s++) {
        const auto& signal = signalEvent->GetSignal(s);
        if (signal.GetNPoints() == 0) continue;
        const int daqID = signal.GetSignalID();

        auto channelPos = fReadout->GetPositionFromChannel(daqID);
        if (std::isnan(channelPos.X()) && std::isnan(channelPos.Y()) && std::isnan(channelPos.Z())) {
            RESTDebug << "TRestDetectorSignalToHitsProcess: channel " << daqID << " not found in readout"
                      << RESTendl;
            continue;
        }

        // (time, energy) pairs of the hits created by this signal
        std::vector<std::pair<double, double>> hits;
        auto graph = signal.GetGraph();
        if (fMethod == "intWindow") {
            for (const auto& hit : GetIntWindow(graph, fIntWindow)) {
                if (hit.second > fThreshold) hits.push_back(hit);
            }
        } else if (fMethod == "integral") {
            const bool wholeSignal = fIntegralRange.first >= fIntegralRange.second;
            double charge = 0, maxCharge = 0, maxTime = -1;
            for (int j = 0; j < signal.GetNPoints(); j++) {
                const auto [time, value] = signal.GetPoint(j);
                if (!wholeSignal && (time < fIntegralRange.first || time > fIntegralRange.second)) continue;
                charge += value;
                if (maxTime < 0 || value > maxCharge) {
                    maxCharge = value;
                    maxTime = time;
                }
            }
            if (maxTime >= 0 && charge > 0) hits.emplace_back(maxTime, charge);
        } else if (fMethod == "qCenter") {
            double charge = 0, weightedTime = 0;
            for (int j = 0; j < signal.GetNPoints(); j++) {
                const auto [time, value] = signal.GetPoint(j);
                charge += value;
                weightedTime += time * value;
            }
            if (charge > 0) hits.emplace_back(weightedTime / charge, charge / signal.GetNPoints());
        } else if (fMethod == "all") {
            for (int j = 0; j < signal.GetNPoints(); j++) hits.push_back(signal.GetPoint(j));
        } else {
            const auto hit = GetMax(graph, fMethod);
            if (hit.first >= 0 && hit.second > 0) hits.push_back(hit);  // fits return -1 on failure
        }

        const auto [driftOffset, driftSlope] = fDriftVelocity > 0
                                                   ? GetDriftLine(*fReadout, daqID, channelPos, fAxis,
                                                                  fDriftDirection)
                                                   : std::pair<double, double>{0, 0};
        for (const auto& [time, energy] : hits) {
            auto position = channelPos;
            if (fDriftVelocity > 0) {
                SetDriftCoordinate(position, fAxis, driftOffset + driftSlope * fDriftVelocity * time);
            }
            hitsEvent->AddHit(position, energy, time, GetHitType(position));
        }
    }

    return hitsEvent->GetNumberOfHits() > 0;
}
