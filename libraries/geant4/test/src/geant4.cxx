/// Unit tests of the geant4 library (GoogleTest, BUILD_TESTING), without Geant4: TRestGeant4Event and its
/// tracks filled by hand (as restG4 fills its TRestGeant4EventData), TRestGeant4Metadata, the generator
/// (also the cry one, with a CRY engine for the tests) and the physics lists from YAML strings, the biasing
/// volumes, and the quenching process (Birks and Lindhard factors on known values) with a small TGeoManager
/// geometry built in memory.

#include <TGeoManager.h>
#include <TGeoMaterial.h>
#include <TGeoMatrix.h>
#include <TGeoMedium.h>
#include <TGeoNode.h>
#include <TGeoVolume.h>
#include <TRandom3.h>
#include <TRestGeant4BiasingVolume.h>
#include <TRestGeant4CRY.h>
#include <TRestGeant4EnergyDistribution.h>
#include <TRestGeant4Event.h>
#include <TRestGeant4Manager.h>
#include <TRestGeant4Metadata.h>
#include <TRestGeant4PhysicsLists.h>
#include <TRestGeant4PrimaryGenerator.h>
#include <TRestGeant4PrimaryGeneratorInfo.h>
#include <TRestGeant4QuenchingProcess.h>
#include <TRestGeant4SpatialGenerator.h>
#include <TRestGeant4Track.h>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using ROOT::Math::XYZVector;

namespace {

// IDs of the tables of the metadata of the tests
constexpr int kGas = 0, kScint = 1;
constexpr int kElectron = 11, kGamma = 22, kNeutron = 2112, kAr40 = 1000180400;
constexpr int kInit = 0, kIonisation = 2002, kHadElastic = 4111;

const std::string kTables =
    "volumeNameMap: {0: gas, 1: scint}\n"
    "particleNamesMap: {11: e-, 22: gamma, 2112: neutron, 1000180400: Ar40}\n"
    "processNamesMap: {0: Init, 2002: eIoni, 4111: hadElastic}\n";

/// A hit of a track: position, deposited energy, volume, process and hadronic target nucleus (A = 0: none)
struct Hit {
    XYZVector position;
    double energy;
    int volume;
    int process;
    int isotopeA = 0;
    int isotopeZ = 0;
};

/// Appends a track to the event as restG4 stores it (all the per track and per hit vectors), adding its
/// energy to the volumes. RebuildSecondaries must be called after the last track.
void AddTrack(TRestGeant4Event& event, int trackID, int parentID, int particleID,
              const std::vector<Hit>& hits) {
    auto& data = event.fEventData;
    double deposited = 0;
    for (const auto& hit : hits) deposited += hit.energy;
    data.trackIDs.push_back(trackID);
    data.parentIDs.push_back(parentID);
    data.trackParticleIDs.push_back(particleID);
    data.trackCreatorProcessIDs.push_back(kInit);
    data.trackDepositedEnergy.push_back(deposited);
    data.trackInitialEnergies.push_back(100);
    data.trackStartIndices.push_back(static_cast<int>(data.hitsStorage.x.size()));
    data.trackNHits.push_back(static_cast<int>(hits.size()));
    data.trackGlobalTimestamps.push_back(0);
    data.trackTimeOffsets.push_back(0);
    data.trackTimeLengths.push_back(0);
    data.trackLengths.push_back(0);
    data.trackWeights.push_back(1);
    const XYZVector start = hits.empty() ? XYZVector() : hits.front().position;
    data.trackInitialPositionX.push_back(start.X());
    data.trackInitialPositionY.push_back(start.Y());
    data.trackInitialPositionZ.push_back(start.Z());
    for (const auto& hit : hits) {
        data.hitsStorage.x.push_back(hit.position.X());
        data.hitsStorage.y.push_back(hit.position.Y());
        data.hitsStorage.z.push_back(hit.position.Z());
        data.hitsStorage.time.push_back(0);
        data.hitsStorage.energy.push_back(hit.energy);
        data.hitsStorage.type.push_back(TRestHitsData::XYZ);
        data.hitProcessID.push_back(hit.process);
        data.hitVolumeID.push_back(hit.volume);
        data.hitKineticEnergy.push_back(0);
        data.hitMomentumDirectionX.push_back(0);
        data.hitMomentumDirectionY.push_back(0);
        data.hitMomentumDirectionZ.push_back(1);
        data.hitHadronicTargetIsotopeA.push_back(hit.isotopeA);
        data.hitHadronicTargetIsotopeZ.push_back(hit.isotopeZ);
        event.AddEnergyInVolumeForParticleForProcess(hit.energy, hit.volume, particleID, hit.process);
    }
}

/// A neutron (track 1) in the gas, with an argon recoil (2) and an electron (3) in the scintillator, which
/// emits a gamma (4) absorbed in the scintillator. Gas: 15 keV, scintillator: 67 keV.
TRestGeant4Event MakeEvent() {
    TRestGeant4Event event;
    event.SetID(12);
    auto& data = event.fEventData;
    data.primaryParticleIDs = {kNeutron};
    data.primaryEnergies = {1000};
    data.primaryDirectionX = {0};
    data.primaryDirectionY = {0};
    data.primaryDirectionZ = {1};
    data.primaryPositionX = 50;
    AddTrack(event, 1, 0, kNeutron,
             {{{50, 0, 0}, 0, kGas, kInit}, {{51, 0, 0}, 10, kGas, kHadElastic, 40, 18}});
    AddTrack(event, 2, 1, kAr40, {{{51, 0, 0}, 5, kGas, kIonisation}});
    AddTrack(event, 3, 1, kElectron,
             {{{-50, 0, 0}, 10, kScint, kIonisation},
              {{-49, 0, 0}, 20, kScint, kIonisation},
              {{-47, 0, 0}, 30, kScint, kIonisation}});
    AddTrack(event, 4, 3, kGamma, {{{-45, 0, 0}, 7, kScint, kIonisation}});
    event.RebuildSecondaries();
    event.RebuildTrackIndex();
    data.sensitiveVolumeEnergy = event.GetEnergyInVolume(kGas);
    return event;
}

/// World with two boxes whose physical volumes (TGeo nodes) are named gas and scint, as the GDML physvols
struct Geometry {
    TGeoManager* geometry = nullptr;

