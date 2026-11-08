#ifndef TRESTDETECTORSIGNALTOHITSPROCESS_H
#define TRESTDETECTORSIGNALTOHITSPROCESS_H

#include <string>
#include <utility>
#include <vector>

#include "TRestEventProcess.h"

class TRestDetectorReadout;

/// \class TRestDetectorSignalToHitsProcess
/// \brief Transforms the signals of a TRestDetectorSignalEvent into the hits of a TRestDetectorHitsEvent
/// using the readout (channel position) and, optionally, a constant drift velocity.
///
/// The parameters (YAML keys), the observables and an example are described in the class documentation
/// (TRestDetectorSignalToHitsProcess.cxx).
class TRestDetectorSignalToHitsProcess : public TRestEventProcess {
   private:
    TRestDetectorReadout* fReadout = nullptr;
    char fAxis = 'Z';         ///< Drift axis parsed from fDriftAxis
    int fDriftDirection = 1;  ///< Drift sense parsed from fDriftAxis (readouts without distance)

   public:
    /// Methods accepted by fMethod. onlyMax, tripleMax, tripleMaxAverage, gaussFit, landauFit and agetFit
    /// are delegated to TRestPulseShapeAnalysis::GetMax
    inline static const std::vector<std::string> kMethods = {
        "onlyMax", "tripleMax", "tripleMaxAverage", "gaussFit", "landauFit",
        "agetFit", "integral",  "qCenter",          "all",      "intWindow"};

    /// Name of the readout (in TRestDetectorReadoutManager) that decodes the signal channels. It can be
    /// omitted if the manager contains a single readout.
    std::string fReadoutName = "";

    /// Drift velocity in position units per signal time unit: coordinate along fDriftAxis where
    /// TRestDetectorReadout::GetDistanceToReadout = driftVelocity * time (readout plane and sense from
    /// the readout), or direction * driftVelocity * time if the readout defines no distance.
    /// Values <= 0 take it from the TRestDetectorMedium at its drift field if there is a medium, otherwise
    /// they disable the drift: the hits are placed on the channel position.
    TRestWithUnits fDriftVelocity = 0;

    /// Name of the TRestDetectorMedium giving the drift velocity when driftVelocity <= 0. It can be omitted
    /// if the manager contains a single one.
    std::string fMediumName = "";

    /// Axis and sense of the drift from the readout into the drift volume (Z, -Z, X, ...; as the legacy
    /// plane normal). Only used if the readout defines no distance: otherwise the axis and the sense come
    /// from the readout normal (e.g. a Micromegas with normal [0, -1, 0] drifts along Y). The drift only
    /// fills it for channels where the readout leaves it undefined (NaN), so channels of detectors without
    /// drift keep their position.
    std::string fDriftAxis = "Z";

    /// Method used to extract the hits from each signal: onlyMax, tripleMax, tripleMaxAverage, gaussFit,
    /// landauFit, agetFit, integral, qCenter (one hit per signal), all (one hit per point) or intWindow
    /// (one hit per integration window).
    std::string fMethod = "tripleMaxAverage";

    /// Points of the moving average applied to the signal before taking its maximum with onlyMax,
    /// tripleMax and tripleMaxAverage (odd, 1 or less: no smoothing), so the noise does not spread the
    /// times; the energy is then the maximum of the smoothed signal
    int fSmoothingPoints = 5;

    /// Time window to integrate when method is intWindow
    TRestWithUnits fIntWindow = 5;

    /// Minimum integral of a window to create a hit when method is intWindow
    double fThreshold = 100;

    /// Time range integrated when method is integral. An empty range (first >= second) integrates the
    /// whole signal.
    std::pair<double, double> fIntegralRange = {0, 0};

    /// \brief Default constructor (no configuration).
    TRestDetectorSignalToHitsProcess();
    /// \brief Builds the process from an in-memory YAML section (LoadConfig).
    TRestDetectorSignalToHitsProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds the process from the section sectionName of a YAML file (LoadConfig).
    TRestDetectorSignalToHitsProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the parameters from the YAML node.
    void LoadConfig() override;
    /// \brief Gets the readout and, if driftVelocity <= 0, the drift velocity of the medium (written to the
    /// metadata).
    void InitProcess() override;
    /// \brief Creates the hits of the signals with the chosen method.
    /// \return false if no hit is created.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Nothing to do at the end of the processing.
    void EndProcess() override {}

    /// \brief Input event type, "TRestDetectorSignalEvent".
    std::string GetInputEvent() const override { return "TRestDetectorSignalEvent"; }
    /// \brief Output event type, "TRestDetectorHitsEvent".
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }

    /// \brief Returns the class name, "TRestDetectorSignalToHitsProcess".
    std::string GetClassName() const override { return "TRestDetectorSignalToHitsProcess"; }
};

#endif
