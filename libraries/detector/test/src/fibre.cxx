/// Unit tests of the fibre readout (TRestFibreReadout) and of the scintillation (light yield of the medium,
/// TRestDetectorScintillationProcess): a box of 100 mm with two families of fibres of pitch 20 mm, built in
/// memory, its light collection, GDML placements and export/import, and the processes fed with hits built
/// by hand through a TRestManager created from a YAML string.

#include <TFile.h>
#include <TGraph.h>
#include <TRestDetectorHitsEvent.h>
#include <TRestDetectorHitsToSignalProcess.h>
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
// Capture probability 0.5 up to 15 mm, quantum efficiency 0.4: probability 0.2 per fibre in range. Without
// decoding file: DAQ ID = physical ID ---
const std::string kReadout =
    "class: TRestFibreReadout\n"
    "name: fibreTest\n"
    "readoutParameters:\n"
    "  size: [100mm, 100mm, 100mm]\n"
    "  position: [0, 0, 50mm]\n"
    "  quantumEfficiency: 0.4\n"
    "  fibres:\n"
    "    - {axis: Y, pitch: 20mm, offset: [10mm, 10mm], captureProbability: \"CAPTURE\", maxDistance: 15mm, "
    "volume: fibreYVolume}\n"
    "    - {axis: X, pitch: 20mm, offset: [10mm, 20mm], captureProbability: \"CAPTURE\", maxDistance: 15mm, "
    "volume: fibreXVolume}\n";

/// The readout YAML with the capture probability given
YAML::Node ReadoutNode(const std::string& capture = "0.5+0*x") {
    std::string text = kReadout;
    for (size_t at = text.find("CAPTURE"); at != std::string::npos; at = text.find("CAPTURE")) {
        text.replace(at, 7, capture);
    }
    return YAML::Load(text);
}

std::unique_ptr<TRestFibreReadout> BuildReadout(const std::string& capture = "0.5+0*x") {
    auto readout = std::make_unique<TRestFibreReadout>("fibreTest", ReadoutNode(capture));
    readout->BuildGeometry();
    return readout;
}

/// A TRestManager with the fibre readout in its TRestDetectorReadoutManager and a scintillator medium with
/// only `light` (no charge)
struct Detector {
    std::unique_ptr<TRestManager> manager;
    TRestDetectorReadout* readout = nullptr;
};

Detector MakeDetector() {
    const YAML::Node node = YAML::Load(
        "class: TRestManager\n"
        "readouts:\n"
        "  class: TRestDetectorReadoutManager\n"
        "  readout: fibreTest\n"
        "scintillator:\n"
        "  class: TRestDetectorMedium\n"
        "  name: LS\n"
        "  state: liquid\n"
        "  light: {lightYield: 100}\n");
    Detector detector;
    detector.manager = std::make_unique<TRestManager>("manager", node);
    auto* readouts = dynamic_cast<TRestDetectorReadoutManager*>(
        detector.manager->GetMetadataClass("TRestDetectorReadoutManager"));
    if (!readouts) throw std::runtime_error("no TRestDetectorReadoutManager in the manager");
    auto readout = BuildReadout();
    detector.readout = readout.get();
    readouts->fReadoutMap["fibreTest"] = readout.release();  // owned by the readout manager
    return detector;
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

TEST(FibreReadout, LightCollection) {
    const auto readout = BuildReadout();

    // On the fibre along Y at z = 30, x = 0 (ID 7): it and the fibres along X at y = 0, z = 20 and z = 40
    // (IDs 35 and 36, 10 mm away) are in range; the others are 20 mm or more away
    const auto collections = readout->GetLightCollection({0, 0, 30});
    ASSERT_EQ(collections.size(), 3u);
    std::set<int> channels;
    for (const auto& collection : collections) {
        channels.insert(collection.daqID);
        EXPECT_NEAR(collection.probability, 0.2, 1e-9);
        // The position is on the fibre: it gives back the same channel
        const auto& p = collection.position;
        EXPECT_EQ(readout->GetChannelFromPosition(p.X(), p.Y(), p.Z()), collection.daqID);
    }
    EXPECT_EQ(channels, std::set<int>({7, 35, 36}));

    // Along X outside the box: the fibres along X do not reach it, those along Y are too far
    EXPECT_TRUE(readout->GetLightCollection({60, 0, 30}).empty());
}

TEST(FibreReadout, CaptureProbabilityFromGraph) {
    const std::string fileName = std::string(TESTS_WORK_DIR) + "/fibre_capture.root";
    {
        std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "RECREATE"));
        ASSERT_TRUE(file && !file->IsZombie());
        TGraph graph(2);
        graph.SetPoint(0, 0, 0.5);
        graph.SetPoint(1, 20, 0);
        graph.Write("capture");
        file->Close();
    }
    const auto readout = BuildReadout(fileName + ":capture");
    const auto collections = readout->GetLightCollection({0, 0, 30});
    ASSERT_EQ(collections.size(), 3u);
    for (const auto& collection : collections) {
        // 0.5 on the fibre, 0.25 at 10 mm, times the quantum efficiency
        const double expected = collection.daqID == 7 ? 0.2 : 0.1;
        EXPECT_NEAR(collection.probability, expected, 1e-9);
    }

    EXPECT_ANY_THROW(TRestFibreReadout("fibreTest", ReadoutNode(fileName + ":missing")));
    EXPECT_ANY_THROW(TRestFibreReadout("fibreTest", ReadoutNode("x*(")));
}

