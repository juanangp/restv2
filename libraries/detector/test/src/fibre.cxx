/// Unit tests of the fibre readout (TRestFibreReadout) and of the light processes: the primary scintillation
/// (TRestDetectorScintillationProcess, light yield of the medium) and the collection of the light by the
/// channels and its photoelectrons (TRestDetectorLightCollectionProcess, models localCapture and attenuation,
/// quantum efficiency). The readout is a box of 100 mm with two families of fibres of pitch
/// 20 mm, built in memory; the attenuation model uses a minimal readout of the test (a bar read at x = 0).
/// The processes get the readout and the medium from a TRestManager created from a YAML string.

#include <TFile.h>
#include <TGraph.h>
#include <TRestDetectorHitsEvent.h>
#include <TRestDetectorHitsToSignalProcess.h>
#include <TRestDetectorLightCollectionProcess.h>
#include <TRestDetectorMedium.h>
#include <TRestDetectorReadoutManager.h>
#include <TRestDetectorScintillationProcess.h>
#include <TRestDetectorSignalEvent.h>
#include <TRestDetectorSignalToHitsProcess.h>
#include <TRestFibreReadout.h>
#include <TRestManager.h>
#include <TRestRun.h>
#include <TTree.h>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

using ROOT::Math::XYZVector;

namespace {

/// Analysis tree in memory (no file) given to the processes through a TRestRun, to read their observables
struct AnalysisTree {
    TRestRun run;
    std::unique_ptr<TTree> tree;

    AnalysisTree() : tree(std::make_unique<TTree>("AnalysisTree", "AnalysisTree")) {
        tree->SetDirectory(nullptr);
        run.fOutputAnalysisTree = tree.get();
    }
    ~AnalysisTree() { run.fOutputAnalysisTree = nullptr; }

    template <typename T>
    T Get(const std::string& name) const {
        return run.GetObservable<T>(name);
    }
};

// --- Fibre readout of the tests: box of 100 mm centred at (0, 0, 50) (x, y in [-50, 50], z in [0, 100]).
// Fibres along Y (transverse Z and X): z = 10 + 20 i, x = -40 + 20 j, physical IDs 0-24 (i * 5 + j).
// Fibres along X (transverse Y and Z): y = -40 + 20 i, z = 20 + 20 j, physical IDs 25-49 (25 + i * 5 + j).
// Without decoding file: DAQ ID = physical ID ---
YAML::Node ReadoutNode() {
    return YAML::Load(
        "class: TRestFibreReadout\n"
        "name: fibreTest\n"
        "readoutParameters:\n"
        "  size: [100mm, 100mm, 100mm]\n"
        "  position: [0, 0, 50mm]\n"
        "  fibres:\n"
        "    - {axis: Y, pitch: 20mm, offset: [10mm, 10mm], volume: fibreYVolume}\n"
        "    - {axis: X, pitch: 20mm, offset: [10mm, 20mm], volume: fibreXVolume}\n");
}

std::unique_ptr<TRestFibreReadout> BuildReadout() {
    auto readout = std::make_unique<TRestFibreReadout>("fibreTest", ReadoutNode());
    readout->BuildGeometry();
    return readout;
}

/// A bar along X (x in [0, 100]) read at x = 0: one channel (0), distance to the readout x
class BarReadout : public TRestDetectorReadout {
   public:
    BarReadout() : TRestDetectorReadout() { fName = "bar"; }
    void BuildGeometry() override {}
    int GetChannelFromPosition(double x, double, double) const override {
        return x >= 0 && x <= 100 ? 0 : -1;
    }
    using TRestDetectorReadout::GetDistanceToReadout;
    double GetDistanceToReadout(int daqID, const XYZVector& position) const override {
        return daqID == 0 ? position.X() : std::numeric_limits<double>::quiet_NaN();
    }
};

/// A TRestManager with the fibre readout (fibreTest) and the bar (bar) in its TRestDetectorReadoutManager
/// and a scintillator medium with only `light` (no charge)
struct Detector {
    std::unique_ptr<TRestManager> manager;
    TRestDetectorReadout* readout = nullptr;
};

Detector MakeDetector(const std::string& light =
                          "{lightYield: 100, attenuationLength: 100 mm, "
                          "effectiveLightSpeed: 100 mm/us}") {
    const YAML::Node node = YAML::Load(
        "class: TRestManager\n"
        "readouts:\n"
        "  class: TRestDetectorReadoutManager\n"
        "  readout: fibreTest\n"
        "scintillator:\n"
        "  class: TRestDetectorMedium\n"
        "  name: LS\n"
        "  state: liquid\n"
        "  light: " +
        light + "\n");
    Detector detector;
    detector.manager = std::make_unique<TRestManager>("manager", node);
    auto* readouts = dynamic_cast<TRestDetectorReadoutManager*>(
        detector.manager->GetMetadataClass("TRestDetectorReadoutManager"));
    if (!readouts) throw std::runtime_error("no TRestDetectorReadoutManager in the manager");
    auto readout = BuildReadout();
    detector.readout = readout.get();
    readouts->fReadoutMap["fibreTest"] = readout.release();  // owned by the readout manager
    readouts->fReadoutMap["bar"] = new BarReadout();
    return detector;
}

/// A process initialized with the manager of the detector and the analysis tree
template <typename Process>
std::unique_ptr<Process> MakeProcess(const Detector& detector, AnalysisTree& analysis,
                                     const std::string& name, const std::string& yaml) {
    auto process = std::make_unique<Process>(name, YAML::Load(yaml));
    process->SetManager(detector.manager.get());
    process->SetRunInfo(&analysis.run);
    process->Initialize();
    return process;
}

}  // namespace

