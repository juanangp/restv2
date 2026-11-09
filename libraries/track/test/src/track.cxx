/// Unit tests of the track library (GoogleTest, BUILD_TESTING): TRestTrackEvent and TRestTrack built by hand,
/// and the reduction, path minimization, linearization and analysis processes on tracks of known geometry
/// (straight lines of hits with a known length and direction). The observables are read from an in-memory
/// analysis tree given to the process through a TRestRun.

#include <TRestRun.h>
#include <TRestTrack.h>
#include <TRestTrackAnalysisProcess.h>
#include <TRestTrackBlobAnalysisProcess.h>
#include <TRestTrackEvent.h>
#include <TRestTrackLineAnalysisProcess.h>
#include <TRestTrackLinearizationProcess.h>
#include <TRestTrackPathMinimizationProcess.h>
#include <TRestTrackReductionProcess.h>
#include <TTree.h>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using ROOT::Math::XYZVector;

namespace {

const double kNaN = std::numeric_limits<double>::quiet_NaN();
using HitType = TRestHitsData::REST_HitType;

/// Analysis tree in memory (no file) given to the processes through a TRestRun, to read their observables
struct AnalysisTree {
    TRestRun run;
    std::unique_ptr<TTree> tree;

    AnalysisTree() : tree(std::make_unique<TTree>("AnalysisTree", "AnalysisTree")) {
        tree->SetDirectory(nullptr);
        run.fOutputAnalysisTree = tree.get();
    }
    ~AnalysisTree() { run.fOutputAnalysisTree = nullptr; }

    /// Current value of the observable processName_name
    template <typename T>
    T Get(const std::string& name) const {
        return run.GetObservable<T>(name);
    }
};

/// Adds a track with the hits start + i * step (i = 0 ... n - 1), the energy of each one given by energy(i)
void AddLine(TRestTrackEvent& event, int trackID, int n, const XYZVector& start, const XYZVector& step,
             const std::function<double(int)>& energy, HitType type = HitType::XYZ) {
    event.AddTrack(trackID, 0);
    for (int i = 0; i < n; i++) {
        XYZVector position = start + double(i) * step;
        if (!TRestHitsData::IsDefined(type, 0)) position.SetX(kNaN);
        if (!TRestHitsData::IsDefined(type, 1)) position.SetY(kNaN);
        if (!TRestHitsData::IsDefined(type, 2)) position.SetZ(kNaN);
        event.AddHitToTrack(position, energy(i), 0, type);
    }
}

double Constant(int) { return 1; }

/// Distance from a point to the line through the origin along direction
double DistanceToLine(const XYZVector& point, const XYZVector& direction) {
    return point.Cross(direction.Unit()).R();
}

}  // namespace

// ============================================================================
// Event
// ============================================================================

