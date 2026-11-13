/// \class TRestDataSetTardis
///
/// Adds `<name>_timeDelay`: the time in seconds since the previous entry of the dataset in chronological
/// order (timeSeconds and timeNanoSecs), across all the runs; NaN for the first entry. Both timestamp
/// columns are needed (datasets generated without timeNanoSecs must be generated again). The seconds and the
/// nanoseconds are subtracted apart, so the delay keeps the nanosecond precision.
///
/// With `channels`, the delay is the time since the previous entry in the same place of the readout, e.g.
/// to find the events correlated in space and time (a decay after an activation, the afterpulses of a
/// spark). The channels are the physical IDs of the readout (after the decoding, not the DAQ channels) of
/// each event, a vector column or an expression, e.g. "rawToSignal_SignalsPhysicalID" or
/// "rawToSignal_SignalsPhysicalID[rawToSignal_SignalsMaxAmplitude > 25]" (see
/// TRestRawToDetectorSignalProcess). The readout (`readout`) is the one of the TRestDetectorReadoutManager of
/// the dataset; it gives:
/// * the neighbours of each channel (TRestDetectorReadout::GetAdjacentChannels: physical ID - 1 and + 1,
///   without crossing between the X and Y strips of a Micromegas or between rows of fibres);
/// * the groups of channels: those measuring the same coordinates (the finite coordinates of
///   GetPositionFromChannel), e.g. the X strips and the Y strips, the families of fibres along each axis; a
///   pixel readout has a single group.
///
/// Two entries are in the same place if they have a group in common and, in each group they both have,
/// channels within `channelTolerance` neighbours (0: the same channel). E.g. with strips, the previous event
/// with an X strip next to one of the X strips of the event and a Y strip next to one of its Y strips; an
/// event without Y strips (below threshold) only needs the X strips. Entries without any channel of the
/// readout get NaN. Events of a single readout are expected (e.g. after the cuts of a detector): the
/// channels that are not in the readout are ignored.
///
/// The dataset is not reordered: each entry keeps its place and gets its delay. The timestamps (and the
/// channels) are read in memory (an event loop when the process is applied) and matched to the entries by
/// runIndex, eventID and subEventID, as TRestDataSetCombine. Entries with the same timestamp are ordered by
/// runIndex, eventID and subEventID: the first of them has the delay from the previous timestamp, the others
/// 0.
///
/// Parameters:
/// * channels: column or expression of the physical channels of each event; empty (default): no place, the
///   previous event wherever it is.
/// * readout: instance name of the readout in the TRestDetectorReadoutManager of the dataset; empty: its
///   data readout (only with channels).
/// * channelTolerance: number of neighbours of a channel in the same place (1).
/// * cuts: names of the TRestCut of the manager, e.g. to remove the noise. The entries that do not pass them
///   are removed from the dataset before the delays are computed: a noise event is never the previous one.
///   The noise of single channels is removed in `channels` with a mask (e.g. an amplitude threshold).
/// * referenceFileName, referenceSelection: not used (a warning). The process has no parameters.
///
/// The delays are those of the entries at this point of the pipeline: later filters (cuts, other processes)
/// do not recompute them.
///
/// \code{.yaml}
/// tardis:
///   class: "TRestDataSetTardis"
///   cuts: ["noise"]                             # optional: removes the noise events
///   channels: "rawToSignal_SignalsPhysicalID"   # optional: the previous event in the same place
///   channelTolerance: 1
/// \endcode
///
/// \author JuanAn García <juanangp@unizar.es>

#include "TRestDataSetTardis.h"

#include <ROOT/RVec.hxx>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

#include "TRestDataSet.h"
#include "TRestDetectorReadout.h"
#include "TRestDetectorReadoutManager.h"

