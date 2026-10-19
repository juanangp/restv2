#ifndef TRESTRAWSIGNALSHAPINGPROCESS_H
#define TRESTRAWSIGNALSHAPINGPROCESS_H

#include <string>
#include <vector>

#include "TRestEventProcess.h"
class TRestRawSignalEvent;

/// \class TRestRawSignalShapingProcess
/// \brief Convolves every signal of a TRestRawSignalEvent with a response kernel
/// (all widths are expressed in bins), to emulate the shaping of real electronics.
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

    /// Factor applied to the kernel. The kernel integral is normalized to `gain`.
    double fShapingGain = 1.0;

    /// Range [first, second) used to estimate the baseline before convolving. The baseline
    /// is removed, the signal convolved, and the baseline added back. If second <= first
    /// the baseline is not handled (legacy behaviour, valid for zero-baseline signals).
    std::pair<int, int> fBaselineRange = {5, 55};

    TRestRawSignalShapingProcess();
    TRestRawSignalShapingProcess(const std::string& instanceName, const YAML::Node& node);
    TRestRawSignalShapingProcess(const std::string& fileName, const std::string& sectionName);

    virtual void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetClassName() const override { return "TRestRawSignalShapingProcess"; }

    /// \brief Builds the kernel for `type`, normalized so that its samples add up to `gain`.
    /// Returns an empty vector if the type is unknown or the kernel would be empty/null.
    static std::vector<double> BuildResponse(const std::string& type, double shapingTime, double gain,
                                             int& center);

    /// \brief Convolves `in` with `response` (same length as `in`). Negative samples are ignored.
    static std::vector<double> Convolve(const std::vector<double>& in, const std::vector<double>& response,
                                    int center, bool ignoreNegative);
};

#endif
