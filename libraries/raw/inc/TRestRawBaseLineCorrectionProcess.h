#ifndef TRESTRAWBASELINECORRECTIONPROCESS_H
#define TRESTRAWBASELINECORRECTIONPROCESS_H

#include "TRestEventProcess.h"

/// \class TRestRawBaseLineCorrectionProcess
/// \brief Subtracts from each signal its baseline, a moving average that excludes the pulse.
///
/// The moving average of `smoothingWindow` bins (made odd) replaces the points further than 3 sigma
/// from the baseline (IQR over the whole signal minus 5 bins at each edge) by the baseline, so the
/// pulse does not bias it (legacy TRestRawSignal::GetBaseLineCorrected). The corrected signals are
/// centred at 0 and follow slow baseline drifts. All the signals are corrected: to correct only some
/// channels (e.g. the veto), select them first with TRestRawSignalChannelSelectionProcess.
///
/// Example:
/// \code
/// baseLineCorrection:
///   class: "TRestRawBaseLineCorrectionProcess"
///   smoothingWindow: 75
/// \endcode
class TRestRawBaseLineCorrectionProcess : public TRestEventProcess {
   public:
    /// Number of bins of the moving average used as baseline
    int fSmoothingWindow = 75;

    TRestRawBaseLineCorrectionProcess();
    TRestRawBaseLineCorrectionProcess(const std::string& instanceName, const YAML::Node& node);
    TRestRawBaseLineCorrectionProcess(const std::string& fileName, const std::string& sectionName);

    virtual void LoadConfig() override;
    void InitProcess() override {}
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetClassName() const override { return "TRestRawBaseLineCorrectionProcess"; }
};

#endif