TEST(FibreReadout, WrongParameters) {
    YAML::Node node = ReadoutNode();
    node["readoutParameters"]["fibres"][0]["axis"] = "W";
    EXPECT_ANY_THROW(TRestFibreReadout("fibreTest", node));

    node = ReadoutNode();
    node["readoutParameters"]["quantumEfficiency"] = 1.5;
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
    EXPECT_EQ(readout->GetLightCollection({0, 0, 30}).size(), 3u);
}

// ============================================================================
// Scintillation
// ============================================================================

TEST(DetectorMedium, LightYield) {
    TRestDetectorMedium medium("LS", YAML::Load("{state: liquid, light: {lightYield: 12}}"));
    EXPECT_DOUBLE_EQ(medium.GetLightResponse("test").GetLightYield(), 12);
    EXPECT_ANY_THROW(TRestDetectorMedium("LS", YAML::Load("{state: liquid, light: {lightYield: -1}}")));
}

TEST(DetectorScintillationProcess, PhotoelectronsInTheFibres) {
    auto detector = MakeDetector();
    AnalysisTree analysis;
    TRestDetectorScintillationProcess process(
        "scintillation", YAML::Load("{seed: 7, observables: [NPhotoelectrons, NHitsWithoutLight]}"));
    process.SetManager(detector.manager.get());
    process.SetRunInfo(&analysis.run);
    process.Initialize();
    EXPECT_EQ(process.fMediumName, "LS");

    // 10 keV: 1000 photons, 200 photoelectrons on average in each of the 3 fibres in range
    TRestDetectorHitsEvent hits;
    hits.SetID(3);
    hits.AddHit(0, 0, 30, 10, 2);
    hits.AddHit(60, 0, 30, 10, 2);  // no fibre collects its light
    TRestDetectorHitsEvent photoelectrons;
    ASSERT_TRUE(process.ProcessEvent(hits, photoelectrons));
    EXPECT_EQ(photoelectrons.GetID(), 3);
    EXPECT_EQ(analysis.Get<int>("scintillation_NHitsWithoutLight"), 1);
    ASSERT_EQ(photoelectrons.GetNumberOfHits(), 3u);
    const double total = analysis.Get<double>("scintillation_NPhotoelectrons");
    EXPECT_NEAR(photoelectrons.GetTotalEnergy(), total, 1e-9);
    EXPECT_NEAR(total, 600, 5 * std::sqrt(600.));
    std::set<int> channels;
    for (size_t n = 0; n < photoelectrons.GetNumberOfHits(); n++) {
        EXPECT_DOUBLE_EQ(photoelectrons.GetTime(n), 2);
        EXPECT_EQ(photoelectrons.GetEnergy(n), std::round(photoelectrons.GetEnergy(n)));
        const auto p = photoelectrons.GetPosition(n);
        channels.insert(detector.readout->GetChannelFromPosition(p.X(), p.Y(), p.Z()));
    }
    EXPECT_EQ(channels, std::set<int>({7, 35, 36}));

    // Same seed and event: the same photoelectrons
    TRestDetectorHitsEvent again;
    ASSERT_TRUE(process.ProcessEvent(hits, again));
    EXPECT_DOUBLE_EQ(again.GetTotalEnergy(), photoelectrons.GetTotalEnergy());

    // Only light that no fibre collects: rejected
    TRestDetectorHitsEvent dark;
    dark.AddHit(60, 0, 30, 10, 0);
    TRestDetectorHitsEvent none;
    EXPECT_FALSE(process.ProcessEvent(dark, none));
}

