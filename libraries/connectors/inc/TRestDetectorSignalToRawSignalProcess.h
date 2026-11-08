#ifndef TRESTDETECTORSIGNALTORAWSIGNALPROCESS_H
#define TRESTDETECTORSIGNALTORAWSIGNALPROCESS_H

#include <string>
#include <utility>
#include <vector>

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
    /// observable trigger mode: value of triggerObservable (a double) in the output analysis tree
    const double* fTriggerObservableValue = nullptr;  //!

    /// \brief Trigger time of the event (see the class description). Returns false if there is none
    bool GetTriggerTime(const TRestDetectorSignalEvent& event, double& time) const;

   public:
    /// Valid values of mode
    inline static const std::vector<std::string> kModes = {"integrate", "sample"};
    /// Valid values of triggerMode
    inline static const std::vector<std::string> kTriggerModes = {"firstDeposit", "integralThreshold",
                                                                  "fixed", "observable"};

    /// Width of the samples (sampling of the electronics), mandatory (> 0)
    TRestWithUnits fSampling = 0;  // us
    /// Number of samples of each raw signal
    int fNPoints = 512;
    /// integrate: the charge of the points inside each sample is added (signals made of charges, e.g. from
    /// TRestDetectorHitsToSignalProcess); sample: the signal is interpolated at the start of each sample
    /// (waveforms, e.g. from TRestDetectorSignalShapingProcess)
    std::string fMode = "integrate";
    /// firstDeposit, integralThreshold, fixed or observable
    std::string fTriggerMode = "firstDeposit";
    /// observable: branch of the analysis tree with the trigger time (of the input file or of a previous
    /// process), e.g. the TriggerTime of the run of another detector
    std::string fTriggerObservable = "";
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
    /// ADC value of a charge 0 (see fGain)
    double fOffset = 0;
    /// Linear calibration instead of gain and offset: the energies calibrationEnergy go to the fractions
    /// calibrationRange of the ADC range (0: minimum, 1: maximum of a short). [0, 0]: not used
    std::pair<TRestWithUnits, TRestWithUnits> fCalibrationEnergy = {0, 0};
    /// Fractions of the ADC range of the two calibrationEnergy values (see fCalibrationEnergy)
    std::pair<double, double> fCalibrationRange = {0, 0};
    /// Saturation of the ADC: the samples are clipped to [-32768, maxValue]
    int fMaxValue = 32767;

    /// \brief Default constructor, with the default parameters.
    TRestDetectorSignalToRawSignalProcess();
    /// \brief Constructor from the YAML section `node` of the instance `instanceName` (calls LoadConfig).
    TRestDetectorSignalToRawSignalProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructor from the section `sectionName` of the YAML file `fileName` (calls LoadConfig).
    TRestDetectorSignalToRawSignalProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the registered fields from the YAML section and normalizes mode and triggerMode.
    void LoadConfig() override;
    /// \brief Checks the parameters (throws if they are not valid), computes the ADC gain and offset (from
    /// the calibration if given), binds the trigger observable (triggerMode observable) and registers
    /// TriggerTime and NSaturatedSignals.
    void InitProcess() override;
    /// \brief Digitizes every signal of the event in the acquisition window of its trigger.
    /// \return False if the event has no signals or no trigger.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestDetectorSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }

    std::string GetClassName() const override { return "TRestDetectorSignalToRawSignalProcess"; }
};

#endif