TEST(TrackEvent, TracksAndGetters) {
    TRestTrackEvent event;
    event.SetID(4);
    AddLine(event, 1, 11, {0, 0, 0}, {1, 0, 0}, [](int) { return 2.; });
    AddLine(event, 2, 3, {0, 5, 0}, {0, 0, 1}, Constant);
    AddLine(event, 3, 4, {0, 0, 1}, {1, 0, 0}, Constant, HitType::XZ);
    AddLine(event, 4, 2, {0, 0, 1}, {0, 1, 0}, [](int) { return 5.; }, HitType::YZ);

    ASSERT_EQ(event.GetNumberOfTracks(), 4);
    EXPECT_EQ(event.GetTotalHits(), 20);
    EXPECT_EQ(event.GetNumberOfTracks("X"), 1);
    EXPECT_EQ(event.GetNumberOfTracks("Y"), 1);
    EXPECT_EQ(event.GetNumberOfTracks("XYZ"), 2);
    EXPECT_EQ(event.GetNumberOfTracks(""), 4);
    EXPECT_FALSE(event.isXYZ());
    EXPECT_TRUE(event.isXZ(2));
    EXPECT_TRUE(event.isYZ(3));

    EXPECT_NEAR(event.GetEnergy(), 22 + 3 + 4 + 10, 1e-6);
    EXPECT_NEAR(event.GetEnergy("X"), 4, 1e-6);
    EXPECT_NEAR(event.GetEnergy("Y"), 10, 1e-6);
    EXPECT_NEAR(event.GetTrackEnergy(0), 22, 1e-6);
    EXPECT_NEAR(event.GetTrackLength(0), 10, 1e-5);
    EXPECT_NEAR(event.GetTrackVolume(0), 100, 1e-4);
    EXPECT_NEAR(event.GetTrackMeanPosition(0).X(), 5, 1e-5);
    // The distances of the XZ track in its plane
    EXPECT_NEAR(event.GetTrackLength(2), 3, 1e-5);

    // Main tracks of each projection ("" is XYZ)
    EXPECT_EQ(event.GetMaxEnergyTrack().GetTrackID(), 1);
    EXPECT_EQ(event.GetSecondMaxEnergyTrack().GetTrackID(), 2);
    EXPECT_EQ(event.GetMaxEnergyTrackInX().GetTrackID(), 3);
    EXPECT_EQ(event.GetMaxEnergyTrackInY().GetTrackID(), 4);
    EXPECT_EQ(event.GetSecondMaxEnergyTrack("X").GetEvent(), nullptr);
    EXPECT_NEAR(event.GetMaxEnergyTrackLength(), 10, 1e-5);
    EXPECT_NEAR(event.GetMaxEnergyTrackVolume(), 100, 1e-4);

    TRestTrack track = event.GetTrackById(2);
    ASSERT_NE(track.GetEvent(), nullptr);
    EXPECT_EQ(track.GetIndex(), 1u);
    EXPECT_EQ(track.GetNumberOfHits(), 3);
    EXPECT_EQ(track.GetParentID(), 0);
    EXPECT_NEAR(track.GetEnergy(), 3, 1e-6);
    EXPECT_NEAR(track.GetLength(), 2, 1e-5);
    EXPECT_NEAR(track.GetHits().GetZ(2), 2, 1e-6);
    EXPECT_NEAR(track.GetHits().GetY(0), 5, 1e-6);

    // Invalid tracks
    EXPECT_EQ(event.GetTrackById(99).GetEvent(), nullptr);
    TRestTrack invalid;
    EXPECT_EQ(invalid.GetTrackID(), -1);
    EXPECT_EQ(invalid.GetNumberOfHits(), 0);
    EXPECT_DOUBLE_EQ(invalid.GetEnergy(), 0);

    TRestTrackEvent copy;
    copy.CopyFrom(&event);
    EXPECT_EQ(copy.GetID(), 4);
    EXPECT_EQ(copy.GetNumberOfTracks(), 4);
    EXPECT_NEAR(copy.GetTrackEnergy(3), 10, 1e-6);
}

