/// \class TRestDetectorAvalancheProcess
/// Applies the avalanche gain of the amplification gap to the electrons drifted by
/// TRestDetectorHitsDriftProcess. The amplification field is E = amplificationVoltage / gap (uniform,
/// parallel plates); the mean gain G and the Polya parameter theta at E come from the gain table of
/// the TRestDetectorMedium with that gap at the pressure and temperature of the medium (`table`, its key,
/// only if there are several; a field out of the range of the table is an error). The gain of one electron
/// follows a Polya distribution; a hit with energy E_hit carries n = E_hit / W electrons (more than one with
/// maxHits in hitsDrift), whose total gain, a sum of n Polya, is drawn from a Gamma(n (theta + 1), G / (theta
/// + 1)). The energy of the hit is multiplied by its gain per electron (total gain / n), as legacy multiplied
/// it by the gain. G and theta used are written to the metadata of the process.
///
/// Differences with legacy: the gain comes from the avalanche simulations of restGarfield at the voltage,
/// with Polya fluctuations, instead of a fixed detectorGain with a gaussian resolution
/// (energyReference, resolutionReference: that is TRestDetectorHitsSmearingProcess, not to be used with
/// this process).
///
/// Parameters: amplificationVoltage, gap, medium (name, optional if there is only one), table (""), seed (0).
///
/// Example:
///
///    avalanche:
///      class: "TRestDetectorAvalancheProcess"
///      amplificationVoltage: 330 V
///      gap: 128 um

#include "TRestDetectorAvalancheProcess.h"

#include <cmath>
#include <stdexcept>

#include "TRestDetectorHitsEvent.h"
#include "TRestDetectorMedium.h"
#include "TRestDetectorTransportManager.h"
#include "TRestManager.h"

static const bool TRestDetectorAvalancheProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorAvalancheProcess;
    reg.RegisterField<P>("amplificationVoltage", &P::fAmplificationVoltage);
    reg.RegisterField<P>("gap", &P::fGap);
    reg.RegisterField<P>("medium", &P::fMediumName);
    reg.RegisterField<P>("table", &P::fTable);
    reg.RegisterField<P>("seed", &P::fSeed);
    reg.RegisterField<P>("gain", &P::fGain);
    reg.RegisterField<P>("theta", &P::fTheta);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorAvalancheProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorAvalancheProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestDetectorAvalancheProcess::TRestDetectorAvalancheProcess() : TRestEventProcess() {
    fName = "TRestDetectorAvalancheProcess";
}

TRestDetectorAvalancheProcess::TRestDetectorAvalancheProcess(const std::string& instanceName,
                                                             const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestDetectorAvalancheProcess::TRestDetectorAvalancheProcess(const std::string& fileName,
                                                             const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorAvalancheProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestDetectorAvalancheProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }
    UpdateParamsFromYAML<TRestDetectorAvalancheProcess>(fNode);
    UpdateYAMLFromParams<TRestDetectorAvalancheProcess>(fNode);
}

void TRestDetectorAvalancheProcess::InitProcess() {
    const std::string user = "TRestDetectorAvalancheProcess";
    if (fAmplificationVoltage <= 0 || fGap <= 0) {
        throw std::runtime_error(user + ": amplificationVoltage and gap must be > 0");
    }
    if (!fManager) throw std::runtime_error(user + ": no TRestManager");
    const auto* medium = fManager->FindMetadata<TRestDetectorMedium>(fMediumName, user, "medium");
    if (!medium) throw std::runtime_error(user + ": needs a TRestDetectorMedium");
    fCharge = &medium->GetChargeResponse(user);
    if (fCharge->GetWValue() <= 0) {
        throw std::runtime_error(user + ": the charge of the medium '" + medium->GetName() +
                                 "' needs wValue > 0");
    }

    const auto& table = fCharge->GetGainTable(fGap, fTable);
    const double field = fAmplificationVoltage / fGap;  // V/mm
    fGain = table.GetGain(field);
    fTheta = table.GetTheta(field);
    if (fGain <= 0 || fTheta <= -1) {
        throw std::runtime_error(user + ": table " + table.GetKey() + " gives gain " + std::to_string(fGain) +
                                 " and theta " + std::to_string(fTheta) +
                                 " (gain > 0 and theta > -1 needed)");
    }
    fMediumName = medium->GetName();
    fTable = table.GetKey();
    // The stored metadata keeps the medium, the table and the gain used
    UpdateYAMLFromParams<TRestDetectorAvalancheProcess>(fNode);

    fEngine.seed(fSeed != 0 ? fSeed : std::random_device{}());
}

bool TRestDetectorAvalancheProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* inputEvent = dynamic_cast<const TRestDetectorHitsEvent*>(&input);
    auto* outputEvent = dynamic_cast<TRestDetectorHitsEvent*>(&output);
    if (!inputEvent || !outputEvent) {
        throw std::runtime_error(
            "TRestDetectorAvalancheProcess: input/output is not a TRestDetectorHitsEvent");
    }

    outputEvent->Initialize();
    outputEvent->SetEventInfo(inputEvent);

    const double wValue = fCharge->GetWValue();
    for (size_t n = 0; n < inputEvent->GetNumberOfHits(); n++) {
        const double energy = inputEvent->GetEnergy(n);
        if (energy <= 0) continue;
        // Sum of the Polya gains of the electrons of the hit
        const double electrons = energy / wValue;
        std::gamma_distribution<double> gain(electrons * (fTheta + 1), fGain / (fTheta + 1));
        outputEvent->AddHit(inputEvent->GetPosition(n), energy * gain(fEngine) / electrons,
                            inputEvent->GetTime(n), inputEvent->GetType(n));
    }

    return outputEvent->GetNumberOfHits() > 0;
}
