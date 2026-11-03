#include <TFile.h>
#include <TFileMerger.h>
#include <TKey.h>
#include <TLeaf.h>
#include <TTree.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "TRestMetadata.h"
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
///    restRoot
///    REST_MergeRuns("R01234_*_ANA.root", "R01234_ANA.root")
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
        std::sort(runFiles.begin(), runFiles.end(),
                  [](const RunFile& a, const RunFile& b) { return a.subRuns.front() < b.subRuns.front(); });

        const RunFile& first = runFiles.front();
        const int runNumber = TRestYAMLUtils::GetParam<int>(first.run, "runNumber", -1);
        if (runNumber < 0) throw std::runtime_error(first.fileName + " has no run number");
        std::vector<int> subRuns;
        double startTime = 0, endTime = 0;
        long long entries = 0;
        for (size_t i = 0; i < runFiles.size(); i++) {
            const RunFile& runFile = runFiles[i];
            if (TRestYAMLUtils::GetParam<int>(runFile.run, "runNumber", -1) != runNumber) {
                throw std::runtime_error(runFile.fileName + " is not of the run " +
                                         std::to_string(runNumber));
            }
            if (i > 0 && runFile.subRuns.front() != subRuns.back() + 1) {
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
                if (it == runFile.metadata.end() || YAML::Dump(it->second) != YAML::Dump(node)) {
                    throw std::runtime_error(runFile.fileName + ": metadata '" + instance +
                                             "' not equal to the one of " + first.fileName +
                                             " (not the same processing)");
                }
            }
            subRuns.insert(subRuns.end(), runFile.subRuns.begin(), runFile.subRuns.end());
            const double start = TRestYAMLUtils::GetParam<double>(runFile.run, "startTime", 0);
            const double end = TRestYAMLUtils::GetParam<double>(runFile.run, "endTime", 0);
            if (i == 0 || start < startTime) startTime = start;
            if (i == 0 || end > endTime) endTime = end;
            entries += TRestYAMLUtils::GetParam<long long>(runFile.run, "entriesSaved", 0);
        }

        // Only the trees, as the threads of TRestManager::RunMultithreaded
        TFileMerger merger(kFALSE);
        merger.SetFastMethod(kFALSE);
        merger.OutputFile(outputPath.c_str(), "RECREATE");
        for (const auto& runFile : runFiles) merger.AddFile(runFile.fileName.c_str());
        for (const auto& [treeName, branches] : first.trees) merger.AddObjectNames(treeName.c_str());
        if (!merger.PartialMerge(TFileMerger::kAll | TFileMerger::kRegular | TFileMerger::kOnlyListed)) {
            throw std::runtime_error("error merging the files");
        }

        // The rest from the first subrun (equal in all of them: same metadata), and the metadata: those of
        // the first subrun and the merged run
        YAML::Node run = YAML::Clone(first.run);
        run["subRunNumber"] = subRuns.front();
        run["mergedSubRuns"] = subRuns;
        run["startTime"] = startTime;
        run["endTime"] = endTime;
        run["entriesSaved"] = entries;
        run["outputFileName"] = outputPath;

        std::unique_ptr<TFile> file(TFile::Open(outputPath.c_str(), "UPDATE"));
        if (!file || file->IsZombie()) throw std::runtime_error("cannot open " + outputPath);
        {
            std::unique_ptr<TFile> firstFile(TFile::Open(first.fileName.c_str(), "READ"));
            if (!firstFile || firstFile->IsZombie())
                throw std::runtime_error("cannot open " + first.fileName);
            CopyObjects(firstFile.get(), file.get());
        }
        for (const auto& [instance, node] : first.metadata)
            TRestMetadata::WriteMetadata(file.get(), instance, node);
        TRestMetadata::WriteMetadata(file.get(), first.runInstance, run);
        file->Close();

        std::cout << "REST_MergeRuns: run " << runNumber << ", subruns " << subRuns.front() << " to "
                  << subRuns.back() << " (" << runFiles.size() << " files) merged in " << outputFile
                  << std::endl;
    } catch (const std::exception& error) {
        std::cerr << "REST_MergeRuns: " << error.what() << std::endl;
        return 1;
    }
    return 0;
}
