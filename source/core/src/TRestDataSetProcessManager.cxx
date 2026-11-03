#include "TRestDataSetProcessManager.h"

#include <TDirectory.h>
#include <TFile.h>
#include <TList.h>

#include <filesystem>
#include <regex>
#include <set>
#include <stdexcept>

#include "TRestCut.h"
#include "TRestDataSet.h"
#include "TRestManager.h"
#include "TRestTools.h"
#include "TRestYAMLUtils.h"

/// \class TRestDataSetProcessManager
///
/// Pipeline of dataset processes, a section of TRestManager (instead of TRestRun and
/// TRestEventProcessManager) run by restManager: the dataset `inputFileName` is opened, each process
/// (TRestDataSetProcess: TRestDataSetCalibration, TRestDataSetGainMap, TRestDataSetLogOdds...), in order,
/// obtains its parameters (from its referenceFileName, see TRestDataSetProcess) and adds its columns, and the
/// processed dataset is written once in `outputFileName`. The processes can use the columns of the previous
/// ones (e.g. a log odds with the calibrated energy), in the dataset processed; their reference is used as it
/// is.
///
/// The output dataset has all the columns of the input and the new ones, the runs of the input, and in
/// RESTMetadataStore the metadata of the input (its TRestDataSet, with this output, and the dataset
/// processes applied before), this pipeline, each process with its results and the TRestCut used by them.
/// The objects of each process (spectra, fits, PDFs) are in RESTDataSetStore/<class>/<name>; those of the
/// processes of the input are kept. A process with the name of one of the input replaces it, with a
/// warning, if it has the same class (otherwise it is an error: the configurations are stored by name).
///
/// ### Parameters
/// * inputFileName: the dataset processed (generated with REST_GenerateDataSet, or processed before);
///   `restManager --i`. With a TRestDataSet section in the same manager it is not used: the dataset is
///   generated and processed in memory and only the processed one is written (the generated one too if the
///   TRestDataSet has an outputFileName); its processes with an empty referenceFileName compute their
///   parameters on it.
/// * outputFileName: the dataset written; `restManager --o`. As the outputFileName of TRestRun, with
///   [key]: a tag of inputFormat, otherwise a key of the runs of the input dataset (TRestDataSet::
///   ResolveFilePattern: [runStart], [runEnd], [runRange], or a parameter of the runs, [runTag],
///   [runInfo.key]..., if it is the same in all of them, or [key|default]), e.g.
///   "D_[runStart]_[runEnd]_[runTag|Mixed]_Calibrated.root".
/// * inputFormat: pattern of the name of the input dataset with [tag] (e.g. "Dataset_[runTag].root",
///   each tag a part without "_"), as the inputFormat of TRestRun; the values of the tags are those of
///   outputFileName, so the same configuration processes several datasets with only --i. An input name
///   that does not match it is an error.
/// * the processes, as sections (name: {class: ..., ...}), in order.
///
/// The TRestAnalysisPlot of the manager without inputFileName draw the output dataset.
///
/// Example:
///
///    manager:
///      class: "TRestManager"
///
///      energy:
///        class: "TRestCut"
///        conditions:
///          range: "calib_Energy > 4 && calib_Energy < 8"
///
///      dataSetProcessing:
///        class: "TRestDataSetProcessManager"
///        inputFileName: "Dataset_Background.root"
///        outputFileName: "Dataset_Background_Processed.root"
///
///        calib:
///          class: "TRestDataSetCalibration"
///          referenceFileName: "Dataset_Calibration.root"
///          observable: "trackAnalysis_MaxTrackEnergy"
///          range: [0, 20000]
///          peaks:
///            - {energy: "5.9 keV"}
///
///        odds:
///          class: "TRestDataSetLogOdds"
///          referenceFileName: "Dataset_Calibration_Processed.root"  # with calib_Energy
///          cuts: ["energy"]
///          observables:
///            - {observable: "trackAnalysis_MaxTrack_XYZ_SigmaZ2", range: [0, 20]}
///
/// New class (legacy: each class read and wrote its own dataset).

static const bool TRestDataSetProcessManager_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    // The dataset processes are sections of the configuration
    reg.RegisterChildMetadata<TRestDataSetProcessManager>();
    reg.RegisterField<TRestDataSetProcessManager>("inputFileName",
                                                  &TRestDataSetProcessManager::fInputFileName);
    reg.RegisterField<TRestDataSetProcessManager>("outputFileName",
                                                  &TRestDataSetProcessManager::fOutputFileName);
    reg.RegisterField<TRestDataSetProcessManager>("inputFormat", &TRestDataSetProcessManager::fInputFormat);
    return true;
}();

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestDataSetProcessManager", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestDataSetProcessManager>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestDataSetProcessManager::TRestDataSetProcessManager() : TRestMetadata() {
    fName = "TRestDataSetProcessManager";
}

