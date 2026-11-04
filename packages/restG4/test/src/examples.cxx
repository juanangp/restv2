/// The restG4 examples as tests (GoogleTest, BUILD_TESTING): each test copies examples/ into its own work
/// directory, runs the restG4 (and restManager) executables there as a user would (from the directory of
/// the example, with fewer events than the example) and checks the output with the REST classes. The
/// statistical checks use the legacy Validate*.C references with tolerances for the number of events.

#include <TRestGeant4Event.h>
#include <TRestGeant4Metadata.h>
#include <TRestGeant4Track.h>
#include <TRestRun.h>
#include <gtest/gtest.h>

#include <ROOT/RDataFrame.hxx>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;
using ROOT::Math::XYZVector;

namespace {

/// Copy of examples/ for the current test (work/<test name>/examples), created again for each test
fs::path Examples() {
    const auto* test = ::testing::UnitTest::GetInstance()->current_test_info();
    const fs::path directory = fs::path(TESTS_WORK_DIR) / test->name() / "examples";
    static std::string copied;
    if (copied != test->name()) {
        fs::remove_all(directory.parent_path());
        fs::create_directories(directory.parent_path());
        fs::copy(EXAMPLES_SOURCE_DIR, directory,
                 fs::copy_options::recursive | fs::copy_options::copy_symlinks);
        copied = test->name();
    }
    return directory;
}

/// Runs a command from the directory of an example; true if it succeeds
bool Run(const std::string& example, const std::string& command) {
    const std::string line = "cd '" + (Examples() / example).string() + "' && " + command;
    std::cout << "Running: " << line << std::endl;
    return std::system(line.c_str()) == 0;
}

/// restG4 with a configuration of an example and options (e.g. "-n 1000 -o output.root")
bool RestG4(const std::string& example, const std::string& config, const std::string& options = "") {
    return Run(example, std::string(RESTG4_EXECUTABLE) + " " + config + " " + options);
}

/// restManager with a configuration of an example, its input and its output
bool RestManager(const std::string& example, const std::string& config, const std::string& input,
                 const std::string& output = "") {
    return Run(example, std::string(RESTMANAGER_EXECUTABLE) + " --c " + config + " --i '" + input + "'" +
                            (output.empty() ? "" : " --o " + output));
}

std::string Path(const std::string& example, const std::string& file) {
    return (Examples() / example / file).string();
}

/// A restG4 output: its run, metadata and event
struct Simulation {
    TRestRun run;
    const TRestGeant4Metadata* metadata = nullptr;
    TRestGeant4Event* event = nullptr;

    explicit Simulation(const std::string& fileName) : run(fileName) {
        const auto metadataList = run.GetMetadataOfType<TRestGeant4Metadata>();
        if (!metadataList.empty()) metadata = metadataList.front();
        if (run.HasEvent("TRestGeant4Event")) {
            event = &run.GetEvent<TRestGeant4Event>("TRestGeant4Event");
            event->SetRestRun(&run);
        }
    }

    /// Calls function for each event
    void ForEachEvent(const std::function<void(const TRestGeant4Event&)>& function) {
        ASSERT_NE(event, nullptr) << "no TRestGeant4Event";
        for (long long entry = 0; entry < run.GetEntries(); entry++) {
            ASSERT_TRUE(run.GetEntry(entry));
            function(*event);
        }
    }
};

/// Angle (deg) between two directions
double AngleDeg(const XYZVector& a, const XYZVector& b) {
    const double cosine = a.Dot(b) / std::sqrt(a.Mag2() * b.Mag2());
    return std::acos(std::max(-1.0, std::min(1.0, cosine))) * 180.0 / M_PI;
}

/// Statistics of the primary of each event: angle to a direction (deg) and energy (keV)
struct Primaries {
    double thetaMin = std::numeric_limits<double>::infinity(), thetaMax = 0, thetaMean = 0;
    double energyMin = std::numeric_limits<double>::infinity(), energyMax = 0, energyMean = 0;
    long long entries = 0;
};

Primaries PrimaryStatistics(Simulation& simulation, const XYZVector& direction) {
    Primaries primaries;
    simulation.ForEachEvent([&](const TRestGeant4Event& event) {
        const double theta = AngleDeg(direction, event.GetPrimaryDirection(0));
        const double energy = event.GetPrimaryEnergy(0);
        primaries.thetaMin = std::min(primaries.thetaMin, theta);
        primaries.thetaMax = std::max(primaries.thetaMax, theta);
        primaries.thetaMean += theta;
        primaries.energyMin = std::min(primaries.energyMin, energy);
        primaries.energyMax = std::max(primaries.energyMax, energy);
        primaries.energyMean += energy;
        primaries.entries++;
    });
    if (primaries.entries > 0) {
        primaries.thetaMean /= primaries.entries;
        primaries.energyMean /= primaries.entries;
    }
    return primaries;
}

/// Relative difference
double Relative(double value, double reference) { return std::abs(value - reference) / std::abs(reference); }

}  // namespace

