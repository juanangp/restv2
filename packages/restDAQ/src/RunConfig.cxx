#include "RunConfig.h"

#include <readline/readline.h>

#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "CommandFetcher.h"
#include "TRestTools.h"
#include "TRestYAMLUtils.h"

namespace {
/// Output file name when the configuration does not give one
const std::string kDefaultOutputFileName =
    "Run[runNumber]_[runTag]_[experimentName]_[runType]_[subRunNumber].root";
}  // namespace

RunConfig::RunConfig(const std::string& fN, const std::map<std::string, std::string>& parameters, bool tcm)
    : isTCM(tcm), fileName(fN) {
    const YAML::Node config = TRestYAMLUtils::OpenConfigFile(fileName, parameters);

    const auto daqSection = TRestYAMLUtils::GetMetadataClass(config, "TRestRawDAQMetadata");
    if (!daqSection.second)
        throw std::runtime_error("no section of class TRestRawDAQMetadata in " + fileName);
    daq = std::make_unique<TRestRawDAQMetadata>(daqSection.first, daqSection.second);
    daq->CheckConfig();

    verboseLevel = daq->GetVerboseLevel();

    if (isTCM) {
        if (daq->fTCM.empty()) throw std::runtime_error("TCM requested, but no TCM IP in " + fileName);
        electronics = "TCM";
        TRestYAMLUtils::CheckParametersUsed();
        return;
    }

    const auto runNode = TRestYAMLUtils::GetMetadataClass(config, "TRestRun");
    if (!runNode.second) throw std::runtime_error("no section of class TRestRun in " + fileName);
    runSection = runNode.first;
    // runNumber: auto takes a new number in each run (NewRun), not when the configuration is read
    YAML::Node runConfig = YAML::Clone(runNode.second);
    autoRunNumber = runConfig["runNumber"] && runConfig["runNumber"].IsScalar() &&
                    runConfig["runNumber"].Scalar() == "auto";
    if (autoRunNumber) runConfig.remove("runNumber");
    run = std::make_unique<TRestRun>(runSection, runConfig);
    run->CheckConfig();
    TRestYAMLUtils::CheckParametersUsed();

    outputPattern = run->GetOutputFileName();
    if (outputPattern.empty() || outputPattern == "/dev/null" || outputPattern == "Null")
        outputPattern = kDefaultOutputFileName;
    firstSubRun = run->GetSubRunNumber();

    electronics = daq->fElectronics;
    if (electronics != "ARC" && electronics != "FEMINOS" && electronics != "DCC")
        throw std::runtime_error("electronics \"" + electronics + "\" not supported (ARC, FEMINOS or DCC)");
    if (daq->fFEMs.empty()) throw std::runtime_error("no fems in the TRestRawDAQMetadata of " + fileName);
    for (const auto& fem : daq->fFEMs) {
        if (fem.fIP.empty()) throw std::runtime_error("FEM " + std::to_string(fem.fId) + " without IP");
        if (electronics == "DCC" && fem.fFECs.empty())
            throw std::runtime_error("DCC electronics: FEM " + std::to_string(fem.fId) + " without FECs");
    }

    nEvents = daq->fNEvents;
    maxTimeSeconds = daq->GetMaxTimeSeconds();
    fileSize = daq->GetMaxFileSizeBytes();
    updateRateTime = static_cast<int>(daq->GetUpdateRateSeconds());
    if (updateRateTime <= 0) updateRateTime = 1;
}

void RunConfig::SetVerboseLevel(const std::string& vFlag) {
    try {
        verboseLevel = TRestLogManager::GetVerboseLevelFromString(vFlag);
    } catch (const std::invalid_argument& e) {
        std::cout << e.what() << ", verbose not changed" << std::endl;
    }
}

void RunConfig::NewRun() {
    if (autoRunNumber) run->SetRunNumber(TRestTools::GetRunNumberAuto());
    run->SetSubRunNumber(firstSubRun);
}

std::string RunConfig::GetOutputFileName() const {
    return run->PrefixMainDataPath(run->ResolveFilePattern(outputPattern));
}

std::string RunConfig::Dump() const {
    YAML::Node config;
    auto& registry = TRestMetadataFieldRegistry::Instance();
    YAML::Node daqNode = YAML::Clone(daq->GetYAMLNode());
    registry.ApplyFieldsToYAML(daq.get(), daqNode);
    config[daq->GetName()] = daqNode;
    if (run) {
        YAML::Node runNode = YAML::Clone(run->GetYAMLNode());
        registry.ApplyFieldsToYAML(run.get(), runNode);
        config[runSection] = runNode;
    }
    return YAML::Dump(config);
}

void RunConfig::UpdateInfo() {
    std::cout << "\n--- Run Info ---" << std::endl;
    std::cout << "Press [ENTER] to keep the default value or type a new one." << std::endl;

    auto readField = [&](const std::string& prompt) -> std::string {
        if (CommandFetcher::g_shutdown.load()) {
            throw std::runtime_error("AddMetadata aborted (CTRL-C)");
        }

        char* buffer = readline(prompt.c_str());

        if (CommandFetcher::g_shutdown.load()) {
            if (buffer) free(buffer);
            throw std::runtime_error("AddMetadata aborted (CTRL-C)");
        }

        if (buffer == nullptr) {
            throw std::runtime_error("AddMetadata aborted (CTRL-C)");
        }

        std::string str(buffer);
        free(buffer);
        return str;
    };

    // The values typed are written in the run section of the configuration file, only them (the rest of
    // the file, its comments and format, is kept)
    const std::string section =
        TRestYAMLUtils::GetMetadataClass(TRestYAMLUtils::LoadConfigFile(fileName), "TRestRun").first;
    if (section.empty()) throw std::runtime_error("no section of class TRestRun in " + fileName);
    std::vector<std::pair<std::string, std::string>> changes;

    std::string input = readField(">>>> Run Tag (" + run->GetRunTag() + "): ");
    if (!input.empty() && input != run->GetRunTag()) {
        run->SetRunTag(input);
        changes.emplace_back(section + ".runTag", input);
    }

    input = readField(">>>> Run Type (" + run->GetRunType() + "): ");
    if (!input.empty() && input != run->GetRunType()) {
        run->SetRunType(input);
        changes.emplace_back(section + ".runType", input);
    }

    for (auto& [field, value] : run->fRunInformation) {
        input = readField(" >>>> " + field + " (" + value + "): ");
        if (!input.empty() && input != value) {
            value = input;
            changes.emplace_back(section + ".runInfo." + field, value);
        }
    }

    TRestYAMLUtils::SetValuesInFile(fileName, changes);

    std::cout << "--- Run Info updated successfully ---\n" << std::endl;
}