static const bool TRestDataSetTardis_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    using C = TRestDataSetTardis;
    reg.RegisterField<C>("channels", &C::fChannels);
    reg.RegisterField<C>("readout", &C::fReadout);
    reg.RegisterField<C>("channelTolerance", &C::fChannelTolerance);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register("TRestDataSetTardis",
                                               [](const std::string& name, const YAML::Node& node) {
                                                   return std::make_unique<TRestDataSetTardis>(name, node);
                                               });
    return true;
}();

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// An entry of the dataset: run, event and subevent
struct EventKey {
    int run;
    int event;
    int subEvent;
    bool operator==(const EventKey& other) const {
        return run == other.run && event == other.event && subEvent == other.subEvent;
    }
};
struct EventKeyHash {
    size_t operator()(const EventKey& key) const {
        size_t hash = std::hash<int>()(key.run);
        hash = hash * 1000003u ^ std::hash<int>()(key.event);
        return hash * 1000003u ^ std::hash<int>()(key.subEvent);
    }
};

/// Seconds from a to b, the seconds and the nanoseconds apart (a double of the whole Unix time would lose
/// the nanoseconds)
double Delay(const TRestDataSetTardis::Entry& a, const TRestDataSetTardis::Entry& b) {
    return static_cast<double>(b.seconds - a.seconds) + (b.nanoseconds - a.nanoseconds) * 1e-9;
}

/// The channels of a readout: group and neighbours of each physical ID, computed once
class ChannelMap {
   public:
    ChannelMap(const TRestDetectorReadout& readout, int tolerance)
        : fReadout(readout), fTolerance(tolerance) {}

    /// Group of a physical channel (the finite coordinates of its position, bits 0-2), -1 if it is not in
    /// the readout
    int Group(int physicalID) { return Get(physicalID).group; }
    /// Physical channels within the tolerance of a physical channel, itself included
    const std::vector<int>& Near(int physicalID) { return Get(physicalID).near; }

   private:
    struct Channel {
        int group = -1;
        std::vector<int> near;
    };
    const Channel& Get(int physicalID) {
        const auto found = fChannels.find(physicalID);
        if (found != fChannels.end()) return found->second;
        Channel& channel = fChannels[physicalID];
        const int daqID = physicalID < 0 ? -1 : fReadout.GetDAQID(physicalID);
        if (daqID < 0) return channel;
        const auto position = fReadout.GetPositionFromChannel(daqID);
        channel.group = (std::isfinite(position.X()) ? 1 : 0) | (std::isfinite(position.Y()) ? 2 : 0) |
                        (std::isfinite(position.Z()) ? 4 : 0);
        // Neighbours in steps of GetAdjacentChannels (DAQ IDs)
        std::set<int> near = {daqID};
        std::vector<int> front = {daqID};
        for (int step = 0; step < fTolerance && !front.empty(); step++) {
            std::vector<int> next;
            for (const int daq : front) {
                const auto [previous, following] = fReadout.GetAdjacentChannels(daq);
                for (const int adjacent : {previous, following}) {
                    if (adjacent >= 0 && near.insert(adjacent).second) next.push_back(adjacent);
                }
            }
            front = std::move(next);
        }
        for (const int daq : near) channel.near.push_back(fReadout.GetPhysicalID(daq));
        return channel;
    }

    const TRestDetectorReadout& fReadout;
    const int fTolerance;
    std::unordered_map<int, Channel> fChannels;
};
}  // namespace