// --- 01.NLDBD: Decay0 events (Xe136 0vbb) in the gas, and the geometry and physics information ---
TEST(restG4Examples, NLDBD) {
    ASSERT_TRUE(RestG4("01.NLDBD", "NLDBD.yaml", "-o NLDBD.root"));
    Simulation simulation(Path("01.NLDBD", "NLDBD.root"));
    EXPECT_EQ(simulation.run.GetRunNumber(), 1);
    EXPECT_EQ(simulation.run.GetRunType(), "restG4");
    EXPECT_EQ(simulation.run.GetRunTag(), "NLDBD");
    EXPECT_EQ(simulation.run.GetEntries(), 100);
    ASSERT_NE(simulation.metadata, nullptr);
    EXPECT_EQ(simulation.metadata->GetNumberOfSensitiveVolumes(), 1u);

    const auto& geometry = simulation.metadata->GetGeant4GeometryInfo();
    EXPECT_EQ(geometry.GetAllLogicalVolumes().size(), 4u);
    EXPECT_EQ(geometry.GetAllPhysicalVolumes().size(), 4u);
    const auto gasVolumes = geometry.GetAllPhysicalVolumesFromLogical("gasVolume");
    ASSERT_EQ(gasVolumes.size(), 1u);
    EXPECT_EQ(gasVolumes[0], "gas");

    // Decay0 gives the electrons; the gammas come from them
    const auto particles = simulation.metadata->GetGeant4PhysicsInfo().GetAllParticles();
    EXPECT_EQ(particles, (std::set<std::string>{"e-", "gamma"}));
    const auto processes = simulation.metadata->GetGeant4PhysicsInfo().GetAllProcesses();
    EXPECT_TRUE(processes.count("Transportation"));
    EXPECT_TRUE(processes.count("compt"));

    // Q value 2458 keV, part of the energy escapes the gas (legacy 2237 keV)
    double sensitiveEnergy = 0;
    simulation.ForEachEvent([&](const TRestGeant4Event& event) {
        sensitiveEnergy += event.GetSensitiveVolumeEnergy() / simulation.run.GetEntries();
    });
    EXPECT_GT(sensitiveEnergy, 1800);
    EXPECT_LT(sensitiveEnergy, 2460);
}

// --- 02.TREXDM: full chain decays from the readout surface, and its analysis ---
TEST(restG4Examples, TREXDM) {
    ASSERT_TRUE(RestG4("02.TREXDM", "FullChainFromReadout.yaml", "-n 20 -o TREXDM.root"));
    ASSERT_TRUE(RestManager("02.TREXDM", "g4Analysis.yaml", "TREXDM.root", "TREXDM_g4Analysis.root"));
    Simulation simulation(Path("02.TREXDM", "TREXDM.root"));
    EXPECT_EQ(simulation.run.GetRunTag(), "Rn222_readout_surface_1um");
    EXPECT_GT(simulation.run.GetEntries(), 0);
}

// --- 03.Fluorescence: copper fluorescences (8.05 keV) of 30 keV gammas ---
TEST(restG4Examples, Fluorescence) {
    constexpr int events = 10000;
    ASSERT_TRUE(RestG4("03.Fluorescence", "gamma.yaml", "-n " + std::to_string(events) + " -o gamma.root"));
    ASSERT_TRUE(RestManager("03.Fluorescence", "g4Analysis.yaml", "gamma.root", "gamma_analysis.root"));
    ROOT::RDataFrame frame("AnalysisTree", Path("03.Fluorescence", "gamma_analysis.root"));
    auto peak = frame.Filter("g4Ana_totalEdep > 1 && g4Ana_totalEdep < 10");
    const double mean = *peak.Mean("g4Ana_totalEdep");
    EXPECT_GT(mean, 7.5);
    EXPECT_LT(mean, 8.5);
    // Legacy: more than 8000 of 30000
    EXPECT_GT(*peak.Count(), 0.24 * events);
}

