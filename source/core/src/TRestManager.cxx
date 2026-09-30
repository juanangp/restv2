#include "TRestManager.h"

#include <filesystem>
#include <stdexcept>
#include <thread>

#include <TFileMerger.h>

#include "TRestProcessManager.h"

using namespace TRestTools;

std::atomic<bool> TRestManager::fStopRequested{false};

namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestManager", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestManager>(instanceName, params);
        });
    return true;
}();
}  // namespace

TRestManager::TRestManager(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestManager::TRestManager(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

void TRestManager::LoadConfig() {
    fMetaObjects.clear();
    fProcessManager = nullptr;
    fConfiguredRun = nullptr;

    for (const auto& element : fNode) {
        const auto key = element.first.as<std::string>();
        auto value = element.second;

        if (!value || value.IsScalar() || !value.IsMap()) continue;

        auto meta = MetadataClassRegistry::Instance().Create(key, value);
        if (!meta) continue;

        if (auto* processManager = dynamic_cast<TRestProcessManager*>(meta.get())) {
            fProcessManager = processManager;
        }
        if (auto* run = dynamic_cast<TRestRun*>(meta.get())) {
            fConfiguredRun = run;
        }

        meta->PrintMetadata();
        fMetaObjects.emplace_back(std::move(meta));
    }
}

void TRestManager::Run() {
    if (fConfiguredRun == nullptr) {
        throw std::runtime_error("TRestManager::Run - no TRestRun configured under manager section.");
    }
    if (fProcessManager == nullptr) {
        throw std::runtime_error(
            "TRestManager::Run - no TRestProcessManager configured under manager section.");
    }

    if(!fileExists(fConfiguredRun->GetInputFileName())){
        throw std::runtime_error(
            "TRestManager::Run - input file "+fConfiguredRun->GetInputFileName()+ " not found");
    }


    fProcessManager->SetRunInfo(fConfiguredRun);
    fProcessManager->SetManager(this);

    const unsigned int nThreads = fProcessManager->fNThreads;
    if (nThreads > 1) {
        RunMultithreaded(nThreads);
    } else {
        fProcessManager->Run();
    }
}

void TRestManager::RunMultithreaded(unsigned int nThreads) {
    if (nThreads <= 1) {
        fProcessManager->Run();
        return;
    }

    Long64_t totalEntries = fProcessManager->fEventsToProcess;

    if (totalEntries <= 0) {
        totalEntries = fConfiguredRun->GetEntries();
    }

    if (totalEntries <= 0) {
        RESTError << "TRestManager::RunMultithreaded: cannot determine the number of entries to run, falling back to single-thread." << RESTendl;
        fProcessManager->Run();
        return;
    }

    auto [runKeyOrig, runNodeOrig] = TRestTools::GetMetadataClass(fNode, "TRestRun");
    if (runNodeOrig && !runNodeOrig.IsNull()) {
        TRestTools::OverrideYAMLParam(runNodeOrig, "runNumber",
                                      std::to_string(fConfiguredRun->GetRunNumber()));
    }

    const std::string baseOutput = fConfiguredRun->GetOutputFileName();
    std::filesystem::path outPath(baseOutput);
    const std::string stem = outPath.stem().string();
    const std::string ext = outPath.extension().string();
    const std::string dir = outPath.parent_path().string();

    const Long64_t chunk = totalEntries / nThreads;
    std::vector<std::pair<Long64_t, Long64_t>> ranges(nThreads);
    for (unsigned int t = 0; t < nThreads; ++t) {
        ranges[t] = {t * chunk, (t == nThreads - 1) ? totalEntries : (t + 1) * chunk};
    }

    std::vector<std::string> threadOutputs(nThreads);
    std::vector<std::thread> workers;

    for (unsigned int t = 0; t < nThreads; ++t) {
        threadOutputs[t] = (dir.empty() ? "" : dir + "/") + stem + ".thread" + std::to_string(t) + ext;

        workers.emplace_back([this, t, &ranges, &threadOutputs] {
            YAML::Node threadManagerNode = YAML::Clone(fNode);

            auto [runKey, runNode] = TRestTools::GetMetadataClass(threadManagerNode, "TRestRun");
            auto [pipeKey, pipeNode] = TRestTools::GetMetadataClass(threadManagerNode, "TRestProcessManager");
            if (!runNode || runNode.IsNull() || !pipeNode || pipeNode.IsNull()) {
                throw std::runtime_error("TRestManager::RunMultithreaded - run/pipeline no encontrados");
            }

            TRestTools::OverrideYAMLParam(runNode, "outputFileName", threadOutputs[t]);
            TRestTools::OverrideYAMLParam(pipeNode, "nThreads", std::string("1"));

            TRestManager threadManager(GetName(), threadManagerNode);

            auto* threadRun = threadManager.GetRunInfo();
            auto* threadPipeline =
                dynamic_cast<TRestProcessManager*>(threadManager.GetMetadataClass("TRestProcessManager"));
            if (!threadRun || !threadPipeline) {
                throw std::runtime_error(
                    "TRestManager::RunMultithreaded - cannot get retreive TRestProcessManager");
            }

            threadPipeline->SetEntryRange(ranges[t].first, ranges[t].second);
            threadPipeline->SetRunInfo(threadRun);
            threadPipeline->SetManager(&threadManager);
            threadPipeline->Run();

            threadRun->CloseFiles();
        });
    }

    for (auto& w : workers) w.join();

    TFileMerger merger(kFALSE);
    merger.SetFastMethod(kFALSE); 

    merger.SetMergeOptions(TString("no-fast unbosom"));
    merger.OutputFile(baseOutput.c_str(), "RECREATE");
    for (unsigned int t = 0; t < nThreads; ++t) merger.AddFile(threadOutputs[t].c_str());
    if (!merger.Merge()) {
        throw std::runtime_error("TRestManager::RunMultithreaded - error merging output files");
    }
    for (const auto& f : threadOutputs) std::filesystem::remove(f);

    fConfiguredRun->OpenOutputFile(baseOutput, "UPDATE");

    if (StopRequested()){
      RESTError<<"TRestManager::RunMultithreaded: Stop requested, output file may miss data"<<RESTendl;
    }

    RESTLog << "TRestManager: " << nThreads << " file merged in " << baseOutput
            << RESTendl;
}

void TRestManager::SaveMetadata() {
    if (fConfiguredRun == nullptr) {
        throw std::runtime_error("TRestManager::Run - no TRestRun configured under manager section.");
    }

    fConfiguredRun->AddHistoricMetadata();

    for (const auto& meta : fMetaObjects) {
        fConfiguredRun->AddMetadata(meta.get());
    }
}