    Geometry() {
        gGeoManager = nullptr;  // the constructor must not delete another geometry
        geometry = new TGeoManager("geant4Test", "geant4Test");
        auto* material = new TGeoMaterial("Vacuum", 0, 0, 0);
        auto* medium = new TGeoMedium("Vacuum", 1, material);
        TGeoVolume* world = geometry->MakeBox("World", medium, 200, 200, 200);
        geometry->SetTopVolume(world);
        TGeoVolume* scint = geometry->MakeBox("scintVolume", medium, 10, 10, 10);
        TGeoVolume* gas = geometry->MakeBox("gasVolume", medium, 10, 10, 10);
        world->AddNode(scint, 1, new TGeoTranslation(-50, 0, 0))->SetName("scint");
        world->AddNode(gas, 1, new TGeoTranslation(50, 0, 0))->SetName("gas");
        geometry->CloseGeometry();
    }
    ~Geometry() { delete geometry; }
};

/// Lindhard quenching factor (as TRestGeant4QuenchingProcess), recoil energy in keV
double Lindhard(double energy, int A, int Z) {
    const double epsilon = 11.5 * energy * std::pow(Z, -7. / 3);
    const double g = 3 * std::pow(epsilon, 0.15) + 0.7 * std::pow(epsilon, 0.6) + epsilon;
    const double k = 0.133 * std::pow(Z, 2. / 3) / std::sqrt(A);
    return k * g / (1 + k * g);
}

}  // namespace

// ============================================================================
// Event and tracks
// ============================================================================

TEST(Geant4Event, TracksHitsAndEnergies) {
    TRestGeant4Event event = MakeEvent();

    EXPECT_EQ(event.GetID(), 12);
    EXPECT_EQ(event.GetNumberOfPrimaries(), 1u);
    EXPECT_DOUBLE_EQ(event.GetPrimaryEnergy(0), 1000);
    EXPECT_NEAR(event.GetPrimaryDirection(0).Z(), 1, 1e-6);
    EXPECT_NEAR(event.GetPrimaryPosition().X(), 50, 1e-6);
    EXPECT_FALSE(event.IsSubEvent());

    ASSERT_EQ(event.GetNumberOfTracks(), 4u);
    EXPECT_EQ(event.GetEventHits().GetNumberOfHits(), 7u);
    EXPECT_NEAR(event.GetTotalDepositedEnergy(), 82, 1e-6);
    EXPECT_NEAR(event.GetSensitiveVolumeEnergy(), 15, 1e-6);
    EXPECT_EQ(event.GetNumberOfActiveVolumes(), 2);
    EXPECT_NEAR(event.GetEnergyInVolume(kGas), 15, 1e-6);
    EXPECT_NEAR(event.GetEnergyInVolume(kScint), 67, 1e-6);
    EXPECT_DOUBLE_EQ(event.GetEnergyInVolume(99), 0);
    // Without metadata there are no names
    EXPECT_EQ(event.GetParticleName(kNeutron), "");
    EXPECT_DOUBLE_EQ(event.GetEnergyInVolume("gas"), 0);

    const auto electron = event.GetTrackByID(3);
    EXPECT_EQ(electron.GetTrackID(), 3);
    EXPECT_EQ(electron.GetParentID(), 1);
    EXPECT_EQ(electron.GetParticleID(), kElectron);
    EXPECT_EQ(electron.GetNumberOfHits(), 3u);
    EXPECT_EQ(electron.GetNumberOfHits(kScint), 3u);
    EXPECT_EQ(electron.GetNumberOfHits(kGas), 0u);
    EXPECT_NEAR(electron.GetTotalEnergy(), 60, 1e-6);
    EXPECT_NEAR(electron.GetDepositedEnergy(), 60, 1e-6);
    EXPECT_NEAR(electron.GetEnergyInVolume(kScint), 60, 1e-6);
    EXPECT_NEAR(electron.GetMeanPositionInVolume(kScint).X(), (-50 * 10 - 49 * 20 - 47 * 30) / 60., 1e-5);
    EXPECT_NEAR(electron.GetFirstPositionInVolume(kScint).X(), -50, 1e-6);
    EXPECT_NEAR(electron.GetLastPositionInVolume(kScint).X(), -47, 1e-6);
    EXPECT_TRUE(std::isnan(electron.GetMeanPositionInVolume(kGas).X()));
    EXPECT_NEAR(electron.GetInitialPosition().X(), -50, 1e-6);
    EXPECT_NEAR(electron.GetHitEnergy(2), 30, 1e-6);
    EXPECT_EQ(electron.GetHitVolumeID(0), kScint);

    // Hits with energy and processes
    const auto neutron = event.GetTrack(0);
    EXPECT_EQ(neutron.GetNumberOfHits(), 2u);
    EXPECT_EQ(neutron.GetNumberOfPhysicalHits(), 1u);
    EXPECT_TRUE(neutron.ContainsProcess(kHadElastic));
    EXPECT_FALSE(neutron.ContainsProcessInVolume(kHadElastic, kScint));
    EXPECT_EQ(neutron.GetHitHadronicTargetIsotopeA(1), 40);
    EXPECT_EQ(neutron.GetHitHadronicTargetIsotopeZ(1), 18);
    EXPECT_TRUE(neutron.GetHadronicOk());

    EXPECT_THROW(event.GetTrackByID(99), std::runtime_error);
    EXPECT_THROW(event.GetTrack(10), std::out_of_range);
}

