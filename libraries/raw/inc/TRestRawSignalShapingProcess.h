#ifndef TRESTRAWSIGNALSHAPINGPROCESS_H
#define TRESTRAWSIGNALSHAPINGPROCESS_H

#include <string>
#include <vector>

#include "TRestEventProcess.h"
class TRestRawSignalEvent;

/// \class TRestRawSignalShapingProcess
/// \brief Convolves every signal of a TRestRawSignalEvent with a response kernel
/// (all widths are expressed in bins), to emulate the shaping of real electronics.
///
/// The kernel is built once in InitProcess (TRestPulseShapeAnalysis::BuildResponse); an unknown type or an
/// empty kernel throws. With a valid `baselineRange` the baseline is removed before the convolution and
/// added back; otherwise the negative samples are ignored in the convolution. The results are rounded and
/// clamped to the short range. Events without signals pass (empty). No observables.
///
/// Example:
/// \code
/// shaping:
///   class: "TRestRawSignalShapingProcess"
///   shapingType: "shaperSin"   # gaus, exponential, shaper or shaperSin
///   shapingTime: 10            # bins
///   gain: 1
///   baselineRange: [5, 55]
/// \endcode
///
/// \author Xinglong
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
class TRestRawSignalShapingProcess : public TRestEventProcess {
   private:
    /// Kernel built once in InitProcess (it does not change between events).
    std::vector<double> fResponse;

    /// Index of the kernel sample that is aligned with the input sample:
    /// 0 for causal kernels, half the kernel length for the centered gaussian.
    int fResponseCenter = 0;

   public:
    /// Kernel type: gaus, exponential, shaper or shaperSin.
    std::string fShapingType = "shaperSin";

    /// Characteristic width of the kernel, in bins (sigma for gaus, time constant otherwise).
    double fShapingTime = 10.0;

    /// Factor applied to the kernel (YAML `gain`). The kernel integral is normalized to `gain`.
    double fShapingGain = 1.0;

    /// Range [first, second) used to estimate the baseline before convolving. The baseline
    /// is removed, the signal convolved, and the baseline added back. If second <= first
    /// the baseline is not handled (legacy behaviour, valid for zero-baseline signals).
    std::pair<int, int> fBaselineRange = {5, 55};

    /// \brief Default constructor (no configuration).
    TRestRawSignalShapingProcess();
    /// \brief Constructor from the YAML node of the process section.
    TRestRawSignalShapingProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructor from a YAML file and the name of the process section.
    TRestRawSignalShapingProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the registered keys and validates `shapingType`.
    virtual void LoadConfig() override;
    /// \brief Builds the response kernel; throws if it cannot be built.
    void InitProcess() override;
    /// \brief Convolves every signal with the kernel; an event without signals passes empty.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetClassName() const override { return "TRestRawSignalShapingProcess"; }
};

#endif
