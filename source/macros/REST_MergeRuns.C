#include <TFile.h>
#include <TFileMerger.h>
#include <TKey.h>
#include <TLeaf.h>
#include <TTree.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "TRestMetadata.h"
#include "TRestSystemOfUnits.h"
#include "TRestTools.h"
#include "TRestYAMLUtils.h"

namespace REST_MergeRuns_Detail {

/// A run file: its TRestRun, the rest of its metadata and the branches of its trees
struct RunFile {
    std::string fileName;
    std::string runInstance;
    YAML::Node run;
    std::vector<int> subRuns;
    std::map<std::string, YAML::Node> metadata;
    std::map<std::string, std::vector<std::string>> trees;
};

RunFile ReadRunFile(const std::string& fileName) {
    std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "READ"));
    if (!file || file->IsZombie()) throw std::runtime_error("cannot open " + fileName);

    RunFile runFile;
    runFile.fileName = fileName;
    for (const auto& entry : TRestYAMLUtils::ReadMetadataStore(file.get())) {
        const std::string instance = entry.first.as<std::string>();
        const YAML::Node node = entry.second;
        if (!node.IsMap() || !node["class"]) continue;
        if (node["class"].as<std::string>() == "TRestRun") {
            runFile.runInstance = instance;
            runFile.run = node;
        } else {
            runFile.metadata[instance] = node;
        }
    }
    if (!runFile.run) throw std::runtime_error(fileName + " has no TRestRun");
    runFile.subRuns = TRestYAMLUtils::GetParam<std::vector<int>>(runFile.run, "mergedSubRuns");
    if (runFile.subRuns.empty())
        runFile.subRuns.push_back(TRestYAMLUtils::GetParam<int>(runFile.run, "subRunNumber", 0));

    // Event trees and analysis tree: name and type of each leaf
    for (const auto* object : *file->GetListOfKeys()) {
        const auto* key = static_cast<const TKey*>(object);
        if (std::string(key->GetClassName()) != "TTree") continue;
        auto* tree = file->Get<TTree>(key->GetName());
        if (!tree) continue;
        auto& branches = runFile.trees[key->GetName()];
        for (const auto* leafObject : *tree->GetListOfLeaves()) {
            const auto* leaf = static_cast<const TLeaf*>(leafObject);
            branches.push_back(std::string(leaf->GetName()) + "/" + leaf->GetTypeName());
        }
    }
    if (!runFile.trees.count("AnalysisTree")) throw std::runtime_error(fileName + " has no AnalysisTree");
    return runFile;
}

/// Copies the objects of `source` (recursively) but its trees and RESTMetadataStore, e.g. the readouts and
/// the geometry, which TFileMerger cannot merge
void CopyObjects(TDirectory* source, TDirectory* target) {
    for (const auto* object : *source->GetListOfKeys()) {
        auto* key = static_cast<TKey*>(const_cast<TObject*>(object));
        const std::string name = key->GetName();
        const std::string className = key->GetClassName();
        if (className == "TTree" || name == "RESTMetadataStore") continue;
        if (className == "TDirectoryFile" || className == "TDirectory") {
            auto* sourceDirectory = source->GetDirectory(name.c_str());
            TDirectory* targetDirectory = target->mkdir(name.c_str(), "", true);
            if (sourceDirectory && targetDirectory) CopyObjects(sourceDirectory, targetDirectory);
            continue;
        }
        std::unique_ptr<TObject> copy(key->ReadObj());
        if (!copy) continue;
        target->cd();
        copy->Write(name.c_str(), TObject::kOverwrite);
    }
}

/// The instance of the TRestGeant4Metadata of a run file (a simulation of restG4), empty if it has none
std::string Geant4Instance(const RunFile& runFile) {
    for (const auto& [instance, node] : runFile.metadata) {
        if (node["class"].as<std::string>() == "TRestGeant4Metadata") return instance;
    }
    return "";
}

/// What changes from a simulation to another of the same configuration: the seed, the events simulated and
/// requested and the showers and exposure of the cry generator
const std::vector<std::string> kSimulationKeys = {"seed", "nEvents", "nRequestedEntries"};
const std::vector<std::string> kCRYKeys = {"showersUsed", "exposureTime"};