TEST(Geant4Event, ParentsAndSecondaries) {
    TRestGeant4Event event = MakeEvent();

    const auto neutron = event.GetTrackByID(1);
    EXPECT_EQ(neutron.GetSecondaryTrackIDs(), (std::vector<int>{2, 3}));
    EXPECT_EQ(neutron.GetSecondaryTracks().size(), 2u);
    EXPECT_FALSE(neutron.GetParentTrack().has_value());

    const auto gamma = event.GetTrackByID(4);
    const auto parent = gamma.GetParentTrack();
    ASSERT_TRUE(parent.has_value());
    EXPECT_EQ(parent->GetTrackID(), 3);
    EXPECT_TRUE(gamma.GetSecondaryTrackIDs().empty());
    EXPECT_EQ(event.GetTracks().size(), 4u);
}

TEST(Geant4Event, CopyAndRemoveTrackHits) {
    TRestGeant4Event event = MakeEvent();

    TRestGeant4Event copy;
    copy.CopyFrom(&event);
    EXPECT_EQ(copy.GetID(), 12);
    EXPECT_EQ(copy.GetNumberOfTracks(), 4u);
    EXPECT_NEAR(copy.GetEnergyInVolume(kScint), 67, 1e-6);
    EXPECT_EQ(copy.GetTrackByID(4).GetParentID(), 3);

    // The hits of the first track are removed, those of the others keep their track
    event.RemoveTrackHits(0);
    EXPECT_EQ(event.GetTrack(0).GetNumberOfHits(), 0u);
    EXPECT_EQ(event.GetEventHits().GetNumberOfHits(), 5u);
    EXPECT_NEAR(event.GetTrack(1).GetHitPosition(0).X(), 51, 1e-6);
    EXPECT_NEAR(event.GetTrack(2).GetHitEnergy(0), 10, 1e-6);
    EXPECT_NEAR(event.GetTrack(3).GetHitPosition(0).X(), -45, 1e-6);
    // The copy is independent
    EXPECT_EQ(copy.GetTrack(0).GetNumberOfHits(), 2u);

    event.Initialize();
    EXPECT_EQ(event.GetNumberOfTracks(), 0u);
    EXPECT_DOUBLE_EQ(event.GetTotalDepositedEnergy(), 0);
    EXPECT_DOUBLE_EQ(event.GetEnergyInVolume(kScint), 0);
}

TEST(Geant4Event, NamesFromMetadata) {
    TRestGeant4Metadata metadata("geant4Metadata", YAML::Load("seed: 1\n" + kTables));
    TRestGeant4Event event = MakeEvent();
    event.SetGeant4Metadata(&metadata);

    EXPECT_EQ(event.GetPrimaryParticleName(0), "neutron");
    EXPECT_EQ(event.GetParticleName(kAr40), "Ar40");
    EXPECT_EQ(event.GetProcessName(kHadElastic), "hadElastic");
    EXPECT_EQ(event.GetVolumeName(kScint), "scint");
    EXPECT_NEAR(event.GetEnergyInVolume("gas"), 15, 1e-6);
    EXPECT_DOUBLE_EQ(event.GetEnergyInVolume("unknown"), 0);

    const auto neutron = event.GetTrackByID(1);
    EXPECT_EQ(neutron.GetParticleName(), "neutron");
    EXPECT_EQ(neutron.GetCreatorProcess(), "Init");
    EXPECT_EQ(neutron.GetInitialVolume(), "gas");
    EXPECT_EQ(neutron.GetHitVolumeName(1), "gas");
    EXPECT_TRUE(neutron.ContainsProcess("hadElastic"));
    EXPECT_EQ(neutron.GetProcessID("hadElastic"), kHadElastic);
    // With the secondaries (all the descendants)
    EXPECT_NEAR(neutron.GetEnergyInVolume("gas"), 10, 1e-6);
    EXPECT_NEAR(neutron.GetEnergyInVolume("gas", true), 15, 1e-6);
    EXPECT_NEAR(neutron.GetEnergyInVolume("scint", true), 67, 1e-6);

    const auto electron = event.GetTrackByID(3);
    EXPECT_EQ(electron.GetFinalVolume(), "scint");
    EXPECT_EQ(electron.GetLastProcessName(), "eIoni");
}

