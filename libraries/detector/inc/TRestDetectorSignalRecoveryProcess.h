#ifndef TRESTDETECTORSIGNALRECOVERYPROCESS_H
#define TRESTDETECTORSIGNALRECOVERYPROCESS_H

#include <string>
#include <vector>

#include "TRestEventProcess.h"

class TRestDetectorReadout;

/// \class TRestDetectorSignalRecoveryProcess
/// \brief Recovers the signals of dead channels of a TRestDetectorSignalEvent from their adjacent channels,
/// preserving the energy.
///
/// The adjacent channels come from the readout (TRestDetectorReadout::GetAdjacentChannels). For a group of
/// k consecutive dead channels between the alive channels L and R, L and R keep half of their charge and
/// the other half is shared among the dead channels with linear weights: the dead channel j (1 to k, from
/// L) gets (k + 1 - j) / S · L / 2 + j / S · R / 2, S = k (k + 1) / 2, point by point in time. With one
/// dead channel s_i = (s_{i-1} + s_{i+1}) / 2, with two s_i = (2 s_{i-1} + s_{i+2}) / 6 and
/// s_{i+1} = (s_{i-1} + 2 s_{i+2}) / 6, as legacy. A neighbour without signal in the event has no charge;
/// a group is not recovered if neither neighbour has a signal or a neighbour is not in the readout (the
/// edge of a plane). A neighbour shared by two groups gives half of its charge to each one. The signals
/// already present in the dead channels are replaced.
///
/// Parameters:
/// - `channelIds`: DAQ IDs of the dead channels.
/// - `readout`: readout (TRestDetectorReadoutManager) that gives the adjacent channels; empty: the data
///   readout.
///
/// No observables. Channels that are not in the readout are skipped with a warning.
///
///    signalRecovery:
///      class: "TRestDetectorSignalRecoveryProcess"
///      channelIds: [4490, 4489]
///      readout: "IAXO_D1_Readout"
class TRestDetectorSignalRecoveryProcess : public TRestEventProcess {
   private:
    /// A group of consecutive dead channels between two alive channels
    struct DeadGroup {
        int left = -1;
        std::vector<int> dead;
        int right = -1;
    };

    TRestDetectorReadout* fReadout = nullptr;
    std::vector<DeadGroup> fGroups;

   public:
    /// DAQ IDs of the dead channels
    std::vector<int> fChannelIds;

    /// Name of the readout giving the adjacent channels (empty: the data readout of the manager)
    std::string fReadoutName = "";

    /// \brief Default constructor (no configuration).
    TRestDetectorSignalRecoveryProcess();
    /// \brief Builds the process from an in-memory YAML section (LoadConfig).
    TRestDetectorSignalRecoveryProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds the process from the section sectionName of a YAML file (LoadConfig).
    TRestDetectorSignalRecoveryProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the parameters from the YAML node.
    void LoadConfig() override;
    /// \brief Gets the readout and builds the groups of consecutive dead channels with their neighbours.
    void InitProcess() override;
    /// \brief Replaces the signals of the dead channels by the ones recovered from their neighbours.
    /// \return Always true.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Nothing to do at the end of the processing.
    void EndProcess() override {}

    /// \brief Input event type, "TRestDetectorSignalEvent".
    std::string GetInputEvent() const override { return "TRestDetectorSignalEvent"; }
    /// \brief Output event type, "TRestDetectorSignalEvent".
    std::string GetOutputEvent() const override { return "TRestDetectorSignalEvent"; }
    /// \brief Returns the class name, "TRestDetectorSignalRecoveryProcess".
    std::string GetClassName() const override { return "TRestDetectorSignalRecoveryProcess"; }
};

#endif
