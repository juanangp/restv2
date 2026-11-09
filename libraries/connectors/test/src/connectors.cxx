/// Unit tests of libraries/connectors (GoogleTest, BUILD_TESTING): the conversions between raw signals,
/// detector signals, hits and tracks on events built in memory, with the processes configured from YAML
/// strings and without readout (the readout is optional in these processes). The observables are read from
/// an analysis tree in memory bound to a TRestRun without files (TRestRun::fOutputAnalysisTree).

#include <TRestDetectorHitsEvent.h>
#include <TRestDetectorHitsToTrackProcess.h>
#include <TRestDetectorSignalEvent.h>
#include <TRestDetectorSignalToRawSignalProcess.h>
#include <TRestRawSignalEvent.h>
#include <TRestRawToDetectorSignalProcess.h>
#include <TRestRun.h>
#include <TRestTrackEvent.h>
#include <TTree.h>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
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

/// Alternating baseline 250 (+-1) with a triangular pulse of height 1000 at bin 200 (bins 181-219)
std::vector<short> TrianglePulse(int n = 512) {
    std::vector<short> s(n);
    for (int b = 0; b < n; b++) s[b] = static_cast<short>(250 + (b % 2 == 0 ? 1 : -1));
    for (int b = 181; b <= 219; b++) s[b] = static_cast<short>(250 + 1000 - 50 * std::abs(b - 200));
    return s;
}

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

}  // namespace

// ---------------------------------------------------------------------------------------------------------
// TRestRawToDetectorSignalProcess
// ---------------------------------------------------------------------------------------------------------

TEST(RawToDetectorSignal, PointsOverThresholdWithoutZeroSuppression) {
    auto process = MakeProcess<TRestRawToDetectorSignalProcess>(
        "rawToSignal",
        "sampling: 0.5\ntriggerStarts: 1\ngain: 2\nthreshold: 0.1\n"
        "observables: [NSignalsRejected, SignalsID, SignalsMaxAmplitude]");
    EXPECT_EQ(process->GetInputEvent(), "TRestRawSignalEvent");
    EXPECT_EQ(process->GetOutputEvent(), "TRestDetectorSignalEvent");
    ObservableRun obs;
    obs.Init(*process);

    TRestRawSignalEvent input;
    input.SetID(7);
    input.AddSignal(3, {0, 5, 0, 10});
    input.AddSignal(4, {0, 0, 0, 0});  // no points: rejected
    TRestDetectorSignalEvent output;

    ASSERT_TRUE(process->ProcessEvent(input, output));
    EXPECT_EQ(output.GetID(), 7);
    ASSERT_EQ(output.GetNumberOfSignals(), 1);
    const auto& signal = output.GetSignal(0);
    EXPECT_EQ(signal.GetSignalID(), 3);
    ASSERT_EQ(signal.GetNPoints(), 2);
    EXPECT_DOUBLE_EQ(signal.GetPoint(0).first, 1.5);
    EXPECT_DOUBLE_EQ(signal.GetPoint(0).second, 10);
    EXPECT_DOUBLE_EQ(signal.GetPoint(1).first, 2.5);
    EXPECT_DOUBLE_EQ(signal.GetPoint(1).second, 20);

    EXPECT_EQ(obs.Get<int>("rawToSignal_NSignalsRejected"), 1);
    EXPECT_EQ(obs.Get<std::vector<int>>("rawToSignal_SignalsID"), (std::vector<int>{3}));
    EXPECT_EQ(obs.Get<std::vector<double>>("rawToSignal_SignalsMaxAmplitude"), (std::vector<double>{20}));
}