// ============================================================================
// Fibre readout
// ============================================================================

TEST(FibreReadout, GridChannelsAndPositions) {
    const auto readout = BuildReadout();
    EXPECT_EQ(readout->GetNumberOfFibres(), 50);

    // Closest fibre: the one along Y at z = 30, x = 0 (physical ID 1 * 5 + 2)
    EXPECT_EQ(readout->GetChannelFromPosition(0.5, 33, 31), 7);
    // On a fibre along X (y = -20, z = 60: 25 + 1 * 5 + 2)
    EXPECT_EQ(readout->GetChannelFromPosition(10, -20, 60), 32);
    // Outside the box
    EXPECT_EQ(readout->GetChannelFromPosition(0, 0, -1), -1);
    EXPECT_EQ(readout->GetChannelFromPosition(0, 60, 50), -1);

    // A fibre measures its transverse coordinates, NaN along it
    const auto yFibre = readout->GetPositionFromChannel(7);
    EXPECT_NEAR(yFibre.X(), 0, 1e-9);
    EXPECT_TRUE(std::isnan(yFibre.Y()));
    EXPECT_NEAR(yFibre.Z(), 30, 1e-9);
    const auto xFibre = readout->GetPositionFromChannel(32);
    EXPECT_TRUE(std::isnan(xFibre.X()));
    EXPECT_NEAR(xFibre.Y(), -20, 1e-9);
    EXPECT_NEAR(xFibre.Z(), 60, 1e-9);
    EXPECT_TRUE(std::isnan(readout->GetPositionFromChannel(50).X()));  // not a fibre

    // Neighbours in the same row, none across rows or families
    EXPECT_EQ(readout->GetAdjacentChannels(7), std::make_pair(6, 8));
    EXPECT_EQ(readout->GetAdjacentChannels(5), std::make_pair(-1, 6));
    EXPECT_EQ(readout->GetAdjacentChannels(24), std::make_pair(23, -1));

    const auto [low, high] = readout->GetChannelsBoundingBox();
    EXPECT_NEAR(low.X(), -50, 1e-9);
    EXPECT_NEAR(high.Z(), 100, 1e-9);
}

