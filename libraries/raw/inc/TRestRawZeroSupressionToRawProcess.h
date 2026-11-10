#ifndef TRESTRAWZEROSUPRESSIONTORAWPROCESS_H
#define TRESTRAWZEROSUPRESSIONTORAWPROCESS_H

#include "TRestEventProcess.h"

class TRestRawSignalEvent;

/// \class TRestRawZeroSupressionToRawProcess
/// \brief Removes the offset of zero-suppressed signals so that they start from 0.
///
/// In each signal, the first non-zero sample is taken as offset and subtracted from all the non-zero
/// samples (the zero samples, not sent by the electronics, are left at 0). No parameters and no
/// observables; the event is never dropped.
///
/// Example:
/// \code
/// zsToRaw:
///   class: "TRestRawZeroSupressionToRawProcess"
/// \endcode
///
/// \author JuanAn García <juanangp@unizar.es>
class TRestRawZeroSupressionToRawProcess : public TRestEventProcess {
   public:
    /// \brief Default constructor (no configuration).
    TRestRawZeroSupressionToRawProcess();
    /// \brief Constructor from the YAML node of the process section.
    TRestRawZeroSupressionToRawProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructor from a YAML file and the name of the process section.
    TRestRawZeroSupressionToRawProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the common process keys (the process has no own parameters).
    virtual void LoadConfig() override;
    /// \brief Nothing to initialize.
    void InitProcess() override;
    /// \brief Copies the input event and applies ZeroSuppressionToRaw to it.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Nothing to finalize.
    void EndProcess() override;

    /// \brief Subtracts from the non-zero samples of each signal its first non-zero sample, in place.
    /// \param rawEvent Event modified in place (nothing is done if null).
    static void ZeroSuppressionToRaw(TRestRawSignalEvent* rawEvent);

    std::string GetInputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }

    std::string GetClassName() const override { return "TRestRawZeroSupressionToRawProcess"; }
};

#endif