TEST(RawToDetectorSignal, ZeroSuppression) {
    auto process = MakeProcess<TRestRawToDetectorSignalProcess>(
        "rawToSignal",
        "sampling: 1\nzeroSuppression: true\nbaselineRange: [5, 55]\nintegralRange: [100, 500]\n"
        "pointsThreshold: 3\nsignalThreshold: 5\npointsOverThreshold: 5\n");
    process->Initialize();

    TRestRawSignalEvent input;
    input.AddSignal(1, TrianglePulse());
    TRestDetectorSignalEvent output;

    ASSERT_TRUE(process->ProcessEvent(input, output));
    ASSERT_EQ(output.GetNumberOfSignals(), 1);
    const auto points = output.GetSignal(0).GetPointsVector();
    // Only the pulse (bins 181-219), with the baseline subtracted
    ASSERT_EQ(points.size(), 39u);
    EXPECT_DOUBLE_EQ(points.front().first, 181);
    EXPECT_NEAR(points.front().second, 50, 1e-6);
    EXPECT_DOUBLE_EQ(points[19].first, 200);
    EXPECT_NEAR(points[19].second, 1000, 1e-6);
    EXPECT_DOUBLE_EQ(points.back().first, 219);
}

TEST(RawToDetectorSignal, RejectsEventWithoutPoints) {
    auto process = MakeProcess<TRestRawToDetectorSignalProcess>("rawToSignal", "threshold: 100");
    process->Initialize();

    TRestRawSignalEvent input;
    input.AddSignal(1, {10, 20, 30});
    TRestDetectorSignalEvent output;
    EXPECT_FALSE(process->ProcessEvent(input, output));
    EXPECT_EQ(output.GetNumberOfSignals(), 0);
}

TEST(RawToDetectorSignal, PhysicalIDNeedsReadout) {
    auto process =
        MakeProcess<TRestRawToDetectorSignalProcess>("rawToSignal", "observables: [SignalsPhysicalID]");
    ObservableRun obs;
    EXPECT_THROW(obs.Init(*process), std::runtime_error);
}

// ---------------------------------------------------------------------------------------------------------
// TRestDetectorSignalToRawSignalProcess
// ---------------------------------------------------------------------------------------------------------

namespace {
/// Signal 7 with charges 10 at 1.0 us, 5 at 1.1 us and 20 at 2.0 us (the events are not copied: their
/// views point to their own storage)
void FillChargeEvent(TRestDetectorSignalEvent& event) {
    event.SetID(5);
    event.AddSignal(7, std::vector<double>{1.0, 1.1, 2.0}, std::vector<double>{10, 5, 20});
}
}  // namespace

TEST(DetectorSignalToRaw, IntegrateWithFixedTrigger) {
    auto process = MakeProcess<TRestDetectorSignalToRawSignalProcess>(
        "signalToRaw",
        "sampling: 0.25\nnPoints: 32\nmode: integrate\ntriggerMode: fixed\ntriggerFixedStartTime: 0\n"
        "gain: 2\noffset: 100\nobservables: [TriggerTime, NSaturatedSignals]");
    EXPECT_EQ(process->GetInputEvent(), "TRestDetectorSignalEvent");
    EXPECT_EQ(process->GetOutputEvent(), "TRestRawSignalEvent");
    ObservableRun obs;
    obs.Init(*process);

    TRestDetectorSignalEvent input;
    FillChargeEvent(input);
    TRestRawSignalEvent output;
    ASSERT_TRUE(process->ProcessEvent(input, output));
    EXPECT_EQ(output.GetID(), 5);
    ASSERT_EQ(output.GetNumberOfSignals(), 1);
    EXPECT_EQ(output.GetSignal(0).GetSignalID(), 7);
    const auto data = output.GetSignal(0).GetData();
    ASSERT_EQ(data.size(), 32u);
    for (int b = 0; b < 32; b++) {
        // 1.0 and 1.1 us in the bin 4 [1.0, 1.25), 2.0 us in the bin 8
        const short expected = b == 4 ? 100 + 2 * 15 : b == 8 ? 100 + 2 * 20 : 100;
        EXPECT_EQ(data[b], expected) << "bin " << b;
    }
    EXPECT_DOUBLE_EQ(obs.Get<double>("signalToRaw_TriggerTime"), 0);
    EXPECT_EQ(obs.Get<int>("signalToRaw_NSaturatedSignals"), 0);
}

