#ifndef TRESTDETECTORSIGNALSHAPINGPROCESS_H
#define TRESTDETECTORSIGNALSHAPINGPROCESS_H

#include <string>
#include <utility>
#include <vector>

#include "TRestEventProcess.h"

/// \class TRestDetectorSignalShapingProcess
/// \brief Shapes the signals of a TRestDetectorSignalEvent with the response of the electronics, in
/// physical time units and before the digitization (TRestDetectorSignalToRawSignalProcess).
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

    TRestDetectorSignalShapingProcess();
    TRestDetectorSignalShapingProcess(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorSignalShapingProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestDetectorSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestDetectorSignalEvent"; }

    std::string GetClassName() const override { return "TRestDetectorSignalShapingProcess"; }
};

#endif
