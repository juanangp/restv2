/// Unit tests of TRestHits (views over a shared TRestHitsData storage): adding and removing hits (only on
/// the last view of the storage), sorting by energy, merging, energy-weighted statistics by hit type,
/// distances in the plane of 2D hits, positions of 2D hits, and the binning helpers used to draw them
/// (GetMinimumDistance, GetBinning). Everything in memory.

#include <Math/Vector3D.h>
#include <TMath.h>
#include <TRandom.h>
#include <TRestHits.h>
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

using ROOT::Math::XYZVector;

namespace {
const double kNaN = std::numeric_limits<double>::quiet_NaN();
}  // namespace

TEST(Hits, AddAndRemove) {
    TRestHitsData data;
    TRestHits hits(&data);
    EXPECT_EQ(hits.GetNumberOfHits(), 0u);
    hits.AddHit({1, 2, 3}, 10, 100, TRestHitsData::XYZ);
    hits.AddHit({4, 5, 6}, 20, 200, TRestHitsData::XYZ);
    hits.AddHit({7, 8, 9}, 30, 300, TRestHitsData::XYZ);
    ASSERT_EQ(hits.GetNumberOfHits(), 3u);
    EXPECT_EQ(data.x.size(), 3u);
    EXPECT_DOUBLE_EQ(hits.GetX(1), 4);
    EXPECT_DOUBLE_EQ(hits.GetY(1), 5);
    EXPECT_DOUBLE_EQ(hits.GetZ(1), 6);
    EXPECT_DOUBLE_EQ(hits.GetEnergy(2), 30);
    EXPECT_DOUBLE_EQ(hits.GetTime(2), 300);
    EXPECT_EQ(hits.GetType(0), TRestHitsData::XYZ);
    EXPECT_TRUE(hits.areXYZ());
    EXPECT_FALSE(hits.areXY());

    hits.RemoveHit(1);
    ASSERT_EQ(hits.GetNumberOfHits(), 2u);
    EXPECT_DOUBLE_EQ(hits.GetEnergy(1), 30);
    EXPECT_DOUBLE_EQ(hits.GetTotalEnergy(), 40);

    // Hits of another view appended
    TRestHitsData otherData;
    TRestHits other(&otherData);
    other.AddHits(hits);
    EXPECT_EQ(other.GetNumberOfHits(), 2u);
    EXPECT_DOUBLE_EQ(other.GetTotalEnergy(), 40);

    hits.RemoveHits();
    EXPECT_EQ(hits.GetNumberOfHits(), 0u);
    EXPECT_TRUE(data.x.empty());
    EXPECT_FALSE(hits.areXYZ());  // no hits

    other.ClearHits();
    EXPECT_EQ(other.GetNumberOfHits(), 0u);
    EXPECT_TRUE(otherData.energy.empty());
}

TEST(Hits, OnlyLastViewChangesStorage) {
    TRestHitsData data;
    TRestHits all(&data);
    for (int i = 0; i < 5; i++) all.AddHit({double(i), 0, 0}, i + 1., 0, TRestHitsData::XYZ);

    // Two views sharing the storage, e.g. two tracks: [0, 3) and [3, 5)
    TRestHits first(&data, 0, 3);
    TRestHits last(&data, 3, 2);
    EXPECT_DOUBLE_EQ(first.GetTotalEnergy(), 1 + 2 + 3);
    EXPECT_DOUBLE_EQ(last.GetTotalEnergy(), 4 + 5);
    EXPECT_DOUBLE_EQ(last.GetX(0), 3);  // local index

    EXPECT_THROW(first.AddHit({0, 0, 0}, 1, 0, TRestHitsData::XYZ), std::logic_error);
    EXPECT_THROW(first.RemoveHit(0), std::logic_error);
    EXPECT_THROW(first.MergeHits(0, 1), std::logic_error);
    EXPECT_THROW(first.RemoveHits(), std::logic_error);
    EXPECT_EQ(data.x.size(), 5u);  // unchanged

    last.AddHit({9, 0, 0}, 6, 0, TRestHitsData::XYZ);
    EXPECT_EQ(last.GetNumberOfHits(), 3u);
    EXPECT_EQ(data.x.size(), 6u);
    last.RemoveHit(0);
    EXPECT_EQ(data.x.size(), 5u);
    EXPECT_DOUBLE_EQ(first.GetTotalEnergy(), 1 + 2 + 3);  // the other view is not affected
}