TRestDataSetProcessManager::TRestDataSetProcessManager(const std::string& instanceName,
                                                       const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

void TRestDataSetProcessManager::LoadConfig() {
    UpdateParamsFromYAML<TRestDataSetProcessManager>(fNode);
    fProcesses.clear();
    for (const auto& element : fNode) {
        const auto key = element.first.as<std::string>();
        auto value = element.second;
        if (!value || !value.IsMap()) continue;
        auto meta = CreateChildMetadata(key, value);
        if (!meta) continue;
        if (auto* process = dynamic_cast<TRestDataSetProcess*>(meta.get())) {
            meta.release();
            fProcesses.emplace_back(process);
        } else {
            throw std::runtime_error("TRestDataSetProcessManager '" + fName + "': '" + key + "' (" +
                                     meta->GetClassName() + ") is not a dataset process");
        }
    }
    UpdateYAMLFromParams<TRestDataSetProcessManager>(fNode);
}

void TRestDataSetProcessManager::Run() {
    const std::string context = "TRestDataSetProcessManager '" + fName + "'";
    if (fInputFileName.empty()) throw std::runtime_error(context + ": no inputFileName (or --i)");
    if (fOutputFileName.empty()) throw std::runtime_error(context + ": no outputFileName (or --o)");
    if (fProcesses.empty()) throw std::runtime_error(context + ": no dataset processes");
    if (!TRestTools::fileExists(fInputFileName))
        throw std::runtime_error(context + ": input file " + fInputFileName + " not found");
    if (!TRestTools::isDataSet(fInputFileName)) {
        throw std::runtime_error(context + ": " + fInputFileName +
                                 " is not a dataset (generate it with REST_GenerateDataSet)");
    }

    TRestDataSet dataSet(fInputFileName);
    Process(dataSet, fInputFileName);
}

void TRestDataSetProcessManager::Run(TRestDataSet& dataSet) {
    const std::string context = "TRestDataSetProcessManager '" + fName + "'";
    if (fOutputFileName.empty()) throw std::runtime_error(context + ": no outputFileName (or --o)");
    if (fProcesses.empty()) throw std::runtime_error(context + ": no dataset processes");
    fInputFileName = "";
    Process(dataSet, "");
}

void TRestDataSetProcessManager::Process(TRestDataSet& dataSet, const std::string& inputFile) {
    const std::string context = "TRestDataSetProcessManager '" + fName + "'";
    fOutputFileName = ResolveOutputFileName(dataSet);
    UpdateYAMLFromParams<TRestDataSetProcessManager>(fNode);
    std::error_code error;
    if (!inputFile.empty() && std::filesystem::equivalent(fOutputFileName, inputFile, error)) {
        throw std::runtime_error(context + ": the outputFileName " + fOutputFileName +
                                 " is the input dataset, give another name");
    }
    // A process already in the input (same name) is replaced if it has the same class; another class would
    // replace a different metadata (the configurations are stored by name)
    const YAML::Node inputMetadata = dataSet.GetInputMetadata();
    for (const auto& process : fProcesses) {
        const YAML::Node stored = inputMetadata[process->GetName()];
        if (!stored) continue;
        const std::string storedClass =
            stored.IsMap() && stored["class"] ? stored["class"].as<std::string>() : "";
        if (storedClass != process->GetClassName()) {
            throw std::runtime_error(context + ": the input already has a " + storedClass + " '" +
                                     process->GetName() + "', give another name to the " +
                                     process->GetClassName());
        }
        RESTWarning << context << ": the " << storedClass << " '" << process->GetName()
                    << "' of the input is replaced" << RESTendl;
    }

    // The file the dataset is written to: the reference of the processes computed on a dataset in memory
    if (inputFile.empty()) dataSet.fOutputFileName = fOutputFileName;
    for (const auto& process : fProcesses) {
        RESTInfo << context << ": " << process->GetClassName() << " '" << process->GetName() << "'"
                 << RESTendl;
        process->ObtainParameters(dataSet, fManager);
        process->Apply(dataSet);
    }
    dataSet.Write(fOutputFileName);

    std::unique_ptr<TFile> file(TFile::Open(fOutputFileName.c_str(), "UPDATE"));
    if (!file || file->IsZombie()) throw std::runtime_error(context + ": cannot open " + fOutputFileName);

    // The objects of the processes of the input dataset (RESTDataSetStore/<class>/<name>), unless processed
    // again (replaced by Save)
    std::set<std::string> names;
    for (const auto& process : fProcesses) names.insert(process->GetName());
    if (!inputFile.empty()) {
        std::unique_ptr<TFile> input(TFile::Open(TRestTools::GetFullPath(inputFile).c_str(), "READ"));
        TDirectory* inputStore = input && !input->IsZombie()
                                     ? input->GetDirectory(TRestDataSetProcess::kStoreDirectory.c_str())
                                     : nullptr;
        TList* classKeys = inputStore ? inputStore->GetListOfKeys() : nullptr;
        for (int c = 0; classKeys && c < classKeys->GetSize(); c++) {
            const TObject* classKey = classKeys->At(c);
            TDirectory* classFolder = inputStore->GetDirectory(classKey->GetName());
            if (!classFolder) continue;
            for (auto* key : *classFolder->GetListOfKeys()) {
                TDirectory* folder = classFolder->GetDirectory(key->GetName());
                if (!folder || names.count(key->GetName())) continue;
                const std::string path =
                    TRestDataSetProcess::kStoreDirectory + "/" + classKey->GetName() + "/" + key->GetName();
                TRestTools::CopyDirectory(folder, file->mkdir(path.c_str(), "", true));
            }
        }
    }

    // This pipeline (the processes are written apart, with their results), the processes and their cuts
    YAML::Node config = TRestYAMLUtils::Detach(fNode);
    for (const auto& name : names) config.remove(name);
    TRestMetadata::WriteMetadata(file.get(), fName, config);
    std::set<std::string> cuts;
    for (const auto& process : fProcesses) {
        process->Save(file.get());
        cuts.insert(process->fCutNames.begin(), process->fCutNames.end());
    }
    if (fManager) {
        for (auto* cut : fManager->GetMetadataOfType<TRestCut>())
            if (cuts.count(cut->GetName())) cut->WriteMetadata(file.get());
    }
    file->Close();

    RESTInfo << context << ": " << fOutputFileName << " written" << RESTendl;
}

std::map<std::string, std::string> TRestDataSetProcessManager::ParseInputFormat() const {
    std::map<std::string, std::string> tags;
    // A dataset generated in memory has no file name: only the keys of its runs
    if (fInputFormat.empty() || fInputFileName.empty()) return tags;

    // The pattern as a regex: each [tag] a part without "_", the rest literal
    std::string pattern = std::filesystem::path(fInputFormat).filename().string();
    if (pattern.size() > 5 && pattern.substr(pattern.size() - 5) == ".root")
        pattern.resize(pattern.size() - 5);
    const std::regex tagRegex(R"(\[([\w.]+)\])");
    const std::regex special(R"([.^$|()\[\]{}*+?\\])");
    std::vector<std::string> names;
    std::string regex;
    std::smatch match;
    std::string rest = pattern;
    while (std::regex_search(rest, match, tagRegex)) {
        regex += std::regex_replace(match.prefix().str(), special, R"(\$&)") + "([^_]+)";
        names.push_back(match[1].str());
        rest = match.suffix().str();
    }
    regex += std::regex_replace(rest, special, R"(\$&)");

    const std::string name = std::filesystem::path(fInputFileName).stem().string();
    std::smatch values;
    if (!std::regex_match(name, values, std::regex(regex))) {
        throw std::runtime_error("TRestDataSetProcessManager '" + fName + "': the input " + fInputFileName +
                                 " does not match the inputFormat \"" + fInputFormat + "\"");
    }
    for (size_t i = 0; i < names.size(); i++) tags[names[i]] = values[i + 1].str();
    return tags;
}

std::string TRestDataSetProcessManager::ResolveOutputFileName(const TRestDataSet& dataSet) const {
    const auto tags = ParseInputFormat();
    return TRestTools::ReplaceDelimited(fOutputFileName, '[', ']',
                                        [&](const std::string& key) -> std::optional<std::string> {
                                            // [key|default]: the default is for the keys of the runs
                                            const auto tag = tags.find(key.substr(0, key.find('|')));
                                            if (tag != tags.end()) return tag->second;
                                            return dataSet.ResolveFilePattern("[" + key + "]");
                                        });
}