TEST(FibreReadout, ChannelsInRange) {
    const auto readout = BuildReadout();

    // On the fibre along Y at z = 30, x = 0 (ID 7): it and the fibres along X at y = 0, z = 20 and z = 40
    // (IDs 35 and 36, 10 mm away) are within 15 mm; the others are 20 mm or more away
    const auto channels = readout->GetChannelsInRange({0, 0, 30}, 15);
    ASSERT_EQ(channels.size(), 3u);
    std::set<int> ids;
    for (const auto& channel : channels) {
        ids.insert(channel.daqID);
        EXPECT_NEAR(channel.distance, channel.daqID == 7 ? 0 : 10, 1e-9);
        // The position is on the fibre: it gives back the same channel
        const auto& p = channel.position;
        EXPECT_EQ(readout->GetChannelFromPosition(p.X(), p.Y(), p.Z()), channel.daqID);
    }
    EXPECT_EQ(ids, std::set<int>({7, 35, 36}));
    EXPECT_EQ(readout->GetChannelsInRange({0, 0, 30}, 5).size(), 1u);

    // Along X outside the box: the fibres along X do not reach it, those along Y are too far
    EXPECT_TRUE(readout->GetChannelsInRange({60, 0, 30}, 15).empty());
}

TEST(FibreReadout, WrongParameters) {
    YAML::Node node = ReadoutNode();
    node["readoutParameters"]["fibres"][0]["axis"] = "W";
    EXPECT_ANY_THROW(TRestFibreReadout("fibreTest", node));

    node = ReadoutNode();
    node["readoutParameters"]["fibres"][1]["pitch"] = "-1 mm";
    EXPECT_ANY_THROW(TRestFibreReadout("fibreTest", node));
}

TEST(FibreReadout, GDMLPlacements) {
    const auto readout = BuildReadout();
    const std::string fileName = std::string(TESTS_WORK_DIR) + "/fibres.xml";
    ASSERT_TRUE(readout->WriteGDMLPlacements(fileName));

    std::ifstream file(fileName);
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    size_t count = 0;
    for (size_t at = text.find("<physvol"); at != std::string::npos; at = text.find("<physvol", at + 1)) {
        count++;
    }
    EXPECT_EQ(count, 50u);
    // The first fibre along Y (z = 10, x = -40), relative to the centre (0, 0, 50), rotated to Y
    EXPECT_NE(text.find("<physvol name=\"fibre_0\" copynumber=\"0\">"), std::string::npos);
    EXPECT_NE(text.find("name=\"fibre_0_position\" unit=\"mm\" x=\"-40\" y=\"0\" z=\"-40\""),
              std::string::npos);
    EXPECT_NE(text.find("name=\"fibre_0_rotation\" unit=\"deg\" x=\"90\" y=\"0\" z=\"0\""),
              std::string::npos);
    EXPECT_NE(text.find("<volumeref ref=\"fibreXVolume\"/>"), std::string::npos);
}

TEST(FibreReadout, ExportAndImportWithManager) {
    const std::string fileName = std::string(TESTS_WORK_DIR) + "/fibre_test_readout.root";
    {
        const auto readout = BuildReadout();
        std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "RECREATE"));
        ASSERT_TRUE(file && !file->IsZombie());
        ASSERT_TRUE(readout->Export(file.get(), "fibreTest", "test_decoding"));
        file->Close();
    }

    const YAML::Node node = YAML::Load(
        "readout: fibreTest\n"
        "readouts:\n"
        "  - {instanceName: fibreTest, decodingName: test_decoding, inputFileName: \"" +
        fileName + "\"}\n");
    TRestDetectorReadoutManager manager("readouts", node);
    manager.LoadReadout();
    std::string name;
    TRestDetectorReadout* readout = manager.SelectReadout(name, "test");
    ASSERT_NE(readout, nullptr);
    EXPECT_EQ(readout->GetClassName(), "TRestFibreReadout");
    EXPECT_EQ(readout->GetChannelFromPosition(0.5, 33, 31), 7);
    EXPECT_EQ(readout->GetChannelsInRange({0, 0, 30}, 15).size(), 3u);
}

// ============================================================================
// Light: scintillation, collection and photoelectrons
// ============================================================================