TEST(Hits, SortSwapShuffle) {
    TRestHitsData data;
    TRestHits hits(&data);
    hits.AddHit({0, 0, 1}, 1, 10, TRestHitsData::XYZ);
    hits.AddHit({0, 0, 2}, 5, 20, TRestHitsData::XYZ);
    hits.AddHit({0, 0, 3}, 3, 30, TRestHitsData::XYZ);
    EXPECT_FALSE(hits.isSortedByEnergy());

    // By default descending in energy, the other arrays permuted with it
    hits.Sort();
    EXPECT_TRUE(hits.isSortedByEnergy());
    EXPECT_DOUBLE_EQ(hits.GetEnergy(0), 5);
    EXPECT_DOUBLE_EQ(hits.GetZ(0), 2);
    EXPECT_DOUBLE_EQ(hits.GetTime(0), 20);
    EXPECT_DOUBLE_EQ(hits.GetEnergy(2), 1);
    EXPECT_EQ(hits.GetMostEnergeticHitInRange(0, 3), 0);
    EXPECT_EQ(hits.GetMostEnergeticHitInRange(1, 3), 1);

    hits.Sort([&hits](int a, int b) { return hits.GetZ(a) < hits.GetZ(b); });
    EXPECT_DOUBLE_EQ(hits.GetZ(0), 1);
    EXPECT_DOUBLE_EQ(hits.GetZ(2), 3);
    EXPECT_DOUBLE_EQ(hits.GetEnergy(2), 3);

    hits.SwapHits(0, 2);
    EXPECT_DOUBLE_EQ(hits.GetZ(0), 3);
    EXPECT_DOUBLE_EQ(hits.GetEnergy(0), 3);

    gRandom->SetSeed(1234);
    hits.Shuffle(10);
    EXPECT_EQ(hits.GetNumberOfHits(), 3u);
    EXPECT_DOUBLE_EQ(hits.GetTotalEnergy(), 9);
    EXPECT_DOUBLE_EQ(hits.GetMaximumHitEnergy(), 5);
    EXPECT_DOUBLE_EQ(hits.GetMinimumHitEnergy(), 1);
    EXPECT_DOUBLE_EQ(hits.GetMeanHitEnergy(), 3);
}

TEST(Hits, MergeHits) {
    TRestHitsData data;
    TRestHits hits(&data);
    hits.AddHit({0, 0, 0}, 1, 0, TRestHitsData::XYZ);
    hits.AddHit({10, 20, 40}, 3, 4, TRestHitsData::XYZ);
    hits.MergeHits(0, 1);
    ASSERT_EQ(hits.GetNumberOfHits(), 1u);
    // Energy-weighted position and time, summed energy
    EXPECT_NEAR(hits.GetX(0), 7.5, 1e-5);
    EXPECT_NEAR(hits.GetY(0), 15, 1e-5);
    EXPECT_NEAR(hits.GetZ(0), 30, 1e-5);
    EXPECT_NEAR(hits.GetTime(0), 3, 1e-5);
    EXPECT_DOUBLE_EQ(hits.GetEnergy(0), 4);
}