TEST(TrackEvent, RemoveSortAndMerge) {
    TRestTrackEvent event;
    AddLine(event, 1, 3, {0, 0, 0}, {1, 0, 0}, Constant);
    event.AddTrack(2, 1);
    event.AddHitToTrack({0, 0, 3}, 1, 0, HitType::XYZ, {0.1, 0.2, 0.3});
    event.AddHitToTrack({0, 0, 1}, 3, 0, HitType::XYZ);
    event.AddHitToTrack({0, 0, 2}, 2, 0, HitType::XYZ);

    EXPECT_NEAR(event.GetSigmaY(1, 0), 0.2, 1e-6);
    EXPECT_NEAR(event.GetClusterSize(1, 0), std::sqrt(0.01 + 0.04 + 0.09), 1e-6);
    EXPECT_EQ(event.GetParentID(1), 1);

    // Default order: increasing z; the sigmas follow their hits
    event.SortTrackHits(1);
    TRestTrack track = event.GetTrack(1);
    EXPECT_NEAR(track.GetHits().GetZ(0), 1, 1e-6);
    EXPECT_NEAR(track.GetHits().GetZ(2), 3, 1e-6);
    EXPECT_NEAR(event.GetSigmaZ(1, 2), 0.3, 1e-6);

    // Energy weighted merge of the hits 0 (z = 1, E = 3) and 1 (z = 2, E = 2)
    event.MergeTrackHits(1, 0, 1);
    ASSERT_EQ(event.GetNumberOfHits(1), 2);
    EXPECT_NEAR(event.GetTrack(1).GetHits().GetZ(0), (3 * 1 + 2 * 2) / 5., 1e-5);
    EXPECT_NEAR(event.GetTrack(1).GetHits().GetEnergy(0), 5, 1e-6);
    EXPECT_NEAR(event.GetTrackEnergy(1), 6, 1e-6);

    event.RemoveTrackHit(0, 0);
    EXPECT_EQ(event.GetNumberOfHits(0), 2);
    EXPECT_NEAR(event.GetTrack(0).GetHits().GetX(0), 1, 1e-6);

    // Removing the first track keeps the hits of the second one
    event.RemoveTrack(0);
    ASSERT_EQ(event.GetNumberOfTracks(), 1);
    EXPECT_EQ(event.GetTrackID(0), 2);
    EXPECT_EQ(event.GetTotalHits(), 2);
    EXPECT_NEAR(event.GetTrackEnergy(0), 6, 1e-6);

    event.RemoveTracks();
    EXPECT_EQ(event.GetNumberOfTracks(), 0);
    EXPECT_EQ(event.GetMaxEnergyTrack().GetEvent(), nullptr);
}

// ============================================================================
// Reconstruction processes
// ============================================================================

TEST(TrackReductionProcess, LineReducedToNodes) {
    for (const std::string kMeans : {"false", "true"}) {
        TRestTrackReductionProcess process(
            "reduction",
            YAML::Load("{startingDistance: 0.5 mm, minimumDistance: 3 mm, distanceStepFactor: 1.5, "
                       "maxNodes: 10, kMeans: " +
                       kMeans + "}"));
        process.Initialize();

        TRestTrackEvent input, output;
        input.SetID(2);
        AddLine(input, 7, 100, {0, 0, 0}, {1, 0, 0}, Constant);
        AddLine(input, 8, 20, {0, 0, 5}, {0.5, 0, 0}, Constant, HitType::XZ);
        ASSERT_TRUE(process.ProcessEvent(input, output));

        EXPECT_EQ(output.GetID(), 2);
        ASSERT_EQ(output.GetNumberOfTracks(), 2);
        EXPECT_EQ(output.GetParentID(0), 7);
        EXPECT_EQ(output.GetParentID(1), 8);
        const int nodes = output.GetNumberOfHits(0);
        EXPECT_GE(nodes, 2) << "kMeans " << kMeans;
        EXPECT_LE(nodes, 10) << "kMeans " << kMeans;
        // The energy is kept and the nodes stay on the line, inside its extent
        EXPECT_NEAR(output.GetTrackEnergy(0), 100, 1e-3);
        const TRestHits hits = output.GetTrack(0).GetHits();
        double sigmaX = 0;
        for (int n = 0; n < nodes; n++) {
            EXPECT_NEAR(hits.GetY(n), 0, 1e-6);
            EXPECT_NEAR(hits.GetZ(n), 0, 1e-6);
            EXPECT_GE(hits.GetX(n), -1e-4);
            EXPECT_LE(hits.GetX(n), 99 + 1e-4);
            sigmaX = std::max(sigmaX, output.GetSigmaX(0, n));
        }
        EXPECT_GT(sigmaX, 0);  // spread of the hits of each node

        // XZ track: the undefined coordinate stays NaN
        EXPECT_NEAR(output.GetTrackEnergy(1), 20, 1e-3);
        EXPECT_TRUE(output.isXZ(1));
        EXPECT_TRUE(std::isnan(output.GetTrack(1).GetHits().GetY(0)));
    }

    TRestTrackReductionProcess bad("reduction", YAML::Load("{distanceStepFactor: 1}"));
    EXPECT_THROW(bad.Initialize(), std::runtime_error);
}