// ============================================================================
// Metadata, generator and physics lists
// ============================================================================

TEST(Geant4Metadata, FromYAML) {
    const YAML::Node node = YAML::Load(
        "nEvents: 100\n"
        "seed: 17\n"
        "subEventTimeDelay: 100 us\n"
        "energyRangeStored: [0, 5 MeV]\n"
        "storeHadronicTargetInfo: true\n"
        "activeVolumes:\n"
        "  - {name: gas, maxStep: 1 mm, sensitive: true, chance: 0.5}\n"
        "  - {name: vessel}\n"
        "killVolumes: [shield]\n"
        "fullChainStopIsotopes: [Pb210]\n"
        "volumeNameMap: {0: gas, 1: vessel}\n"
        "particleNamesMap: {11: e-, 2112: neutron}\n"
        "processNamesMap: {0: Init, 4111: hadElastic}\n");
    TRestGeant4Metadata metadata("geant4Metadata", node);

    EXPECT_EQ(metadata.GetNumberOfEvents(), 100);
    EXPECT_EQ(metadata.GetSeed(), 17);
    EXPECT_DOUBLE_EQ(metadata.GetSubEventTimeDelay(), 100);
    EXPECT_DOUBLE_EQ(metadata.GetMinimumEnergyStored(), 0);
    EXPECT_DOUBLE_EQ(metadata.GetMaximumEnergyStored(), 5000);
    EXPECT_TRUE(metadata.GetStoreHadronicTargetInfo());
    EXPECT_DOUBLE_EQ(metadata.GetEquivalentNumberOfEvents(), 100);

    EXPECT_EQ(metadata.GetActiveVolumes(), (std::vector<std::string>{"gas", "vessel"}));
    EXPECT_TRUE(metadata.IsActiveVolume("vessel"));
    EXPECT_FALSE(metadata.IsActiveVolume("shield"));
    EXPECT_DOUBLE_EQ(metadata.GetStorageChance("gas"), 0.5);
    EXPECT_DOUBLE_EQ(metadata.GetStorageChance("vessel"), 1);
    EXPECT_DOUBLE_EQ(metadata.GetMaxStepSize("gas"), 1);
    EXPECT_EQ(metadata.GetSensitiveVolumes(), (std::vector<std::string>{"gas"}));
    EXPECT_EQ(metadata.GetSensitiveVolumeExpressions(), (std::vector<std::string>{"gas"}));
    EXPECT_TRUE(metadata.IsKeepTracksVolume("vessel"));
    EXPECT_TRUE(metadata.IsKillVolume("shield"));
    EXPECT_TRUE(metadata.IsIsotopeFullChainStop("Pb210"));

    const auto& geometry = metadata.GetGeant4GeometryInfo();
    EXPECT_EQ(geometry.GetIDFromVolume("vessel"), 1);
    EXPECT_EQ(geometry.GetVolumeFromID(0), "gas");
    EXPECT_EQ(geometry.GetIDFromVolume("unknown"), -1);
    const auto& physics = metadata.GetGeant4PhysicsInfo();
    EXPECT_EQ(physics.GetParticleName(2112), "neutron");
    EXPECT_EQ(physics.GetParticleID("e-"), 11);
    EXPECT_EQ(physics.GetProcessID("hadElastic"), 4111);
    EXPECT_EQ(physics.GetProcessID("Init"), 0);
    EXPECT_EQ(physics.GetProcessID("unknown"), -1);
    EXPECT_EQ(physics.GetAllParticles(), (std::set<std::string>{"e-", "neutron"}));

    EXPECT_THROW(TRestGeant4Metadata("m", YAML::Load("{seed: 1, activeVolumes: [{name: gas, chance: 1.5}]}")),
                 std::runtime_error);
}

TEST(Geant4Metadata, PointGenerator) {
    const YAML::Node node = YAML::Load(
        "seed: 1\n"
        "generator:\n"
        "  type: point\n"
        "  position: [0, 0, -200 mm]\n"
        "  source:\n"
        "    particle: e-\n"
        "    angular: {type: flux, direction: [0, 0, 2]}\n"
        "    energy: {type: mono, energy: 30 keV}\n");
    TRestGeant4Metadata metadata("geant4Metadata", node);

    auto& generator = metadata.GetGeant4PrimaryGeneratorInfo();
    ASSERT_EQ(generator.GetNumberOfSources(), 1u);
    EXPECT_EQ(metadata.GetNumberOfSources(), 1);
    ASSERT_NE(generator.GetSpatialGenerator(), nullptr);
    EXPECT_EQ(generator.GetSpatialGenerator()->GetClassName(), "TRestGeant4SpatialGeneratorPoint");
    EXPECT_NEAR(generator.GetSpatialGenerator()->GetCenter().Z(), -200, 1e-9);
    EXPECT_FALSE(generator.IsCosmic());
    EXPECT_FALSE(generator.IsCRY());
    EXPECT_FALSE(generator.IsGdml());
    EXPECT_FALSE(generator.HasDecay0Sources());

    auto& source = generator.fSources[0];
    EXPECT_EQ(source.fParticleName, "e-");
    EXPECT_FALSE(source.IsDecay0());
    EXPECT_EQ(source.GetEnergyDistribution()->GetClassName(), "TRestGeant4EnergyDistributionMono");
    EXPECT_EQ(source.GetAngularDistribution()->GetClassName(), "TRestGeant4AngularDistributionFlux");

    generator.LoadDistributions();
    TRandom3 random(1);
    const XYZVector origin = generator.GetSpatialGenerator()->GetCenter();
    const auto particles = source.Generate(random, origin, origin, false);
    ASSERT_EQ(particles.size(), 1u);
    EXPECT_EQ(particles[0].GetParticleName(), "e-");
    EXPECT_DOUBLE_EQ(particles[0].GetEnergy(), 30);
    EXPECT_NEAR(particles[0].GetDirection().Z(), 1, 1e-12);
    EXPECT_NEAR(particles[0].GetOrigin().Z(), -200, 1e-12);
}