TEST(Hits, StatisticsByType) {
    TRestHitsData data;
    TRestHits hits(&data);
    // Strips: XZ hits define X and Z, YZ hits define Y and Z
    hits.AddHit({0, kNaN, 10}, 1, 0, TRestHitsData::XZ);
    hits.AddHit({10, kNaN, 20}, 3, 0, TRestHitsData::XZ);
    hits.AddHit({kNaN, 4, 30}, 2, 0, TRestHitsData::YZ);

    EXPECT_EQ(hits.GetNumberOfHitsX(), 2);
    EXPECT_EQ(hits.GetNumberOfHitsY(), 1);
    EXPECT_EQ(hits.GetNumberOfHitsByType(TRestHitsData::Z), 3);
    EXPECT_DOUBLE_EQ(hits.GetEnergyX(), 4);
    EXPECT_DOUBLE_EQ(hits.GetEnergyY(), 2);
    EXPECT_DOUBLE_EQ(hits.GetTotalEnergy(), 6);

    // Means and sigmas with the hits defining the coordinate only (the NaN are not used)
    EXPECT_NEAR(hits.GetMeanPositionX(), 7.5, 1e-6);
    EXPECT_NEAR(hits.GetMeanPositionY(), 4, 1e-6);
    EXPECT_NEAR(hits.GetMeanPositionZ(), (10 * 1 + 20 * 3 + 30 * 2) / 6., 1e-6);
    EXPECT_NEAR(hits.GetSigmaX2(), 18.75, 1e-6);
    EXPECT_NEAR(hits.GetSigmaY2(), 0, 1e-6);
    EXPECT_NEAR(hits.GetSigmaXY2(), 18.75, 1e-6);
    EXPECT_NEAR(hits.GetSigmaX(), std::sqrt(18.75), 1e-6);

    EXPECT_TRUE(hits.isNaN(0));
    EXPECT_FALSE(hits.areXZ());  // not all of them
    // The undefined coordinate of a 2D hit is 0 in its position
    EXPECT_DOUBLE_EQ(hits.GetPosition(0).Y(), 0);
    EXPECT_DOUBLE_EQ(hits.GetPosition(2).X(), 0);
    const auto points = hits.GetPoints();
    ASSERT_EQ(points.size(), 3u);
    EXPECT_DOUBLE_EQ(points[1].X(), 10);
    EXPECT_DOUBLE_EQ(points[1].Y(), 0);
    EXPECT_EQ(hits.GetEnergies(), (std::vector<double>{1, 3, 2}));
    EXPECT_DOUBLE_EQ(hits.GetCoordinates(2)[2], 30);

    // Without hits
    TRestHitsData emptyData;
    TRestHits empty(&emptyData);
    EXPECT_DOUBLE_EQ(empty.GetMeanPositionX(), 0);
    EXPECT_DOUBLE_EQ(empty.GetSigmaZ2(), 0);
    EXPECT_DOUBLE_EQ(empty.GetSkewZ(), 0);
    EXPECT_DOUBLE_EQ(empty.GetMaximumHitEnergy(), 0);
    EXPECT_DOUBLE_EQ(empty.GetMeanHitEnergy(), 0);
    EXPECT_DOUBLE_EQ(empty.GetMaximumHitDistance(), 0);
}

TEST(Hits, Distances) {
    TRestHitsData data;
    TRestHits hits(&data);
    hits.AddHit({0, 0, 0}, 1, 0, TRestHitsData::XYZ);
    hits.AddHit({3, 4, 0}, 1, 0, TRestHitsData::XYZ);
    hits.AddHit({3, 4, 12}, 1, 0, TRestHitsData::XYZ);
    EXPECT_NEAR(hits.GetDistance(0, 1), 5, 1e-6);
    EXPECT_NEAR(hits.GetDistance2(0, 2), 169, 1e-4);
    EXPECT_NEAR(hits.GetTotalDistance(), 5 + 12, 1e-6);
    EXPECT_NEAR(hits.GetHitsPathLength(1, 2), 12, 1e-6);
    EXPECT_NEAR(hits.GetHitsPathLength(0, 10), 17, 1e-6);  // clamped to the view
    EXPECT_NEAR(hits.GetDistanceToNode(2), 17, 1e-6);
    EXPECT_NEAR(hits.GetMaximumHitDistance(), 13, 1e-6);
    EXPECT_EQ(hits.GetClosestHit({3, 4, 10}), 2);
    EXPECT_EQ(hits.GetClosestHit({0, 0, 1}), 0);
    EXPECT_NEAR(hits.GetTransversalProjection({0, 0, 0}, {0, 0, 1}, {3, 4, 7}), 5, 1e-9);

    // All the hits XY: the distance in the plane (the Z, undefined, is not used)
    TRestHitsData planeData;
    TRestHits plane(&planeData);
    plane.AddHit({0, 0, kNaN}, 1, 0, TRestHitsData::XY);
    plane.AddHit({6, 8, kNaN}, 1, 0, TRestHitsData::XY);
    EXPECT_TRUE(plane.areXY());
    EXPECT_NEAR(plane.GetDistance(0, 1), 10, 1e-6);
    EXPECT_NEAR(plane.GetMaximumHitDistance(), 10, 1e-6);
    EXPECT_DOUBLE_EQ(plane.GetPosition(1).Z(), 0);
}

