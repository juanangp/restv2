/// \class TRestGeant4AnalysisProcess
/// Observables of a TRestGeant4Event. Energies in keV, positions in mm and angles in rad; times as the hits.
///
/// Global and primary observables: totalEdep, sensitiveVolumeEnergy, nTracks, boundingSize (diagonal of
/// the box containing the hits with energy), energyPrimary, x/y/zOriginPrimary, x/y/zDirectionPrimary,
/// thetaPrimary, phiPrimary, eventPrimaryParticleName and subEventPrimaryParticleName ("generator" for
/// the main event) (strings).
///
/// Selections: each entry of `selections` selects the hits with energy of the given `volumes` (regular
/// expressions, as in TRestGeant4QuenchingProcess), `particles` and `processes` (Geant4 names, e.g.
/// compt, phot, eIoni, hadElastic, nCapture); an empty or missing list of particles or processes selects
/// any, e.g. only `volumes` for all the particles and processes in those volumes. Without `volumes`, the
/// sensitive volumes of the simulation are selected (`volumes: [".*"]` or `["^."]` for all of them).
/// Without `selections`, there is one for each sensitive entry of `activeVolumes` in the simulation, of
/// its volumes, named as its `name` (characters not allowed as `_`, without the leading and trailing
/// ones) and with `entering: true`, e.g. gas_Energy, gas_EntryX... for `name: "gas"`, and veto_* for a
/// second one `name: "veto"`. All the observables are one value per event; each selection gives:
///
/// * <name>_Energy, <name>_MeanPosX/Y/Z (energy weighted), <name>_FirstHitTime and <name>_NTracks
/// (tracks with energy in the selection). Without energy, MeanPos and FirstHitTime are NaN.
/// * <name>_<particle>_Energy for each particle of `splitByParticle`: the energy of the selection given by
/// that particle. In the observable name, `-` and `+` become `Minus` and `Plus` and any other character
/// not allowed `_` (e.g. gas_eMinus_Energy).
/// * With `entering: true`, the particle entering the volumes of the selection: for each track with
/// energy in the selection, the first one of the track and its ancestors whose first hit is out of the
/// volumes, i.e. the particle which entered them giving the energy (e.g. the alpha or the
/// neutron entering the gas). Observables: <name>_NEntering (number of different ones, 1 for a single
/// entering particle) and, of the one with the lowest ID:
///   - <name>_EntryParticle, _EntryParentParticle (empty for a primary) and _EntryCreatorProcess
///     (strings);
///   - <name>_EntryEnergy, _EntryTime and _EntryX/Y/Z: kinetic energy, time and position of its first hit
///     in the volumes;
///   - <name>_EndX/Y/Z: position of its last hit in the volumes (e.g. where the alpha stops in the gas,
///     or where a gamma leaves it), to compare with the reconstructed tracks;
///   - <name>_InitialEnergy and _InitialX/Y/Z: kinetic energy and position where it was created.
///   If the energy is given only by particles created in the volumes (e.g. a source inside the gas),
///   <name>_NEntering is 0, the strings are empty and the numbers NaN. Its cost is a walk over the
///   ancestors of the tracks with energy in the selection.
///
/// At initialization an error is thrown for a volume list without any volume in the geometry, for a
/// particle or process not in the physics information of the simulation metadata, for a splitByParticle
/// particle excluded by `particles` and for a selection without `volumes` when there is no sensitive
/// volume; for each event, a selection without hits is just 0 (empty).
///
/// Difference with legacy: the observables are not defined by the observable name (<volume>VolumeEDep,
/// <volume>MeanPos<X|Y|Z>, <volume><Process>Process, <particle>TracksEDep, PerProcess<Name>,
/// containsProcess<Process>, <particle>TracksCounter) but by the selections, e.g. containsProcessCompt is
/// compt_Energy > 0, PerProcess<Name> normalized is <name>_Energy / sensitiveVolumeEnergy and
/// <particle>TracksCounter is the <name>_NTracks of a selection by particle (only the tracks with
/// energy). firstTrackInSensitive* are the <name>_Entry* observables of a selection of the sensitive
/// volumes with `entering: true` (firstTrackInSensitiveOk is <name>_NEntering == 1), with the entry and
/// end points. The number of hits is not an observable, it depends on the Geant4 step length. As legacy,
/// processes sharing Geant4 type and subtype (e.g. eIoni and ionIoni) share the ID, are registered once
/// and are not distinguished. zenithYDegrees and zenithSourceDegrees are removed (they follow from the
/// primary direction).
///
/// Example:
///
///    g4Ana:
///      class: "TRestGeant4AnalysisProcess"
///      observables: ["all"]
///      selections:
///        - name: "gas"
///          volumes: ["^gasVolume"]
///          splitByParticle: ["alpha", "e-", "proton"]
///          entering: true
///        - name: "gasCompton"
///          volumes: ["^gasVolume"]
///          processes: ["compt"]
///        - name: "scint"
///          volumes: ["^scintillatorVolume"]