// --- 04.MuonScan: muons from a point, a wall, a circle and the cosmic generator ---
TEST(restG4Examples, MuonsFromPoint) {
    constexpr int events = 1000;
    ASSERT_TRUE(
        RestG4("04.MuonScan", "MuonsFromPoint.yaml", "-n " + std::to_string(events) + " -o point.root"));
    Simulation simulation(Path("04.MuonScan", "point.root"));
    EXPECT_EQ(simulation.run.GetRunNumber(), 111);
    EXPECT_EQ(simulation.run.GetRunType(), "restG4");
    EXPECT_EQ(simulation.run.GetRunTag(), "MuonsFromPoint");
    EXPECT_EQ(simulation.run.GetEntries(), events);
    ASSERT_NE(simulation.metadata, nullptr);
    EXPECT_EQ(simulation.metadata->GetNumberOfActiveVolumes(), 2u);
    double totalEnergy = 0, sensitiveEnergy = 0;
    simulation.ForEachEvent([&](const TRestGeant4Event& event) {
        totalEnergy += event.GetTotalDepositedEnergy() / simulation.run.GetEntries();
        sensitiveEnergy += event.GetSensitiveVolumeEnergy() / simulation.run.GetEntries();
    });
    EXPECT_LT(Relative(totalEnergy, 18.281), 0.2) << totalEnergy;
    EXPECT_LT(Relative(sensitiveEnergy, 8.71483), 0.2) << sensitiveEnergy;
}

TEST(restG4Examples, CosmicMuonsFromWall) {
    ASSERT_TRUE(RestG4("04.MuonScan", "CosmicMuonsFromWall.yaml", "-o wall.root"));
    Simulation simulation(Path("04.MuonScan", "wall.root"));
    EXPECT_EQ(simulation.run.GetRunTag(), "MuonsFromWall");
    // 2500 muons, about 1000 through both detectors
    EXPECT_GE(simulation.run.GetEntries(), 900);
    EXPECT_LE(simulation.run.GetEntries(), 1050);
    double radius2 = 0;
    simulation.ForEachEvent([&](const TRestGeant4Event& event) {
        const auto origin = event.GetPrimaryPosition();
        radius2 += (origin.X() * origin.X() + origin.Y() * origin.Y()) / simulation.run.GetEntries();
    });
    EXPECT_GT(radius2, 10000);
    EXPECT_LT(radius2, 20000);
}

TEST(restG4Examples, CosmicMuonsFromCircle) {
    ASSERT_TRUE(RestG4("04.MuonScan", "CosmicMuonsFromCircle.yaml", "-o circle.root"));
    Simulation simulation(Path("04.MuonScan", "circle.root"));
    EXPECT_EQ(simulation.run.GetRunTag(), "MuonsFromCircle");
    EXPECT_GE(simulation.run.GetEntries(), 1300);
    EXPECT_LE(simulation.run.GetEntries(), 1450);
    double radiusMean = 0, radiusMax = 0;
    simulation.ForEachEvent([&](const TRestGeant4Event& event) {
        const auto origin = event.GetPrimaryPosition();
        const double radius = std::hypot(origin.X(), origin.Z());
        radiusMean += radius / simulation.run.GetEntries();
        radiusMax = std::max(radiusMax, radius);
    });
    // Circle of 400 mm, the muons through the detectors (legacy mean 292 mm)
    EXPECT_LE(radiusMax, 400 + 1e-3);
    EXPECT_GT(radiusMean, 250);
    EXPECT_LT(radiusMean, 330);
}

