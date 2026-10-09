#ifndef TRESTRAWCOMMONNOISEREDUCTIONPROCESS_H
#define TRESTRAWCOMMONNOISEREDUCTIONPROCESS_H

#include <utility>
#include <vector>

#include "TRestEventProcess.h"
#include "TRestMetadata.h"
#include "TRestRawSignalEvent.h"

/// \brief Describes one group of signals that share the same common-mode
/// noise source, and the parameters used to correct it.
class TRestCommonNoiseBlock : public TRestMetadata {
   public:
    /// Signal ID ranges [min, max] belonging to this block. Empty means "all
    /// signals in the event" (only meaningful for the implicit default block).
    std::vector<std::pair<int, int>> fChannelRange;

    /// Correction method: 0 = median bin, 1 = average of a center window.
    Int_t fMode = 0;

    /// Percentage of signals around the center used for the average in mode 1.
    Int_t fCenterWidth = 10;

    /// Minimum number of signals in this block required to apply the correction.
    Int_t fMinSignalsRequired = 20;

    /// Range used to estimate each signal's baseline before correction.
    std::pair<int, int> fBaselineRange = {20, 150};

    /// Sigma threshold (relative to each channel's own baseline sigma) above
    /// which a channel is considered to carry real signal in a given bin, and is
    /// therefore excluded from that bin's common-noise estimate (though it still
    /// receives the correction).
    Double_t fSignalSigmaThreshold = 7.0;

    /// Sigma threshold (relative to each channel's own baseline sigma) above
    /// which a channel noise is corrected
    Double_t fBaselineSigmaThreshold = 2.3;

    TRestCommonNoiseBlock();
    TRestCommonNoiseBlock(const std::string& name, const YAML::Node& node);
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
class TRestRawCommonNoiseReductionProcess : public TRestEventProcess {
   private:

    /// Parameters used for the single implicit block when fBlocks is empty.
    TRestCommonNoiseBlock fDefaultBlock;

    /// \brief Applies common-noise subtraction to the signals of one block,
    /// filtering by baseline sigma first, and writes the result (corrected or
    /// passed-through) into `outEvent`.
    void ProcessBlock(const TRestRawSignalEvent& inEvent, TRestRawSignalEvent& outEvent,
                      const TRestCommonNoiseBlock& block) const;

   public:
    /// User-defined noise blocks (YAML key: "channels"). Empty means "treat
    /// the whole event as a single block" using fDefaultBlock's parameters.
    std::vector<TRestCommonNoiseBlock> fBlocks;

    TRestRawCommonNoiseReductionProcess();
    TRestRawCommonNoiseReductionProcess(const std::string& instanceName, const YAML::Node& node);
    TRestRawCommonNoiseReductionProcess(const std::string& fileName, const std::string& sectionName);

    virtual void LoadConfig() override;
    void InitProcess() override {}
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestRawSignalEvent"; }
    std::string GetOutputEvent() const override { return "TRestRawSignalEvent"; }

    std::string GetClassName() const override { return "TRestRawCommonNoiseReductionProcess"; }
};

#endif