namespace {
/// A CRY engine for the tests (no CRY): a muon per shower at (100, 0, 0) mm going down, 1 s per shower
class FakeCRYEngine : public TRestGeant4CRYEngine {
   public:
    std::vector<TRestGeant4ParticleState> Generate() override {
        fShowers++;
        TRestGeant4ParticleState muon;
        muon.SetParticleName("mu-");
        muon.SetEnergy(1e6);
        muon.SetOrigin({100, 0, 0});
        muon.SetDirection({0, 0, -1});
        return {muon};
    }
    double GetTimeSimulated() const override { return fShowers * 1e6; }

   private:
    int fShowers = 0;
};
}  // namespace

TEST(Geant4Metadata, CRYGenerator) {
    // The options of CRY in their own section of the manager, linked to the cry generator
    const std::string metadataSection =
        "geant4Metadata:\n"
        "  class: TRestGeant4Metadata\n"
        "  seed: 5\n"
        "  generator:\n"
        "    type: cry\n"
        "    position: [0, 1 m, 0]\n"
        "    verticalAxis: [0, 1, 0]\n";
    const std::string crySection =
        "cry:\n"
        "  class: TRestGeant4CRY\n"
        "  subBoxLength: 2 m\n"
        "  latitude: 42\n"
        "  maxExposureTime: 3 s\n";
    TRestGeant4Manager manager("g4Manager", YAML::Load(metadataSection + crySection));
    ASSERT_NE(manager.fG4Metadata, nullptr);
    ASSERT_NE(manager.fCRY, nullptr);
    TRestGeant4Metadata& metadata = *manager.fG4Metadata;
    auto& info = metadata.GetGeant4PrimaryGeneratorInfo();
    EXPECT_TRUE(info.IsCRY());
    EXPECT_EQ(metadata.GetNumberOfSources(), 0);

    // The CRY square, from y = 1 m (verticalAxis +Y)
    const auto corners = info.GetCRYPlaneCorners();
    ASSERT_EQ(corners.size(), 4u);
    for (const auto& corner : corners) {
        EXPECT_NEAR(corner.Y(), 1000, 1e-9);
        EXPECT_NEAR(std::abs(corner.X()), 1000, 1e-9);
        EXPECT_NEAR(std::abs(corner.Z()), 1000, 1e-9);
    }

    // Without engine (restG4 without CRY): an error
    TRestGeant4SpatialGeneratorCRY::SetEngineFactory(nullptr);
    EXPECT_FALSE(TRestGeant4SpatialGeneratorCRY::HasEngine());
    EXPECT_THROW(info.LoadDistributions(metadata.GetSeed()), std::runtime_error);

    std::string setup;
    long seed = 0;
    TRestGeant4SpatialGeneratorCRY::SetEngineFactory([&](const std::string& s, long engineSeed) {
        setup = s;
        seed = engineSeed;
        return std::unique_ptr<TRestGeant4CRYEngine>(std::make_unique<FakeCRYEngine>());
    });
    EXPECT_TRUE(TRestGeant4SpatialGeneratorCRY::HasEngine());
    info.LoadDistributions(metadata.GetSeed());
    EXPECT_EQ(seed, 5);
    EXPECT_NE(setup.find("latitude 42"), std::string::npos);
    EXPECT_NE(setup.find("subboxLength 2"), std::string::npos);

    // Two generators (threads) share the CRY generator: the CRY +Z is +Y, the plane at y = 1 m
    TRestGeant4PrimaryGenerator first(info, 1), second(info, 2);
    for (auto* generator : {&first, &second, &first}) {
        const auto particles = generator->Generate();
        ASSERT_EQ(particles.size(), 1u);
        EXPECT_EQ(particles[0].GetParticleName(), "mu-");
        EXPECT_NEAR(particles[0].GetOrigin().X(), 100, 1e-9);
        EXPECT_NEAR(particles[0].GetOrigin().Y(), 1000, 1e-9);
        EXPECT_NEAR(particles[0].GetOrigin().Z(), 0, 1e-9);
        EXPECT_NEAR(particles[0].GetDirection().Y(), -1, 1e-12);
    }
    // maxExposureTime (3 s) reached: no more showers
    EXPECT_TRUE(second.Generate().empty());

    info.UpdateCRYExposure();
    EXPECT_EQ(metadata.GetCRYShowersUsed(), 3);
    EXPECT_NEAR(metadata.GetCRYExposureTime(), 3e6, 1e-6);
    TRestGeant4SpatialGeneratorCRY::SetEngineFactory(nullptr);

    // The cry generator needs the CRY section; with another generator the section is not used
    EXPECT_THROW(TRestGeant4Manager("g4Manager", YAML::Load(metadataSection)), std::runtime_error);
    const std::string pointSection =
        "geant4Metadata:\n"
        "  class: TRestGeant4Metadata\n"
        "  generator:\n"
        "    type: point\n"
        "    source: {particle: gamma, angular: {type: flux}, energy: {type: mono, energy: 1}}\n";
    const TRestGeant4Manager pointManager("g4Manager", YAML::Load(pointSection + crySection));
    EXPECT_EQ(pointManager.fCRY, nullptr);

    // Only some particles: CRY returns only them
    const TRestGeant4CRY muons("cry", YAML::Load("{subBoxLength: 1 m, particles: [muons, neutrons]}"));
    EXPECT_TRUE(muons.Returns("muons"));
    EXPECT_TRUE(muons.Returns("neutrons"));
    EXPECT_FALSE(muons.Returns("gammas"));
    EXPECT_NE(muons.GetCRYSetup().find("returnMuons 1 returnPions 0"), std::string::npos);
    EXPECT_TRUE(manager.fCRY->Returns("electrons"));  // all by default
    // subBoxLength is needed, the particles known, the altitude one of the CRY tables
    EXPECT_THROW(TRestGeant4CRY("cry", YAML::Load("{latitude: 42}")), std::runtime_error);
    EXPECT_THROW(TRestGeant4CRY("cry", YAML::Load("{subBoxLength: 1 m, particles: [muon]}")),
                 std::runtime_error);
    EXPECT_THROW(TRestGeant4CRY("cry", YAML::Load("{subBoxLength: 1 m, altitude: 5 m}")), std::runtime_error);
}