TEST(TrackReductionProcess, MixedTypesKept) {
    TRestTrackReductionProcess process("reduction", YAML::Load("{maxNodes: 2}"));
    process.Initialize();

    TRestTrackEvent input, output;
    input.AddTrack(1, 0);
    input.AddHitToTrack({0, kNaN, 0}, 1, 0, HitType::XZ);
    input.AddHitToTrack({kNaN, 1, 0}, 1, 0, HitType::YZ);
    input.AddHitToTrack({kNaN, 2, 0}, 1, 0, HitType::YZ);
    ASSERT_TRUE(process.ProcessEvent(input, output));
    ASSERT_EQ(output.GetNumberOfTracks(), 1);
    EXPECT_EQ(output.GetNumberOfHits(0), 3);
    EXPECT_EQ(output.GetParentID(0), 1);
}

TEST(TrackPathMinimizationProcess, OrdersShuffledLine) {
    AnalysisTree analysis;
    TRestTrackPathMinimizationProcess process("path", YAML::Load("{observables: [Branches]}"));
    process.SetRunInfo(&analysis.run);
    process.Initialize();

    // A line of 10 hits along X every 2 mm, in a scrambled order
    TRestTrackEvent input, output;
    input.AddTrack(5, 0);
    for (int i : {5, 0, 9, 3, 7, 1, 8, 2, 6, 4}) input.AddHitToTrack({2. * i, 1, 0}, 1 + i, 0, HitType::XYZ);
    EXPECT_GT(input.GetTrackLength(0), 18);
    ASSERT_TRUE(process.ProcessEvent(input, output));

    ASSERT_EQ(output.GetNumberOfTracks(), 1);
    EXPECT_EQ(output.GetParentID(0), 5);
    ASSERT_EQ(output.GetNumberOfHits(0), 10);
    EXPECT_NEAR(output.GetTrackLength(0), 18, 1e-4);
    EXPECT_NEAR(output.GetTrackEnergy(0), input.GetTrackEnergy(0), 1e-4);
    // Monotonic in X (either direction), each hit with its energy
    const TRestHits hits = output.GetTrack(0).GetHits();
    const double sense = hits.GetX(9) > hits.GetX(0) ? 1 : -1;
    for (int n = 1; n < 10; n++) EXPECT_GT(sense * (hits.GetX(n) - hits.GetX(n - 1)), 0);
    for (int n = 0; n < 10; n++) EXPECT_NEAR(hits.GetEnergy(n), 1 + hits.GetX(n) / 2, 1e-5);
    EXPECT_EQ(analysis.Get<int>("path_Branches"), 0);

    // A gap: one jump longer than branchFactor times the median step
    TRestTrackEvent gap, gapOutput;
    gap.AddTrack(1, 0);
    for (double x : {21., 0., 3., 1., 23., 2., 20., 4., 22.})
        gap.AddHitToTrack({x, 0, 0}, 1, 0, HitType::XYZ);
    ASSERT_TRUE(process.ProcessEvent(gap, gapOutput));
    EXPECT_NEAR(gapOutput.GetTrackLength(0), 4 + 16 + 3, 1e-4);
    EXPECT_EQ(analysis.Get<int>("path_Branches"), 1);

    TRestTrackPathMinimizationProcess bad("path", YAML::Load("{branchFactor: 1}"));
    EXPECT_THROW(bad.Initialize(), std::runtime_error);
}

