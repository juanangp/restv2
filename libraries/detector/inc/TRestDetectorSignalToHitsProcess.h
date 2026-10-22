#ifndef TRESTDETECTORSIGNALTOHITSPROCESS_H
#define TRESTDETECTORSIGNALTOHITSPROCESS_H

#include <set>
#include <string>
#include <utility>

#include "TRestEventProcess.h"

class TRestDetectorReadout;

/// \class TRestDetectorSignalToHitsProcess
/// \brief Transforms the signals of a TRestDetectorSignalEvent into the hits of a TRestDetectorHitsEvent
/// using the readout (channel position) and, optionally, a constant drift velocity.
class TRestDetectorSignalToHitsProcess : public TRestEventProcess {
   private:
    TRestDetectorReadout* fReadout = nullptr;
    char fAxis = 'Z';    ///< Drift axis parsed from fDriftAxis
    int fDriftDirection = 1;  ///< Drift sense parsed from fDriftAxis

   public:
    /// Methods accepted by fMethod. onlyMax, tripleMax, tripleMaxAverage, gaussFit, landauFit and agetFit
    /// are delegated to TRestPulseShapeAnalysis::GetMax
    inline static const std::set<std::string> kMethods = {
        "onlyMax", "tripleMax", "tripleMaxAverage", "gaussFit", "landauFit",
        "agetFit", "integral",  "qCenter",          "all",      "intWindow"};

    /// Name of the readout (in TRestDetectorReadoutManager) that decodes the signal channels. It can be
    /// omitted if the manager contains a single readout.
    std::string fReadoutName = "";

    /// Drift velocity in position units per signal time unit: coordinate along fDriftAxis = readout
    /// plane (where TRestDetectorReadout::GetDistanceToReadout is 0) + direction * driftVelocity * time.
    /// Values <= 0 disable the drift: the hits are placed on the channel position.
    TRestWithUnits fDriftVelocity = 0;

    /// Axis and sense of the drift from the readout into the drift volume (Z, -Z, X, ...; as the legacy
    /// plane normal). The drift only fills it for channels where the readout leaves it undefined (NaN),
    /// so channels of detectors without drift keep their position.
    std::string fDriftAxis = "Z";

    /// Method used to extract the hits from each signal: onlyMax, tripleMax, tripleMaxAverage, gaussFit,
    /// landauFit, agetFit, integral, qCenter (one hit per signal), all (one hit per point) or intWindow
    /// (one hit per integration window).
    std::string fMethod = "tripleMaxAverage";

    /// Time window to integrate when method is intWindow
    TRestWithUnits fIntWindow = 5;

    /// Minimum integral of a window to create a hit when method is intWindow
    double fThreshold = 100;

    /// Time range integrated when method is integral. An empty range (first >= second) integrates the
    /// whole signal.
    std::pair<double, double> fIntegralRange = {0, 0};

    TRestDetectorSignalToHitsProcess();
    TRestDetectorSignalToHitsProcess(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorSignalToHitsProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestDetectorSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }

    std::string GetClassName() const override { return "TRestDetectorSignalToHitsProcess"; }
};

#endif