TEST(restG4Examples, CosmicMuons) {
    ASSERT_TRUE(RestG4("04.MuonScan", "CosmicMuons.yaml", "-e 1000 -o cosmic.root"));
    Simulation simulation(Path("04.MuonScan", "cosmic.root"));
    EXPECT_EQ(simulation.run.GetRunTag(), "CosmicMuons");
    EXPECT_GE(simulation.run.GetEntries(), 1000);
    double totalEnergy = 0, sensitiveEnergy = 0;
    simulation.ForEachEvent([&](const TRestGeant4Event& event) {
        totalEnergy += event.GetTotalDepositedEnergy() / simulation.run.GetEntries();
        sensitiveEnergy += event.GetSensitiveVolumeEnergy() / simulation.run.GetEntries();
    });
    EXPECT_LT(Relative(totalEnergy, 15.244), 0.2) << totalEnergy;
    EXPECT_LT(Relative(sensitiveEnergy, 14.8731), 0.2) << sensitiveEnergy;
}

// --- 05.PandaXIII: Decay0 events in the gas of the PandaX-III geometry ---
TEST(restG4Examples, PandaXIII) {
    ASSERT_TRUE(RestG4("05.PandaXIII", "Xe136bb0n.yaml", "-n 100 -o Xe136bb0n.root"));
    Simulation simulation(Path("05.PandaXIII", "Xe136bb0n.root"));
    EXPECT_EQ(simulation.run.GetRunNumber(), 1);
    EXPECT_EQ(simulation.run.GetRunType(), "restG4");
    EXPECT_EQ(simulation.run.GetRunTag(), "Xe136bb0n");
    EXPECT_EQ(simulation.run.GetEntries(), 100);
    ASSERT_NE(simulation.metadata, nullptr);
    EXPECT_EQ(simulation.metadata->GetNumberOfActiveVolumes(), 1u);
}

// --- 06.IonRecoils: F20 recoils in the gas ---
TEST(restG4Examples, IonRecoils) {
    ASSERT_TRUE(RestG4("06.IonRecoils", "recoils.yaml", "-o recoils.root"));
    Simulation simulation(Path("06.IonRecoils", "recoils.root"));
    EXPECT_EQ(simulation.run.GetRunTag(), "F20");
    EXPECT_EQ(simulation.run.GetEntries(), 2);
    EXPECT_GT(simulation.run.GetEndTime(), simulation.run.GetStartTime());
    simulation.ForEachEvent([&](const TRestGeant4Event& event) {
        EXPECT_EQ(event.GetPrimaryParticleName(0), "F20");
        EXPECT_GT(event.GetSensitiveVolumeEnergy(), 0);
        EXPECT_LE(event.GetSensitiveVolumeEnergy(), 10 + 1e-6);
    });
}

// --- 07.FullChainDecay: the isotopes decaying in the full chain of U238 (16) and in a single decay ---
namespace {
std::set<std::string> DecayingIsotopes(Simulation& simulation) {
    std::set<std::string> isotopes;
    simulation.ForEachEvent([&](const TRestGeant4Event& event) {
        isotopes.insert(event.IsSubEvent() ? event.GetSubEventParticleName()
                                           : event.GetPrimaryParticleName(0));
    });
    return isotopes;
}
}  // namespace

TEST(restG4Examples, FullChainDecay) {
    ASSERT_TRUE(RestG4("07.FullChainDecay", "fullChain.yaml", "-o fullChain.root"));
    Simulation fullChain(Path("07.FullChainDecay", "fullChain.root"));
    EXPECT_EQ(DecayingIsotopes(fullChain).size(), 16u);

    ASSERT_TRUE(RestG4("07.FullChainDecay", "singleDecay.yaml", "-n 1000 -o singleDecay.root"));
    Simulation singleDecay(Path("07.FullChainDecay", "singleDecay.root"));
    EXPECT_EQ(DecayingIsotopes(singleDecay), (std::set<std::string>{"U238"}));

    // Another isotope with --set (parameter isotope)
    ASSERT_TRUE(RestG4("07.FullChainDecay", "singleDecay.yaml", "-n 1000 --set isotope=Be7 -o Be7.root"));
    Simulation be7(Path("07.FullChainDecay", "Be7.root"));
    EXPECT_EQ(be7.run.GetRunTag(), "Be7");
    EXPECT_EQ(DecayingIsotopes(be7), (std::set<std::string>{"Be7"}));
}