TEST(TrackLinearizationProcess, NodesAlongTheLine) {
    const XYZVector direction(1, 1, 0);
    {
        TRestTrackLinearizationProcess process("linear", YAML::Load("{maxNodes: 6}"));
        process.Initialize();

        // 61 hits from the origin to 30 mm along (1, 1, 0)
        TRestTrackEvent input, output;
        AddLine(input, 3, 61, {0, 0, 0}, 0.5 * direction.Unit(), Constant);
        ASSERT_TRUE(process.ProcessEvent(input, output));
        ASSERT_EQ(output.GetNumberOfTracks(), 1);
        EXPECT_EQ(output.GetParentID(0), 3);
        ASSERT_EQ(output.GetNumberOfHits(0), 6);
        EXPECT_NEAR(output.GetTrackEnergy(0), 61, 1e-3);
        const TRestHits nodes = output.GetTrack(0).GetHits();
        for (int n = 0; n < 6; n++) {
            EXPECT_NEAR(DistanceToLine(nodes.GetPosition(n), direction), 0, 1e-3);
            const double along = nodes.GetPosition(n).Dot(direction.Unit());
            EXPECT_GE(along, -1e-3);
            EXPECT_LE(along, 30 + 1e-3);
        }
    }
    {
        // nodeDistance: length / nodeDistance + 1 nodes
        TRestTrackLinearizationProcess process("linear", YAML::Load("{maxNodes: 20, nodeDistance: 10 mm}"));
        process.Initialize();
        TRestTrackEvent input, output;
        AddLine(input, 3, 61, {0, 0, 0}, 0.5 * direction.Unit(), Constant);
        ASSERT_TRUE(process.ProcessEvent(input, output));
        EXPECT_EQ(output.GetNumberOfHits(0), 4);
    }
    {
        // fixBoundaries: the first and last nodes at the ends of the line, without energy
        TRestTrackLinearizationProcess process("linear", YAML::Load("{maxNodes: 5, fixBoundaries: true}"));
        process.Initialize();
        TRestTrackEvent input, output;
        AddLine(input, 3, 41, {0, 0, 0}, {0, 0, 0.5}, Constant);
        ASSERT_TRUE(process.ProcessEvent(input, output));
        const TRestHits nodes = output.GetTrack(0).GetHits();
        ASSERT_GE(nodes.GetNumberOfHits(), 2u);
        const int last = (int)nodes.GetNumberOfHits() - 1;
        EXPECT_NEAR(std::min(nodes.GetZ(0), nodes.GetZ(last)), 0, 1e-3);
        EXPECT_NEAR(std::max(nodes.GetZ(0), nodes.GetZ(last)), 20, 1e-3);
        EXPECT_NEAR(output.GetTrackEnergy(0), 41, 1e-3);
    }
}

// ============================================================================
// Analysis processes
// ============================================================================

TEST(TrackAnalysisProcess, MainTrackObservables) {
    AnalysisTree analysis;
    TRestTrackAnalysisProcess process("trackAna", YAML::Load("{observables: [all]}"));
    process.SetRunInfo(&analysis.run);
    process.Initialize();

    TRestTrackEvent input, output;
    AddLine(input, 1, 3, {0, 5, 0}, {0, 0, 1}, Constant);
    AddLine(input, 2, 11, {0, 0, 0}, {1, 0, 0}, [](int) { return 2.; });
    ASSERT_TRUE(process.ProcessEvent(input, output));
    EXPECT_EQ(output.GetNumberOfTracks(), 2);  // copied unchanged

    EXPECT_EQ(analysis.Get<int>("trackAna_NTracks"), 2);
    EXPECT_NEAR(analysis.Get<double>("trackAna_TotalEnergy"), 25, 1e-5);
    EXPECT_NEAR(analysis.Get<double>("trackAna_MaxTrackEnergy"), 22, 1e-5);
    EXPECT_EQ(analysis.Get<int>("trackAna_MaxTrackNHits"), 11);
    EXPECT_NEAR(analysis.Get<double>("trackAna_MaxTrackLength"), 10, 1e-4);
    EXPECT_NEAR(analysis.Get<double>("trackAna_MaxTrackVolume"), 100, 1e-3);
    EXPECT_NEAR(analysis.Get<double>("trackAna_MaxTrackMeanX"), 5, 1e-5);
    EXPECT_NEAR(analysis.Get<double>("trackAna_MaxTrackSigmaY"), 0, 1e-5);
    EXPECT_NEAR(analysis.Get<double>("trackAna_MaxTrackSigmaX"), std::sqrt(10.), 1e-4);
    EXPECT_NEAR(analysis.Get<double>("trackAna_MaxTrackEnergyRatio"), 3. / 25, 1e-6);
    // The other tracks
    const auto energies = analysis.Get<std::vector<double>>("trackAna_TracksEnergy");
    ASSERT_EQ(energies.size(), 1u);
    EXPECT_NEAR(energies[0], 3, 1e-5);
    EXPECT_EQ(analysis.Get<std::vector<int>>("trackAna_TracksNHits"), std::vector<int>{3});

    // Cut on the number of tracks
    TRestTrackAnalysisProcess cut("trackAnaCut", YAML::Load("{cutsEnabled: true, nTracksCut: [1, 1]}"));
    cut.Initialize();
    TRestTrackEvent cutOutput;
    EXPECT_FALSE(cut.ProcessEvent(input, cutOutput));
}

