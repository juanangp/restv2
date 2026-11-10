#ifndef TRESTRAWCOMMONNOISEREDUCTIONPROCESS_H
#define TRESTRAWCOMMONNOISEREDUCTIONPROCESS_H

#include <utility>
#include <vector>

#include "TRestEventProcess.h"
#include "TRestMetadata.h"
class TRestRawSignalEvent;

/// \class TRestCommonNoiseBlock
/// \brief Describes one group of signals that share the same common-mode
/// noise source, and the parameters used to correct it.
///
/// One entry of the `channels` list of TRestRawCommonNoiseReductionProcess. YAML keys: `channelRange`,
/// `mode`, `centerWidth`, `minSignalsRequired`, `baselineRange`, `baselineSigmaThreshold` and
/// `signalSigmaThreshold`.
class TRestCommonNoiseBlock : public TRestMetadata {
   public:
    /// Signal ID ranges [min, max] belonging to this block. Empty means "all
    /// signals in the event" (only meaningful for the implicit default block).
    std::vector<std::pair<int, int>> fChannelRange;

    /// Correction method: 0 = median of the signals in each bin, 1 = average of a window of signals
    /// around the median.
    int fMode = 0;

    /// Mode 1: half width of the averaging window around the median, as a percentage of the number of
    /// signals used.
    int fCenterWidth = 10;

    /// Minimum number of signals in this block required to apply the correction (checked on the block
    /// signals and again on the signals without real signal used for the estimate).
    int fMinSignalsRequired = 20;

    /// Bin range [first, second) used to estimate each signal's baseline and baseline sigma.
    std::pair<int, int> fBaselineRange = {20, 150};

    /// Sigma threshold (relative to each channel's own baseline sigma) above
    /// which a channel is considered to carry real signal in a given bin, and is
    /// therefore excluded from that bin's common-noise estimate (though it still
    /// receives the correction).
    double fSignalSigmaThreshold = 7.0;

    /// Minimum baseline sigma (absolute, in ADC units) of a channel for it to be corrected: channels
    /// with a smaller baseline sigma are neither used for the estimate nor corrected.
    double fBaselineSigmaThreshold = 2.3;

    /// \brief Default constructor (default parameters, all signals).
    TRestCommonNoiseBlock();
    /// \brief Constructor from the YAML node of one `channels` entry.
    TRestCommonNoiseBlock(const std::string& name, const YAML::Node& node);
    /// \brief Reads the registered keys of the block from its YAML node.
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestCommonNoiseBlock"; }

    /// \brief Whether `signalID` belongs to this block (true for all IDs if
    /// fChannelRange is empty).
    bool ContainsID(int signalID) const;
};

/// \class TRestRawCommonNoiseReductionProcess
/// \brief Subtracts common-mode noise per block of signals that share a noise
/// source. If no blocks are configured, treats the whole event as one block.
///
/// For each block (TRestCommonNoiseBlock), the signals whose ID is in `channelRange` and with the same
/// number of points as the first one are taken. Signals with a baseline sigma below
/// `baselineSigmaThreshold` are left untouched. Among the others, those with any point beyond
/// `signalSigmaThreshold` times their baseline sigma (real signal) are corrected but not used to estimate
/// the noise. In each time bin, the common noise is the median (mode 0) or the average of a window around
/// the median (mode 1) of the remaining signals, and every corrected signal is shifted by the mean
/// baseline minus that value (clamped to the short range). Nothing is done if fewer than
/// `minSignalsRequired` signals are available. The event is never dropped; no observables.
///
/// Example (one block per AGET chip):
/// \code
/// commonNoiseReduction:
///   class: "TRestRawCommonNoiseReductionProcess"
///   channels:
///     - channelRange: [[4320, 4391]]
///       mode: 1
///       centerWidth: 10
///       minSignalsRequired: 20
///       baselineRange: [20, 150]
///       baselineSigmaThreshold: 2.3
///       signalSigmaThreshold: 7
///     - channelRange: [[4392, 4463]]
///       mode: 0
/// \endcode
///
/// \author Benjamin Manier
/// \author David Díez Ibáñez
/// \author JuanAn García <juanangp@unizar.es>
class TRestRawCommonNoiseReductionProcess : public TRestEventProcess {
   private:
    /// Parameters used for the single implicit block when fBlocks is empty.
    TRestCommonNoiseBlock fDefaultBlock;

   public:
    /// \brief Applies common-noise subtraction in place to the signals of `signalEvent` that belong to
    /// `block`, filtering by baseline sigma first (the signals not corrected are left unchanged).
    /// \param signalEvent Event modified in place.
    /// \param block Signal IDs and parameters of the correction.
    static void ProcessBlock(TRestRawSignalEvent& signalEvent, const TRestCommonNoiseBlock& block);

    /// User-defined noise blocks (YAML key: "channels"). Empty means "treat
    /// the whole event as a single block" using fDefaultBlock's parameters.
    std::vector<TRestCommonNoiseBlock> fBlocks;

    /// \brief Default constructor (no configuration).
    TRestRawCommonNoiseReductionProcess();
    /// \brief Constructor from the YAML node of the process section.
    TRestRawCommonNoiseReductionProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructor from a YAML file and the name of the process section.
    TRestRawCommonNoiseReductionProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the `channels` blocks.
    virtual void LoadConfig() override;
    void InitProcess() override {}
    /// \brief Copies the input event and corrects each block (or the whole event without blocks).
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }

    std::string GetClassName() const override { return "TRestRawCommonNoiseReductionProcess"; }
};

#endif