// --- 08.Alphas: alphas from a foil (energy and thickness as parameters), analysis and plots ---
TEST(restG4Examples, Alphas) {
    constexpr int events = 2000;
    const std::string n = "-n " + std::to_string(events);
    fs::create_directories(Examples() / "08.Alphas" / "data");
    // Fraction of the alphas leaving the foil with energy in the gas (legacy: 1926, 7487 and 9432 of 10000)
    const std::vector<std::tuple<std::string, std::string, double>> runs = {
        {"1", "5", 0.1926}, {"5", "5", 0.7487}, {"5", "1", 0.9432}};
    for (const auto& [energy, foil, fraction] : runs) {
        const std::string tag = energy + "MeV_" + foil + "um";
        ASSERT_TRUE(
            RestG4("08.Alphas", "alphas.yaml", n + " --set energy=" + energy + " --set foil=" + foil));
        ASSERT_TRUE(RestManager("08.Alphas", "analysis.yaml", "data/Run_" + tag + ".root"));
        Simulation simulation(Path("08.Alphas", "data/Run_" + tag + ".root"));
        EXPECT_EQ(simulation.run.GetRunTag(), tag);
        EXPECT_NEAR(double(simulation.run.GetEntries()) / events, fraction, 0.04) << tag;

        // Isotropic: mean theta pi/2, mean phi 0
        ROOT::RDataFrame frame("AnalysisTree", Path("08.Alphas", "data/Run_g4Analysis_" + tag + ".root"));
        EXPECT_NEAR(*frame.Mean("g4Ana_thetaPrimary"), M_PI / 2, 0.1) << tag;
        EXPECT_NEAR(*frame.Mean("g4Ana_phiPrimary"), 0, 0.2) << tag;
    }
    ASSERT_TRUE(RestManager("08.Alphas", "plots.yaml", "data/Run_g4Analysis_*.root"));
    EXPECT_TRUE(fs::exists(Path("08.Alphas", "plots.root")));

    // --update-gdml writes the parameter in the constant of the GDML, without simulating
    ASSERT_TRUE(RestG4("08.Alphas", "alphas.yaml", "--set foil=1 --update-gdml"));
    std::ifstream gdml(Path("08.Alphas", "geometry/setup.gdml"));
    std::stringstream text;
    text << gdml.rdbuf();
    EXPECT_NE(text.str().find(R"(<constant name="foil" value="1"/>)"), std::string::npos);
}

// --- 09.Pb210_Shield: Pb210 chain in the lead, events with (0, 10) keV in the xenon ---
TEST(restG4Examples, Pb210Shield) {
    constexpr int events = 20000;
    ASSERT_TRUE(RestG4("09.Pb210_Shield", "Pb210.yaml", "-n " + std::to_string(events) + " -o Pb210.root"));
    Simulation simulation(Path("09.Pb210_Shield", "Pb210.root"));
    // Legacy: 252 of 100000
    const double expected = 252.0 * events / 100000;
    EXPECT_NEAR(simulation.run.GetEntries(), expected, 4 * std::sqrt(expected));
}

// --- 10.Geometries: geantinos through the assemblies of the IAXO geometry, straight at the speed of light
// ---
TEST(restG4Examples, Geometries) {
    ASSERT_TRUE(RestG4("10.Geometries", "Assembly.yaml", "-n 100 -o Assembly.root"));
    Simulation simulation(Path("10.Geometries", "Assembly.root"));
    EXPECT_EQ(simulation.run.GetEntries(), 100);
    ASSERT_TRUE(simulation.run.GetEntry(0));
    ASSERT_NE(simulation.event, nullptr);
    constexpr double speedOfLight = 299792.458;  // mm/us (REST time unit)
    for (const auto& track : simulation.event->GetTracks()) {
        const auto hits = track.GetHits();
        for (size_t i = 0; i + 1 < hits.GetNumberOfHits(); i++) {
            const XYZVector step = hits.GetPosition(i + 1) - hits.GetPosition(i);
            const double time = hits.GetTime(i + 1) - hits.GetTime(i);
            if (step.R() == 0 || time <= 0) continue;
            EXPECT_LT(Relative(step.R() / time, speedOfLight), 1e-4);
            EXPECT_LT(AngleDeg(track.GetHitMomentumDirection(i), step), 1e-2);
            EXPECT_NEAR(track.GetHitMomentumDirection(i).R(), 1, 1e-4);
        }
    }
}