TEST(TrackAnalysisProcess, ProjectionObservables) {
    AnalysisTree analysis;
    TRestTrackAnalysisProcess process("trackAna", YAML::Load("{projections: [XZ, YZ], observables: [all]}"));
    process.SetRunInfo(&analysis.run);
    process.Initialize();

    TRestTrackEvent input, output;
    AddLine(input, 1, 5, {0, 0, 10}, {1, 0, 0}, [](int) { return 2.; }, HitType::XZ);  // E = 10
    AddLine(input, 2, 1, {20, 0, 10}, {1, 0, 0}, Constant, HitType::XZ);               // E = 1
    AddLine(input, 3, 3, {0, 0, 10}, {0, 1, 0}, [](int) { return 3.; }, HitType::YZ);  // E = 9
    ASSERT_TRUE(process.ProcessEvent(input, output));

    EXPECT_EQ(analysis.Get<int>("trackAna_NTracks"), 3);
    EXPECT_EQ(analysis.Get<int>("trackAna_NTracks_XZ"), 2);
    EXPECT_EQ(analysis.Get<int>("trackAna_NTracks_YZ"), 1);
    EXPECT_NEAR(analysis.Get<double>("trackAna_MaxTrack_XZ_Energy"), 10, 1e-5);
    EXPECT_EQ(analysis.Get<int>("trackAna_MaxTrack_XZ_NHits"), 5);
    EXPECT_NEAR(analysis.Get<double>("trackAna_MaxTrack_XZ_Length"), 4, 1e-4);
    EXPECT_NEAR(analysis.Get<double>("trackAna_MaxTrack_XZ_MeanX"), 2, 1e-5);
    EXPECT_NEAR(analysis.Get<double>("trackAna_MaxTrack_XZ_EnergyRatio"), 1. / 11, 1e-6);
    EXPECT_NEAR(analysis.Get<double>("trackAna_MaxTrack_YZ_Energy"), 9, 1e-5);
    EXPECT_NEAR(analysis.Get<double>("trackAna_MaxTrack_YZ_MeanY"), 1, 1e-5);
    // The main tracks of both projections together
    EXPECT_NEAR(analysis.Get<double>("trackAna_MaxTrackEnergy"), 19, 1e-5);
    EXPECT_EQ(analysis.Get<int>("trackAna_MaxTrackNHits"), 8);
    EXPECT_NEAR(analysis.Get<double>("trackAna_MaxTrackMeanZ"), 10, 1e-5);
    EXPECT_NEAR(analysis.Get<double>("trackAna_MaxTrackEnergyRatio"), 1. / 20, 1e-6);
    EXPECT_NEAR(analysis.Get<double>("trackAna_EnergyBalance_XZ_YZ"), 1. / 19, 1e-6);
    EXPECT_NEAR(analysis.Get<double>("trackAna_SigmaBalance_XZ_YZ"),
                (std::sqrt(2.) - std::sqrt(2. / 3)) / (std::sqrt(2.) + std::sqrt(2. / 3)), 1e-5);
    // Mean, sigma and gaussian sigma only along the coordinates of the projection
    EXPECT_THROW(analysis.Get<double>("trackAna_MaxTrack_XZ_MeanY"), std::runtime_error);
}