TEST(DetectorMedium, LightYield) {
    TRestDetectorMedium medium("LS", YAML::Load("{state: liquid, light: {lightYield: 12}}"));
    EXPECT_DOUBLE_EQ(medium.GetLightResponse("test").GetLightYield(), 12);
    EXPECT_ANY_THROW(TRestDetectorMedium("LS", YAML::Load("{state: liquid, light: {lightYield: -1}}")));
}

TEST(DetectorScintillationProcess, PhotonsPerHit) {
    auto detector = MakeDetector();
    AnalysisTree analysis;
    auto process = MakeProcess<TRestDetectorScintillationProcess>(detector, analysis, "scintillation",
                                                                  "{seed: 7, observables: [NPhotons]}");
    EXPECT_EQ(process->fMediumName, "LS");

    // 10 keV, 100 photons per keV: 1000 photons on average, at the same position and time
    TRestDetectorHitsEvent hits;
    hits.SetID(3);
    hits.AddHit(1, 2, 30, 10, 2);
    hits.AddHit(5, 5, 5, 0, 2);  // no energy: dropped
    TRestDetectorHitsEvent photons;
    ASSERT_TRUE(process->ProcessEvent(hits, photons));
    EXPECT_EQ(photons.GetID(), 3);
    ASSERT_EQ(photons.GetNumberOfHits(), 1u);
    EXPECT_NEAR(photons.GetEnergy(0), 1000, 5 * std::sqrt(1000.));
    EXPECT_EQ(photons.GetEnergy(0), std::round(photons.GetEnergy(0)));
    EXPECT_NEAR(photons.GetZ(0), 30, 1e-9);
    EXPECT_DOUBLE_EQ(photons.GetTime(0), 2);
    EXPECT_DOUBLE_EQ(analysis.Get<double>("scintillation_NPhotons"), photons.GetEnergy(0));

    // Same seed and event: the same photons
    TRestDetectorHitsEvent again;
    ASSERT_TRUE(process->ProcessEvent(hits, again));
    EXPECT_DOUBLE_EQ(again.GetEnergy(0), photons.GetEnergy(0));

    // Without lightYield
    auto noYield = MakeDetector("{attenuationLength: 1 m}");
    TRestDetectorScintillationProcess failing("scintillation", YAML::Load("{}"));
    failing.SetManager(noYield.manager.get());
    EXPECT_THROW(failing.Initialize(), std::runtime_error);
}

TEST(DetectorLightCollectionProcess, LocalCaptureByTheFibres) {
    auto detector = MakeDetector();
    AnalysisTree analysis;
    auto process = MakeProcess<TRestDetectorLightCollectionProcess>(
        detector, analysis, "lightCollection",
        "{model: localCapture, captureProbability: \"0.2+0*x\", maxDistance: 15mm, seed: 5, "
        "observables: [NPhotoelectrons, NHitsWithoutChannel]}");

    // 1000 photons on the fibre 7: the fibres 7, 35 and 36 capture 200 each on average
    TRestDetectorHitsEvent photons;
    photons.AddHit(0, 0, 30, 1000, 2);
    photons.AddHit(60, 0, 30, 1000, 2);  // no fibre in range
    TRestDetectorHitsEvent collected;
    ASSERT_TRUE(process->ProcessEvent(photons, collected));
    EXPECT_EQ(analysis.Get<int>("lightCollection_NHitsWithoutChannel"), 1);
    ASSERT_EQ(collected.GetNumberOfHits(), 3u);
    const double total = analysis.Get<double>("lightCollection_NPhotoelectrons");
    EXPECT_NEAR(collected.GetTotalEnergy(), total, 1e-9);
    EXPECT_NEAR(total, 600, 5 * std::sqrt(600.));
    std::set<int> channels;
    for (size_t n = 0; n < collected.GetNumberOfHits(); n++) {
        EXPECT_DOUBLE_EQ(collected.GetTime(n), 2);
        const auto p = collected.GetPosition(n);
        channels.insert(detector.readout->GetChannelFromPosition(p.X(), p.Y(), p.Z()));
    }
    EXPECT_EQ(channels, std::set<int>({7, 35, 36}));

    // Probabilities adding up to more than 1 around a position
    auto tooLarge = MakeProcess<TRestDetectorLightCollectionProcess>(
        detector, analysis, "lightCollection", "{captureProbability: \"0.5+0*x\", maxDistance: 15mm}");
    TRestDetectorHitsEvent output;
    EXPECT_THROW(tooLarge->ProcessEvent(photons, output), std::runtime_error);

    // Wrong configurations
    EXPECT_ANY_THROW(TRestDetectorLightCollectionProcess("c", YAML::Load("{model: unknown}")));
    EXPECT_ANY_THROW(TRestDetectorLightCollectionProcess("c", YAML::Load("{captureProbability: \"0.1\"}")));
    EXPECT_ANY_THROW(TRestDetectorLightCollectionProcess(
        "c", YAML::Load("{captureProbability: \"x*(\", maxDistance: 1}")));
}

