#ifndef TRESTDETECTORSIGNALTORAWSIGNALPROCESS_H
#define TRESTDETECTORSIGNALTORAWSIGNALPROCESS_H

#include <set>
#include <string>
#include <utility>

#include "TRestEventProcess.h"

class TRestDetectorSignalEvent;

/// \class TRestDetectorSignalToRawSignalProcess
/// \brief Digitizes a TRestDetectorSignalEvent into a TRestRawSignalEvent: nPoints samples of width sampling
/// from a trigger time, converted to ADC units (the inverse of TRestRawToDetectorSignalProcess).
class TRestDetectorSignalToRawSignalProcess : public TRestEventProcess {
   private:
    /// Gain and offset used (from calibrationEnergy and calibrationRange if given)
    double fADCGain = 1;    //!
    double fADCOffset = 0;  //!
    /// Observables
    double fTriggerTime = 0;     //!
    int fNSaturatedSignals = 0;  //!

    /// \brief Trigger time of the event (see the class description). Returns false if there is none
    bool GetTriggerTime(const TRestDetectorSignalEvent& event, double& time) const;

   public:
    inline static const std::set<std::string> kModes = {"integrate", "sample"};
    inline static const std::set<std::string> kTriggerModes = {"firstDeposit", "integralThreshold", "fixed"};

    /// Width of the samples (sampling of the electronics)
    TRestWithUnits fSampling = 0;  // us
    /// Number of samples of each raw signal
    int fNPoints = 512;
    /// integrate: the charge of the points inside each sample is added (signals made of charges, e.g. from
    /// TRestDetectorHitsToSignalProcess); sample: the signal is interpolated at the start of each sample
    /// (waveforms, e.g. from TRestDetectorSignalShapingProcess)
    std::string fMode = "integrate";
    /// firstDeposit, integralThreshold or fixed
    std::string fTriggerMode = "firstDeposit";
    /// Time of the trigger inside the acquisition window: the first sample starts at trigger - triggerDelay
    TRestWithUnits fTriggerDelay = 0;  // us
    /// integralThreshold: charge of the event in integralWindow that triggers
    double fIntegralThreshold = 0;
    /// integralThreshold: time window of the integral (0: half the acquisition window, as legacy)
    TRestWithUnits fIntegralWindow = 0;  // us
    /// fixed: trigger time
    TRestWithUnits fTriggerFixedStartTime = 0;  // us
    /// ADC value = offset + gain * charge
    double fGain = 1;
    double fOffset = 0;
    /// Linear calibration instead of gain and offset: the energies calibrationEnergy go to the fractions
    /// calibrationRange of the ADC range (0: minimum, 1: maximum of a short)
    std::pair<TRestWithUnits, TRestWithUnits> fCalibrationEnergy = {0, 0};
    std::pair<double, double> fCalibrationRange = {0, 0};
    /// Saturation of the ADC: the samples are clipped to [-32768, maxValue]
    int fMaxValue = 32767;

    TRestDetectorSignalToRawSignalProcess();
    TRestDetectorSignalToRawSignalProcess(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorSignalToRawSignalProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestDetectorSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }

    std::string GetClassName() const override { return "TRestDetectorSignalToRawSignalProcess"; }
};

#endif
