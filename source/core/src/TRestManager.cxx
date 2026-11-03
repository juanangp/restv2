#include "TRestManager.h"

#include <TFile.h>
#include <TFileMerger.h>

#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <thread>

#include "TRestAnalysisPlot.h"
#include "TRestCut.h"
#include "TRestDataSet.h"
#include "TRestDataSetProcessManager.h"
#include "TRestEventProcessManager.h"
#include "TRestLogManager.h"
#include "TRestRun.h"
#include "TRestTools.h"
#include "TRestYAMLUtils.h"

using namespace TRestTools;

std::atomic<bool> TRestManager::fStopRequested{false};

namespace {
const bool kRegistered = []() {
    // The metadata of the manager (run, pipeline, readouts...)
    TRestMetadataFieldRegistry::Instance().RegisterChildMetadata<TRestManager>();
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
    fDataSetProcessManager = nullptr;
    fDataSet = nullptr;
    fRunInfo = nullptr;

    for (const auto& element : fNode) {
        const auto key = element.first.as<std::string>();
        auto value = element.second;

        if (!value || value.IsScalar() || !value.IsMap()) continue;

        auto meta = CreateChildMetadata(key, value);
        if (!meta) continue;

        if (auto* processManager = dynamic_cast<TRestEventProcessManager*>(meta.get())) {
            fProcessManager = processManager;
        }
        if (auto* dataSetProcessManager = dynamic_cast<TRestDataSetProcessManager*>(meta.get())) {
            fDataSetProcessManager = dataSetProcessManager;
        }
        if (auto* dataSet = dynamic_cast<TRestDataSet*>(meta.get())) {
            fDataSet = dataSet;
        }
        if (auto* run = dynamic_cast<TRestRun*>(meta.get())) {
            fRunInfo = run;
        }

        meta->PrintMetadata();
        fMetaObjects.emplace_back(std::move(meta));
    }

    // After the loop: the run may be declared after the metadata that uses it
    if (fRunInfo) {
        for (const auto& meta : fMetaObjects) {
            if (meta.get() != fRunInfo) meta->SetRunInfo(fRunInfo);
        }
    }
}

void TRestManager::Run() {
    const auto plotList = GetMetadataOfType<TRestAnalysisPlot>();
    const bool plots = !plotList.empty();
    // Datasets: generation (TRestDataSet) and/or pipeline (TRestDataSetProcessManager), no TRestRun nor
    // event pipeline
    if (fDataSet || fDataSetProcessManager) {
        if (fProcessManager || fRunInfo) {
            throw std::runtime_error(
                "TRestManager::Run - a TRestDataSet or a TRestDataSetProcessManager works on datasets: the "
                "manager cannot have a TRestRun nor a TRestEventProcessManager");
        }
        if (fDataSet) {
            if (fDataSet->fFilePattern.empty()) {
                throw std::runtime_error("TRestManager::Run - the TRestDataSet '" + fDataSet->GetName() +
                                         "' has no filePattern (or --i)");
            }
            if (fDataSet->fOutputFileName.empty() && !fDataSetProcessManager) {
                throw std::runtime_error("TRestManager::Run - the TRestDataSet '" + fDataSet->GetName() +
                                         "' has no outputFileName (or --o)");
            }
            fDataSet->Generate(GetMetadataOfType<TRestCut>());
            // Written with the name resolved ([runStart], [runTag|Mixed]...), kept in fOutputFileName; with a
            // dataset pipeline only if it has an outputFileName (the dataset before the processes)
            if (!fDataSet->fOutputFileName.empty()) fDataSet->Write(fDataSet->fOutputFileName);
        }
        if (fDataSetProcessManager) {
            fDataSetProcessManager->SetManager(this);
            // The dataset generated is processed in memory: only the processed one is written
            if (fDataSet) {
                fDataSetProcessManager->Run(*fDataSet);
            } else {
                fDataSetProcessManager->Run();
            }
        }
        return;
    }
    // Only plots (RunPlots): each one with its own input, no TRestRun
    if (fProcessManager == nullptr && fRunInfo == nullptr && plots) return;
    if (fProcessManager == nullptr && plots) {
        throw std::runtime_error(
            "TRestManager::Run - a manager with only plots has no TRestRun: remove the TRestRun section '" +
            fRunInfo->GetName() +
            "', the input is the inputFileName of each TRestAnalysisPlot (--i sets it when there is no "
            "TRestRun)");
    }
    if (fProcessManager == nullptr) {
        throw std::runtime_error(
            "TRestManager::Run - no TRestEventProcessManager configured under manager section (a TRestRun is "
            "the "
            "input of the pipeline; for only plots, the input is the inputFileName of each "
            "TRestAnalysisPlot, "
            "or --i)");
    }
    if (fRunInfo == nullptr) {
        throw std::runtime_error("TRestManager::Run - no TRestRun configured under manager section.");
    }

    // The plots without inputFileName are made with the output of the pipeline
    for (const auto* plot : plotList) {
        if (plot->fInputFileName.empty() && fRunInfo->HasNullOutput()) {
            throw std::runtime_error("TRestManager::Run - the TRestAnalysisPlot '" + plot->GetName() +
                                     "' is made with the output of the pipeline: the TRestRun needs an "
                                     "outputFileName");
        }
    }

    if (!fileExists(fRunInfo->GetInputFileName())) {
        throw std::runtime_error("TRestManager::Run - input file " + fRunInfo->GetInputFileName() +
                                 " not found");
    }

    fProcessManager->SetManager(this);

    const unsigned int nThreads = fProcessManager->fNThreads;
    if (nThreads > 1) {
        RunMultithreaded(nThreads);
    } else {
        fProcessManager->Run();
    }

    if (fRunInfo->HasNullOutput()) {
        RESTError << "TRestManager: no outputFileName in the TRestRun, nothing has been saved" << RESTendl;
    }
}

void TRestManager::RunMultithreaded(unsigned int nThreads) {
    if (nThreads <= 1) {
        fProcessManager->Run();
        return;
    }

    long long totalEntries = fProcessManager->fEventsToProcess;

    if (totalEntries <= 0) {
        totalEntries = fRunInfo->GetEntries();
    }

    if (totalEntries <= 0) {
        RESTError << "TRestManager::RunMultithreaded: cannot determine the number of entries to run, falling "
                     "back to single-thread."
                  << RESTendl;
        fProcessManager->Run();
        return;
    }

    auto [runKeyOrig, runNodeOrig] = TRestYAMLUtils::GetMetadataClass(fNode, "TRestRun");
    if (runNodeOrig && !runNodeOrig.IsNull()) {
        TRestYAMLUtils::OverrideParam(runNodeOrig, "runNumber", std::to_string(fRunInfo->GetRunNumber()));
    }

    const std::string baseOutput = fRunInfo->GetOutputFileName();
    // Without output (/dev/null) the threads do not write either, and there is nothing to merge
    const bool nullOutput = fRunInfo->HasNullOutput();
    std::filesystem::path outPath(baseOutput);
    const std::string stem = outPath.stem().string();
    const std::string ext = outPath.extension().string();
    const std::string dir = outPath.parent_path().string();

    const long long chunk = totalEntries / nThreads;
    std::vector<std::pair<long long, long long>> ranges(nThreads);
    for (unsigned int t = 0; t < nThreads; ++t) {
        ranges[t] = {t * chunk, (t == nThreads - 1) ? totalEntries : (t + 1) * chunk};
    }

    std::vector<std::string> threadOutputs(nThreads);
    std::vector<std::thread> workers;

    for (unsigned int t = 0; t < nThreads; ++t) {
        threadOutputs[t] = (dir.empty() ? "" : dir + "/") + stem + ".thread" + std::to_string(t) + ext;
        if (nullOutput) threadOutputs[t] = baseOutput;

        workers.emplace_back([this, t, &ranges, &threadOutputs] {
            YAML::Node threadManagerNode = YAML::Clone(fNode);

            auto [runKey, runNode] = TRestYAMLUtils::GetMetadataClass(threadManagerNode, "TRestRun");
            auto [pipeKey, pipeNode] =
                TRestYAMLUtils::GetMetadataClass(threadManagerNode, "TRestEventProcessManager");
            if (!runNode || runNode.IsNull() || !pipeNode || pipeNode.IsNull()) {
                throw std::runtime_error("TRestManager::RunMultithreaded - run/pipeline no encontrados");
            }

            TRestYAMLUtils::OverrideParam(runNode, "outputFileName", threadOutputs[t]);
            TRestYAMLUtils::OverrideParam(pipeNode, "nThreads", std::string("1"));

            TRestManager threadManager(GetName(), threadManagerNode);

            auto* threadRun = threadManager.GetRunInfo();
            auto* threadPipeline = dynamic_cast<TRestEventProcessManager*>(
                threadManager.GetMetadataClass("TRestEventProcessManager"));
            if (!threadRun || !threadPipeline) {
                throw std::runtime_error(
                    "TRestManager::RunMultithreaded - cannot get retreive TRestEventProcessManager");
            }

            threadPipeline->SetEntryRange(ranges[t].first, ranges[t].second);
            threadPipeline->SetManager(&threadManager);
            threadPipeline->Run();

            threadRun->CloseFiles();
        });
    }

    for (auto& w : workers) w.join();

    if (nullOutput) return;

    TFileMerger merger(kFALSE);
    merger.SetFastMethod(kFALSE);

    merger.SetMergeOptions(std::string_view("no-fast unbosom"));
    merger.OutputFile(baseOutput.c_str(), "RECREATE");
    for (unsigned int t = 0; t < nThreads; ++t) merger.AddFile(threadOutputs[t].c_str());
    if (!merger.Merge()) {
        throw std::runtime_error("TRestManager::RunMultithreaded - error merging output files");
    }
    for (const auto& f : threadOutputs) std::filesystem::remove(f);

    fRunInfo->OpenOutputFile(baseOutput, "UPDATE");

    if (StopRequested()) {
        RESTError << "TRestManager::RunMultithreaded: Stop requested, output file may be cropped" << RESTendl;
    }

    RESTLog << "TRestManager: " << nThreads << " file merged in " << baseOutput << RESTendl;
}

void TRestManager::SaveMetadata() {
    // Without pipeline (only plots) there is no output file
    if (fRunInfo == nullptr || !fRunInfo->HasOutputFileOpen()) return;

    fRunInfo->AddHistoricMetadata();

    // Last saved event type, the one selected when the file is opened (otherwise the one of the input file)
    if (fProcessManager && !fProcessManager->GetOutputEvent().empty())
        fRunInfo->SetOutputEvent(fProcessManager->GetOutputEvent());

    for (const auto& meta : fMetaObjects) {
        fRunInfo->AddMetadata(meta.get());
    }
}

void TRestManager::RunPlots() {
    const auto plots = GetMetadataOfType<TRestAnalysisPlot>();
    if (plots.empty()) return;
    // The output of the pipeline, for the plots without inputFileName; closed, so the plots can update it
    // (outputFileName of each plot)
    std::string pipelineOutput;
    if (fProcessManager && fRunInfo) {
        pipelineOutput = fRunInfo->GetOutputFileName();
        fRunInfo->CloseFiles();
    }
    if (fDataSet) pipelineOutput = fDataSet->fOutputFileName;
    if (fDataSetProcessManager) pipelineOutput = fDataSetProcessManager->fOutputFileName;
    for (auto* plot : plots) {
        const std::string fileName = plot->fInputFileName.empty() ? pipelineOutput : plot->fInputFileName;
        if (fileName.empty()) {
            throw std::runtime_error("TRestManager::RunPlots - the TRestAnalysisPlot '" + plot->GetName() +
                                     "' has no input: its inputFileName (or --i), or a pipeline");
        }
        plot->PlotCombinedCanvas(fileName, this);
    }
}
