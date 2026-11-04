#include "TRestRawFeminosRootToSignalProcess.h"

#include <TFile.h>
#include <TTree.h>

#include <sstream>

#include "TRestRawSignalEvent.h"
#include "TRestRun.h"

using namespace std;

static const bool TRestRawFeminosRootToSignalProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestRawFeminosRootToSignalProcess>(
        "useFeminosDaqRunInfo", &TRestRawFeminosRootToSignalProcess::fUseFeminosDaqRunInfo);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestRawFeminosRootToSignalProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestRawFeminosRootToSignalProcess>(instanceName, params);
        });
    return true;
}();

/// feminos-daq writes "\n" for an empty tag or comment
std::string CleanText(const std::string& text) { return text == "\n" ? "" : text; }
}  // namespace

TRestRawFeminosRootToSignalProcess::TRestRawFeminosRootToSignalProcess() : TRestEventProcess() {
    fName = "TRestRawFeminosRootToSignalProcess";
}

TRestRawFeminosRootToSignalProcess::TRestRawFeminosRootToSignalProcess(const std::string& instanceName,
                                                                       const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

TRestRawFeminosRootToSignalProcess::TRestRawFeminosRootToSignalProcess(const std::string& fileName,
                                                                       const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

void TRestRawFeminosRootToSignalProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestRawFeminosRootToSignalProcess::LoadConfig YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestRawFeminosRootToSignalProcess>(fNode);
    UpdateYAMLFromParams<TRestRawFeminosRootToSignalProcess>(fNode);
}

void TRestRawFeminosRootToSignalProcess::ReadRunInfo(TTree* runTree) {
    if (runTree->GetEntries() == 0) {
        RESTWarning << "TRestRawFeminosRootToSignalProcess: the 'run' tree is empty" << RESTendl;
        return;
    }

    unsigned long long startTimestamp = 0;
    unsigned long long runNumber = 0;
    std::string* runTag = nullptr;
    std::string* comments = nullptr;
    std::string* detector = nullptr;
    float driftField = 0;
    float meshVoltage = 0;
    float pressure = 0;

    // Only the branches in the file (older feminos-daq versions may not have all of them)
    auto connect = [runTree](const char* name, auto* address) {
        if (!runTree->GetBranch(name)) return false;
        runTree->SetBranchAddress(name, address);
        return true;
    };
    const bool hasStart = connect("timestamp", &startTimestamp);
    const bool hasNumber = connect("number", &runNumber);
    connect("tag", &runTag);
    connect("comments", &comments);
    connect("detector", &detector);
    const bool hasDriftField = connect("drift_field_V_cm_bar", &driftField);
    const bool hasMeshVoltage = connect("mesh_voltage_V", &meshVoltage);
    const bool hasPressure = connect("detector_pressure_bar", &pressure);

    runTree->GetEntry(0);

    if (hasStart) fRunInfo->SetStartTimeStamp(startTimestamp / 1000.0);  // ms to s
    if (hasNumber) fRunInfo->SetRunNumber(static_cast<int>(runNumber));
    if (runTag && !CleanText(*runTag).empty()) fRunInfo->SetRunTag(CleanText(*runTag));
    if (comments && !CleanText(*comments).empty()) fRunInfo->SetRunDescription(CleanText(*comments));

    // runInfo, unless the configuration (or the input file name) already gives the key
    auto addRunInfo = [this](const std::string& key, const std::string& value) {
        if (value.empty() || fRunInfo->fRunInformation.count(key) > 0) return;
        fRunInfo->fRunInformation[key] = value;
    };
    auto withUnits = [](float value, const std::string& units) {
        std::ostringstream text;
        text << value << " " << units;
        return text.str();
    };
    if (detector) addRunInfo("detector", CleanText(*detector));
    if (hasDriftField) addRunInfo("driftField", withUnits(driftField, "V/cm/bar"));
    if (hasMeshVoltage) addRunInfo("meshVoltage", withUnits(meshVoltage, "V"));
    if (hasPressure) addRunInfo("pressure", withUnits(pressure, "bar"));

    runTree->ResetBranchAddresses();
    delete runTag;
    delete comments;
    delete detector;
}

void TRestRawFeminosRootToSignalProcess::InitProcess() {
    const std::string inputFileName = fRunInfo ? fRunInfo->GetInputFileName() : "";
    if (inputFileName.empty() || inputFileName == "Null") {
        throw std::runtime_error("TRestRawFeminosRootToSignalProcess: the TRestRun has no inputFileName");
    }
    if (inputFileName.size() < 5 || inputFileName.substr(inputFileName.size() - 5) != ".root") {
        throw std::runtime_error("TRestRawFeminosRootToSignalProcess: the input file is not a root file: " +
                                 inputFileName);
    }

    fInputFile = TFile::Open(inputFileName.c_str(), "READ");
    if (!fInputFile || fInputFile->IsZombie()) {
        throw std::runtime_error("TRestRawFeminosRootToSignalProcess: error opening the input file " +
                                 inputFileName);
    }

    TTree* runTree = fInputFile->Get<TTree>("run");
    fInputTree = fInputFile->Get<TTree>("events");
    if (!runTree || !fInputTree) {
        throw std::runtime_error("TRestRawFeminosRootToSignalProcess: no 'run' or 'events' tree in " +
                                 inputFileName);
    }

    if (fUseFeminosDaqRunInfo) ReadRunInfo(runTree);

    fInputTree->SetBranchAddress("timestamp", &fTimestamp);
    fInputTree->SetBranchAddress("signal_ids", &fSignalIds);
    fInputTree->SetBranchAddress("signal_values", &fSignalValues);
}

bool TRestRawFeminosRootToSignalProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    if (fInputTreeEntry >= fInputTree->GetEntries()) return false;  // end of the file (InputFinished)

    auto& signalEvent = dynamic_cast<TRestRawSignalEvent&>(output);
    signalEvent.Initialize();

    fInputTree->GetEntry(fInputTreeEntry);

    signalEvent.SetID(static_cast<int>(fInputTreeEntry));
    // Timestamp in milliseconds
    signalEvent.SetTime(static_cast<long>(fTimestamp / 1000), static_cast<int>(fTimestamp % 1000 * 1000000));

    if (fTimestamp < fStartTimestamp) fStartTimestamp = fTimestamp;
    if (fTimestamp > fEndTimestamp) fEndTimestamp = fTimestamp;

    // The samples of the signals one after the other, all with the same number of samples
    const size_t nSignals = fSignalIds->size();
    if (nSignals > 0) {
        if (fSignalValues->size() % nSignals != 0) {
            throw std::runtime_error("TRestRawFeminosRootToSignalProcess: entry " +
                                     std::to_string(fInputTreeEntry) + " has " +
                                     std::to_string(fSignalValues->size()) + " samples for " +
                                     std::to_string(nSignals) + " signals");
        }
        const size_t nSamples = fSignalValues->size() / nSignals;
        std::vector<short> samples(nSamples);
        for (size_t i = 0; i < nSignals; i++) {
            for (size_t j = 0; j < nSamples; j++)
                samples[j] = static_cast<short>((*fSignalValues)[i * nSamples + j]);
            signalEvent.AddSignal(fSignalIds->at(i), samples);
        }
    }

    fInputTreeEntry++;
    return true;
}

void TRestRawFeminosRootToSignalProcess::EndProcess() {
    if (fRunInfo && fEndTimestamp > 0) {
        if (!fUseFeminosDaqRunInfo) fRunInfo->SetStartTimeStamp(fStartTimestamp / 1000.0);  // ms to s
        fRunInfo->SetEndTimeStamp(fEndTimestamp / 1000.0);
    }

    if (fInputFile) {
        fInputFile->Close();
        delete fInputFile;
        fInputFile = nullptr;
        fInputTree = nullptr;
    }
}
