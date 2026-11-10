#ifndef TRESTRAWSIGNALCHANNELSELECTIONPROCESS_H
#define TRESTRAWSIGNALCHANNELSELECTIONPROCESS_H

#include <utility>
#include <vector>

#include "TRestEventProcess.h"

class TRestRawSignalEvent;

/// \class TRestRawSignalChannelSelectionProcess
/// \brief Selects the signals (DAQ channels) of a TRestRawSignalEvent by their signal ID.
///
/// The signal IDs are given one by one (`...ChannelIds`) or as inclusive [min, max] ranges
/// (`...ChannelRange`, an inverted range is ignored with a warning):
/// - `keepChannelIds`, `keepChannelRange`: only these signals are kept. Without them all are kept.
/// - `removeChannelIds`, `removeChannelRange`: these signals are removed, also when they are kept.
///
/// It is the only selection of channels of the raw processes (they have no `signalsRange`), e.g.:
/// - Separate the detectors of a file: keep the veto channels to analyse them alone, remove them for the
///   Micromegas (see libraries/raw/examples/VetoAnalysis.yaml). Removing whole cards keeps the common
///   noise of the remaining ones.
/// - Dead channels: remove them to evaluate their impact on the detector response.
///
/// The event is dropped (not processed further) when no signal remains. No observables.
///
/// Example (the veto without channel 300):
/// \code
/// keepVeto:
///   class: "TRestRawSignalChannelSelectionProcess"
///   keepChannelRange:
///     - [288, 346]
///   removeChannelIds: [300]
/// \endcode
///
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
class TRestRawSignalChannelSelectionProcess : public TRestEventProcess {
   public:
    /// Individual signal IDs to remove.
    std::vector<int> fRemoveChannelIds;

    /// Inclusive [min, max] signal ID ranges to remove.
    std::vector<std::pair<int, int>> fRemoveChannelRange;

    /// Individual signal IDs to keep (all if no keep field is given).
    std::vector<int> fKeepChannelIds;

    /// Inclusive [min, max] signal ID ranges to keep (all if no keep field is given).
    std::vector<std::pair<int, int>> fKeepChannelRange;

    /// \brief Default constructor (keeps all the signals).
    TRestRawSignalChannelSelectionProcess();
    /// \brief Constructor from the YAML node of the process section.
    TRestRawSignalChannelSelectionProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructor from a YAML file and the name of the process section.
    TRestRawSignalChannelSelectionProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the keep/remove keys and drops inverted ranges with a warning.
    virtual void LoadConfig() override;
    void InitProcess() override {}
    /// \brief Copies the input event and removes the signals for which IsRemoved is true; returns false
    /// (event dropped) when no signal remains.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetClassName() const override { return "TRestRawSignalChannelSelectionProcess"; }

    /// \brief True if `signalID` is in fRemoveChannelIds or inside any fRemoveChannelRange (even if it is
    /// kept), or keep fields are given and it is not in them.
    bool IsRemoved(int signalID) const;
};

#endif
