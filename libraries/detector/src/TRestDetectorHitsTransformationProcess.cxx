/// \class TRestDetectorHitsTransformationProcess
/// Transforms the hits of a TRestDetectorHitsEvent. The operations are applied in this order, each one
/// only if it is given:
/// 1. energyFactor: multiplies the energy of all the hits (legacy TRestDetectorHitsNormalizationProcess).
/// 2. mirrorPosition, mirrorNormal: reflects the positions on the plane through mirrorPosition with normal
///    mirrorNormal (legacy TRestDetectorHitsSpecularProcess). A normal (0, 0, 0) gives no mirror.
/// 3. rotationCenter, rotationAxis, rotationAngle: rotates the positions by rotationAngle (right hand)
///    around the axis through rotationCenter along rotationAxis (legacy TRestDetectorHitsRotationProcess).
/// 4. translation: adds the vector to the positions (legacy TRestDetectorHitsTranslationProcess).
///
/// As legacy, the positions of the hits that are not XYZ (e.g. veto or strip hits, with undefined
/// coordinates) are not changed; their energy is scaled.
///
/// Parameters: energyFactor (1), mirrorPosition ((0, 0, 0) mm), mirrorNormal ((0, 0, 0)),
/// rotationCenter ((0, 0, 0) mm), rotationAxis ((0, 0, 1)), rotationAngle (0 deg), translation
/// ((0, 0, 0) mm).
///
/// Example (detector rotated 90 degrees around z and moved 10 cm along x):
///
///    hitsTransformation:
///      class: "TRestDetectorHitsTransformationProcess"
///      rotationAxis: [0, 0, 1]
///      rotationAngle: 90 deg
///      translation: [100, 0, 0]
///
/// Differences with legacy: one process instead of four; the legacy defaults (translation (0, 0, 1) mm,
/// specular plane z = 1 mm with normal z) are replaced by no transformation; the mirror is not applied to
/// the non-XYZ hits either (legacy specular did the same).

#include "TRestDetectorHitsTransformationProcess.h"

#include <Math/AxisAngle.h>
#include <Math/Vector3D.h>

#include <stdexcept>

#include "TRestDetectorHitsEvent.h"

static const bool TRestDetectorHitsTransformationProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using P = TRestDetectorHitsTransformationProcess;
    reg.RegisterField<P>("energyFactor", &P::fEnergyFactor);
    reg.RegisterField<P>("mirrorPosition", &P::fMirrorPosition);
    reg.RegisterField<P>("mirrorNormal", &P::fMirrorNormal);
    reg.RegisterField<P>("rotationCenter", &P::fRotationCenter);
    reg.RegisterField<P>("rotationAxis", &P::fRotationAxis);
    reg.RegisterField<P>("rotationAngle", &P::fRotationAngle);
    reg.RegisterField<P>("translation", &P::fTranslation);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDetectorHitsTransformationProcess",
        [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDetectorHitsTransformationProcess>(instanceName, params);
        });
    return true;
}();

using XYZVector = ROOT::Math::XYZVector;

XYZVector ToVector(const std::array<TRestWithUnits, 3>& v) { return {v[0], v[1], v[2]}; }
XYZVector ToVector(const std::array<double, 3>& v) { return {v[0], v[1], v[2]}; }
}  // namespace

TRestDetectorHitsTransformationProcess::TRestDetectorHitsTransformationProcess() : TRestEventProcess() {
    fName = "TRestDetectorHitsTransformationProcess";
}

TRestDetectorHitsTransformationProcess::TRestDetectorHitsTransformationProcess(
    const std::string& instanceName, const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestDetectorHitsTransformationProcess::TRestDetectorHitsTransformationProcess(const std::string& fileName,
                                                                               const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestDetectorHitsTransformationProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();
    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestDetectorHitsTransformationProcess::LoadConfig - YAML node is missing" << RESTendl;
        return;
    }
    UpdateParamsFromYAML<TRestDetectorHitsTransformationProcess>(fNode);
    UpdateYAMLFromParams<TRestDetectorHitsTransformationProcess>(fNode);
}

void TRestDetectorHitsTransformationProcess::InitProcess() {
    if (fRotationAngle != 0 && ToVector(fRotationAxis).R() == 0) {
        throw std::runtime_error("TRestDetectorHitsTransformationProcess: rotationAxis cannot be (0, 0, 0)");
    }
}

bool TRestDetectorHitsTransformationProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    const auto* inputEvent = dynamic_cast<const TRestDetectorHitsEvent*>(&input);
    auto* outputEvent = dynamic_cast<TRestDetectorHitsEvent*>(&output);
    if (!inputEvent || !outputEvent) {
        throw std::runtime_error(
            "TRestDetectorHitsTransformationProcess: input/output is not a TRestDetectorHitsEvent");
    }

    outputEvent->Initialize();
    outputEvent->SetEventInfo(inputEvent);

    const XYZVector mirrorPosition = ToVector(fMirrorPosition);
    XYZVector mirrorNormal = ToVector(fMirrorNormal);
    const bool mirror = mirrorNormal.R() > 0;
    if (mirror) mirrorNormal = mirrorNormal.Unit();

    const bool rotate = fRotationAngle != 0;
    const XYZVector rotationCenter = ToVector(fRotationCenter);
    const ROOT::Math::AxisAngle rotation(ToVector(fRotationAxis), rotate ? (double)fRotationAngle : 0.);

    const XYZVector translation = ToVector(fTranslation);

    for (size_t n = 0; n < inputEvent->GetNumberOfHits(); n++) {
        XYZVector position = inputEvent->GetPosition(n);
        const auto type = inputEvent->GetType(n);

        if (type == TRestHitsData::REST_HitType::XYZ) {
            if (mirror) {
                const XYZVector relative = position - mirrorPosition;
                position = mirrorPosition + relative - 2 * relative.Dot(mirrorNormal) * mirrorNormal;
            }
            if (rotate) position = rotationCenter + rotation * (position - rotationCenter);
            position += translation;
        }

        outputEvent->AddHit(position, inputEvent->GetEnergy(n) * fEnergyFactor, inputEvent->GetTime(n), type);
    }

    return true;
}
