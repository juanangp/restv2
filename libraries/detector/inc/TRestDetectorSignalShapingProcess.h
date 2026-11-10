#ifndef TRESTDETECTORSIGNALSHAPINGPROCESS_H
#define TRESTDETECTORSIGNALSHAPINGPROCESS_H

#include <string>
#include <utility>
#include <vector>

#include "TRestEventProcess.h"

/// \class TRestDetectorSignalShapingProcess
/// \brief Shapes the signals of a TRestDetectorSignalEvent with the response of the electronics, in
/// physical time units and before the digitization (TRestDetectorSignalToRawSignalProcess).
///
/// The parameters (YAML keys), the observables and an example are described in the class documentation
/// (TRestDetectorSignalShapingProcess.cxx).
///
/// \author Xinglong
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
class TRestDetectorSignalShapingProcess : public TRestEventProcess {
   private:
    /// Range of u = t / shapingTime of the response
    std::pair<double, double> fSupport = {0, 0};  //!
    /// Response tabulated over fSupport (linear interpolation), not to evaluate it for each point
    std::vector<double> fResponse;  //!
    /// Factor of the response: gain / maximum of the response
    double fNormalization = 0;  //!

    double Response(double u) const;

   public:
    /// Response type: gaus, exponential, shaper or shaperSin (TRestPulseShapeAnalysis::ShapingFunction)
    std::string fShapingType = "shaperSin";
    /// Width of the response (sigma for gaus, time constant otherwise)
    TRestWithUnits fShapingTime = 1;  // us
    /// Height of the pulse of an isolated charge per unit of charge
    double fGain = 1;

    /// \brief Default constructor (no configuration).
    TRestDetectorSignalShapingProcess();
    /// \brief Builds the process from an in-memory YAML section (LoadConfig).
    TRestDetectorSignalShapingProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds the process from the section sectionName of a YAML file (LoadConfig).
    TRestDetectorSignalShapingProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the parameters from the YAML node.
    void LoadConfig() override;
    /// \brief Checks shapingTime and tabulates the response of shapingType (throws for an unknown type).
    void InitProcess() override;
    /// \brief Replaces each signal by its convolution with the response (see the class description).
    /// \return false if the event has no signals.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Nothing to do at the end of the processing.
    void EndProcess() override {}

    /// \brief Input event type, "TRestDetectorSignalEvent".
    std::string GetInputEvent() const override { return "TRestDetectorSignalEvent"; }
    /// \brief Output event type, "TRestDetectorSignalEvent".
    std::string GetOutputEvent() const override { return "TRestDetectorSignalEvent"; }

    /// \brief Returns the class name, "TRestDetectorSignalShapingProcess".
    std::string GetClassName() const override { return "TRestDetectorSignalShapingProcess"; }
};

#endif