#include "TRestGeant4AnalysisProcess.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

#include "TRestGeant4Event.h"
#include "TRestGeant4Metadata.h"
#include "TRestRun.h"

static const bool TRestGeant4SelectionInfo_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using S = TRestGeant4SelectionInfo;
    reg.RegisterField<S>("name", &S::fSelectionName);
    reg.RegisterField<S>("volumes", &S::fVolumes);
    reg.RegisterField<S>("particles", &S::fParticles);
    reg.RegisterField<S>("processes", &S::fProcesses);
    reg.RegisterField<S>("splitByParticle", &S::fSplitByParticle);
    reg.RegisterField<S>("entering", &S::fEntering);
    return true;
}();

static const bool TRestGeant4AnalysisProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestGeant4AnalysisProcess;
    reg.RegisterField<P>("selections", &P::fSelections);
    return true;
}();

namespace {
/// Particle name as part of an observable name: - and + as Minus and Plus, other characters not allowed as _
std::string ParticleObservableName(const std::string& particle) {
    std::string name;
    for (const char c : particle) {
        if (c == '-') {
            name += "Minus";
        } else if (c == '+') {
            name += "Plus";
        } else {
            name += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
        }
    }
    return name;
}

/// Volume name (expression) as an observable name: the characters not allowed as _, without the leading
/// and trailing ones (e.g. ^scintillatorVolume as scintillatorVolume)
std::string VolumeObservableName(const std::string& volume) {
    std::string name;
    for (const char c : volume) name += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
    const auto first = name.find_first_not_of('_');
    if (first == std::string::npos) return "sensitive";
    return name.substr(first, name.find_last_not_of('_') - first + 1);
}

const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestGeant4AnalysisProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestGeant4AnalysisProcess>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestGeant4SelectionInfo::TRestGeant4SelectionInfo() : TRestMetadata() { fName = "TRestGeant4SelectionInfo"; }

TRestGeant4SelectionInfo::TRestGeant4SelectionInfo(const std::string& name, const YAML::Node& node)
    : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestGeant4SelectionInfo::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4SelectionInfo>(fNode);
    UpdateYAMLFromParams<TRestGeant4SelectionInfo>(fNode);
}

void TRestGeant4AnalysisProcess::EnteringObservables::clear() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    nEntering = 0;
    particle.clear();
    parentParticle.clear();
    creatorProcess.clear();
    entryEnergy = entryTime = entryX = entryY = entryZ = nan;
    endX = endY = endZ = nan;
    initialEnergy = initialX = initialY = initialZ = nan;
}

TRestGeant4AnalysisProcess::TRestGeant4AnalysisProcess() : TRestEventProcess() {
    fName = "TRestGeant4AnalysisProcess";
}

