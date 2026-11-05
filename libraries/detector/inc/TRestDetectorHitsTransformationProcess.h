#ifndef TRESTDETECTORHITSTRANSFORMATIONPROCESS_H
#define TRESTDETECTORHITSTRANSFORMATIONPROCESS_H

#include <array>
#include <string>

#include "TRestEventProcess.h"

/// \class TRestDetectorHitsTransformationProcess
/// \brief Scales the energy of the hits of a TRestDetectorHitsEvent and mirrors, rotates and translates
/// their positions, in this order (replaces the legacy TRestDetectorHitsNormalizationProcess,
/// TRestDetectorHitsSpecularProcess, TRestDetectorHitsRotationProcess and
/// TRestDetectorHitsTranslationProcess).
class TRestDetectorHitsTransformationProcess : public TRestEventProcess {
   public:
    /// Factor applied to the energy of all the hits
    double fEnergyFactor = 1;

    /// Point of the mirror plane
    std::array<TRestWithUnits, 3> fMirrorPosition = {0.0, 0.0, 0.0};
    /// Normal of the mirror plane; (0, 0, 0): no mirror
    std::array<double, 3> fMirrorNormal = {0.0, 0.0, 0.0};

    /// Point of the rotation axis
    std::array<TRestWithUnits, 3> fRotationCenter = {0.0, 0.0, 0.0};
    /// Direction of the rotation axis
    std::array<double, 3> fRotationAxis = {0.0, 0.0, 1.0};
    /// Rotation angle (right hand around rotationAxis); 0: no rotation
    TRestWithUnits fRotationAngle = 0;

    /// Translation vector
    std::array<TRestWithUnits, 3> fTranslation = {0.0, 0.0, 0.0};

    TRestDetectorHitsTransformationProcess();
    TRestDetectorHitsTransformationProcess(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorHitsTransformationProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }

    std::string GetClassName() const override { return "TRestDetectorHitsTransformationProcess"; }
};

#endif