TEST(TrackBlobAnalysisProcess, BlobsAtTheEnds) {
    AnalysisTree analysis;
    TRestTrackBlobAnalysisProcess process(
        "blob", YAML::Load("{hitsToCheckFraction: 0.2, radii: [1.5 mm], observables: [all]}"));
    process.SetRunInfo(&analysis.run);
    process.Initialize();

    // Nodes along z = 1 ... 10; blobs at z = 1 (E = 10) and z = 10 (E = 5)
    TRestTrackEvent input, output;
    AddLine(input, 1, 10, {0, 0, 1}, {0, 0, 1}, [](int i) { return i == 0 ? 10. : i == 9 ? 5. : 1.; });
    ASSERT_TRUE(process.ProcessEvent(input, output));

    EXPECT_NEAR(analysis.Get<double>("blob_Z1"), 1, 1e-6);
    EXPECT_NEAR(analysis.Get<double>("blob_Z2"), 10, 1e-6);
    EXPECT_NEAR(analysis.Get<double>("blob_X1"), 0, 1e-6);
    EXPECT_NEAR(analysis.Get<double>("blob_Distance"), 9, 1e-5);
    const auto q1 = analysis.Get<std::vector<double>>("blob_Q1");
    const auto q2 = analysis.Get<std::vector<double>>("blob_Q2");
    ASSERT_EQ(q1.size(), 1u);
    ASSERT_EQ(q2.size(), 1u);
    EXPECT_NEAR(q1[0], 11, 1e-5);
    EXPECT_NEAR(q2[0], 6, 1e-5);
    EXPECT_NEAR(analysis.Get<std::vector<double>>("blob_QBalance")[0], 5. / 17, 1e-6);
    EXPECT_NEAR(analysis.Get<std::vector<double>>("blob_QRatio")[0], 11. / 6, 1e-6);

    // Without tracks: NaN positions
    TRestTrackEvent empty, emptyOutput;
    ASSERT_TRUE(process.ProcessEvent(empty, emptyOutput));
    EXPECT_TRUE(std::isnan(analysis.Get<double>("blob_Distance")));

    TRestTrackBlobAnalysisProcess bad("blob", YAML::Load("{hitsToCheckFraction: 0.7}"));
    EXPECT_THROW(bad.Initialize(), std::runtime_error);
}

TEST(TrackLineAnalysisProcess, OriginEndAndDirection) {
    AnalysisTree analysis;
    TRestTrackLineAnalysisProcess process("lineAna", YAML::Load("{observables: [all]}"));
    process.SetRunInfo(&analysis.run);
    process.Initialize();

    // Main track: 11 nodes from (0, 0, 0) to (30, 0, 40), the energy growing towards the end (Bragg peak)
    TRestTrackEvent input, output;
    input.SetID(8);
    AddLine(input, 1, 11, {0, 0, 0}, {3, 0, 4}, [](int i) { return 1. + i; });
    AddLine(input, 2, 2, {100, 100, 100}, {1, 0, 0}, [](int) { return 3.; });
    ASSERT_TRUE(process.ProcessEvent(input, output));

    // The output keeps the main track only
    EXPECT_EQ(output.GetID(), 8);
    ASSERT_EQ(output.GetNumberOfTracks(), 1);
    EXPECT_EQ(output.GetParentID(0), 1);

    EXPECT_NEAR(analysis.Get<double>("lineAna_OriginX"), 0, 1e-4);
    EXPECT_NEAR(analysis.Get<double>("lineAna_OriginZ"), 0, 1e-4);
    EXPECT_NEAR(analysis.Get<double>("lineAna_EndX"), 30, 1e-4);
    EXPECT_NEAR(analysis.Get<double>("lineAna_EndY"), 0, 1e-4);
    EXPECT_NEAR(analysis.Get<double>("lineAna_EndZ"), 40, 1e-4);
    EXPECT_NEAR(analysis.Get<double>("lineAna_Length"), 50, 1e-4);
    // origin - end = (-30, 0, -40)
    EXPECT_NEAR(analysis.Get<double>("lineAna_Angle"), std::acos(-0.8), 1e-6);
    EXPECT_NEAR(std::abs(analysis.Get<double>("lineAna_Phi")), M_PI, 1e-6);
    EXPECT_EQ(analysis.Get<int>("lineAna_Downwards"), -1);
    EXPECT_NEAR(analysis.Get<double>("lineAna_Energy"), 66, 1e-4);
    EXPECT_NEAR(analysis.Get<double>("lineAna_TrackBalance"), 66. / 72, 1e-6);
    EXPECT_NEAR(analysis.Get<double>("lineAna_Residual"), 0, 1e-3);
    EXPECT_GT(analysis.Get<double>("lineAna_Asymmetry"), 0);
    EXPECT_LE(analysis.Get<double>("lineAna_Asymmetry"), 1);

    // No tracks: rejected
    TRestTrackEvent empty, emptyOutput;
    EXPECT_FALSE(process.ProcessEvent(empty, emptyOutput));
}