TRestGeant4AnalysisProcess::TRestGeant4AnalysisProcess(const std::string& instanceName,
                                                       const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestGeant4AnalysisProcess::TRestGeant4AnalysisProcess(const std::string& fileName,
                                                       const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestGeant4AnalysisProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestGeant4AnalysisProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestGeant4AnalysisProcess>(fNode);
    UpdateYAMLFromParams<TRestGeant4AnalysisProcess>(fNode);
}

void TRestGeant4AnalysisProcess::InitProcess() {
    const auto* metadata =
        fRunInfo ? dynamic_cast<TRestGeant4Metadata*>(fRunInfo->GetMetadataClass("TRestGeant4Metadata"))
                 : nullptr;
    if (!metadata) throw std::runtime_error("TRestGeant4AnalysisProcess: no TRestGeant4Metadata found");
    fGeometryInfo = &metadata->GetGeant4GeometryInfo();
    const auto& physicsInfo = metadata->GetGeant4PhysicsInfo();
    const auto knownParticles = physicsInfo.GetAllParticles();
    const auto knownProcesses = physicsInfo.GetAllProcesses();

    // Volume IDs of a list of expressions, an error if none of them matches
    auto resolveVolumes = [this](const std::vector<std::string>& expressions, const std::string& what) {
        std::set<int> ids;
        for (const auto& expression : expressions) {
            const auto matched = fGeometryInfo->GetVolumeIDsMatchingExpression(expression);
            if (matched.empty()) {
                RESTWarning << "TRestGeant4AnalysisProcess: " << what << ", no volume matches '"
                            << expression << "'" << RESTendl;
            }
            ids.insert(matched.begin(), matched.end());
        }
        if (!expressions.empty() && ids.empty()) {
            throw std::runtime_error("TRestGeant4AnalysisProcess: no volume of " + what +
                                     " found in the geometry");
        }
        return ids;
    };
    // Volume IDs of the sensitive volumes, the default of a selection without volumes
    auto sensitiveIDs = [this, metadata](const std::string& what) {
        const auto& sensitive = metadata->GetSensitiveVolumes();
        const auto ids = fGeometryInfo->GetVolumeIDsFromNames({sensitive.begin(), sensitive.end()});
        if (ids.empty()) {
            throw std::runtime_error("TRestGeant4AnalysisProcess: " + what +
                                     " without 'volumes' and no sensitive volume found");
        }
        return ids;
    };
    // Names checked against the physics information, an error if one is not there
    auto checkNames = [](const std::vector<std::string>& values, const std::set<std::string>& known,
                         const std::string& what) {
        for (const auto& value : values) {
            if (!known.count(value)) {
                throw std::runtime_error("TRestGeant4AnalysisProcess: " + what + " '" + value +
                                         "' not in the physics information of the simulation metadata");
            }
        }
        return std::set<std::string>(values.begin(), values.end());
    };
    auto checkName = [](const std::string& name, std::set<std::string>& names) {
        if (name.empty()) throw std::runtime_error("TRestGeant4AnalysisProcess: selection without name");
        if (!names.insert(name).second) {
            throw std::runtime_error("TRestGeant4AnalysisProcess: duplicated selection '" + name + "'");
        }
    };

    // The selections are resolved before registering, so the vector does not change afterwards and the
    // registered references stay valid
    // Without selections, one per sensitive volume of activeVolumes, named as it, with the entering particle
    auto selections = fSelections;
    if (selections.empty()) {
        for (const auto& expression : metadata->GetSensitiveVolumeExpressions()) {
            TRestGeant4SelectionInfo sensitive;
            sensitive.fSelectionName = VolumeObservableName(expression);
            sensitive.fVolumes = {expression};
            sensitive.fEntering = true;
            selections.push_back(sensitive);
        }
    }

    fSelectionObs.clear();
    std::set<std::string> names;
    for (const auto& info : selections) {
        checkName(info.fSelectionName, names);
        const auto what = "selection '" + info.fSelectionName + "'";
        Selection selection;
        selection.name = info.fSelectionName;
        selection.volumeIDs =
            info.fVolumes.empty() ? sensitiveIDs(what) : resolveVolumes(info.fVolumes, what);
        selection.particles = checkNames(info.fParticles, knownParticles, what + ", particle");
        // Processes sharing type and subtype (e.g. eIoni and ionIoni) are registered once
        for (const auto& process : checkNames(info.fProcesses, knownProcesses, what + ", process")) {
            selection.processIDs.insert(physicsInfo.GetProcessID(process));
        }
        for (const auto& particle : checkNames(info.fSplitByParticle, knownParticles, what + ", particle")) {
            if (!selection.particles.empty() && !selection.particles.count(particle)) {
                throw std::runtime_error("TRestGeant4AnalysisProcess: " + what + ", splitByParticle '" +
                                         particle + "' not in its particles");
            }
            selection.splitParticles.push_back(particle);
        }
        selection.splitEnergy.assign(selection.splitParticles.size(), 0);
        selection.entering = info.fEntering;
        fSelectionObs.push_back(selection);
    }

    RegisterObservable("totalEdep", fObs.totalEdep);
    RegisterObservable("sensitiveVolumeEnergy", fObs.sensitiveVolumeEnergy);
    RegisterObservable("nTracks", fObs.nTracks);
    RegisterObservable("boundingSize", fObs.boundingSize);
    RegisterObservable("energyPrimary", fObs.energyPrimary);
    RegisterObservable("xOriginPrimary", fObs.xOriginPrimary);
    RegisterObservable("yOriginPrimary", fObs.yOriginPrimary);
    RegisterObservable("zOriginPrimary", fObs.zOriginPrimary);
    RegisterObservable("xDirectionPrimary", fObs.xDirectionPrimary);
    RegisterObservable("yDirectionPrimary", fObs.yDirectionPrimary);
    RegisterObservable("zDirectionPrimary", fObs.zDirectionPrimary);
    RegisterObservable("thetaPrimary", fObs.thetaPrimary);
    RegisterObservable("phiPrimary", fObs.phiPrimary);
    RegisterObservable("eventPrimaryParticleName", fObs.eventPrimaryParticleName);
    RegisterObservable("subEventPrimaryParticleName", fObs.subEventPrimaryParticleName);

    for (auto& selection : fSelectionObs) {
        const auto prefix = selection.name + "_";
        RegisterObservable(prefix + "Energy", selection.energy);
        RegisterObservable(prefix + "MeanPosX", selection.meanPosX);
        RegisterObservable(prefix + "MeanPosY", selection.meanPosY);
        RegisterObservable(prefix + "MeanPosZ", selection.meanPosZ);
        RegisterObservable(prefix + "FirstHitTime", selection.firstHitTime);
        RegisterObservable(prefix + "NTracks", selection.nTracks);
        for (size_t p = 0; p < selection.splitParticles.size(); p++) {
            RegisterObservable(prefix + ParticleObservableName(selection.splitParticles[p]) + "_Energy",
                               selection.splitEnergy[p]);
        }
        if (!selection.entering) continue;
        auto& entering = selection.enteringObs;
        RegisterObservable(prefix + "NEntering", entering.nEntering);
        RegisterObservable(prefix + "EntryParticle", entering.particle);
        RegisterObservable(prefix + "EntryParentParticle", entering.parentParticle);
        RegisterObservable(prefix + "EntryCreatorProcess", entering.creatorProcess);
        RegisterObservable(prefix + "EntryEnergy", entering.entryEnergy);
        RegisterObservable(prefix + "EntryTime", entering.entryTime);
        RegisterObservable(prefix + "EntryX", entering.entryX);
        RegisterObservable(prefix + "EntryY", entering.entryY);
        RegisterObservable(prefix + "EntryZ", entering.entryZ);
        RegisterObservable(prefix + "EndX", entering.endX);
        RegisterObservable(prefix + "EndY", entering.endY);
        RegisterObservable(prefix + "EndZ", entering.endZ);
        RegisterObservable(prefix + "InitialEnergy", entering.initialEnergy);
        RegisterObservable(prefix + "InitialX", entering.initialX);
        RegisterObservable(prefix + "InitialY", entering.initialY);
        RegisterObservable(prefix + "InitialZ", entering.initialZ);
    }
}

bool TRestGeant4AnalysisProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    output.CopyFrom(&input);
    const auto* event = dynamic_cast<const TRestGeant4Event*>(&output);
    if (!event) throw std::runtime_error("TRestGeant4AnalysisProcess: output is not a TRestGeant4Event");

    const auto& data = event->fEventData;
    const double nan = std::numeric_limits<double>::quiet_NaN();

    // Assignments keep the addresses of the registered members
    fObs.totalEdep = event->GetTotalDepositedEnergy();
    fObs.sensitiveVolumeEnergy = event->GetSensitiveVolumeEnergy();
    fObs.nTracks = static_cast<int>(event->GetNumberOfTracks());

    fObs.energyPrimary = data.primaryEnergies.empty() ? nan : data.primaryEnergies[0];
    fObs.xOriginPrimary = data.primaryPosition.X();
    fObs.yOriginPrimary = data.primaryPosition.Y();
    fObs.zOriginPrimary = data.primaryPosition.Z();
    const ROOT::Math::XYZVector direction = data.primaryDirections.empty()
                                                ? ROOT::Math::XYZVector(nan, nan, nan)
                                                : data.primaryDirections[0].Unit();
    fObs.xDirectionPrimary = direction.X();
    fObs.yDirectionPrimary = direction.Y();
    fObs.zDirectionPrimary = direction.Z();
    fObs.thetaPrimary = direction.Theta();
    fObs.phiPrimary = direction.Phi();
    fObs.eventPrimaryParticleName = data.primaryParticleNames.empty() ? "" : data.primaryParticleNames[0];
    fObs.subEventPrimaryParticleName = event->IsSubEvent() ? data.subEventParticleName : "generator";

    ProcessSelections(*event);
    for (auto& selection : fSelectionObs) {
        if (selection.entering) ProcessEntering(*event, selection);
    }

    return true;
}