std::vector<double> TRestDataSetTardis::TimeDelays(const std::vector<Entry>& entries,
                                                   const TRestDetectorReadout* readout,
                                                   int channelTolerance) {
    std::vector<double> delays(entries.size(), kNaN);
    if (!readout) {
        for (size_t i = 1; i < entries.size(); i++) delays[i] = Delay(entries[i - 1], entries[i]);
        return delays;
    }

    ChannelMap map(*readout, channelTolerance);
    // Entries (indices, increasing) with each physical channel
    std::unordered_map<int, std::vector<int>> history;
    // The physical channels of the readout of each entry (sorted) and its groups
    std::vector<std::vector<int>> channels(entries.size());
    std::vector<std::set<int>> groups(entries.size());

    // The last entry before `before` with a channel of `near`, -1 if none
    auto lastWith = [&history](const std::vector<int>& near, int before) {
        int last = -1;
        for (const int channel : near) {
            const auto found = history.find(channel);
            if (found == history.end()) continue;
            const auto& entriesOf = found->second;
            auto it = std::lower_bound(entriesOf.begin(), entriesOf.end(), before);
            if (it != entriesOf.begin()) last = std::max(last, *std::prev(it));
        }
        return last;
    };
    auto hasAny = [](const std::vector<int>& sorted, const std::vector<int>& near) {
        for (const int channel : near) {
            if (std::binary_search(sorted.begin(), sorted.end(), channel)) return true;
        }
        return false;
    };

    for (size_t e = 0; e < entries.size(); e++) {
        // The channels of the readout of the entry and the near channels of each of its groups
        std::map<int, std::vector<int>> nearByGroup;
        for (const int physicalID : entries[e].channels) {
            const int group = map.Group(physicalID);
            if (group < 0) continue;
            channels[e].push_back(physicalID);
            groups[e].insert(group);
            auto& near = nearByGroup[group];
            const auto& channelNear = map.Near(physicalID);
            near.insert(near.end(), channelNear.begin(), channelNear.end());
        }
        std::sort(channels[e].begin(), channels[e].end());
        channels[e].erase(std::unique(channels[e].begin(), channels[e].end()), channels[e].end());
        for (auto& [group, near] : nearByGroup) {
            std::sort(near.begin(), near.end());
            near.erase(std::unique(near.begin(), near.end()), near.end());
        }

        // The candidates, from the last one: an entry near in a group; accepted if it is near in all the
        // groups that both entries have
        auto lastCandidate = [&](int before) {
            int last = -1;
            for (const auto& [group, near] : nearByGroup) last = std::max(last, lastWith(near, before));
            return last;
        };
        for (int k = lastCandidate(static_cast<int>(e)); k >= 0; k = lastCandidate(k)) {
            bool same = true;
            for (const auto& [group, near] : nearByGroup) {
                if (groups[k].count(group) && !hasAny(channels[k], near)) {
                    same = false;
                    break;
                }
            }
            if (same) {
                delays[e] = Delay(entries[k], entries[e]);
                break;
            }
        }
        for (const int channel : channels[e]) history[channel].push_back(static_cast<int>(e));
    }
    return delays;
}

TRestDataSetTardis::TRestDataSetTardis() { fName = "TRestDataSetTardis"; }

TRestDataSetTardis::TRestDataSetTardis(const std::string& name, const YAML::Node& node)
    : TRestDataSetProcess(name, node) {
    LoadConfig();
}

void TRestDataSetTardis::LoadConfig() {
    UpdateParamsFromYAML<TRestDataSetTardis>(fNode);
    if (fChannelTolerance < 0) {
        throw std::runtime_error(GetClassName() + " '" + fName + "': channelTolerance must not be negative");
    }
    if (!fReferenceFileName.empty() || !fReferenceSelection.empty()) {
        RESTWarning << GetClassName() << " '" << fName
                    << "': referenceFileName and referenceSelection are not used (no parameters)" << RESTendl;
        fReferenceFileName.clear();
        fReferenceSelection.clear();
    }
    UpdateYAMLFromParams<TRestDataSetTardis>(fNode);
}

void TRestDataSetTardis::ComputeParameters(const TRestDataSet&, ROOT::RDF::RNode) {}