TEST(DetectorLightCollectionProcess, CaptureProbabilityFromGraph) {
    const std::string fileName = std::string(TESTS_WORK_DIR) + "/light_capture.root";
    {
        std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "RECREATE"));
        ASSERT_TRUE(file && !file->IsZombie());
        TGraph graph(2);
        graph.SetPoint(0, 0, 0.5);
        graph.SetPoint(1, 20, 0);
        graph.Write("capture");
        file->Close();
    }
    TRestDetectorLightCollectionProcess process(
        "lightCollection",
        YAML::Load("{captureProbability: \"" + fileName + ":capture\", maxDistance: 15 mm}"));
    EXPECT_NEAR(process.GetCaptureProbability(0), 0.5, 1e-9);
    EXPECT_NEAR(process.GetCaptureProbability(10), 0.25, 1e-9);
    EXPECT_DOUBLE_EQ(process.GetCaptureProbability(16), 0);  // beyond maxDistance

    EXPECT_ANY_THROW(TRestDetectorLightCollectionProcess(
        "c", YAML::Load("{captureProbability: \"" + fileName + ":missing\", maxDistance: 15 mm}")));
}

TEST(DetectorLightCollectionProcess, AttenuationToTheReadout) {
    auto detector = MakeDetector();
    AnalysisTree analysis;
    auto process = MakeProcess<TRestDetectorLightCollectionProcess>(
        detector, analysis, "lightCollection",
        "{model: attenuation, readout: bar, seed: 3, observables: [NPhotoelectrons, NHitsWithoutChannel]}");

    // 10000 photons at 50 mm from the readout: exp(-50 / 100) of them, 0.5 us later
    TRestDetectorHitsEvent photons;
    photons.AddHit(50, 0, 0, 10000, 1);
    photons.AddHit(200, 0, 0, 10000, 1);  // no channel
    TRestDetectorHitsEvent collected;
    ASSERT_TRUE(process->ProcessEvent(photons, collected));
    ASSERT_EQ(collected.GetNumberOfHits(), 1u);
    const double expected = 10000 * std::exp(-0.5);
    EXPECT_NEAR(collected.GetEnergy(0), expected, 5 * std::sqrt(expected));
    EXPECT_NEAR(collected.GetTime(0), 1.5, 1e-9);
    EXPECT_NEAR(collected.GetX(0), 50, 1e-9);
    EXPECT_EQ(analysis.Get<int>("lightCollection_NHitsWithoutChannel"), 1);

    // The fibre readout has no distance to the readout of its channels
    auto fibres = MakeProcess<TRestDetectorLightCollectionProcess>(
        detector, analysis, "lightCollection", "{model: attenuation, readout: fibreTest}");
    TRestDetectorHitsEvent onFibre;
    onFibre.AddHit(0, 0, 30, 100, 0);
    TRestDetectorHitsEvent output;
    EXPECT_THROW(fibres->ProcessEvent(onFibre, output), std::runtime_error);
}