TEST(DetectorSignalToRaw, FirstDepositTriggerWithDelay) {
    auto process = MakeProcess<TRestDetectorSignalToRawSignalProcess>(
        "signalToRaw",
        "sampling: 0.25\nnPoints: 32\ntriggerMode: firstDeposit\ntriggerDelay: 1\n"
        "observables: [TriggerTime]");
    ObservableRun obs;
    obs.Init(*process);

    TRestDetectorSignalEvent input;
    FillChargeEvent(input);
    TRestRawSignalEvent output;
    ASSERT_TRUE(process->ProcessEvent(input, output));
    // Trigger at the first deposit (1.0 us), the window starts 1 us before it
    EXPECT_DOUBLE_EQ(obs.Get<double>("signalToRaw_TriggerTime"), 1.0);
    const auto data = output.GetSignal(0).GetData();
    EXPECT_EQ(data[4], 15);
    EXPECT_EQ(data[8], 20);
    EXPECT_EQ(data[0], 0);
}

TEST(DetectorSignalToRaw, SaturationAtMaxValue) {
    auto process = MakeProcess<TRestDetectorSignalToRawSignalProcess>(
        "signalToRaw",
        "sampling: 0.25\nnPoints: 32\ntriggerMode: fixed\ngain: 10\nmaxValue: 120\n"
        "observables: [NSaturatedSignals]");
    ObservableRun obs;
    obs.Init(*process);

    TRestDetectorSignalEvent input;
    FillChargeEvent(input);
    input.AddSignal(8, std::vector<double>{3.0}, std::vector<double>{1});  // 10 ADC, not saturated
    TRestRawSignalEvent output;
    ASSERT_TRUE(process->ProcessEvent(input, output));
    EXPECT_EQ(obs.Get<int>("signalToRaw_NSaturatedSignals"), 1);
    EXPECT_EQ(output.GetSignalByID(7).GetPoint(4), 120);  // 150 clipped
    EXPECT_EQ(output.GetSignalByID(7).GetPoint(8), 120);  // 200 clipped
    EXPECT_EQ(output.GetSignalByID(8).GetPoint(12), 10);
}

TEST(DetectorSignalToRaw, SampleModeInterpolates) {
    auto process = MakeProcess<TRestDetectorSignalToRawSignalProcess>(
        "signalToRaw", "sampling: 0.25\nnPoints: 16\nmode: Sample\ntriggerMode: fixed");
    EXPECT_EQ(process->fMode, "sample");  // normalized option
    process->Initialize();

    TRestDetectorSignalEvent input;
    input.AddSignal(1, std::vector<double>{0.0, 1.0, 2.0}, std::vector<double>{0, 100, 0});
    TRestRawSignalEvent output;
    ASSERT_TRUE(process->ProcessEvent(input, output));
    const auto data = output.GetSignal(0).GetData();
    EXPECT_EQ(data[0], 0);
    EXPECT_EQ(data[1], 25);
    EXPECT_EQ(data[2], 50);
    EXPECT_EQ(data[4], 100);
    EXPECT_EQ(data[6], 50);
    EXPECT_EQ(data[8], 0);
    EXPECT_EQ(data[12], 0);  // after the last point
}

TEST(DetectorSignalToRaw, CalibrationRange) {
    // Energy 0 at the middle of the ADC range, 10 at its maximum
    auto process = MakeProcess<TRestDetectorSignalToRawSignalProcess>(
        "signalToRaw",
        "sampling: 1\nnPoints: 4\ntriggerMode: fixed\ncalibrationEnergy: [0, 10]\n"
        "calibrationRange: [0.5, 1]");
    process->Initialize();

    TRestDetectorSignalEvent input;
    input.AddSignal(1, std::vector<double>{1.5}, std::vector<double>{10});
    TRestRawSignalEvent output;
    ASSERT_TRUE(process->ProcessEvent(input, output));
    const auto data = output.GetSignal(0).GetData();
    // offset -32768 + 0.5 * 65535 = -0.5, rounded away from zero
    EXPECT_EQ(data[0], -1);
    EXPECT_EQ(data[1], 32767);
}

