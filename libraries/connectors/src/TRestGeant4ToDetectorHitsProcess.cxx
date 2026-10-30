/// \class TRestGeant4ToDetectorHitsProcess
/// Transfers the hits (position, energy, time) of a TRestGeant4Event with positive energy to a
/// TRestDetectorHitsEvent as XYZ hits. Only the hits of the volumes selected as in
/// TRestGeant4QuenchingProcess are transferred (TRestGeant4GeometryInfo::GetVolumesMatchingExpression:
/// physical volumes matching each regular expression or, if none, those of the matching logical volumes);
/// without `volumes`, those of the sensitive volumes of the simulation (`volumes: [".*"]` or `["^."]`
/// for all of them).
///
/// Difference with legacy: there is no `type` per volume (legacy VETO hits), the hits of different
/// detectors are transferred by separate processes or pipelines with their own `volumes`.
///
/// Parameters: volumes ([], the sensitive volumes), list of regular expressions (e.g. "gas" for any name
/// containing gas, "^gas" for the names starting with it).
///
/// Example:
///
///    geant4ToHits:
///      class: "TRestGeant4ToDetectorHitsProcess"
///      volumes:
///        - "gasVolume"
///        - "^scintillatorVolume"

#include "TRestGeant4ToDetectorHitsProcess.h"

#include <stdexcept>

#include "TRestDetectorHitsEvent.h"
#include "TRestGeant4Event.h"
#include "TRestGeant4Metadata.h"
#include "TRestRun.h"

static const bool TRestGeant4ToDetectorHitsProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestGeant4ToDetectorHitsProcess;
    reg.RegisterField<P>("volumes", &P::fVolumes);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestGeant4ToDetectorHitsProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestGeant4ToDetectorHitsProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestGeant4ToDetectorHitsProcess::TRestGeant4ToDetectorHitsProcess() : TRestEventProcess() {
    fName = "TRestGeant4ToDetectorHitsProcess";
}

TRestGeant4ToDetectorHitsProcess::TRestGeant4ToDetectorHitsProcess(const std::string& instanceName,
                                                                   const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestGeant4ToDetectorHitsProcess::TRestGeant4ToDetectorHitsProcess(const std::string& fileName,
                                                                   const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestGeant4ToDetectorHitsProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestGeant4ToDetectorHitsProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestGeant4ToDetectorHitsProcess>(fNode);
    UpdateYAMLFromParams<TRestGeant4ToDetectorHitsProcess>(fNode);
}

void TRestGeant4ToDetectorHitsProcess::InitProcess() {
    fVolumeIDs.clear();

    const auto* metadata =
        fRunInfo ? dynamic_cast<TRestGeant4Metadata*>(fRunInfo->GetMetadataClass("TRestGeant4Metadata"))
                 : nullptr;
    if (!metadata) {
        throw std::runtime_error(
            "TRestGeant4ToDetectorHitsProcess: no TRestGeant4Metadata found, needed to select volumes");
    }
    const auto& geometryInfo = metadata->GetGeant4GeometryInfo();

    if (fVolumes.empty()) {
        const auto& sensitive = metadata->GetSensitiveVolumes();
        fVolumeIDs = geometryInfo.GetVolumeIDsFromNames({sensitive.begin(), sensitive.end()});
        if (fVolumeIDs.empty()) {
            throw std::runtime_error(
                "TRestGeant4ToDetectorHitsProcess: no 'volumes' and no sensitive volume found");
        }
    }

    // Volume selection as TRestGeant4QuenchingProcess: the names matching each expression, compared
    // with the physical and filtered names of the hit volumes (resolved here once per volume ID)
    for (const auto& expression : fVolumes) {
        const auto ids = geometryInfo.GetVolumeIDsMatchingExpression(expression);
        if (ids.empty()) {
            RESTWarning << "TRestGeant4ToDetectorHitsProcess: no volume matches '" << expression << "'"
                        << RESTendl;
        }
        fVolumeIDs.insert(ids.begin(), ids.end());
    }
    for (const auto id : fVolumeIDs) {
        RESTDebug << "TRestGeant4ToDetectorHitsProcess: volume " << geometryInfo.GetVolumeFromID(id)
                  << " (id " << id << ") selected" << RESTendl;
    }

    if (fVolumeIDs.empty()) {
        throw std::runtime_error("TRestGeant4ToDetectorHitsProcess: no volume matches 'volumes'");
    }
}

bool TRestGeant4ToDetectorHitsProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* g4Event = dynamic_cast<const TRestGeant4Event*>(&input);
    auto* hitsEvent = dynamic_cast<TRestDetectorHitsEvent*>(&output);
    if (!g4Event || !hitsEvent) {
        throw std::runtime_error(
            "TRestGeant4ToDetectorHitsProcess: input/output is not a TRestGeant4Event/"
            "TRestDetectorHitsEvent");
    }

    hitsEvent->Initialize();
    hitsEvent->SetEventInfo(&input);

    const auto& data = g4Event->fEventData;
    const auto& hits = data.hitsStorage;
    for (size_t i = 0; i < hits.energy.size(); i++) {
        if (hits.energy[i] <= 0) continue;
        if (fVolumeIDs.count(data.hitVolumeID.at(i)) == 0) continue;
        if (hits.time[i] < 0) {
            throw std::runtime_error("TRestGeant4ToDetectorHitsProcess: hit with negative time");
        }
        hitsEvent->AddHit(hits.x[i], hits.y[i], hits.z[i], hits.energy[i], hits.time[i]);
    }

    RESTDebug << "TRestGeant4ToDetectorHitsProcess: " << hitsEvent->GetNumberOfHits() << " hits added"
              << RESTendl;

    return hitsEvent->GetNumberOfHits() > 0;
}