TEST(DetectorLightCollectionProcess, QuantumEfficiency) {
    auto detector = MakeDetector();
    AnalysisTree analysis;
    // On the fibre 7 only (maxDistance 5 mm), all the photons captured: Binomial(10000, 0.4) photoelectrons
    auto process = MakeProcess<TRestDetectorLightCollectionProcess>(
        detector, analysis, "lightCollection",
        "{captureProbability: \"1+0*x\", maxDistance: 5mm, quantumEfficiency: 0.4, seed: 9, "
        "observables: [NPhotoelectrons]}");

    TRestDetectorHitsEvent photons;
    photons.AddHit(0, 0, 30, 10000, 1);
    TRestDetectorHitsEvent photoelectrons;
    ASSERT_TRUE(process->ProcessEvent(photons, photoelectrons));
    ASSERT_EQ(photoelectrons.GetNumberOfHits(), 1u);
    EXPECT_NEAR(photoelectrons.GetEnergy(0), 4000, 5 * std::sqrt(10000 * 0.4 * 0.6));
    EXPECT_DOUBLE_EQ(analysis.Get<double>("lightCollection_NPhotoelectrons"), photoelectrons.GetEnergy(0));

    auto blind = MakeProcess<TRestDetectorLightCollectionProcess>(
        detector, analysis, "lightCollection",
        "{captureProbability: \"1+0*x\", maxDistance: 5mm, quantumEfficiency: 0}");
    TRestDetectorHitsEvent none;
    EXPECT_FALSE(blind->ProcessEvent(photons, none));

    EXPECT_ANY_THROW(TRestDetectorLightCollectionProcess(
        "c", YAML::Load("{captureProbability: \"1\", maxDistance: 5mm, quantumEfficiency: 1.5}")));
}

TEST(DetectorLightProcesses, FromEnergyToHitsOfTheFibres) {
    auto detector = MakeDetector();
    AnalysisTree analysis;
    auto scintillation =
        MakeProcess<TRestDetectorScintillationProcess>(detector, analysis, "scintillation", "{seed: 3}");
    auto collection = MakeProcess<TRestDetectorLightCollectionProcess>(
        detector, analysis, "lightCollection",
        "{captureProbability: \"0.2+0*x\", maxDistance: 15mm, quantumEfficiency: 0.5, seed: 3}");
    // The medium has no charge: no drift for the fibres
    auto hitsToSignal =
        MakeProcess<TRestDetectorHitsToSignalProcess>(detector, analysis, "hitsToSignal", "{}");
    auto signalToHits = MakeProcess<TRestDetectorSignalToHitsProcess>(detector, analysis, "signalToHits",
                                                                      "{method: integral}");
    EXPECT_DOUBLE_EQ(signalToHits->fDriftVelocity, 0);

    TRestDetectorHitsEvent hits;
    hits.AddHit(0, 0, 30, 10, 4);
    TRestDetectorHitsEvent photons, photoelectrons;
    ASSERT_TRUE(scintillation->ProcessEvent(hits, photons));
    ASSERT_TRUE(collection->ProcessEvent(photons, photoelectrons));
    TRestDetectorSignalEvent signals;
    ASSERT_TRUE(hitsToSignal->ProcessEvent(photoelectrons, signals));
    ASSERT_EQ(signals.GetNumberOfSignals(), 3);
    for (int id : {7, 35, 36}) {
        ASSERT_TRUE(signals.HasSignalID(id));
        EXPECT_NEAR(signals.GetSignalByID(id).GetPoint(0).first, 4, 1e-9);
    }

    // Back to hits: an XZ hit for the fibre along Y, YZ hits for those along X
    TRestDetectorHitsEvent reconstructed;
    ASSERT_TRUE(signalToHits->ProcessEvent(signals, reconstructed));
    ASSERT_EQ(reconstructed.GetNumberOfHits(), 3u);
    int xz = 0, yz = 0;
    for (size_t n = 0; n < reconstructed.GetNumberOfHits(); n++) {
        if (reconstructed.GetType(n) == TRestHitsData::REST_HitType::XZ) xz++;
        if (reconstructed.GetType(n) == TRestHitsData::REST_HitType::YZ) yz++;
    }
    EXPECT_EQ(xz, 1);
    EXPECT_EQ(yz, 2);
}
