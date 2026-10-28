/// \class TRestTrackLineAnalysisProcess
/// Analyses linear tracks, such as alpha tracks, for any readout: origin, end, length and direction of
/// the main track. The hits of the tracks must be in path order, e.g. after
/// TRestTrackLinearizationProcess.
///
/// Main track:
/// * Without `projections`: the most energetic track, whatever its type (e.g. XYZ for a 3D readout).
/// * With `projections` (hit types, e.g. [XZ, YZ] for strips): the most energetic track of each
///   projection, together. Events with no track in any of them are rejected (not enough information).
/// Events with no tracks are rejected. The output event holds the tracks of the main track (parent ID =
/// ID of the input track). The macro ViewTrackLine (libraries/track/macros) browses the events with the
/// origin and end over the tracks and hits, and the signals split in X and Y.
///
/// Origin and end of each track: the end is the extreme on the half of the track with more energy (the
/// Bragg peak), TRestHitsAnalysis::GetTrackEnds, which also gives the energy asymmetry of the halves (how
/// clear the direction is). With several projections, the tracks are first oriented coherently: the
/// direction of the track with the clearest Bragg peak (largest asymmetry x energy) is kept, and every
/// other track sharing coordinates with it (Z for XZ and YZ) is reversed if it goes the opposite way
/// along them. Then, depending on lineAnaMethod:
/// * "default": each coordinate of the origin (end) is the mean of the origins (ends) of the tracks that
///   define it (for XZ and YZ: X from XZ, Y from YZ and Z the mean of both).
/// * "3D": the tracks are paired along their common coordinate (Z for XZ and YZ; the one with the
///   largest extent if there are several): the common coordinate of the origin (end) is the mean over the
///   tracks, and each other coordinate is interpolated in the tracks that define it at that value
///   (nodes sorted along the common coordinate, the extreme node beyond them). Unlike pairing the nodes
///   one to one (legacy), it does not need the same number of nodes in each projection. Without a common
///   coordinate, "default" is used.
/// Coordinates no track of the main track defines are NaN, also in the observables derived from them.
///
/// ### Parameters
/// * projections: hit types of the main track (X, Y, Z, XY, XZ, YZ, XYZ), empty by default.
/// * lineAnaMethod: "default" or "3D" (only with several projections).
///
/// ### Observables
/// Branches `processName_observable` of the analysis tree (e.g. trackLineAnalysis_OriginX):
/// * OriginX/Y/Z, EndX/Y/Z: origin and end of the track.
/// * Length: distance from origin to end (in the defined coordinates).
/// * Angle: polar angle (radians, from the Z axis) of origin - end, NaN without Z or with length 0.
/// * Phi: azimuthal angle (radians, in the XY plane from X) of origin - end, NaN without X and Y or
///   with no XY length.
/// * Downwards: 1 if the origin is above the end (origin Z > end Z), -1 below, 0 if unknown (no Z,
///   or same Z).
/// * Energy: energy of the main track.
/// * Asymmetry: energy asymmetry of the halves of the tracks of the main track (energy weighted mean),
///   from 0 (no preferred direction) to 1: how reliable the origin and end are.
/// * Residual: energy weighted RMS distance of the hits to the line fitted to them
///   (TRestHitsAnalysis::GetLineResidual), energy weighted mean over the tracks of the main track: 0 for a
///   straight track, larger for curved or scattered ones.
/// * RelativeZ: position of the half of the energy along Z, relative to the Z extent of the main track
///   and from its minimum Z: below 0.5 the track is downwards, upwards otherwise. NaN without Z.
/// * TrackBalance: energy fraction of the main track, among the tracks of the projections (all the
///   tracks without projections).
/// * With projections, for each projection P: TrackBalance_P (the same in P), Length_P and
///   Angle_P (of the main track of P, from its own origin and end; the angle NaN without Z),
///   Asymmetry_P and Residual_P.
///
/// Example (strip readout):
///
///    trackLineAnalysis:
///      class: "TRestTrackLineAnalysisProcess"
///      projections: ["XZ", "YZ"]
///      lineAnaMethod: "default"
///      observables:
///        - "all"
///
/// Migrated from the legacy track library (JuanAn Garcia), redesigned to be readout independent (legacy
/// only for XZ and YZ tracks). Legacy observables with projections [XZ, YZ]: trackBalanceX/Y =
/// TrackBalance_XZ/YZ, trackBalance = TrackBalance, originX = OriginX, ..., length =
/// Length, angle = Angle (NaN instead of -10 for length 0), downwards = Downwards (> 0),
/// totalEnergy = Energy, relativeZ = RelativeZ. Differences: the end from the energy of the
/// halves instead of the most energetic node; the projections oriented coherently; "3D" pairs along the
/// common coordinate instead of node by node; Phi, Asymmetry, Residual and the per
/// projection length, angle, asymmetry and residual are new.

