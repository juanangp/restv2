/// Unit tests of libraries/raw (GoogleTest, BUILD_TESTING): TRestRawSignalEvent / TRestRawSignal /
/// TRestRawSignalData and the processing processes, with events built in memory and the processes configured
/// from YAML strings. The observables are read from an analysis tree in memory bound to a TRestRun without
/// files (TRestRun::fOutputAnalysisTree), as the process managers do.

#include <TRandom.h>
#include <TRestPulseShapeAnalysis.h>
#include <TRestRawBaseLineCorrectionProcess.h>
#include <TRestRawCommonNoiseReductionProcess.h>
#include <TRestRawSignalAddNoiseProcess.h>
#include <TRestRawSignalAnalysisProcess.h>
#include <TRestRawSignalChannelSelectionProcess.h>
#include <TRestRawSignalEvent.h>
#include <TRestRawSignalRecoverSaturationProcess.h>
#include <TRestRawSignalShapingProcess.h>
#include <TRestRawZeroSupressionToRawProcess.h>
#include <TRestRun.h>
#include <TTree.h>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

/// A process of class P configured from a YAML string
template <class P>
std::unique_ptr<P> MakeProcess(const std::string& name, const std::string& yaml) {
    return std::make_unique<P>(name, YAML::Load(yaml));
}

/// Analysis tree in memory bound to a TRestRun without files, so the processes register their observables
struct ObservableRun {
    std::unique_ptr<TTree> tree;
    TRestRun run;

    ObservableRun() : tree(std::make_unique<TTree>("AnalysisTree", "AnalysisTree")) {
        tree->SetDirectory(nullptr);
        run.fOutputAnalysisTree = tree.get();
    }
    ~ObservableRun() { run.fOutputAnalysisTree = nullptr; }

    /// Binds the process to the run and initializes it (registers its observables)
    void Init(TRestEventProcess& process) {
        process.SetRunInfo(&run);
        process.Initialize();
    }

    template <typename T>
    const T& Get(const std::string& name) const {
        return run.GetObservable<T>(name);
    }
};

std::vector<short> Flat(int n, short value) { return std::vector<short>(n, value); }

/// Baseline with a deterministic +-1 alternating noise (mean `base`, standard deviation 1 over an even
/// number of bins)
std::vector<short> Alternating(int n, short base) {
    std::vector<short> s(n);
    for (int b = 0; b < n; b++) s[b] = static_cast<short>(base + (b % 2 == 0 ? 1 : -1));
    return s;
}

/// Alternating baseline 250 with a triangular pulse of height 1000 at bin 200 (bins 181-219, no noise)
std::vector<short> TrianglePulse(int n = 512) {
    auto s = Alternating(n, 250);
    for (int b = 181; b <= 219; b++) s[b] = static_cast<short>(250 + 1000 - 50 * std::abs(b - 200));
    return s;
}

/// Common noise pattern of period 10 (sum 0 over a period, standard deviation ~3.7)
constexpr short kNoisePattern[10] = {0, 3, 5, 5, 3, 0, -3, -5, -5, -3};

std::vector<short> CommonNoiseSignal(int n, short base) {
    std::vector<short> s(n);
    for (int b = 0; b < n; b++) s[b] = static_cast<short>(base + kNoisePattern[b % 10]);
    return s;
}

double Mean(const std::vector<short>& v) { return std::accumulate(v.begin(), v.end(), 0.0) / v.size(); }