/// The TRestGeant4Metadata without the keys that change from a simulation to another
YAML::Node SimulationConfiguration(const YAML::Node& node) {
    YAML::Node configuration = YAML::Clone(node);
    for (const auto& key : kSimulationKeys) configuration.remove(key);
    if (configuration["generator"] && configuration["generator"].IsMap()) {
        for (const auto& key : kCRYKeys) configuration["generator"].remove(key);
    }
    return configuration;
}

/// A number of the YAML (also with units, in REST units), 0 if it is not there
double NumberOf(const YAML::Node& node) {
    if (!node || !node.IsScalar()) return 0;
    return REST_Units::ParseValueWithUnits(node.Scalar()).value_or(0);
}

/// Copies the trees of the simulations in `output` adding to the eventID of each file the events simulated
/// in the previous ones (eventOffsets), as if they had been a single simulation: the events of different
/// files have different IDs (restG4 numbers the events from 0 in each simulation)
void MergeSimulationTrees(const std::vector<RunFile>& runFiles, const std::vector<long long>& eventOffsets,
                          TFile* output) {
    for (const auto& [treeName, branches] : runFiles.front().trees) {
        TTree* merged = nullptr;
        // Open until the next file sets its addresses in the merged tree (CopyAddresses)
        std::unique_ptr<TFile> previous;
        for (size_t i = 0; i < runFiles.size(); i++) {
            std::unique_ptr<TFile> file(TFile::Open(runFiles[i].fileName.c_str(), "READ"));
            if (!file || file->IsZombie()) throw std::runtime_error("cannot open " + runFiles[i].fileName);
            auto* tree = file->Get<TTree>(treeName.c_str());
            if (!tree) throw std::runtime_error(runFiles[i].fileName + " has no " + treeName);
            if (!merged) {
                output->cd();
                merged = tree->CloneTree(0);
            } else {
                tree->CopyAddresses(merged);
            }
            TLeaf* eventID = tree->GetLeaf("eventID");
            for (Long64_t entry = 0; entry < tree->GetEntries(); entry++) {
                tree->GetEntry(entry);
                if (eventID) *static_cast<int*>(eventID->GetValuePointer()) += eventOffsets[i];
                merged->Fill();
            }
            previous = std::move(file);
        }
        output->cd();
        merged->Write("", TObject::kOverwrite);
        delete merged;
    }
}

}  // namespace REST_MergeRuns_Detail