TEST(DetectorScintillationProcess, NeedsLightYield) {
    const YAML::Node node = YAML::Load(
        "class: TRestManager\n"
        "readouts:\n"
        "  class: TRestDetectorReadoutManager\n"
        "  readout: fibreTest\n"
        "scintillator:\n"
        "  class: TRestDetectorMedium\n"
        "  name: LS\n"
        "  state: liquid\n"
        "  light: {attenuationLength: 1 m}\n");
    TRestManager manager("manager", node);
    auto* readouts =
        dynamic_cast<TRestDetectorReadoutManager*>(manager.GetMetadataClass("TRestDetectorReadoutManager"));
    ASSERT_NE(readouts, nullptr);
    readouts->fReadoutMap["fibreTest"] = BuildReadout().release();
    TRestDetectorScintillationProcess process("scintillation", YAML::Load("{}"));
    process.SetManager(&manager);
    EXPECT_THROW(process.Initialize(), std::runtime_error);
}

TEST(DetectorScintillationProcess, SignalsAndHitsOfTheFibres) {
    auto detector = MakeDetector();
    TRestDetectorScintillationProcess scintillation("scintillation", YAML::Load("{seed: 3}"));
    scintillation.SetManager(detector.manager.get());
    scintillation.Initialize();
    // The medium has no charge: no drift for the fibres
    TRestDetectorHitsToSignalProcess hitsToSignal("hitsToSignal", YAML::Load("{}"));
    hitsToSignal.SetManager(detector.manager.get());
    ASSERT_NO_THROW(hitsToSignal.Initialize());
    TRestDetectorSignalToHitsProcess signalToHits("signalToHits", YAML::Load("{method: integral}"));
    signalToHits.SetManager(detector.manager.get());
    ASSERT_NO_THROW(signalToHits.Initialize());
    EXPECT_DOUBLE_EQ(signalToHits.fDriftVelocity, 0);

    TRestDetectorHitsEvent hits;
    hits.AddHit(0, 0, 30, 10, 4);
    TRestDetectorHitsEvent photoelectrons;
    ASSERT_TRUE(scintillation.ProcessEvent(hits, photoelectrons));
    TRestDetectorSignalEvent signals;
    ASSERT_TRUE(hitsToSignal.ProcessEvent(photoelectrons, signals));
    ASSERT_EQ(signals.GetNumberOfSignals(), 3);
    for (int id : {7, 35, 36}) {
        ASSERT_TRUE(signals.HasSignalID(id));
        EXPECT_NEAR(signals.GetSignalByID(id).GetPoint(0).first, 4, 1e-9);
    }

    // Back to hits: an XZ hit for the fibre along Y, YZ hits for those along X
    TRestDetectorHitsEvent reconstructed;
    ASSERT_TRUE(signalToHits.ProcessEvent(signals, reconstructed));
    ASSERT_EQ(reconstructed.GetNumberOfHits(), 3u);
    int xz = 0, yz = 0;
    for (size_t n = 0; n < reconstructed.GetNumberOfHits(); n++) {
        if (reconstructed.GetType(n) == TRestHitsData::REST_HitType::XZ) xz++;
        if (reconstructed.GetType(n) == TRestHitsData::REST_HitType::YZ) yz++;
    }
    EXPECT_EQ(xz, 1);
    EXPECT_EQ(yz, 2);
}