void TRestDataSetTardis::Apply(TRestDataSet& dataSet) const {
    const std::string context = GetClassName() + " '" + fName + "'";
    ROOT::RDF::RNode frame = dataSet.GetDataFrame();
    for (const std::string column : {"runIndex", "eventID", "subEventID", "timeSeconds", "timeNanoSecs"}) {
        if (!frame.HasColumn(column)) {
            throw std::runtime_error(context + ": the dataset has no column " + column +
                                     " (generate it again with timeSeconds and timeNanoSecs)");
        }
    }
    const std::string cuts = GetCutExpression(fCutNames);
    if (!cuts.empty()) frame = frame.Filter(cuts, fName);

    // The readout of the channels
    const TRestDetectorReadout* readout = nullptr;
    if (!fChannels.empty()) {
        const auto managers = dataSet.GetMetadataOfType<TRestDetectorReadoutManager>();
        if (managers.size() != 1) {
            throw std::runtime_error(context + ": the dataset has " + std::to_string(managers.size()) +
                                     " TRestDetectorReadoutManager (one expected for channels)");
        }
        std::string name = fReadout;
        readout = managers.front()->SelectReadout(name, context);
    }

    // The timestamps and the channels of the entries (a single event loop)
    auto runs = frame.Take<int>("runIndex");
    auto events = frame.Take<int>("eventID");
    auto subEvents = frame.Take<int>("subEventID");
    auto seconds = frame.Take<long long>("timeSeconds");
    auto nanoseconds = frame.Take<int>("timeNanoSecs");
    ROOT::RDF::RResultPtr<std::vector<ROOT::RVec<int>>> channels;
    if (readout) {
        const std::string channelsColumn = "__tardisChannels_" + fName;
        frame = frame.Define(channelsColumn, fChannels);
        channels = frame.Take<ROOT::RVec<int>>(channelsColumn);
    }
    const size_t nEntries = runs->size();

    std::vector<size_t> order(nEntries);
    std::iota(order.begin(), order.end(), 0);
    for (size_t n = 0; n < nEntries; n++) {
        if ((*nanoseconds)[n] < 0 || (*nanoseconds)[n] >= 1000000000) {
            throw std::runtime_error(context + ": timeNanoSecs " + std::to_string((*nanoseconds)[n]) +
                                     " outside [0, 1e9) in the event " + std::to_string((*events)[n]));
        }
    }
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return std::tie((*seconds)[a], (*nanoseconds)[a], (*runs)[a], (*events)[a], (*subEvents)[a]) <
               std::tie((*seconds)[b], (*nanoseconds)[b], (*runs)[b], (*events)[b], (*subEvents)[b]);
    });

    std::vector<Entry> entries(nEntries);
    for (size_t i = 0; i < nEntries; i++) {
        const size_t n = order[i];
        entries[i].seconds = (*seconds)[n];
        entries[i].nanoseconds = (*nanoseconds)[n];
        if (readout) entries[i].channels.assign((*channels)[n].begin(), (*channels)[n].end());
    }
    const auto timeDelays = TimeDelays(entries, readout, fChannelTolerance);

    auto delays = std::make_shared<std::unordered_map<EventKey, double, EventKeyHash>>();
    delays->reserve(nEntries);
    for (size_t i = 0; i < nEntries; i++) {
        const size_t n = order[i];
        const EventKey key{(*runs)[n], (*events)[n], (*subEvents)[n]};
        if (!delays->emplace(key, timeDelays[i]).second) {
            throw std::runtime_error(context + ": the event " + std::to_string(key.event) + " (subevent " +
                                     std::to_string(key.subEvent) + ") of the run index " +
                                     std::to_string(key.run) + " is twice in the dataset");
        }
    }
    RESTInfo << context << ": time delays of " << nEntries << " entries"
             << (readout ? " in the same channels of " + readout->GetName() : std::string()) << RESTendl;

    const std::string delayColumn = fName + "_timeDelay";
    frame = DefineColumn(frame, delayColumn,
                         [delays](int run, int event, int subEvent) {
                             const auto found = delays->find({run, event, subEvent});
                             return found == delays->end() ? kNaN : found->second;
                         },
                         {"runIndex", "eventID", "subEventID"});
    dataSet.SetDataFrame(frame, {delayColumn});
}