#include "TRestTrackLineAnalysisProcess.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

#include "TRestHitsAnalysis.h"
#include "TRestRun.h"
#include "TRestTrack.h"
#include "TRestTrackEvent.h"

static const bool TRestTrackLineAnalysisProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestTrackLineAnalysisProcess;
    reg.RegisterField<P>("projections", &P::fProjections);
    reg.RegisterField<P>("lineAnaMethod", &P::fLineAnaMethod);
    return true;
}();

namespace {
using TRestHitsAnalysis::XYZVector;

const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestTrackLineAnalysisProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestTrackLineAnalysisProcess>(instanceName, params);
        });
    return true;
}();

const double kNaN = std::numeric_limits<double>::quiet_NaN();

double Get(const XYZVector& v, int axis) { return axis == 0 ? v.X() : axis == 1 ? v.Y() : v.Z(); }

XYZVector Make(const double* c) { return XYZVector(c[0], c[1], c[2]); }

/// Origin, end and analysis of one track of the main track
struct TrackLine {
    int type = 0;
    double energy = 0;
    XYZVector origin, end;
    double asymmetry = 0;
    double residual = 0;
};

TrackLine AnalyseTrack(const TRestHits& hits) {
    TrackLine line;
    line.type = hits.GetType(0);
    line.energy = hits.GetTotalEnergy();
    const auto points = hits.GetPoints();
    const auto energies = hits.GetEnergies();
    line.asymmetry = TRestHitsAnalysis::GetTrackEnds(points, energies, line.origin, line.end);
    line.residual = std::max(TRestHitsAnalysis::GetLineResidual(points, energies), 0.);
    return line;
}

// Reverses the tracks going along the coordinates they share with the reference track (clearest Bragg
// peak: largest asymmetry x energy) in the opposite way
void OrientTracks(std::vector<TrackLine>& lines) {
    size_t reference = 0;
    for (size_t t = 1; t < lines.size(); t++) {
        if (lines[t].asymmetry * lines[t].energy > lines[reference].asymmetry * lines[reference].energy) {
            reference = t;
        }
    }
    const XYZVector referenceDirection = lines[reference].end - lines[reference].origin;
    for (size_t t = 0; t < lines.size(); t++) {
        if (t == reference) continue;
        const XYZVector direction = lines[t].end - lines[t].origin;
        double dot = 0;
        for (int a = 0; a < 3; a++) {
            if (TRestHitsData::IsDefined(lines[t].type, a) && TRestHitsData::IsDefined(lines[reference].type, a)) {
                dot += Get(direction, a) * Get(referenceDirection, a);
            }
        }
        if (dot < 0) std::swap(lines[t].origin, lines[t].end);
    }
}

// Each coordinate the mean over the tracks that define it (0 if none)
void GetCombinedEnds(const std::vector<TrackLine>& lines, XYZVector& origin, XYZVector& end) {
    double originSum[3] = {}, endSum[3] = {};
    int count[3] = {};
    for (const auto& line : lines) {
        for (int a = 0; a < 3; a++) {
            if (!TRestHitsData::IsDefined(line.type, a)) continue;
            originSum[a] += Get(line.origin, a);
            endSum[a] += Get(line.end, a);
            count[a]++;
        }
    }
    for (int a = 0; a < 3; a++) {
        if (count[a] == 0) continue;
        originSum[a] /= count[a];
        endSum[a] /= count[a];
    }
    origin = Make(originSum);
    end = Make(endSum);
}

// Coordinate `axis` of the hits at `value` of the coordinate `common`, linearly interpolated between the
// hits sorted along `common` (the extreme hit beyond them)
double Interpolate(const TRestHits& hits, int common, int axis, double value) {
    std::vector<std::pair<double, double>> nodes;
    for (size_t i = 0; i < hits.GetNumberOfHits(); i++) {
        nodes.emplace_back(hits.GetCoordinate(i, common), hits.GetCoordinate(i, axis));
    }
    std::sort(nodes.begin(), nodes.end());
    if (value <= nodes.front().first) return nodes.front().second;
    if (value >= nodes.back().first) return nodes.back().second;
    const auto upper = std::upper_bound(nodes.begin(), nodes.end(), std::make_pair(value, kNaN),
                                        [](const auto& a, const auto& b) { return a.first < b.first; });
    const auto lower = upper - 1;
    const double width = upper->first - lower->first;
    if (width <= 0) return lower->second;
    return lower->second + (value - lower->first) / width * (upper->second - lower->second);
}