/// \brief Bounding size and selection observables, in one loop over the hits.
void TRestGeant4AnalysisProcess::ProcessSelections(const TRestGeant4Event& event) {
    const auto& data = event.fEventData;
    const auto& hits = data.hitsStorage;
    const double nan = std::numeric_limits<double>::quiet_NaN();

    for (auto& selection : fSelectionObs) {
        selection.energy = 0;
        selection.meanPosX = selection.meanPosY = selection.meanPosZ = 0;
        selection.firstHitTime = std::numeric_limits<double>::infinity();
        selection.nTracks = 0;
        std::fill(selection.splitEnergy.begin(), selection.splitEnergy.end(), 0);
        selection.trackIndices.clear();
    }

    double min[3] = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
                     std::numeric_limits<double>::max()};
    double max[3] = {std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest(),
                     std::numeric_limits<double>::lowest()};
    bool anyHit = false;

    for (size_t t = 0; t < data.trackIDs.size(); t++) {
        const auto& particle = data.trackParticleNames[t];
        const size_t start = data.trackStartIndices[t];
        const size_t end = start + data.trackNHits[t];

        for (size_t h = start; h < end; h++) {
            if (hits.energy[h] <= 0) continue;
            const double position[3] = {hits.x[h], hits.y[h], hits.z[h]};
            for (int a = 0; a < 3; a++) {
                min[a] = std::min(min[a], position[a]);
                max[a] = std::max(max[a], position[a]);
            }
            anyHit = true;
        }

        for (auto& selection : fSelectionObs) {
            if (!selection.particles.empty() && !selection.particles.count(particle)) continue;
            double trackEnergy = 0;
            bool any = false;
            for (size_t h = start; h < end; h++) {
                const double energy = hits.energy[h];
                if (energy <= 0) continue;
                if (!selection.volumeIDs.count(data.hitVolumeID[h])) continue;
                if (!selection.processIDs.empty() && !selection.processIDs.count(data.hitProcessID[h]))
                    continue;
                trackEnergy += energy;
                selection.meanPosX += energy * hits.x[h];
                selection.meanPosY += energy * hits.y[h];
                selection.meanPosZ += energy * hits.z[h];
                selection.firstHitTime = std::min(selection.firstHitTime, static_cast<double>(hits.time[h]));
                any = true;
            }
            if (!any) continue;

            selection.energy += trackEnergy;
            selection.nTracks++;
            for (size_t p = 0; p < selection.splitParticles.size(); p++) {
                if (selection.splitParticles[p] == particle) selection.splitEnergy[p] += trackEnergy;
            }
            selection.trackIndices.push_back(t);
        }
    }

    fObs.boundingSize =
        anyHit ? std::sqrt((max[0] - min[0]) * (max[0] - min[0]) + (max[1] - min[1]) * (max[1] - min[1]) +
                           (max[2] - min[2]) * (max[2] - min[2]))
               : 0;

    for (auto& selection : fSelectionObs) {
        if (selection.energy > 0) {
            selection.meanPosX /= selection.energy;
            selection.meanPosY /= selection.energy;
            selection.meanPosZ /= selection.energy;
        } else {
            selection.meanPosX = selection.meanPosY = selection.meanPosZ = nan;
            selection.firstHitTime = nan;
        }
        RESTDebug << "TRestGeant4AnalysisProcess: " << selection.name << " energy " << selection.energy
                  << ", tracks " << selection.nTracks << RESTendl;
    }
}