/// Merges the subruns of a run in a single run file: the files of filePattern (TRestTools::
/// GetFilesMatchingPattern) must have the same runNumber, consecutive subRunNumber (also files already
/// merged, by their mergedSubRuns), the same event trees and analysis tree (branches and types) and the
/// same processing (all their metadata but the TRestRun equal). The trees are merged as the threads of
/// TRestManager (TFileMerger), in subrun order; the other objects (readouts, geometry) are copied from the
/// first subrun. The TRestRun of the merged file is the one of the first
/// subrun with subRunNumber the first one, mergedSubRuns all of them, startTime the earliest, endTime the
/// latest, entriesSaved the sum and outputFileName the merged file.
///
/// Simulations (files with a TRestGeant4Metadata, restG4 outputs or their analysis) of the same
/// configuration with different seeds (e.g. restG4 --seed in parallel jobs) are merged as a single
/// simulation: their run number and subrun do not matter (the TRestRun is the one of the first file, without
/// mergedSubRuns); the TRestGeant4Metadata may differ only in seed, nEvents, nRequestedEntries and the
/// showersUsed and exposureTime of the cry generator, which are added up (the seed is the one of the first
/// file), and a seed repeated is an error (the same events). The files are merged in the order of the
/// pattern and the eventID of each one is shifted by the events simulated in the previous ones, so all the
/// events have different IDs (as REST_Geant4_MergeRestG4Files of legacy).
///
///    restRoot
///    REST_MergeRuns("R01234_*_ANA.root", "R01234_ANA.root")
///    REST_MergeRuns("Simulation_seed*.root", "Simulation.root")
int REST_MergeRuns(const std::string& filePattern, const std::string& outputFile) {
    using namespace REST_MergeRuns_Detail;
    try {
        std::vector<RunFile> runFiles;
        for (const auto& fileName : TRestTools::GetFilesMatchingPattern(filePattern)) {
            runFiles.push_back(ReadRunFile(TRestTools::GetFullPath(fileName)));
        }
        if (runFiles.size() < 2) throw std::runtime_error("less than two files match " + filePattern);
        const std::string outputPath = TRestTools::GetFullPath(outputFile);
        for (const auto& runFile : runFiles) {
            if (runFile.fileName == outputPath) throw std::runtime_error("the output file is an input file");
        }
        // Simulations: merged as a single one, in the order of the pattern; otherwise subruns in order
        const std::string geant4Instance = Geant4Instance(runFiles.front());
        const bool simulation = !geant4Instance.empty();
        if (!simulation) {
            std::sort(runFiles.begin(), runFiles.end(), [](const RunFile& a, const RunFile& b) {
                return a.subRuns.front() < b.subRuns.front();
            });
        }

        const RunFile& first = runFiles.front();
        const int runNumber = TRestYAMLUtils::GetParam<int>(first.run, "runNumber", -1);
        if (runNumber < 0 && !simulation) throw std::runtime_error(first.fileName + " has no run number");
        std::vector<int> subRuns;
        double startTime = 0, endTime = 0;
        long long entries = 0;
        // Simulations: the seeds, the events simulated before each file and the sums of the counts
        std::set<long long> seeds;
        std::vector<long long> eventOffsets;
        long long events = 0, requestedEntries = 0, showersUsed = 0;
        double exposureTime = 0;
        for (size_t i = 0; i < runFiles.size(); i++) {
            const RunFile& runFile = runFiles[i];
            if (!simulation && TRestYAMLUtils::GetParam<int>(runFile.run, "runNumber", -1) != runNumber) {
                throw std::runtime_error(runFile.fileName + " is not of the run " +
                                         std::to_string(runNumber));
            }
            if (!simulation && i > 0 && runFile.subRuns.front() != subRuns.back() + 1) {
                throw std::runtime_error("subruns not consecutive: " + std::to_string(subRuns.back()) +
                                         " and " + std::to_string(runFile.subRuns.front()) + " (" +
                                         runFile.fileName + ")");
            }
            if (runFile.trees != first.trees) {
                throw std::runtime_error(runFile.fileName + " has not the same trees and branches as " +
                                         first.fileName);
            }
            if (runFile.metadata.size() != first.metadata.size()) {
                throw std::runtime_error(runFile.fileName + " has not the same metadata as " +
                                         first.fileName);
            }
            for (const auto& [instance, node] : first.metadata) {
                const auto it = runFile.metadata.find(instance);
                bool equal = it != runFile.metadata.end();
                if (equal && simulation && instance == geant4Instance) {
                    equal = YAML::Dump(SimulationConfiguration(it->second)) ==
                            YAML::Dump(SimulationConfiguration(node));
                } else if (equal) {
                    equal = YAML::Dump(it->second) == YAML::Dump(node);
                }
                if (!equal) {
                    throw std::runtime_error(runFile.fileName + ": metadata '" + instance +
                                             "' not equal to the one of " + first.fileName +
                                             (simulation ? " (not the same simulation and processing)"
                                                         : " (not the same processing)"));
                }
            }
            if (simulation) {
                const YAML::Node& geant4 = runFile.metadata.at(geant4Instance);
                const auto seed = TRestYAMLUtils::GetParam<long long>(geant4, "seed", 0);
                if (!seeds.insert(seed).second) {
                    throw std::runtime_error(runFile.fileName + ": seed " + std::to_string(seed) +
                                             " repeated (the same events)");
                }
                eventOffsets.push_back(events);
                events += TRestYAMLUtils::GetParam<long long>(geant4, "nEvents", 0);
                requestedEntries += TRestYAMLUtils::GetParam<long long>(geant4, "nRequestedEntries", 0);
                if (geant4["generator"] && geant4["generator"].IsMap()) {
                    showersUsed += static_cast<long long>(NumberOf(geant4["generator"]["showersUsed"]));
                    exposureTime += NumberOf(geant4["generator"]["exposureTime"]);
                }
            }
            subRuns.insert(subRuns.end(), runFile.subRuns.begin(), runFile.subRuns.end());
            const double start = TRestYAMLUtils::GetParam<double>(runFile.run, "startTime", 0);
            const double end = TRestYAMLUtils::GetParam<double>(runFile.run, "endTime", 0);
            if (i == 0 || start < startTime) startTime = start;
            if (i == 0 || end > endTime) endTime = end;
            entries += TRestYAMLUtils::GetParam<long long>(runFile.run, "entriesSaved", 0);
        }

        if (simulation) {
            std::unique_ptr<TFile> output(TFile::Open(outputPath.c_str(), "RECREATE"));
            if (!output || output->IsZombie()) throw std::runtime_error("cannot create " + outputPath);
            MergeSimulationTrees(runFiles, eventOffsets, output.get());
            output->Close();
        } else {
            // Only the trees, as the threads of TRestManager::RunMultithreaded
            TFileMerger merger(kFALSE);
            merger.SetFastMethod(kFALSE);
            merger.OutputFile(outputPath.c_str(), "RECREATE");
            for (const auto& runFile : runFiles) merger.AddFile(runFile.fileName.c_str());
            for (const auto& [treeName, branches] : first.trees) merger.AddObjectNames(treeName.c_str());
            if (!merger.PartialMerge(TFileMerger::kAll | TFileMerger::kRegular | TFileMerger::kOnlyListed)) {
                throw std::runtime_error("error merging the files");
            }
        }

        // The rest from the first subrun (equal in all of them: same metadata), and the metadata: those of
        // the first subrun and the merged run
        YAML::Node run = YAML::Clone(first.run);
        if (!simulation) {
            run["subRunNumber"] = subRuns.front();
            run["mergedSubRuns"] = subRuns;
        }
        run["startTime"] = startTime;
        run["endTime"] = endTime;
        run["entriesSaved"] = entries;
        run["outputFileName"] = outputPath;
        std::map<std::string, YAML::Node> metadata = first.metadata;
        if (simulation) {
            YAML::Node geant4 = YAML::Clone(metadata.at(geant4Instance));
            geant4["nEvents"] = events;
            if (geant4["nRequestedEntries"]) geant4["nRequestedEntries"] = requestedEntries;
            if (geant4["generator"] && geant4["generator"]["showersUsed"]) {
                geant4["generator"]["showersUsed"] = showersUsed;
                geant4["generator"]["exposureTime"] = exposureTime;
            }
            metadata[geant4Instance] = geant4;
        }

        std::unique_ptr<TFile> file(TFile::Open(outputPath.c_str(), "UPDATE"));
        if (!file || file->IsZombie()) throw std::runtime_error("cannot open " + outputPath);
        {
            std::unique_ptr<TFile> firstFile(TFile::Open(first.fileName.c_str(), "READ"));
            if (!firstFile || firstFile->IsZombie())
                throw std::runtime_error("cannot open " + first.fileName);
            CopyObjects(firstFile.get(), file.get());
        }
        for (const auto& [instance, node] : metadata)
            TRestMetadata::WriteMetadata(file.get(), instance, node);
        TRestMetadata::WriteMetadata(file.get(), first.runInstance, run);
        file->Close();

        if (simulation) {
            std::cout << "REST_MergeRuns: " << runFiles.size() << " simulations (" << events << " events, "
                      << entries << " entries) merged in " << outputFile << std::endl;
        } else {
            std::cout << "REST_MergeRuns: run " << runNumber << ", subruns " << subRuns.front() << " to "
                      << subRuns.back() << " (" << runFiles.size() << " files) merged in " << outputFile
                      << std::endl;
        }
    } catch (const std::exception& error) {
        std::cerr << "REST_MergeRuns: " << error.what() << std::endl;
        return 1;
    }
    return 0;
}