// The tracks paired along their common coordinate (the one with the largest extent of the oriented ends).
// False if they share no coordinate
bool GetPairedEnds(const std::vector<TRestHits>& hits, const std::vector<TrackLine>& lines, XYZVector& origin,
                   XYZVector& end) {
    int common = -1;
    double maxExtent = -1;
    for (int a = 0; a < 3; a++) {
        bool shared = true;
        double extent = 0;
        for (const auto& line : lines) {
            shared &= TRestHitsData::IsDefined(line.type, a);
            extent += std::abs(Get(line.end, a) - Get(line.origin, a));
        }
        if (shared && extent > maxExtent) {
            maxExtent = extent;
            common = a;
        }
    }
    if (common < 0) return false;

    double commonOrigin = 0, commonEnd = 0;
    for (const auto& line : lines) {
        commonOrigin += Get(line.origin, common);
        commonEnd += Get(line.end, common);
    }
    commonOrigin /= lines.size();
    commonEnd /= lines.size();

    double o[3] = {}, e[3] = {};
    for (int a = 0; a < 3; a++) {
        if (a == common) {
            o[a] = commonOrigin;
            e[a] = commonEnd;
            continue;
        }
        int count = 0;
        for (size_t t = 0; t < lines.size(); t++) {
            if (!TRestHitsData::IsDefined(lines[t].type, a)) continue;
            o[a] += Interpolate(hits[t], common, a, commonOrigin);
            e[a] += Interpolate(hits[t], common, a, commonEnd);
            count++;
        }
        if (count > 0) {
            o[a] /= count;
            e[a] /= count;
        }
    }
    origin = Make(o);
    end = Make(e);
    return true;
}

// Polar angle (from Z) of a direction, NaN without Z or with length 0
double GetPolarAngle(const XYZVector& direction, bool hasZ) {
    const double length = direction.R();
    return hasZ && length > 0 ? std::acos(direction.Z() / length) : kNaN;
}

// Position of the half of the energy along Z, relative to the Z extent and from the minimum Z, as legacy
// TRestTrackEvent::GetMaxTrackRelativeZ. Only the hits that define Z
double GetRelativeZ(const std::vector<TRestHits>& hits) {
    std::vector<std::pair<double, double>> zEnergy;
    double totalEnergy = 0;
    for (const auto& h : hits) {
        for (size_t i = 0; i < h.GetNumberOfHits(); i++) {
            if (!TRestHitsData::IsDefined(h.GetType(i), 2)) continue;
            zEnergy.emplace_back(h.GetZ(i), h.GetEnergy(i));
            totalEnergy += h.GetEnergy(i);
        }
    }
    if (zEnergy.empty()) return kNaN;
    std::sort(zEnergy.begin(), zEnergy.end());

    double integral = 0;
    size_t half = 0;
    for (; half + 1 < zEnergy.size(); half++) {
        integral += zEnergy[half].second;
        if (integral >= totalEnergy / 2.) break;
    }
    const double length = zEnergy.front().first - zEnergy.back().first;
    return length == 0 ? 0 : (zEnergy.front().first - zEnergy[half].first) / length;
}
}  // namespace

TRestTrackLineAnalysisProcess::TRestTrackLineAnalysisProcess() : TRestEventProcess() {
    fName = "TRestTrackLineAnalysisProcess";
}