TEST(DetectorSignalToRaw, InvalidConfigurationThrows) {
    // sampling is mandatory
    auto noSampling = MakeProcess<TRestDetectorSignalToRawSignalProcess>("signalToRaw", "nPoints: 512");
    EXPECT_THROW(noSampling->Initialize(), std::runtime_error);

    auto observableWithoutMode = MakeProcess<TRestDetectorSignalToRawSignalProcess>(
        "signalToRaw", "sampling: 0.1\ntriggerObservable: g4Ana_gas_FirstHitTime");
    EXPECT_THROW(observableWithoutMode->Initialize(), std::runtime_error);

    auto noThreshold = MakeProcess<TRestDetectorSignalToRawSignalProcess>(
        "signalToRaw", "sampling: 0.1\ntriggerMode: integralThreshold");
    EXPECT_THROW(noThreshold->Initialize(), std::runtime_error);

    EXPECT_ANY_THROW(
        MakeProcess<TRestDetectorSignalToRawSignalProcess>("signalToRaw", "sampling: 0.1\nmode: average"));
}

TEST(DetectorSignalToRaw, RejectsEventWithoutSignalsOrTrigger) {
    auto process = MakeProcess<TRestDetectorSignalToRawSignalProcess>(
        "signalToRaw", "sampling: 0.1\ntriggerMode: integralThreshold\nintegralThreshold: 100");
    process->Initialize();

    TRestDetectorSignalEvent empty;
    TRestRawSignalEvent output;
    EXPECT_FALSE(process->ProcessEvent(empty, output));

    // Charge 35 in total, below the threshold 100
    TRestDetectorSignalEvent lowCharge;
    FillChargeEvent(lowCharge);
    EXPECT_FALSE(process->ProcessEvent(lowCharge, output));
}

TEST(DetectorSignalToRaw, RoundTripWithRawToDetectorSignal) {
    auto toRaw = MakeProcess<TRestDetectorSignalToRawSignalProcess>(
        "signalToRaw", "sampling: 0.25\nnPoints: 32\ntriggerMode: fixed");
    toRaw->Initialize();
    auto back = MakeProcess<TRestRawToDetectorSignalProcess>("rawToSignal", "sampling: 0.25\nthreshold: 0.5");
    back->Initialize();

    TRestDetectorSignalEvent input;
    input.AddSignal(2, std::vector<double>{1.0, 2.0}, std::vector<double>{10, 20});
    TRestRawSignalEvent raw;
    ASSERT_TRUE(toRaw->ProcessEvent(input, raw));
    TRestDetectorSignalEvent output;
    ASSERT_TRUE(back->ProcessEvent(raw, output));

    ASSERT_EQ(output.GetNumberOfSignals(), 1);
    EXPECT_EQ(output.GetSignal(0).GetSignalID(), 2);
    EXPECT_EQ(output.GetSignal(0).GetTime(), (std::vector<double>{1.0, 2.0}));
    EXPECT_EQ(output.GetSignal(0).GetCharge(), (std::vector<double>{10, 20}));
}

// ---------------------------------------------------------------------------------------------------------
// TRestDetectorHitsToTrackProcess
// ---------------------------------------------------------------------------------------------------------

namespace {
/// Two groups of XYZ hits: three at x = 0, 1, 2 (energies 1, 2, 3) and two at x = 20, 21 (energies 5, 5)
void FillTwoClusters(TRestDetectorHitsEvent& event) {
    event.SetID(9);
    event.AddHit(0, 0, 0, 1);
    event.AddHit(20, 0, 0, 5);
    event.AddHit(1, 0, 0, 2);
    event.AddHit(21, 0, 0, 5);
    event.AddHit(2, 0, 0, 3);
}
}  // namespace

TEST(HitsToTrack, GroupsHitsByDistance) {
    auto process = MakeProcess<TRestDetectorHitsToTrackProcess>("hitsToTrack", "clusterDistance: 3");
    EXPECT_EQ(process->GetInputEvent(), "TRestDetectorHitsEvent");
    EXPECT_EQ(process->GetOutputEvent(), "TRestTrackEvent");
    process->Initialize();

    TRestDetectorHitsEvent input;
    FillTwoClusters(input);
    TRestTrackEvent output;
    ASSERT_TRUE(process->ProcessEvent(input, output));
    EXPECT_EQ(output.GetID(), 9);
    ASSERT_EQ(output.GetNumberOfTracks(), 2);
    // Tracks in the order of their first hit
    EXPECT_EQ(output.GetNumberOfHits(0), 3);
    EXPECT_NEAR(output.GetTrackEnergy(0), 6, 1e-5);
    EXPECT_EQ(output.GetNumberOfHits(1), 2);
    EXPECT_NEAR(output.GetTrackEnergy(1), 10, 1e-5);
    EXPECT_NEAR(output.GetEnergy(), 16, 1e-5);
    EXPECT_EQ(output.GetTotalHits(), 5);
    EXPECT_TRUE(output.isXYZ());

    // A larger distance joins everything
    auto joined = MakeProcess<TRestDetectorHitsToTrackProcess>("hitsToTrack", "clusterDistance: 20");
    joined->Initialize();
    TRestTrackEvent oneTrack;
    ASSERT_TRUE(joined->ProcessEvent(input, oneTrack));
    ASSERT_EQ(oneTrack.GetNumberOfTracks(), 1);
    EXPECT_EQ(oneTrack.GetNumberOfHits(0), 5);
}