// --- 11.Xrays: the Fe55 peak (5.9 keV) and the gammas of the quantum efficiency ---
TEST(restG4Examples, Xrays) {
    constexpr int events = 20000;
    ASSERT_TRUE(RestG4("11.Xrays", "Fe55.yaml", "-n " + std::to_string(events) + " -o Fe55.root"));
    ASSERT_TRUE(RestManager("11.Xrays", "analysis.yaml", "Fe55.root", "Fe55_analysis.root"));
    ROOT::RDataFrame frame("AnalysisTree", Path("11.Xrays", "Fe55_analysis.root"));
    // Legacy: 1600 to 1750 of 100000 in the peak
    const double fraction =
        double(*frame.Filter("g4Ana_totalEdep > 5.7 && g4Ana_totalEdep < 5.95").Count()) / events;
    EXPECT_GT(fraction, 0.0145);
    EXPECT_LT(fraction, 0.019);

    ASSERT_TRUE(RestG4("11.Xrays", "xrays.yaml", "-o xrays.root"));
    ASSERT_TRUE(RestManager("11.Xrays", "analysis.yaml", "xrays.root", "xrays_analysis.root"));
    EXPECT_GT(*ROOT::RDataFrame("AnalysisTree", Path("11.Xrays", "xrays_analysis.root")).Count(), 0u);
}

// --- 12.Generators: the distributions of the primaries ---
TEST(restG4Examples, CosineSquaredCircle) {
    ASSERT_TRUE(RestG4("12.Generators", "CosineSquaredCircle.yaml", "-n 20000"));
    Simulation simulation(Path("12.Generators", "CosineSquaredCircle.root"));
    EXPECT_EQ(simulation.run.GetRunTag(), "CosineSquaredCircle");
    EXPECT_EQ(simulation.run.GetEntries(), 20000);
    double radiusMean = 0, radiusMax = 0;
    simulation.ForEachEvent([&](const TRestGeant4Event& event) {
        const double radius = std::hypot(event.GetPrimaryPosition().X(), event.GetPrimaryPosition().Z());
        radiusMean += radius / simulation.run.GetEntries();
        radiusMax = std::max(radiusMax, radius);
    });
    // Uniform in a circle of 400 mm: mean 2/3 R
    EXPECT_LE(radiusMax, 400 + 1e-3);
    EXPECT_LT(Relative(radiusMean, 2.0 / 3 * 400), 0.02);
    const auto primaries = PrimaryStatistics(simulation, {0, -1, 0});
    EXPECT_GE(primaries.energyMin, 5000 - 1e-6);
    EXPECT_LE(primaries.energyMax, 150000 + 1e-6);
    EXPECT_LT(Relative(primaries.energyMean, 53818.8), 0.1);
    // Density cos^2 in [0, 90] deg: mean (pi^2/16 - 1/4) / (pi/4) rad
    EXPECT_LE(primaries.thetaMax, 90 + 1e-6);
    EXPECT_LT(Relative(primaries.thetaMean, (M_PI * M_PI / 16 - 0.25) / (M_PI / 4) * 180 / M_PI), 0.03);
}

TEST(restG4Examples, CosmicGenerator) {
    ASSERT_TRUE(RestG4("12.Generators", "CosmicGenerator.yaml", "-e 20000"));
    Simulation simulation(Path("12.Generators", "CosmicGenerator.root"));
    EXPECT_GE(simulation.run.GetEntries(), 20000);
    ASSERT_NE(simulation.metadata, nullptr);
    // The origins on the sphere of the generation surface (pi R^2)
    const double radius = std::sqrt(simulation.metadata->GetCosmicGeneratorSurfaceCm2() / M_PI) * 10;
    EXPECT_LT(Relative(radius, 173.20508), 0.01);
    simulation.ForEachEvent([&](const TRestGeant4Event& event) {
        EXPECT_LT(Relative(event.GetPrimaryPosition().R(), radius), 1e-3);
    });
    const auto primaries = PrimaryStatistics(simulation, {0.1, -0.5, 1.25});
    EXPECT_LE(primaries.thetaMax, 90 + 1e-6);
    EXPECT_LT(Relative(primaries.thetaMean, 37.8595), 0.05);
    EXPECT_GE(primaries.energyMin, 2e5 - 1);
    EXPECT_LE(primaries.energyMax, 5e9);
}