TRestTrackLineAnalysisProcess::TRestTrackLineAnalysisProcess(const std::string& instanceName,
                                                             const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestTrackLineAnalysisProcess::TRestTrackLineAnalysisProcess(const std::string& fileName,
                                                             const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestTrackLineAnalysisProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestTrackLineAnalysisProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestTrackLineAnalysisProcess>(fNode);
    UpdateYAMLFromParams<TRestTrackLineAnalysisProcess>(fNode);
}

void TRestTrackLineAnalysisProcess::InitProcess() {
    fMethodWarned = false;

    // The vector does not change after this, so the registered references stay valid
    const std::map<std::string, int> hitTypes = {{"X", TRestHitsData::X},   {"Y", TRestHitsData::Y},
                                                 {"Z", TRestHitsData::Z},   {"XY", TRestHitsData::XY},
                                                 {"XZ", TRestHitsData::XZ}, {"YZ", TRestHitsData::YZ},
                                                 {"XYZ", TRestHitsData::XYZ}};
    fProjectionTracks.clear();
    for (const auto& name : fProjections) {
        auto it = hitTypes.find(name);
        if (it == hitTypes.end()) {
            RESTError << "TRestTrackLineAnalysisProcess: unknown projection '" << name
                      << "' (X, Y, Z, XY, XZ, YZ or XYZ)" << RESTendl;
            continue;
        }
        ProjectionTracks projection;
        projection.name = name;
        projection.type = it->second;
        fProjectionTracks.push_back(projection);
    }

    RegisterObservable("OriginX", fOriginX);
    RegisterObservable("OriginY", fOriginY);
    RegisterObservable("OriginZ", fOriginZ);
    RegisterObservable("EndX", fEndX);
    RegisterObservable("EndY", fEndY);
    RegisterObservable("EndZ", fEndZ);
    RegisterObservable("Length", fLength);
    RegisterObservable("Angle", fAngle);
    RegisterObservable("Phi", fPhi);
    RegisterObservable("Downwards", fDownwards);
    RegisterObservable("Energy", fEnergy);
    RegisterObservable("Asymmetry", fAsymmetry);
    RegisterObservable("Residual", fResidual);
    RegisterObservable("RelativeZ", fRelativeZ);
    RegisterObservable("TrackBalance", fTrackBalance);
    for (auto& projection : fProjectionTracks) {
        RegisterObservable("TrackBalance_" + projection.name, projection.balance);
        RegisterObservable("Length_" + projection.name, projection.length);
        RegisterObservable("Angle_" + projection.name, projection.angle);
        RegisterObservable("Asymmetry_" + projection.name, projection.asymmetry);
        RegisterObservable("Residual_" + projection.name, projection.residual);
    }
}

bool TRestTrackLineAnalysisProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* inputEvent = dynamic_cast<const TRestTrackEvent*>(&input);
    auto* outputEvent = dynamic_cast<TRestTrackEvent*>(&output);
    if (!inputEvent || !outputEvent) {
        throw std::runtime_error("TRestTrackLineAnalysisProcess: input/output is not a TRestTrackEvent");
    }

    outputEvent->Initialize();
    outputEvent->SetEventInfo(inputEvent);

    // Main tracks: the most energetic one or the most energetic of each projection (type of the first hit)
    for (auto& projection : fProjectionTracks) {
        projection.index = -1;
        projection.maxEnergy = projection.energy = projection.balance = 0;
        projection.length = projection.asymmetry = projection.residual = 0;
        projection.angle = kNaN;
    }
    int maxIndex = -1;
    double maxEnergy = 0, totalEnergy = 0;
    for (int t = 0; t < inputEvent->GetNumberOfTracks(); t++) {
        if (inputEvent->GetNumberOfHits(t) == 0) continue;
        const double energy = inputEvent->GetTrackEnergy(t);
        totalEnergy += energy;
        if (energy > maxEnergy) {
            maxEnergy = energy;
            maxIndex = t;
        }
        const int type = inputEvent->GetTrack(t).GetHits().GetType(0);
        for (auto& projection : fProjectionTracks) {
            if (type != projection.type) continue;
            projection.energy += energy;
            if (energy > projection.maxEnergy) {
                projection.maxEnergy = energy;
                projection.index = t;
            }
        }
    }

    std::vector<int> mainTracks;
    if (fProjectionTracks.empty()) {
        if (maxIndex < 0) return false;
        mainTracks.push_back(maxIndex);
        fEnergy = maxEnergy;
        fTrackBalance = totalEnergy > 0 ? maxEnergy / totalEnergy : 0;
    } else {
        double projectionsEnergy = 0;
        fEnergy = 0;
        for (auto& projection : fProjectionTracks) {
            if (projection.index < 0) return false;
            mainTracks.push_back(projection.index);
            fEnergy += projection.maxEnergy;
            projectionsEnergy += projection.energy;
            projection.balance = projection.energy > 0 ? projection.maxEnergy / projection.energy : 0;
        }
        fTrackBalance = projectionsEnergy > 0 ? fEnergy / projectionsEnergy : 0;
    }

    std::vector<TRestHits> hits;
    std::vector<TrackLine> lines;
    int definedType = 1;  // Coordinates any track of the main track defines
    for (int t : mainTracks) {
        hits.push_back(inputEvent->GetTrack(t).GetHits());
        lines.push_back(AnalyseTrack(hits.back()));
        for (int a = 0; a < 3; a++) {
            if (TRestHitsData::IsDefined(lines.back().type, a) && !TRestHitsData::IsDefined(definedType, a)) {
                definedType *= TRestHitsData::kAxisTypes[a];
            }
        }
    }

    XYZVector origin, end;
    if (lines.size() == 1) {
        origin = lines[0].origin;
        end = lines[0].end;
    } else {
        OrientTracks(lines);
        bool paired = false;
        if (fLineAnaMethod == "3D") {
            paired = GetPairedEnds(hits, lines, origin, end);
            if (!paired && !fMethodWarned) {
                RESTWarning << "TRestTrackLineAnalysisProcess: the projections share no coordinate, the 3D method "
                               "cannot pair them, using default"
                            << RESTendl;
                fMethodWarned = true;
            }
        } else if (fLineAnaMethod != "default" && !fMethodWarned) {
            RESTWarning << "TRestTrackLineAnalysisProcess: line analysis method " << fLineAnaMethod
                        << " is not implemented (default or 3D), using default" << RESTendl;
            fMethodWarned = true;
        }
        if (!paired) GetCombinedEnds(lines, origin, end);
    }

    // Undefined coordinates: NaN in origin and end, 0 in the direction (not in the length)
    const bool defined[3] = {TRestHitsData::IsDefined(definedType, 0), TRestHitsData::IsDefined(definedType, 1),
                             TRestHitsData::IsDefined(definedType, 2)};
    fOriginX = defined[0] ? origin.X() : kNaN;
    fOriginY = defined[1] ? origin.Y() : kNaN;
    fOriginZ = defined[2] ? origin.Z() : kNaN;
    fEndX = defined[0] ? end.X() : kNaN;
    fEndY = defined[1] ? end.Y() : kNaN;
    fEndZ = defined[2] ? end.Z() : kNaN;
    const XYZVector direction(defined[0] ? origin.X() - end.X() : 0, defined[1] ? origin.Y() - end.Y() : 0,
                              defined[2] ? origin.Z() - end.Z() : 0);
    fLength = direction.R();
    fAngle = GetPolarAngle(direction, defined[2]);
    fPhi = defined[0] && defined[1] && direction.Rho() > 0 ? direction.Phi() : kNaN;
    fDownwards = !defined[2] || direction.Z() == 0 ? 0 : direction.Z() > 0 ? 1 : -1;
    fRelativeZ = GetRelativeZ(hits);

    double energySum = 0, asymmetrySum = 0, residualSum = 0;
    for (const auto& line : lines) {
        energySum += line.energy;
        asymmetrySum += line.energy * line.asymmetry;
        residualSum += line.energy * line.residual;
    }
    fAsymmetry = energySum > 0 ? asymmetrySum / energySum : 0;
    fResidual = energySum > 0 ? residualSum / energySum : 0;

    // Main track of each projection, with its own (oriented) origin and end
    for (size_t p = 0; p < fProjectionTracks.size(); p++) {
        auto& projection = fProjectionTracks[p];
        const auto& line = lines[p];
        const XYZVector d = line.origin - line.end;
        const XYZVector projected(TRestHitsData::IsDefined(line.type, 0) ? d.X() : 0,
                                  TRestHitsData::IsDefined(line.type, 1) ? d.Y() : 0,
                                  TRestHitsData::IsDefined(line.type, 2) ? d.Z() : 0);
        projection.length = projected.R();
        projection.angle = GetPolarAngle(projected, TRestHitsData::IsDefined(line.type, 2));
        projection.asymmetry = line.asymmetry;
        projection.residual = line.residual;
    }

    RESTDebug << "TRestTrackLineAnalysisProcess: origin (" << fOriginX << ", " << fOriginY << ", " << fOriginZ
              << "), end (" << fEndX << ", " << fEndY << ", " << fEndZ << "), length " << fLength
              << ", asymmetry " << fAsymmetry << RESTendl;

    // The tracks of the main track, as they are
    for (size_t m = 0; m < mainTracks.size(); m++) {
        std::vector<XYZVector> sigmas;
        for (int h = 0; h < (int)hits[m].GetNumberOfHits(); h++) sigmas.push_back(inputEvent->GetSigma(mainTracks[m], h));
        outputEvent->AddTrack(outputEvent->GetNumberOfTracks() + 1, inputEvent->GetTrackID(mainTracks[m]), hits[m],
                              sigmas);
    }
    return true;
}