TEST(TrackLineAnalysisProcess, Projections) {
    AnalysisTree analysis;
    TRestTrackLineAnalysisProcess process("lineAna",
                                          YAML::Load("{projections: [XZ, YZ], observables: [all]}"));
    process.SetRunInfo(&analysis.run);
    process.Initialize();

    // The same track seen by X strips (X from 0 to 10) and Y strips (Y from 0 to 20), Z from 0 to 40
    TRestTrackEvent input, output;
    AddLine(input, 1, 11, {0, 0, 0}, {1, 0, 4}, [](int i) { return 1. + i; }, HitType::XZ);
    AddLine(input, 2, 11, {0, 0, 0}, {0, 2, 4}, [](int i) { return 1. + i; }, HitType::YZ);
    ASSERT_TRUE(process.ProcessEvent(input, output));
    EXPECT_EQ(output.GetNumberOfTracks(), 2);

    EXPECT_NEAR(analysis.Get<double>("lineAna_EndX"), 10, 1e-4);
    EXPECT_NEAR(analysis.Get<double>("lineAna_EndY"), 20, 1e-4);
    EXPECT_NEAR(analysis.Get<double>("lineAna_EndZ"), 40, 1e-4);
    EXPECT_NEAR(analysis.Get<double>("lineAna_OriginZ"), 0, 1e-4);
    EXPECT_NEAR(analysis.Get<double>("lineAna_Length"), std::sqrt(100. + 400. + 1600.), 1e-4);
    EXPECT_NEAR(analysis.Get<double>("lineAna_Length_XZ"), std::sqrt(100. + 1600.), 1e-4);
    EXPECT_NEAR(analysis.Get<double>("lineAna_Length_YZ"), std::sqrt(400. + 1600.), 1e-4);
    EXPECT_NEAR(analysis.Get<double>("lineAna_TrackBalance_XZ"), 1, 1e-9);
    EXPECT_NEAR(analysis.Get<double>("lineAna_Energy"), 132, 1e-4);

    // A projection without tracks: rejected
    TRestTrackEvent onlyX, onlyXOutput;
    AddLine(onlyX, 1, 5, {0, 0, 0}, {1, 0, 1}, Constant, HitType::XZ);
    EXPECT_FALSE(process.ProcessEvent(onlyX, onlyXOutput));
}

// A copied event (copy constructor and assignment) has its own storage: changing the original after the
// copy does not change the copy (its track views do not point to the storage of the original)
TEST(TrackEvent, CopyHasItsOwnStorage) {
    TRestTrackEvent original;
    original.AddTrack(1, 0);
    original.AddHitToTrack({1, 2, 3}, 10, 0, TRestHitsData::XYZ);
    TRestTrackEvent copy(original);
    TRestTrackEvent assigned;
    assigned = original;
    original.Initialize();
    original.AddTrack(5, 0);
    original.AddHitToTrack({100, 200, 300}, 1000, 0, TRestHitsData::XYZ);
    for (const auto* event : {&copy, &assigned}) {
        ASSERT_EQ(event->GetNumberOfTracks(), 1);
        const TRestHits hits = event->GetTrack(0).GetHits();
        ASSERT_EQ(hits.GetNumberOfHits(), 1u);
        EXPECT_DOUBLE_EQ(hits.GetX(0), 1);
        EXPECT_DOUBLE_EQ(hits.GetEnergy(0), 10);
    }
}