TEST(restG4Examples, CosmicMuonsEnergyAngularCorrelated) {
    ASSERT_TRUE(RestG4("12.Generators", "CosmicMuonsEnergyAngularCorrelated.yaml", "-n 20000"));
    Simulation simulation(Path("12.Generators", "CosmicMuonsEnergyAngularCorrelated.root"));
    EXPECT_EQ(simulation.run.GetEntries(), 20000);
    const auto primaries = PrimaryStatistics(simulation, {0, -1, 0});
    EXPECT_GE(primaries.thetaMin, 20 - 1e-6);
    EXPECT_LE(primaries.thetaMax, 50 + 1e-6);
    EXPECT_LT(Relative(primaries.thetaMean, 34.0558), 0.03);
    EXPECT_GE(primaries.energyMin, 1e5 - 1e-6);
    EXPECT_LE(primaries.energyMax, 8.5e5 + 1e-6);
    EXPECT_LT(Relative(primaries.energyMean, 507020), 0.05);
}

TEST(restG4Examples, EnergyAndAngularRange) {
    ASSERT_TRUE(RestG4("12.Generators", "EnergyAndAngularRange.yaml", "-n 20000"));
    Simulation simulation(Path("12.Generators", "EnergyAndAngularRange.root"));
    EXPECT_EQ(simulation.run.GetEntries(), 20000);
    const auto primaries = PrimaryStatistics(simulation, {0, -1, 0});
    EXPECT_LE(primaries.thetaMax, 55 + 1e-6);
    EXPECT_LT(Relative(primaries.thetaMean, 23.1283), 0.03);
    EXPECT_LE(primaries.energyMax, 15000 + 1e-6);
    EXPECT_LT(Relative(primaries.energyMean, 2915.84), 0.1);
}

TEST(restG4Examples, IsotropicWithRange) {
    ASSERT_TRUE(RestG4("12.Generators", "IsotropicWithRange.yaml", "-n 20000"));
    Simulation simulation(Path("12.Generators", "IsotropicWithRange.root"));
    EXPECT_EQ(simulation.run.GetEntries(), 20000);
    const auto primaries = PrimaryStatistics(simulation, {0, -1, 0});
    // Isotropic in a cone of 15 deg: density sin(theta), mean about 2/3 of the half angle
    EXPECT_LE(primaries.thetaMax, 15 + 1e-6);
    EXPECT_LT(Relative(primaries.thetaMean, 10.0), 0.02);
    EXPECT_NEAR(primaries.energyMin, 10000, 1e-6);
    EXPECT_NEAR(primaries.energyMax, 10000, 1e-6);
}

// --- 13.IAXO: cosmic neutrons and muons on the IAXO-D1 geometry ---
TEST(restG4Examples, IAXO) {
    for (const std::string config : {"Neutrons", "Muons"}) {
        ASSERT_TRUE(RestG4("13.IAXO", config + ".yaml", "-e 1 -o " + config + ".root"));
        Simulation simulation(Path("13.IAXO", config + ".root"));
        EXPECT_GE(simulation.run.GetEntries(), 1) << config;
        ASSERT_NE(simulation.metadata, nullptr);
        const auto& geometry = simulation.metadata->GetGeant4GeometryInfo();
        EXPECT_EQ(geometry.GetAllPhysicalVolumes().size(), 309u) << config;
        EXPECT_EQ(geometry.GetAllLogicalVolumes().size(), 22u) << config;
    }
}

// --- 14.DetectorResponse: efficiency of a Xe-Ne detector to (0, 15) keV gammas ---
TEST(restG4Examples, DetectorResponse) {
    constexpr int events = 20000;
    ASSERT_TRUE(RestG4("14.DetectorResponse", "DetectorResponse.yaml",
                       "-n " + std::to_string(events) + " -o response.root"));
    ASSERT_TRUE(
        RestManager("14.DetectorResponse", "analysis.yaml", "response.root", "response_analysis.root"));
    ROOT::RDataFrame frame("AnalysisTree", Path("14.DetectorResponse", "response_analysis.root"));
    // Legacy: 0.91 (150000 events)
    const double efficiency = double(*frame.Filter("g4Ana_totalEdep > 0").Count()) / events;
    EXPECT_NEAR(efficiency, 0.91, 0.03);
}
