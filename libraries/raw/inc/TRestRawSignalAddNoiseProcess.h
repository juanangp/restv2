#ifndef TRESTRAWSIGNALADDNOISPROCESS_H
#define TRESTRAWSIGNALADDNOISPROCESS_H

#include <vector>

#include "TRestEventProcess.h"
#include "TRestMetadata.h"
class TRestRawSignalEvent;

/// \class TRestNoiseParameters
/// \brief One noise component of TRestRawSignalAddNoiseProcess (an entry of its `noise` list).
///
/// YAML keys: `method` ("white", "sin" or "sinusoidal"), `amplitude` and `period`.
class TRestNoiseParameters : public TRestMetadata {
   public:
    /// Sigma of the white noise, or maximum amplitude of the sinusoidal noise (ADC units)
    double fNoiseAmplitude = 10;  // ADC/Energy units
    /// Period of the sinusoidal noise in bins (not necessarily an integer), not used by white noise
    double fNoisePeriod = 50;  // index units

    /// Noise method: "white" (gaussian) or "sin"/"sinusoidal"
    std::string fNoiseMethod = "white";  // white and sinusoidal(sin) noise implemented

    /// \brief Default constructor (white noise of amplitude 10).
    TRestNoiseParameters();
    /// \brief Constructor from the YAML node of one `noise` entry.
    TRestNoiseParameters(const std::string& name, const YAML::Node& node);
    /// \brief Reads the keys and validates `method`.
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestNoiseParameters"; }
};

/// \class TRestRawSignalAddNoiseProcess
/// \brief Adds simulated electronic noise to every signal of a TRestRawSignalEvent.
///
/// Each entry of `noise` (TRestNoiseParameters) is added in order to all the signals, the samples clamped
/// to the short range:
/// - `white`: gaussian noise of sigma `amplitude`, independent for each bin and signal.
/// - `sin` (or `sinusoidal`): a sine of period `period` bins with the same random phase for all the
///   signals of the event, and a slowly modulated random envelope of maximum `amplitude`.
///
/// No observables. Example:
/// \code
/// addNoise:
///   class: "TRestRawSignalAddNoiseProcess"
///   noise:
///     - method: "white"
///       amplitude: 15
///     - method: "sin"
///       amplitude: 100
///       period: 55
/// \endcode
///
/// \author Javier Gracia
/// \author JuanAn García <juanangp@unizar.es>
class TRestRawSignalAddNoiseProcess : public TRestEventProcess {
   public:
    /// Noise components added to the signals (YAML `noise`)
    std::vector<TRestNoiseParameters> fNoiseParams;

    /// \brief Default constructor (no noise).
    TRestRawSignalAddNoiseProcess();
    /// \brief Constructor from the YAML node of the process section.
    TRestRawSignalAddNoiseProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructor from a YAML file and the name of the process section.
    TRestRawSignalAddNoiseProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the `noise` list.
    virtual void LoadConfig() override;
    void InitProcess() override {}
    /// \brief Copies the input event and adds each noise component to all its signals.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }

    std::string GetClassName() const override { return "TRestRawSignalAddNoiseProcess"; }
};

#endif