TEST(HitsToTrack, ProjectionsGiveSeparateTracks) {
    auto process = MakeProcess<TRestDetectorHitsToTrackProcess>("hitsToTrack", "clusterDistance: 3");
    process->Initialize();

    TRestDetectorHitsEvent input;
    input.AddHit(0, kNaN, 0, 1, 0, TRestHitsData::XZ);
    input.AddHit(1, kNaN, 0, 1, 0, TRestHitsData::XZ);
    input.AddHit(kNaN, 0, 0, 2, 0, TRestHitsData::YZ);
    input.AddHit(kNaN, 1, 0, 2, 0, TRestHitsData::YZ);
    TRestTrackEvent output;

    ASSERT_TRUE(process->ProcessEvent(input, output));
    ASSERT_EQ(output.GetNumberOfTracks(), 2);
    // The hits of each type separately, XZ (type 10) before YZ (type 15)
    EXPECT_TRUE(output.isXZ(0));
    EXPECT_TRUE(output.isYZ(1));
    EXPECT_NEAR(output.GetTrackEnergy(0), 2, 1e-5);
    EXPECT_NEAR(output.GetTrackEnergy(1), 4, 1e-5);
    EXPECT_EQ(output.GetNumberOfTracks("X"), 1);
    EXPECT_EQ(output.GetNumberOfTracks("Y"), 1);
}

TEST(HitsToTrack, AttachesIsolatedLowEnergyHits) {
    TRestDetectorHitsEvent input;
    FillTwoClusters(input);
    input.AddHit(5.5, 0, 0, 0.1);  // 3.5 mm from the first group: isolated with clusterDistance 3

    auto without = MakeProcess<TRestDetectorHitsToTrackProcess>("hitsToTrack", "clusterDistance: 3");
    without->Initialize();
    TRestTrackEvent separate;
    ASSERT_TRUE(without->ProcessEvent(input, separate));
    EXPECT_EQ(separate.GetNumberOfTracks(), 3);

    auto with = MakeProcess<TRestDetectorHitsToTrackProcess>(
        "hitsToTrack", "clusterDistance: 3\nattachEnergy: 1\nattachDistance: [4, 4, 10]");
    with->Initialize();
    TRestTrackEvent attached;
    ASSERT_TRUE(with->ProcessEvent(input, attached));
    ASSERT_EQ(attached.GetNumberOfTracks(), 2);
    EXPECT_EQ(attached.GetNumberOfHits(0), 4);
    EXPECT_NEAR(attached.GetTrackEnergy(0), 6.1, 1e-5);
    EXPECT_EQ(attached.GetNumberOfHits(1), 2);
    with->EndProcess();
}

TEST(HitsToTrack, EmptyEventAndInvalidConfiguration) {
    auto process = MakeProcess<TRestDetectorHitsToTrackProcess>("hitsToTrack", "clusterDistance: 3");
    process->Initialize();
    TRestDetectorHitsEvent empty;
    TRestTrackEvent output;
    EXPECT_FALSE(process->ProcessEvent(empty, output));
    EXPECT_EQ(output.GetNumberOfTracks(), 0);

    auto negative = MakeProcess<TRestDetectorHitsToTrackProcess>(
        "hitsToTrack", "attachEnergy: 1\nattachDistance: [-1, 3, 10]");
    EXPECT_THROW(negative->Initialize(), std::runtime_error);
}
