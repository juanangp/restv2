#ifndef TRESTDATASETTARDIS_H
#define TRESTDATASETTARDIS_H

#include <string>
#include <vector>

#include "TRestDataSetProcess.h"

class TRestDetectorReadout;

/// \class TRestDataSetTardis
/// \brief Dataset process that adds the time since the previous event in chronological order, across all the
/// runs of the dataset (`<name>_timeDelay`, in seconds); optionally the previous event in the same channels
/// of the readout (`channels`). See TRestDataSetTardis.cxx.
///
/// \author JuanAn García <juanangp@unizar.es>
class TRestDataSetTardis : public TRestDataSetProcess {
   protected:
    /// \brief Nothing: the process has no parameters
    void ComputeParameters(const TRestDataSet& reference, ROOT::RDF::RNode frame) override;

   public:
    /// `channels`: column or expression with the physical channels of each event (a vector), e.g.
    /// "rawToSignal_SignalsPhysicalID"; empty: the previous event, wherever it is
    std::string fChannels = "";
    /// `readout`: instance name of the readout of the channels in the TRestDetectorReadoutManager of the
    /// dataset; empty: its data readout
    std::string fReadout = "";
    /// `channelTolerance`: neighbours (TRestDetectorReadout::GetAdjacentChannels) of a channel that count as
    /// the same place (0: only the same channel)
    int fChannelTolerance = 1;

    /// \brief An entry of the dataset for TimeDelays: its timestamp and its physical channels
    struct Entry {
        long long seconds = 0;
        int nanoseconds = 0;
        std::vector<int> channels;
    };

    /// \brief Time in seconds since the previous entry (entries in chronological order); NaN for the first.
    /// With a readout, the previous entry in the same place: for each group of channels of the readout that
    /// both entries have (a group: the channels measuring the same coordinates, e.g. the X strips and the Y
    /// strips of a Micromegas, the families of fibres), one channel of each within channelTolerance
    /// neighbours; NaN if there is none or the entry has no channel of the readout.
    static std::vector<double> TimeDelays(const std::vector<Entry>& entries,
                                          const TRestDetectorReadout* readout = nullptr,
                                          int channelTolerance = 1);

    TRestDataSetTardis();
    /// \brief Configured from a YAML node.
    TRestDataSetTardis(const std::string& instanceName, const YAML::Node& node);

    /// \brief Returns "TRestDataSetTardis".
    std::string GetClassName() const override { return "TRestDataSetTardis"; }
    /// \brief Reads the fields (throws if channelTolerance is negative); a reference (referenceFileName,
    /// referenceSelection) is not used: warning and cleared.
    void LoadConfig() override;
    /// \brief Filters the dataset with the cuts and adds `<name>_timeDelay` (an event loop on the timestamps
    /// and the channels)
    void Apply(TRestDataSet& dataSet) const override;
};

#endif
