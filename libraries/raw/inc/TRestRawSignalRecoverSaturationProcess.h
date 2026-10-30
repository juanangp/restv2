#ifndef TRESTRAWSIGNALRECOVERSATURATIONPROCESS_H
#define TRESTRAWSIGNALRECOVERSATURATIONPROCESS_H

#include <utility>
#include <vector>

#include "TRestEventProcess.h"
class TRestRawSignalEvent;

/// \class TRestRawSignalRecoverSaturationProcess
/// \brief Recovers ADC-saturated signals by fitting the AGET pulse shape to the non-saturated bins.
class TRestRawSignalRecoverSaturationProcess : public TRestEventProcess {
   private:
    struct Observables {
        double AddedAmplitude = 0;
        double AddedIntegral = 0;
        int SignalsSaturated = 0;
        int SignalsRecovered = 0;
        std::vector<int> SignalsIDsRecovered;

        void clear() {
            AddedAmplitude = 0;
            AddedIntegral = 0;
            SignalsSaturated = 0;
            SignalsRecovered = 0;
            SignalsIDsRecovered.clear();
        }
    };

    Observables fObs;

   public:
    /// Minimum number of saturated bins to consider a signal as saturated
    int fMinSaturatedBins = 3;

    /// Process all signals in the event
    bool fProcessAllSignals = false;

    /// Number of bins to consider if the signal is not saturated
    int fNBinsIfNotSaturated = 20;

    /// Minimum value to consider a signal as saturated (12-bit ADC maximum)
    int fMinSaturationValue = 4095;

    /// Range of bins used to calculate the baseline (fixed in the fit)
    std::pair<int, int> fBaselineRange = {5, 55};

    /// Option for the baseline calculation, can be set to "robust" or "outliers"
    std::string fBaselineOption = "";

    /// Number of sigmas over baseline fluctuations to identify a point over threshold
    double fPointsThreshold = 2;

    /// Minimum signal fluctuation, measured in sigmas
    double fSignalThreshold = 5;

    /// Minimum number of points over threshold to identify a pulse. The fit range is deduced from them
    int fPointsOverThreshold = 5;

    /// Bins added before the first and after the last point over threshold to define the fit range
    int fFitMargin = 10;

    /// Number of flat points that artificially end a pulse
    int fNPointsFlat = 512;

    TRestRawSignalRecoverSaturationProcess();
    TRestRawSignalRecoverSaturationProcess(const std::string& instanceName, const YAML::Node& node);
    TRestRawSignalRecoverSaturationProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }

    std::string GetClassName() const override { return "TRestRawSignalRecoverSaturationProcess"; }
};

#endif