TEST(Geant4Metadata, GeneratorErrors) {
    const std::string source =
        "source: {particle: gamma, angular: {type: flux}, energy: {type: mono, energy: 1}}";
    EXPECT_NO_THROW(
        TRestGeant4PrimaryGeneratorInfo("generator", YAML::Load("{type: point, " + source + "}")));
    // No source, unknown type, a source without energy, a volume without shape
    EXPECT_THROW(TRestGeant4PrimaryGeneratorInfo("generator", YAML::Load("{type: point}")),
                 std::runtime_error);
    EXPECT_THROW(TRestGeant4PrimaryGeneratorInfo("generator", YAML::Load("{type: unknown, " + source + "}")),
                 std::runtime_error);
    EXPECT_THROW(
        TRestGeant4PrimaryGeneratorInfo(
            "generator", YAML::Load("{type: point, source: {particle: gamma, angular: {type: flux}}}")),
        std::runtime_error);
    EXPECT_THROW(TRestGeant4PrimaryGeneratorInfo("generator", YAML::Load("{type: volume, " + source + "}")),
                 std::runtime_error);
}

TEST(Geant4Generator, VolumeShapesAndEnergy) {
    TRandom3 random(3);
    auto box = TRestGeant4SpatialGenerator::Create(
        "generator",
        YAML::Load("{type: volume, shape: box, size: [10 mm, 2 mm, 4 mm], position: [1, 2, 3]}"));
    ASSERT_NE(box, nullptr);
    EXPECT_NEAR((box->GetCenter() - XYZVector(1, 2, 3)).R(), 0, 1e-12);
    for (int i = 0; i < 1000; i++) {
        const auto p = box->Sample(random);
        EXPECT_LE(std::abs(p.X() - 1), 5 + 1e-9);
        EXPECT_LE(std::abs(p.Y() - 2), 1 + 1e-9);
        EXPECT_LE(std::abs(p.Z() - 3), 2 + 1e-9);
    }

    // Rotated 90 deg around Z: the long side along Y
    auto rotated = TRestGeant4SpatialGenerator::Create(
        "generator",
        YAML::Load(
            "{type: volume, shape: box, size: [10, 2, 4], rotationAxis: [0, 0, 1], rotationValue: 90 deg}"));
    double maxX = 0, maxY = 0;
    for (int i = 0; i < 1000; i++) {
        const auto p = rotated->Sample(random);
        maxX = std::max(maxX, std::abs(p.X()));
        maxY = std::max(maxY, std::abs(p.Y()));
    }
    EXPECT_LE(maxX, 1 + 1e-9);
    EXPECT_GT(maxY, 4);
    EXPECT_LE(maxY, 5 + 1e-9);

    auto sphere = TRestGeant4SpatialGenerator::Create(
        "generator", YAML::Load("{type: surface, shape: sphere, size: [7, 0, 0]}"));
    for (int i = 0; i < 100; i++) EXPECT_NEAR(sphere->Sample(random).R(), 7, 1e-9);

    EXPECT_THROW(TRestGeant4SpatialGenerator::Create("generator", YAML::Load("{type: volume}")),
                 std::runtime_error);
    EXPECT_THROW(TRestGeant4SpatialGenerator::Create("generator", YAML::Load("{position: [0, 0, 0]}")),
                 std::runtime_error);

    // Flat energy in a range given with units
    auto flat =
        TRestGeant4EnergyDistribution::Create("energy", YAML::Load("{type: Flat, range: [1 MeV, 2 MeV]}"));
    EXPECT_EQ(flat->GetClassName(), "TRestGeant4EnergyDistributionFlat");
    double sum = 0;
    const int n = 2000;
    for (int i = 0; i < n; i++) {
        const double energy = flat->Sample(random);
        EXPECT_GE(energy, 1000);
        EXPECT_LE(energy, 2000);
        sum += energy;
    }
    EXPECT_NEAR(sum / n, 1500, 25);
    EXPECT_THROW(TRestGeant4EnergyDistribution::Create("energy", YAML::Load("{type: unknown}")),
                 std::runtime_error);
}

