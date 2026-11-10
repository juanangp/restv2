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
///
/// The parameters (YAML keys), the observables and an example are described in the class documentation
/// (TRestDetectorHitsTransformationProcess.cxx).
///
/// \author Javier G. Garza
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
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

    /// \brief Default constructor (no configuration).
    TRestDetectorHitsTransformationProcess();
    /// \brief Builds the process from an in-memory YAML section (LoadConfig).
    TRestDetectorHitsTransformationProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds the process from the section sectionName of a YAML file (LoadConfig).
    TRestDetectorHitsTransformationProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the parameters from the YAML node.
    void LoadConfig() override;
    /// \brief Checks that rotationAxis is not null when there is a rotation.
    void InitProcess() override;
    /// \brief Applies the energy factor, mirror, rotation and translation to the hits.
    /// \return Always true.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Nothing to do at the end of the processing.
    void EndProcess() override {}

    /// \brief Input event type, "TRestDetectorHitsEvent".
    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    /// \brief Output event type, "TRestDetectorHitsEvent".
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }

    /// \brief Returns the class name, "TRestDetectorHitsTransformationProcess".
    std::string GetClassName() const override { return "TRestDetectorHitsTransformationProcess"; }
};

#endif