TEST(Hits, TranslateAndRotate) {
    TRestHitsData data;
    TRestHits hits(&data);
    hits.AddHit({1, 0, 0}, 1, 0, TRestHitsData::XYZ);
    hits.Translate(0, 1, 2, 3);
    EXPECT_NEAR(hits.GetX(0), 2, 1e-6);
    EXPECT_NEAR(hits.GetY(0), 2, 1e-6);
    EXPECT_NEAR(hits.GetZ(0), 3, 1e-6);

    // 90 degrees around Z, about the center (0, 2, 3): (2, 2, 3) -> (0, 4, 3)
    hits.RotateIn3D(0, 0, 0, TMath::Pi() / 2, XYZVector(0, 2, 3));
    EXPECT_NEAR(hits.GetX(0), 0, 1e-5);
    EXPECT_NEAR(hits.GetY(0), 4, 1e-5);
    EXPECT_NEAR(hits.GetZ(0), 3, 1e-5);
}

TEST(Hits, MinimumDistanceAndBinning) {
    EXPECT_DOUBLE_EQ(TRestHits::GetMinimumDistance({0, 9, 3, 3, 6}), 3);  // repeated values ignored
    EXPECT_DOUBLE_EQ(TRestHits::GetMinimumDistance({5, 5}), 0);
    EXPECT_DOUBLE_EQ(TRestHits::GetMinimumDistance({}), 0);

    // Strips at a pitch of 3: bins of 3 with the values at their centres and 10 bins of margin
    const TRestHits::Binning strips = TRestHits::GetBinning({0, 3, 6, 9});
    EXPECT_EQ(strips.nBins, 24);
    EXPECT_NEAR(strips.min, -31.5, 1e-9);
    EXPECT_NEAR(strips.max, 40.5, 1e-9);
    const double binSize = (strips.max - strips.min) / strips.nBins;
    EXPECT_NEAR(binSize, 3, 1e-9);
    EXPECT_NEAR(std::remainder(0 - (strips.min + binSize / 2), binSize), 0, 1e-9);  // 0 at a bin centre

    // Given bin size
    const TRestHits::Binning given = TRestHits::GetBinning({0, 10}, 1);
    EXPECT_NEAR((given.max - given.min) / given.nBins, 1, 1e-9);
    EXPECT_LE(given.min, -10);
    EXPECT_GE(given.max, 20);

    // A single value: bins of 1
    const TRestHits::Binning single = TRestHits::GetBinning({2});
    EXPECT_EQ(single.nBins, 21);
    EXPECT_NEAR(single.min, -8.5, 1e-9);
    EXPECT_NEAR(single.max, 12.5, 1e-9);

    // No values: [-1, 1] with one bin
    const TRestHits::Binning none = TRestHits::GetBinning({});
    EXPECT_EQ(none.nBins, 1);
    EXPECT_DOUBLE_EQ(none.min, -1);
    EXPECT_DOUBLE_EQ(none.max, 1);
}

// The coordinates a hit does not define are stored as NaN, also when the hits are copied through their
// projected positions (GetPosition gives 0 for them) and whatever the value given
TEST(Hits, UndefinedCoordinatesStayNaN) {
    TRestHitsData data;
    TRestHits hits(&data);
    hits.AddHit({1, kNaN, 3}, 10, 0, TRestHitsData::XZ);
    hits.AddHit({kNaN, 2, 4}, 20, 0, TRestHitsData::YZ);
    EXPECT_DOUBLE_EQ(hits.GetPosition(0).Y(), 0);

    TRestHitsData copyData;
    TRestHits copy(&copyData);
    copy.AddHits(hits);
    ASSERT_EQ(copy.GetNumberOfHits(), 2u);
    EXPECT_DOUBLE_EQ(copy.GetX(0), 1);
    EXPECT_TRUE(std::isnan(copy.GetY(0)));
    EXPECT_DOUBLE_EQ(copy.GetZ(0), 3);
    EXPECT_TRUE(std::isnan(copy.GetX(1)));
    EXPECT_DOUBLE_EQ(copy.GetY(1), 2);

    copy.AddHit({5, 6, 7}, 1, 0, TRestHitsData::XY);
    EXPECT_DOUBLE_EQ(copy.GetY(2), 6);
    EXPECT_TRUE(std::isnan(copy.GetZ(2)));
}