double StdDev(const std::vector<short>& v) {
    const double m = Mean(v);
    double s2 = 0;
    for (short x : v) s2 += (x - m) * (x - m);
    return std::sqrt(s2 / v.size());
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------
// TRestRawSignalEvent, TRestRawSignal, TRestRawSignalData
// ---------------------------------------------------------------------------------------------------------

TEST(RawSignalEvent, AddSignalsAndAccess) {
    TRestRawSignalEvent event;
    event.AddSignal(10, {1, 2, 3});
    event.AddSignal(20, {4, 5});
    event.AddSignal(30, {6, 7, 8, 9});

    ASSERT_EQ(event.GetNumberOfSignals(), 3);
    EXPECT_TRUE(event.SignalExists(20));
    EXPECT_FALSE(event.SignalExists(40));

    EXPECT_EQ(event.GetSignal(0).GetSignalID(), 10);
    EXPECT_EQ(event.GetSignal(1).GetNPoints(), 2);
    EXPECT_EQ(event.GetSignal(2).GetNPoints(), 4);
    EXPECT_EQ(event.GetSignal(2).GetPoint(3), 9);
    EXPECT_EQ(event.GetSignalByID(20).GetData(), (std::vector<short>{4, 5}));
    EXPECT_EQ(event.GetSignalByID(30).GetData(), (std::vector<short>{6, 7, 8, 9}));

    const TRestRawSignalEvent& constEvent = event;
    EXPECT_EQ(constEvent.GetSignal(0).GetData(), (std::vector<short>{1, 2, 3}));
    EXPECT_EQ(constEvent.GetSignalByID(10).GetPoint(1), 2);
}

TEST(RawSignalEvent, DuplicatedIDIsIgnored) {
    TRestRawSignalEvent event;
    event.AddSignal(5, {1, 2});
    event.AddSignal(5, {3, 4, 5});
    ASSERT_EQ(event.GetNumberOfSignals(), 1);
    EXPECT_EQ(event.GetSignalByID(5).GetData(), (std::vector<short>{1, 2}));
}

TEST(RawSignalEvent, InvalidAccessThrows) {
    TRestRawSignalEvent event;
    event.AddSignal(1, {0, 0});
    EXPECT_THROW(event.GetSignal(1), std::out_of_range);
    EXPECT_THROW(event.GetSignalByID(2), std::runtime_error);
}

TEST(RawSignalEvent, RemoveSignalCompactsStorage) {
    TRestRawSignalEvent event;
    event.AddSignal(1, {1, 1, 1});
    event.AddSignal(2, {2, 2});
    event.AddSignal(3, {3, 3, 3, 3});

    event.RemoveSignal(1);
    ASSERT_EQ(event.GetNumberOfSignals(), 2);
    EXPECT_FALSE(event.SignalExists(2));
    EXPECT_EQ(event.GetSignal(1).GetSignalID(), 3);
    EXPECT_EQ(event.GetSignal(1).GetData(), (std::vector<short>{3, 3, 3, 3}));

    event.RemoveSignal(5);  // beyond the last signal: nothing
    EXPECT_EQ(event.GetNumberOfSignals(), 2);

    event.RemoveSignalByID(1);
    ASSERT_EQ(event.GetNumberOfSignals(), 1);
    EXPECT_EQ(event.GetSignal(0).GetSignalID(), 3);
    EXPECT_EQ(event.GetSignal(0).GetData(), (std::vector<short>{3, 3, 3, 3}));

    event.RemoveSignalByID(99);  // not found: nothing
    EXPECT_EQ(event.GetNumberOfSignals(), 1);

    // Adding after removing appends at the end of the compacted storage
    event.AddSignal(4, {4, 5});
    EXPECT_EQ(event.GetSignalByID(4).GetData(), (std::vector<short>{4, 5}));
    EXPECT_EQ(event.GetSignalByID(3).GetNPoints(), 4);
}

TEST(RawSignalEvent, IncreaseBin) {
    TRestRawSignalEvent event;
    event.AddSignal(1, {10, 20, 30});
    event.AddSignal(2, {SHRT_MAX - 5, SHRT_MIN + 5, 0});

    event.GetSignal(0).IncreaseBinBy(1, 5);
    EXPECT_EQ(event.GetSignal(0).GetData(), (std::vector<short>{10, 25, 30}));
    // The other signal is not modified
    EXPECT_EQ(event.GetSignal(1).GetPoint(2), 0);

    auto& sig = event.GetSignal(1);
    sig.IncreaseBinByClamped(0, 100);
    sig.IncreaseBinByClamped(1, -100);
    sig.IncreaseBinByClamped(2, -7);
    EXPECT_EQ(sig.GetPoint(0), SHRT_MAX);
    EXPECT_EQ(sig.GetPoint(1), SHRT_MIN);
    EXPECT_EQ(sig.GetPoint(2), -7);
}

TEST(RawSignalEvent, ADCSaturation) {
    TRestRawSignalEvent event;
    event.AddSignal(1, {0, 10, 4095, 4095, 4095, 20, 0});  // flat top of 3 bins
    event.AddSignal(2, {0, 10, 4095, 4095, 20, 0});        // flat top of 2 bins
    event.AddSignal(3, {0, 10, 500, 20, 0});               // single maximum
    event.AddSignal(4, {0, 10, 20, 100, 100});             // flat top cut by the end of the signal
    event.AddSignal(5, {});

    EXPECT_TRUE(event.GetSignal(0).IsADCSaturation());
    EXPECT_FALSE(event.GetSignal(1).IsADCSaturation());
    EXPECT_TRUE(event.GetSignal(1).IsADCSaturation(2));
    EXPECT_FALSE(event.GetSignal(2).IsADCSaturation());
    EXPECT_TRUE(event.GetSignal(2).IsADCSaturation(1));
    EXPECT_FALSE(event.GetSignal(3).IsADCSaturation());
    EXPECT_TRUE(event.GetSignal(3).IsADCSaturation(2));
    EXPECT_FALSE(event.GetSignal(4).IsADCSaturation());
    EXPECT_FALSE(event.GetSignal(0).IsADCSaturation(0));
}

TEST(RawSignalEvent, CopyMoveAndInitialize) {
    TRestRawSignalEvent source;
    source.SetID(42);
    source.SetSubEventTag("tag");
    source.AddSignal(1, {1, 2});
    source.AddSignal(2, {3});

    TRestRawSignalEvent copy;
    copy.AddSignal(9, {9, 9, 9});
    copy.CopyFrom(&source);
    EXPECT_EQ(copy.GetID(), 42);
    EXPECT_EQ(copy.GetSubEventTag(), "tag");
    ASSERT_EQ(copy.GetNumberOfSignals(), 2);
    EXPECT_FALSE(copy.SignalExists(9));
    EXPECT_EQ(copy.GetSignalByID(2).GetData(), (std::vector<short>{3}));
    // The copy is independent of the source
    copy.GetSignal(0).IncreaseBinBy(0, 10);
    EXPECT_EQ(source.GetSignal(0).GetPoint(0), 1);

    TRestRawSignalEvent moved;
    moved.MoveFrom(&source);
    EXPECT_EQ(moved.GetID(), 42);
    ASSERT_EQ(moved.GetNumberOfSignals(), 2);
    EXPECT_EQ(moved.GetSignalByID(1).GetData(), (std::vector<short>{1, 2}));
    EXPECT_EQ(source.GetNumberOfSignals(), 0);

    moved.Initialize();
    EXPECT_EQ(moved.GetNumberOfSignals(), 0);
    EXPECT_EQ(moved.GetID(), 0);
    EXPECT_EQ(moved.GetSubEventTag(), "");
}

TEST(RawSignalEvent, ViewOverSignalData) {
    TRestRawSignalData data;
    data.allSamples = {1, 2, 3, 4, 5};
    data.signalIDs = {7, 8};
    data.offsets = {0, 3};

    const TRestRawSignal first(&data, 0), second(&data, 1);
    EXPECT_EQ(first.GetSignalID(), 7);
    EXPECT_EQ(first.GetNPoints(), 3);
    EXPECT_EQ(second.GetNPoints(), 2);
    EXPECT_EQ(second.GetData(), (std::vector<short>{4, 5}));

    const TGraph graph = second.GetGraph();
    EXPECT_EQ(graph.GetN(), 2);

    data.clear();
    EXPECT_TRUE(data.allSamples.empty());
    EXPECT_TRUE(data.signalIDs.empty());
    EXPECT_TRUE(data.offsets.empty());
}

TEST(RawSignalEvent, RegisteredInEventRegistry) {
    ASSERT_TRUE(EventRegistry::Instance().Contains("TRestRawSignalEvent"));
    auto event = EventRegistry::Instance().Create("TRestRawSignalEvent", "myRaw");
    EXPECT_EQ(event->GetName(), "myRaw");
    EXPECT_NE(dynamic_cast<TRestRawSignalEvent*>(event.get()), nullptr);
}

// ---------------------------------------------------------------------------------------------------------
// Processes: creation from the registry
// ---------------------------------------------------------------------------------------------------------

TEST(RawProcesses, CreatedFromRegistry) {
    auto metadata = MetadataClassRegistry::Instance().Create(
        "sel", YAML::Load("class: TRestRawSignalChannelSelectionProcess\nkeepChannelIds: [3]"));
    auto* process = dynamic_cast<TRestRawSignalChannelSelectionProcess*>(metadata.get());
    ASSERT_NE(process, nullptr);
    EXPECT_EQ(process->GetName(), "sel");
    EXPECT_EQ(process->fKeepChannelIds, (std::vector<int>{3}));
    EXPECT_EQ(process->GetInputEvent(), "TRestRawSignalEvent");
    EXPECT_EQ(process->GetOutputEvent(), "TRestRawSignalEvent");
}

TEST(RawProcesses, UnknownKeyThrows) {
    EXPECT_ANY_THROW(MetadataClassRegistry::Instance().Create(
        "bl", YAML::Load("class: TRestRawBaseLineCorrectionProcess\nsmoothingWindw: 5")));
}

// ---------------------------------------------------------------------------------------------------------
// TRestRawSignalChannelSelectionProcess
// ---------------------------------------------------------------------------------------------------------

TEST(RawChannelSelection, KeepAndRemove) {
    auto process = MakeProcess<TRestRawSignalChannelSelectionProcess>(
        "sel", "keepChannelRange: [[10, 12]]\nkeepChannelIds: [20]\nremoveChannelIds: [11]");
    process->Initialize();

    TRestRawSignalEvent input, output;
    input.SetID(3);
    for (int id : {9, 10, 11, 12, 13, 20}) input.AddSignal(id, {static_cast<short>(id)});

    ASSERT_TRUE(process->ProcessEvent(input, output));
    ASSERT_EQ(output.GetNumberOfSignals(), 3);
    EXPECT_TRUE(output.SignalExists(10));
    EXPECT_TRUE(output.SignalExists(12));
    EXPECT_TRUE(output.SignalExists(20));
    EXPECT_EQ(output.GetSignalByID(12).GetPoint(0), 12);
    EXPECT_EQ(output.GetID(), 3);
    // The input is not modified
    EXPECT_EQ(input.GetNumberOfSignals(), 6);

    EXPECT_TRUE(process->IsRemoved(11));
    EXPECT_TRUE(process->IsRemoved(13));
    EXPECT_FALSE(process->IsRemoved(10));
}

TEST(RawChannelSelection, RemoveRangeAndDropEmptyEvent) {
    auto process = MakeProcess<TRestRawSignalChannelSelectionProcess>("sel", "removeChannelRange: [[0, 5]]");
    process->Initialize();

    TRestRawSignalEvent input, output;
    input.AddSignal(2, {1});
    input.AddSignal(6, {1});
    ASSERT_TRUE(process->ProcessEvent(input, output));
    ASSERT_EQ(output.GetNumberOfSignals(), 1);
    EXPECT_EQ(output.GetSignal(0).GetSignalID(), 6);

    TRestRawSignalEvent onlyRemoved, output2;
    onlyRemoved.AddSignal(0, {1});
    onlyRemoved.AddSignal(5, {1});
    EXPECT_FALSE(process->ProcessEvent(onlyRemoved, output2));
}

TEST(RawChannelSelection, InvertedRangeIgnored) {
    auto process = MakeProcess<TRestRawSignalChannelSelectionProcess>("sel", "keepChannelRange: [[12, 10]]");
    EXPECT_TRUE(process->fKeepChannelRange.empty());
    // Without keep fields all the signals are kept
    EXPECT_FALSE(process->IsRemoved(11));
    EXPECT_FALSE(process->IsRemoved(100));
}

// ---------------------------------------------------------------------------------------------------------
// TRestRawBaseLineCorrectionProcess
// ---------------------------------------------------------------------------------------------------------

TEST(RawBaseLineCorrection, RemovesFlatBaselineKeepingPulse) {
    auto process = MakeProcess<TRestRawBaseLineCorrectionProcess>("bl", "smoothingWindow: 75");
    process->Initialize();

    auto samples = Flat(512, 250);
    for (int b = 100; b < 110; b++) samples[b] = 350;
    TRestRawSignalEvent input, output;
    input.AddSignal(1, samples);

    ASSERT_TRUE(process->ProcessEvent(input, output));
    const auto corrected = output.GetSignal(0).GetData();
    ASSERT_EQ(corrected.size(), 512u);
    for (int b = 0; b < 512; b++) {
        const short expected = (b >= 100 && b < 110) ? 100 : 0;
        ASSERT_EQ(corrected[b], expected) << "bin " << b;
    }
}

TEST(RawBaseLineCorrection, InvalidWindowThrows) {
    EXPECT_THROW(MakeProcess<TRestRawBaseLineCorrectionProcess>("bl", "smoothingWindow: 0"),
                 std::runtime_error);
}

// ---------------------------------------------------------------------------------------------------------
// TRestRawCommonNoiseReductionProcess
// ---------------------------------------------------------------------------------------------------------

TEST(RawCommonNoiseReduction, RemovesCommonNoise) {
    // Without blocks: the whole event, minSignalsRequired 20
    auto process =
        MakeProcess<TRestRawCommonNoiseReductionProcess>("cnr", "class: TRestRawCommonNoiseReductionProcess");
    process->Initialize();

    TRestRawSignalEvent input, output;
    for (int id = 0; id < 24; id++) {
        auto samples = CommonNoiseSignal(512, 250);
        if (id == 5) {
            for (int b = 300; b < 310; b++) samples[b] += 200;  // real signal, not used in the estimate
        }
        input.AddSignal(id, samples);
    }

    ASSERT_TRUE(process->ProcessEvent(input, output));
    ASSERT_EQ(output.GetNumberOfSignals(), 24);
    for (int s = 0; s < 24; s++) {
        const auto data = output.GetSignal(s).GetData();
        const int id = output.GetSignal(s).GetSignalID();
        for (int b = 0; b < 512; b++) {
            const short expected = (id == 5 && b >= 300 && b < 310) ? 450 : 250;
            ASSERT_EQ(data[b], expected) << "signal " << id << " bin " << b;
        }
    }
}

TEST(RawCommonNoiseReduction, BlocksAndMinimumSignals) {
    // Only the block [0, 11] is corrected (12 signals >= 5)
    auto process = MakeProcess<TRestRawCommonNoiseReductionProcess>(
        "cnr", "channels:\n  - channelRange: [[0, 11]]\n    minSignalsRequired: 5\n    mode: 1\n");
    ASSERT_EQ(process->fBlocks.size(), 1u);
    EXPECT_EQ(process->fBlocks[0].fMode, 1);
    EXPECT_TRUE(process->fBlocks[0].ContainsID(11));
    EXPECT_FALSE(process->fBlocks[0].ContainsID(12));
    process->Initialize();

    TRestRawSignalEvent input, output;
    for (int id = 0; id < 24; id++) input.AddSignal(id, CommonNoiseSignal(256, 250));

    ASSERT_TRUE(process->ProcessEvent(input, output));
    for (int s = 0; s < 24; s++) {
        const int id = output.GetSignal(s).GetSignalID();
        const auto data = output.GetSignal(s).GetData();
        if (id <= 11) {
            EXPECT_EQ(*std::min_element(data.begin(), data.end()), 250) << "signal " << id;
            EXPECT_EQ(*std::max_element(data.begin(), data.end()), 250) << "signal " << id;
        } else {
            EXPECT_EQ(data, CommonNoiseSignal(256, 250)) << "signal " << id;
        }
    }

    // Fewer signals than the default minSignalsRequired (20): nothing is done
    auto defaultProcess = MakeProcess<TRestRawCommonNoiseReductionProcess>(
        "cnr2", "class: TRestRawCommonNoiseReductionProcess");
    defaultProcess->Initialize();
    TRestRawSignalEvent few, fewOutput;
    for (int id = 0; id < 10; id++) few.AddSignal(id, CommonNoiseSignal(256, 250));
    ASSERT_TRUE(defaultProcess->ProcessEvent(few, fewOutput));
    for (int s = 0; s < 10; s++) EXPECT_EQ(fewOutput.GetSignal(s).GetData(), CommonNoiseSignal(256, 250));
}

// ---------------------------------------------------------------------------------------------------------
// TRestRawZeroSupressionToRawProcess
// ---------------------------------------------------------------------------------------------------------

TEST(RawZeroSuppressionToRaw, SubtractsFirstNonZeroSample) {
    auto process =
        MakeProcess<TRestRawZeroSupressionToRawProcess>("zs", "class: TRestRawZeroSupressionToRawProcess");
    process->Initialize();

    TRestRawSignalEvent input, output;
    input.AddSignal(1, {0, 0, 100, 105, 0, 110, 0});
    input.AddSignal(2, {0, 0, 0});
    input.AddSignal(3, {50, 60});

    ASSERT_TRUE(process->ProcessEvent(input, output));
    EXPECT_EQ(output.GetSignalByID(1).GetData(), (std::vector<short>{0, 0, 0, 5, 0, 10, 0}));
    EXPECT_EQ(output.GetSignalByID(2).GetData(), (std::vector<short>{0, 0, 0}));
    EXPECT_EQ(output.GetSignalByID(3).GetData(), (std::vector<short>{0, 10}));

    // Static version, in place; nothing for a null event
    TRestRawZeroSupressionToRawProcess::ZeroSuppressionToRaw(&input);
    EXPECT_EQ(input.GetSignalByID(1).GetData(), (std::vector<short>{0, 0, 0, 5, 0, 10, 0}));
    TRestRawZeroSupressionToRawProcess::ZeroSuppressionToRaw(nullptr);
}

// ---------------------------------------------------------------------------------------------------------
// TRestRawSignalRecoverSaturationProcess
// ---------------------------------------------------------------------------------------------------------

namespace {
/// AGET pulse without sine (the shape fitted by the process) of amplitude 6000 over a baseline 250 with its
/// maximum at bin 200 and width 10 bins, clipped at 4095; +-1 noise in the bins before 150 (baseline sigma)
std::vector<short> SaturatedAgetPulse(double amplitude = 6000) {
    std::vector<short> s(512);
    const double norm = TRestPulseShapeAnalysis::AgetShape(1.0, false);
    for (int b = 0; b < 512; b++) {
        double value = (b < 150) ? 250 + (b % 2 == 0 ? 1 : -1) : 250;
        const double u = (b - 200) / 10.0 + 1.0;
        if (u > 0) value += amplitude * TRestPulseShapeAnalysis::AgetShape(u, false) / norm;
        s[b] = static_cast<short>(std::min(4095L, std::lround(value)));
    }
    return s;
}
}  // namespace

TEST(RawRecoverSaturation, RecoversClippedPulse) {
    auto process = MakeProcess<TRestRawSignalRecoverSaturationProcess>(
        "recSat", "observables: [SignalsSaturated, SignalsRecovered, AddedAmplitude]");
    ObservableRun obs;
    obs.Init(*process);

    TRestRawSignalEvent input, output;
    input.AddSignal(1, SaturatedAgetPulse());
    ASSERT_TRUE(input.GetSignal(0).IsADCSaturation());

    ASSERT_TRUE(process->ProcessEvent(input, output));
    EXPECT_EQ(obs.Get<int>("recSat_SignalsSaturated"), 1);
    EXPECT_EQ(obs.Get<int>("recSat_SignalsRecovered"), 1);
    EXPECT_GT(obs.Get<double>("recSat_AddedAmplitude"), 0);

    const auto data = output.GetSignal(0).GetData();
    const short maximum = *std::max_element(data.begin(), data.end());
    // True maximum 250 + 6000
    EXPECT_GT(maximum, 5800);
    EXPECT_LT(maximum, 6700);
    // Only the saturated bins change
    const auto original = input.GetSignal(0).GetData();
    for (int b = 0; b < 512; b++) {
        if (original[b] < 4095) {
            ASSERT_EQ(data[b], original[b]) << "bin " << b;
        }
    }
}

TEST(RawRecoverSaturation, NotSaturatedSignalUnchanged) {
    auto process = MakeProcess<TRestRawSignalRecoverSaturationProcess>(
        "recSat", "observables: [SignalsSaturated, SignalsRecovered]");
    ObservableRun obs;
    obs.Init(*process);

    TRestRawSignalEvent input, output;
    input.AddSignal(1, SaturatedAgetPulse(1000));  // maximum 1250, below 4095
    ASSERT_TRUE(process->ProcessEvent(input, output));
    EXPECT_EQ(obs.Get<int>("recSat_SignalsSaturated"), 0);
    EXPECT_EQ(obs.Get<int>("recSat_SignalsRecovered"), 0);
    EXPECT_EQ(output.GetSignal(0).GetData(), input.GetSignal(0).GetData());
}

// ---------------------------------------------------------------------------------------------------------
// TRestRawSignalShapingProcess
// ---------------------------------------------------------------------------------------------------------

TEST(RawShaping, GaussianConservesIntegral) {
    auto process =
        MakeProcess<TRestRawSignalShapingProcess>("shaping", "shapingType: gaus\nshapingTime: 5\ngain: 1");
    process->Initialize();

    auto samples = Flat(256, 0);
    samples[100] = 1000;
    TRestRawSignalEvent input, output;
    input.AddSignal(1, samples);

    ASSERT_TRUE(process->ProcessEvent(input, output));
    const auto data = output.GetSignal(0).GetData();
    ASSERT_EQ(data.size(), 256u);
    const int maxBin = std::max_element(data.begin(), data.end()) - data.begin();
    EXPECT_EQ(maxBin, 100);
    // Peak of a normalized gaussian of sigma 5: 1000 / (sqrt(2 pi) 5) ~ 79.8
    EXPECT_NEAR(data[100], 79.8, 1.5);
    EXPECT_EQ(data[95], data[105]);
    EXPECT_NEAR(std::accumulate(data.begin(), data.end(), 0.0), 1000, 20);
    EXPECT_EQ(data[50], 0);
}

TEST(RawShaping, ExponentialIsCausalAndKeepsBaseline) {
    auto process = MakeProcess<TRestRawSignalShapingProcess>(
        "shaping", "shapingType: Exponential\nshapingTime: 4\nbaselineRange: [5, 55]");
    EXPECT_EQ(process->fShapingType, "exponential");  // normalized option
    process->Initialize();

    auto samples = Flat(256, 300);
    samples[100] = 1300;
    TRestRawSignalEvent input, output;
    input.AddSignal(1, samples);

    ASSERT_TRUE(process->ProcessEvent(input, output));
    const auto data = output.GetSignal(0).GetData();
    EXPECT_EQ(data[0], 300);
    EXPECT_EQ(data[99], 300);
    EXPECT_GT(data[100], 300);
    EXPECT_EQ(std::max_element(data.begin(), data.end()) - data.begin(), 100);
    EXPECT_GT(data[100], data[101]);
    EXPECT_EQ(data[255], 300);
}

TEST(RawShaping, InvalidTypeThrows) {
    EXPECT_ANY_THROW(MakeProcess<TRestRawSignalShapingProcess>("shaping", "shapingType: triangle"));
    auto process = MakeProcess<TRestRawSignalShapingProcess>("shaping", "shapingType: gaus\nshapingTime: 0");
    EXPECT_THROW(process->Initialize(), std::runtime_error);
}

// ---------------------------------------------------------------------------------------------------------
// TRestRawSignalAddNoiseProcess
// ---------------------------------------------------------------------------------------------------------

TEST(RawAddNoise, WhiteNoiseStatistics) {
    auto process =
        MakeProcess<TRestRawSignalAddNoiseProcess>("noise", "noise:\n  - method: white\n    amplitude: 10\n");
    ASSERT_EQ(process->fNoiseParams.size(), 1u);
    process->Initialize();

    gRandom->SetSeed(12345);
    TRestRawSignalEvent input, output;
    for (int id = 0; id < 4; id++) input.AddSignal(id, Flat(4096, 1000));
    ASSERT_TRUE(process->ProcessEvent(input, output));

    std::vector<short> all;
    for (int s = 0; s < 4; s++) {
        const auto data = output.GetSignal(s).GetData();
        all.insert(all.end(), data.begin(), data.end());
    }
    EXPECT_NEAR(Mean(all), 1000, 0.5);
    // Truncated to short: slightly below 10
    EXPECT_GT(StdDev(all), 9.0);
    EXPECT_LT(StdDev(all), 10.5);
    // Independent noise in each signal
    EXPECT_NE(output.GetSignal(0).GetData(), output.GetSignal(1).GetData());
}

TEST(RawAddNoise, ReproducibleWithSeed) {
    auto process = MakeProcess<TRestRawSignalAddNoiseProcess>(
        "noise",
        "noise:\n  - method: white\n    amplitude: 15\n  - method: sin\n    amplitude: 50\n    period: 20\n");
    process->Initialize();

    TRestRawSignalEvent input, first, second;
    input.AddSignal(1, Flat(512, 500));
    input.AddSignal(2, Flat(512, 500));

    gRandom->SetSeed(777);
    ASSERT_TRUE(process->ProcessEvent(input, first));
    gRandom->SetSeed(777);
    ASSERT_TRUE(process->ProcessEvent(input, second));
    for (int s = 0; s < 2; s++) EXPECT_EQ(first.GetSignal(s).GetData(), second.GetSignal(s).GetData());
    EXPECT_NE(first.GetSignal(0).GetData(), Flat(512, 500));
}

TEST(RawAddNoise, SinNoiseBounded) {
    auto process = MakeProcess<TRestRawSignalAddNoiseProcess>(
        "noise", "noise:\n  - method: sinusoidal\n    amplitude: 50\n    period: 20\n");
    process->Initialize();

    gRandom->SetSeed(4321);
    TRestRawSignalEvent input, output;
    input.AddSignal(1, Flat(1024, 1000));
    ASSERT_TRUE(process->ProcessEvent(input, output));
    for (short v : output.GetSignal(0).GetData()) {
        ASSERT_LE(std::abs(v - 1000), 50);
    }
}

TEST(RawAddNoise, NoNoiseAndInvalidMethod) {
    auto process =
        MakeProcess<TRestRawSignalAddNoiseProcess>("noise", "class: TRestRawSignalAddNoiseProcess");
    EXPECT_TRUE(process->fNoiseParams.empty());
    process->Initialize();
    TRestRawSignalEvent input, output;
    input.AddSignal(1, Flat(64, 7));
    ASSERT_TRUE(process->ProcessEvent(input, output));
    EXPECT_EQ(output.GetSignal(0).GetData(), Flat(64, 7));

    EXPECT_ANY_THROW(
        MakeProcess<TRestRawSignalAddNoiseProcess>("noise", "noise:\n  - method: pink\n    amplitude: 1\n"));
}

// ---------------------------------------------------------------------------------------------------------
// TRestRawSignalAnalysisProcess
// ---------------------------------------------------------------------------------------------------------

TEST(RawSignalAnalysis, ObservablesOfSyntheticPulse) {
    auto process = MakeProcess<TRestRawSignalAnalysisProcess>(
        "rawAna",
        "baselineRange: [5, 55]\nintegralRange: [10, 500]\nmaxOption: onlyMax\n"
        "observables: [NSignals, NGoodSignals, BaselineAvg, MaxPeakAmplitude, MaxPeakTime, "
        "ThresholdIntegralSum, FullIntegralSum, TimeBinLength, Baseline, AmpSgnMaxMethod, SignalsID]");
    ObservableRun obs;
    obs.Init(*process);

    TRestRawSignalEvent input, output;
    input.SetID(11);
    input.AddSignal(1, TrianglePulse());
    input.AddSignal(2, Alternating(512, 250));

    ASSERT_TRUE(process->ProcessEvent(input, output));
    // The event passes unchanged
    EXPECT_EQ(output.GetNumberOfSignals(), 2);
    EXPECT_EQ(output.GetID(), 11);
    EXPECT_EQ(output.GetSignalByID(1).GetData(), TrianglePulse());

    EXPECT_EQ(obs.Get<int>("rawAna_NSignals"), 2);
    EXPECT_EQ(obs.Get<int>("rawAna_NGoodSignals"), 1);
    EXPECT_EQ(obs.Get<int>("rawAna_TimeBinLength"), 512);
    EXPECT_NEAR(obs.Get<double>("rawAna_BaselineAvg"), 250, 1e-9);
    EXPECT_NEAR(obs.Get<double>("rawAna_MaxPeakAmplitude"), 1000, 1e-6);
    EXPECT_NEAR(obs.Get<double>("rawAna_MaxPeakTime"), 200, 1e-6);
    // Pulse points: 1000 + 2 * (950 + 900 + ... + 50)
    EXPECT_NEAR(obs.Get<double>("rawAna_ThresholdIntegralSum"), 20000, 1e-3);
    // Plus the alternating noise of [10, 500) outside the pulse
    EXPECT_NEAR(obs.Get<double>("rawAna_FullIntegralSum"), 20000, 2);

    const auto& ids = obs.Get<std::vector<int>>("rawAna_SignalsID");
    EXPECT_EQ(ids, (std::vector<int>{1, 2}));
    const auto& baselines = obs.Get<std::vector<double>>("rawAna_Baseline");
    ASSERT_EQ(baselines.size(), 2u);
    EXPECT_NEAR(baselines[0], 250, 1e-9);
    const auto& amplitudes = obs.Get<std::vector<double>>("rawAna_AmpSgnMaxMethod");
    ASSERT_EQ(amplitudes.size(), 2u);
    EXPECT_NEAR(amplitudes[0], 1000, 1e-6);
    EXPECT_EQ(amplitudes[1], 0);  // no points over threshold
}

TEST(RawSignalAnalysis, EmptyEventHasNeutralValues) {
    auto process = MakeProcess<TRestRawSignalAnalysisProcess>(
        "rawAna", "observables: [NSignals, MinPeakAmplitude, MinPeakTime, MinEventValue]");
    ObservableRun obs;
    obs.Init(*process);

    TRestRawSignalEvent input, output;
    ASSERT_TRUE(process->ProcessEvent(input, output));
    EXPECT_EQ(obs.Get<int>("rawAna_NSignals"), 0);
    EXPECT_EQ(obs.Get<double>("rawAna_MinPeakAmplitude"), 0);
    EXPECT_EQ(obs.Get<double>("rawAna_MinPeakTime"), 0);
    EXPECT_EQ(obs.Get<double>("rawAna_MinEventValue"), 0);
}

TEST(RawSignalAnalysis, InvalidConfigurationThrows) {
    EXPECT_ANY_THROW(MakeProcess<TRestRawSignalAnalysisProcess>("rawAna", "maxOption: secondMax"));
    EXPECT_ANY_THROW(MakeProcess<TRestRawSignalAnalysisProcess>("rawAna", "baselineOption: median"));

    // An observable the process does not offer
    auto process = MakeProcess<TRestRawSignalAnalysisProcess>("rawAna", "observables: [NSignalz]");
    EXPECT_THROW(process->Initialize(), std::runtime_error);
}

// A copied event (copy constructor and assignment) has its own storage: changing the original after the
// copy does not change the copy (its views do not point to the storage of the original)
TEST(RawSignalEvent, CopyHasItsOwnStorage) {
    TRestRawSignalEvent original;
    original.AddSignal(3, std::vector<short>{1, 2, 3});
    original.GetSignal(0);  // the views are built before the copy
    TRestRawSignalEvent copy(original);
    TRestRawSignalEvent assigned;
    assigned = original;
    original.Initialize();
    original.AddSignal(9, std::vector<short>{7, 8, 9});
    original.GetSignal(0);
    for (auto* event : {&copy, &assigned}) {
        ASSERT_EQ(event->GetNumberOfSignals(), 1);
        EXPECT_EQ(event->GetSignal(0).GetSignalID(), 3);
        EXPECT_EQ(event->GetSignal(0).GetData(), (std::vector<short>{1, 2, 3}));
    }
}
