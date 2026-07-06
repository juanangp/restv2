#include "TRestRawFemDAQToSignalProcess.h"
#include <TObjString.h>
#include <iostream>

using namespace std;

// Registration in MetadataRegistry
namespace {
const bool kRegistered = []() {
    MetadataRegistry::Instance().Register(
        "TRestRawFemDAQToSignalProcess",
        [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestRawFemDAQToSignalProcess>(instanceName, params);
        });
    return true;
}();
}

void TRestRawFemDAQToSignalProcess::InitProcess() {
    
    // Read parameters from YAML
    std::string inputFilename;
    if (fNode["inputFileName"]) {
        inputFilename = fNode["inputFileName"].as<std::string>();
    }
    if ((inputFilename.empty() || inputFilename == "Null") && fRunInfo) {
        inputFilename = fRunInfo->GetInputFileName();
    }
    if (inputFilename.empty() || inputFilename == "Null") {
        throw std::runtime_error("TRestRawFemDAQToSignalProcess: missing input file in process config and TRestRun");
    }

    if (fNode["useFeminosDaqRunInfo"]) fUseFeminosDaqRunInfo = fNode["useFeminosDaqRunInfo"].as<bool>();
    if (fNode["setRunStartEndFromEvents"]) fSetRunStartEndFromEvents = fNode["setRunStartEndFromEvents"].as<bool>();

    if (inputFilename.substr(inputFilename.size() - 5) != ".root") {
        throw std::runtime_error("TRestRawFemDAQToSignalProcess: Input file is not a root file");
    }

    fInputFile = TFile::Open(inputFilename.c_str(), "READ");
    if (!fInputFile || fInputFile->IsZombie()) {
        throw std::runtime_error("TRestRawFemDAQToSignalProcess: Error opening input file " + inputFilename);
    }
    fInputFile->cd();

    fInputTree = fInputFile->Get<TTree>("SignalEvent");
    if (!fInputTree) {
        throw std::runtime_error("TRestRawFemDAQToSignalProcess: Error opening 'SignalEvent' tree");
    }

    TObjString* tsObj = (TObjString*)fInputFile->Get("startTime");
    TObjString* etObj = (TObjString*)fInputFile->Get("endTime");
    if (tsObj && fRunInfo) {
        Double_t startTimestamp = std::stod(tsObj->GetString().Data());
        fRunInfo->SetStartTimeStamp(startTimestamp);
    }
    if (etObj && fRunInfo) {
        Double_t endTimestamp = std::stod(etObj->GetString().Data());
        fRunInfo->SetEndTimeStamp(endTimestamp);
    }

    if (fUseFeminosDaqRunInfo) {
        TObjString* yamlConfigObj = (TObjString*)fInputFile->Get("RunConfigYAML");
        TObjString* yamlfNameObj = (TObjString*)fInputFile->Get("yaml_fileName");

        if (yamlConfigObj && fRunInfo) {
            std::string yamlConfig = yamlConfigObj->GetString().Data();
            std::cout << "Config YAML: " << yamlConfig << std::endl;
            YAML::Node config = YAML::Load(yamlConfig);
            if (config["run"]) {
                YAML::Node runNode = config["run"];
                if (runNode["tag"]) fRunInfo->SetRunTag(runNode["tag"].as<std::string>());
                if (runNode["type"]) fRunInfo->SetRunType(runNode["type"].as<std::string>());
            }
        }

        if (yamlfNameObj) {
            std::string fileName = yamlfNameObj->GetString().Data();
            std::cout << "Config fileName: " << fileName << std::endl;
        }
    }

    fInputTree->SetBranchAddress("timestamp", &fTimestamp);
    fInputTree->SetBranchAddress("signalsID", &fSignalIds);
    fInputTree->SetBranchAddress("pulses", &fSignalValues);
    fInputTree->SetBranchAddress("eventID", &fEventID);
}

bool TRestRawFemDAQToSignalProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    if (fInputTreeEntry >= fInputTree->GetEntries()) {
        return false; // EOF reached
    }

    auto* sigEvent = dynamic_cast<TRestRawSignalEvent*>(&output);
    if (!sigEvent) {
        throw std::runtime_error("TRestRawFemDAQToSignalProcess: output is not a TRestRawSignalEvent");
    }

    sigEvent->Initialize();

    fInputTree->GetEntry(fInputTreeEntry);

    sigEvent->SetID(fEventID);
    sigEvent->SetTime(fTimestamp);

    if (fTimestamp < fStartTimestamp) fStartTimestamp = fTimestamp;
    if (fTimestamp > fEndTimestamp) fEndTimestamp = fTimestamp;

    if (fSignalIds && fSignalValues) {
        for (size_t i = 0; i < fSignalIds->size(); i++) {
            const auto id = fSignalIds->at(i);
            std::vector<short> samples;
            samples.reserve(512);

            for (int j = 0; j < 512; j++) {
                if ((i * 512 + j) < fSignalValues->size()) {
                    samples.push_back(short(fSignalValues->at(i * 512 + j)));
                }
            }
            sigEvent->AddSignal(id, samples);
        }
    }

    fInputTreeEntry++;
    return true;
}

void TRestRawFemDAQToSignalProcess::EndProcess() {
    if (fSetRunStartEndFromEvents && fRunInfo) {
        fRunInfo->SetStartTimeStamp(fStartTimestamp);
        fRunInfo->SetEndTimeStamp(fEndTimestamp);
    }
    
    if (fInputFile) {
        fInputFile->Close();
        delete fInputFile;
        fInputFile = nullptr;
    }
}

void TRestRawFemDAQToSignalProcess::PrintMetadata() {
    std::cout << "=== TRestRawFemDAQToSignalProcess ===" << std::endl;
    std::cout << "Use fem-daq run information: " << (fUseFeminosDaqRunInfo ? "true" : "false") << std::endl;
    std::cout << "Set run start/end times from events: " << (fSetRunStartEndFromEvents ? "true" : "false") << std::endl;
}