TEST(Geant4PhysicsLists, ListsOptionsAndCuts) {
    const YAML::Node node = YAML::Load(
        "cutForGamma: 1 mm\n"
        "cutForElectron: 2 cm\n"
        "maxEnergyRangeProductionCuts: 1 GeV\n"
        "ionLimitStepList: [F20, Ne20]\n"
        "physicsList:\n"
        "  - name: G4EmLivermorePhysics\n"
        "    option:\n"
        "      - {name: pixe, value: \"true\"}\n"
        "  - name: G4DecayPhysics\n");
    TRestGeant4PhysicsLists lists("physics", node);

    EXPECT_DOUBLE_EQ(lists.GetCutForGamma(), 1);
    EXPECT_DOUBLE_EQ(lists.GetCutForElectron(), 20);
    EXPECT_DOUBLE_EQ(lists.GetCutForPositron(), 1);  // default
    EXPECT_DOUBLE_EQ(lists.GetMaximumEnergyProductionCuts(), 1e6);
    EXPECT_EQ(lists.GetIonStepList(), (std::vector<std::string>{"F20", "Ne20"}));

    ASSERT_EQ(lists.fPhysicsLists.size(), 2u);
    EXPECT_EQ(lists.FindPhysicsList("G4DecayPhysics"), 1);
    EXPECT_EQ(lists.FindPhysicsList("G4EmPenelopePhysics"), -1);  // valid, not used
    EXPECT_TRUE(lists.PhysicsListExists("G4EmPenelopePhysics"));
    EXPECT_FALSE(lists.PhysicsListExists("NotAList"));
    EXPECT_EQ(lists.GetPhysicsListOptionValue("G4EmLivermorePhysics", "pixe"), "true");
    EXPECT_EQ(lists.GetPhysicsListOptionValue("G4EmLivermorePhysics", "auger", "false"), "false");
    EXPECT_EQ(lists.GetPhysicsListOptionValue("G4DecayPhysics", "pixe"), "NotDefined");

    EXPECT_THROW(
        TRestGeant4PhysicsLists("physics", YAML::Load("{physicsList: [{name: G4EmLivermorePhysicz}]}")),
        std::runtime_error);
}

TEST(Geant4Metadata, BiasingVolumes) {
    const YAML::Node node = YAML::Load(
        "nEvents: 10\n"
        "seed: 1\n"
        "biasingVolumes:\n"
        "  - {type: virtualBox, size: 100 mm, biasingFactor: 2}\n"
        "  - {type: virtualSphere, size: 2 cm, biasingFactor: 3}\n");
    TRestGeant4Metadata metadata("geant4Metadata", node);

    ASSERT_EQ(metadata.GetNumberOfBiasingVolumes(), 2u);
    EXPECT_NO_THROW(metadata.CheckBiasingVolumes());
    const auto box = metadata.GetBiasingVolume(0);
    const auto sphere = metadata.GetBiasingVolume(1);
    EXPECT_EQ(sphere.GetBiasingVolumeType(), "virtualSphere");
    EXPECT_DOUBLE_EQ(sphere.GetBiasingVolumeSize(), 20);
    EXPECT_TRUE(box.Contains(sphere));
    EXPECT_FALSE(sphere.Contains(box));
    EXPECT_EQ(box.isInside(40, 0, 0), 1);
    EXPECT_EQ(sphere.isInside(30, 0, 0), 0);
    // Entry point of a segment as the fraction of its length, -1 if it starts inside or does not enter
    EXPECT_NEAR(box.GetEntryFraction({-100, 0, 0}, {100, 0, 0}), 0.25, 1e-12);
    EXPECT_NEAR(sphere.GetEntryFraction({0, -40, 0}, {0, 40, 0}), 0.25, 1e-12);
    EXPECT_DOUBLE_EQ(box.GetEntryFraction({0, 0, 0}, {100, 0, 0}), -1);
    EXPECT_DOUBLE_EQ(box.GetEntryFraction({-100, 80, 0}, {100, 80, 0}), -1);

    metadata.InitializeBiasingStages();
    EXPECT_DOUBLE_EQ(metadata.GetBiasingTotalFactor(), 6);
    EXPECT_DOUBLE_EQ(metadata.GetBiasingEventWeight(), 1. / 6);
    EXPECT_EQ(metadata.GetNumberOfBiasingStages(), 3u);
    EXPECT_DOUBLE_EQ(metadata.GetEquivalentNumberOfEvents(), 60);
    // Stage 1 simulated 8 of the 5 x 2 particles planned, stage 2 all of them
    metadata.SetBiasingStageParticles(0, 5);
    metadata.SetBiasingStageEvents(1, 8);
    metadata.SetBiasingStageParticles(1, 4);
    metadata.SetBiasingStageEvents(2, 12);
    EXPECT_DOUBLE_EQ(metadata.GetBiasingCompletedFraction(), 0.8);
    EXPECT_DOUBLE_EQ(metadata.GetEquivalentNumberOfEvents(), 48);

    // Not nested from the outermost to the innermost
    TRestGeant4Metadata reversed(
        "geant4Metadata", YAML::Load("{seed: 1, biasingVolumes: [{type: virtualSphere, size: 20, "
                                     "biasingFactor: 2}, {type: virtualBox, size: 100, biasingFactor: 2}]}"));
    EXPECT_THROW(reversed.CheckBiasingVolumes(), std::runtime_error);
    EXPECT_THROW(TRestGeant4BiasingVolume("b", YAML::Load("{size: 10, biasingFactor: 1.5}")),
                 std::runtime_error);
    EXPECT_THROW(TRestGeant4BiasingVolume("b", YAML::Load("{size: 0}")), std::runtime_error);
}