/// \brief Observables of the particle entering the volumes of a selection.
void TRestGeant4AnalysisProcess::ProcessEntering(const TRestGeant4Event& event, Selection& selection) {
    const auto& data = event.fEventData;
    const auto& hits = data.hitsStorage;

    auto& entering = selection.enteringObs;
    entering.clear();

    auto inVolumes = [&selection, &data](size_t h) {
        return selection.volumeIDs.count(data.hitVolumeID[h]) > 0;
    };
    const auto& indexOfID = event.GetTrackIDToTrackIndex();

    // Indices of the entering tracks, by track ID
    std::map<int, size_t> enteringTracks;
    for (const size_t t : selection.trackIndices) {
        // The track itself or the first ancestor whose first hit is out of the volumes
        size_t current = t;
        while (true) {
            if (data.trackNHits[current] > 0 && !inVolumes(data.trackStartIndices[current])) {
                enteringTracks.emplace(data.trackIDs[current], current);
                break;
            }
            const auto parent = indexOfID.find(data.parentIDs[current]);
            if (data.parentIDs[current] == 0 || parent == indexOfID.end()) break;
            current = static_cast<size_t>(parent->second);
        }
    }

    entering.nEntering = static_cast<int>(enteringTracks.size());
    if (enteringTracks.empty()) return;

    const size_t t = enteringTracks.begin()->second;
    entering.particle = data.trackParticleNames[t];
    const auto parent = indexOfID.find(data.parentIDs[t]);
    if (data.parentIDs[t] != 0 && parent != indexOfID.end()) {
        entering.parentParticle = data.trackParticleNames[parent->second];
    }
    entering.creatorProcess = data.trackCreatorProcesses[t];
    entering.initialEnergy = data.trackInitialEnergies[t];
    entering.initialX = data.trackInitialPositions[t].X();
    entering.initialY = data.trackInitialPositions[t].Y();
    entering.initialZ = data.trackInitialPositions[t].Z();

    // First and last hits in the volumes, the track entered them so there is at least one
    const size_t start = data.trackStartIndices[t];
    bool entered = false;
    for (size_t h = start; h < start + data.trackNHits[t]; h++) {
        if (!inVolumes(h)) continue;
        if (!entered) {
            entering.entryEnergy = data.hitKineticEnergy[h];
            entering.entryTime = hits.time[h];
            entering.entryX = hits.x[h];
            entering.entryY = hits.y[h];
            entering.entryZ = hits.z[h];
            entered = true;
        }
        entering.endX = hits.x[h];
        entering.endY = hits.y[h];
        entering.endZ = hits.z[h];
    }

    RESTDebug << "TRestGeant4AnalysisProcess: " << selection.name << ", " << entering.nEntering
              << " entering tracks, first " << entering.particle << RESTendl;
}