// ============================================================================
// Quenching process
// ============================================================================

TEST(Geant4QuenchingProcess, BirksAndLindhard) {
    Geometry geometry;
    TRestGeant4Metadata metadata(
        "geant4Metadata",
        YAML::Load("seed: 1\nstoreHadronicTargetInfo: true\nactiveVolumes: [{name: gas, sensitive: true}]\n" +
                   kTables));
    metadata.fGeant4GeometryInfo.LoadGeometry(geometry.geometry);
    EXPECT_EQ(metadata.GetSensitiveVolumes(), (std::vector<std::string>{"gas"}));

    TRestGeant4QuenchingProcess process(
        "quenching", YAML::Load("{volume: [{name: '^scint$', model: birks, birksConstant: 0.01, "
                                "birksFallbackStepLength: 0.5}, {name: '^gas$', model: Lindhard}]}"));
    // Without a run (no input file), the metadata is given directly
    process.fGeant4Metadata = &metadata;
    process.Initialize();
    EXPECT_EQ(process.GetVolumes(), (std::set<std::string>{"gas", "scint"}));
    EXPECT_EQ(process.fVolumeModels.at("scint"), "birks");
    EXPECT_EQ(process.fVolumeModels.at("gas"), "lindhard");
    EXPECT_DOUBLE_EQ(process.fVolumeBirksConstants.at("scint"), 0.01);

    const TRestGeant4Event input = MakeEvent();
    TRestGeant4Event output;
    ASSERT_TRUE(process.ProcessEvent(input, output));
    ASSERT_EQ(output.GetNumberOfTracks(), 4u);

    // Birks: E / (1 + kB E / step), the step from the previous hit of the track (fallback for the first)
    const auto electron = output.GetTrackByID(3);
    EXPECT_NEAR(electron.GetHitEnergy(0), 10 / (1 + 0.01 * 10 / 0.5), 1e-4);
    EXPECT_NEAR(electron.GetHitEnergy(1), 20 / (1 + 0.01 * 20 / 1), 1e-4);
    EXPECT_NEAR(electron.GetHitEnergy(2), 30 / (1 + 0.01 * 30 / 2), 1e-4);
    // Neutral particles are not quenched by Birks
    EXPECT_NEAR(output.GetTrackByID(4).GetHitEnergy(0), 7, 1e-5);

    // Lindhard: the hit with a target nucleus (A = 40, Z = 18); A = 0 is not quenched
    const double factor = Lindhard(10, 40, 18);
    EXPECT_NEAR(factor, 0.2706, 2e-3);
    EXPECT_NEAR(output.GetTrackByID(1).GetHitEnergy(1), 10 * factor, 1e-4);
    EXPECT_NEAR(output.GetTrackByID(2).GetHitEnergy(0), 5, 1e-5);

    // The energies of the event are recomputed from the quenched hits
    const double scint = 10 / 1.2 + 20 / 1.2 + 30 / 1.15 + 7;
    const double gas = 10 * factor + 5;
    EXPECT_NEAR(output.GetEnergyInVolume(kScint), scint, 1e-3);
    EXPECT_NEAR(output.GetEnergyInVolume(kGas), gas, 1e-3);
    EXPECT_NEAR(output.GetTotalDepositedEnergy(), scint + gas, 1e-3);
    EXPECT_NEAR(output.GetSensitiveVolumeEnergy(), gas, 1e-3);
    EXPECT_NEAR(electron.GetDepositedEnergy(), 10 / 1.2 + 20 / 1.2 + 30 / 1.15, 1e-3);
    // The input is not modified
    EXPECT_NEAR(input.GetEnergyInVolume(kScint), 67, 1e-6);

    // Lindhard needs the hadronic target information in the simulation
    TRestGeant4Metadata noTarget("geant4Metadata", YAML::Load("seed: 1\n" + kTables));
    noTarget.fGeant4GeometryInfo.LoadGeometry(geometry.geometry);
    TRestGeant4QuenchingProcess lindhard("quenching", YAML::Load("{volume: [{name: gas, model: lindhard}]}"));
    lindhard.fGeant4Metadata = &noTarget;
    EXPECT_THROW(lindhard.Initialize(), std::runtime_error);

    // A volume without model (the error of the entry, wrapped by the field registry)
    EXPECT_THROW(TRestGeant4QuenchingProcess("quenching", YAML::Load("{volume: [{name: gas}]}")),
                 std::runtime_error);
}
