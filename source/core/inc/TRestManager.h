#ifndef TRESTMANAGER_H
#define TRESTMANAGER_H

#include <memory>
#include <vector>

#include "TRestLogManager.h"
#include "TRestRun.h"
#include "TRestTools.h"

class TRestProcessManager;

/// \class TRestManager
/// \brief Top-level orchestrator that loads configured metadata classes.
///
/// `TRestManager` parses configuration and instantiates metadata objects.
/// Event pipeline execution is delegated to `TRestProcessManager`.
class TRestManager : public TRestMetadata {
   private:
    std::vector<std::unique_ptr<TRestMetadata>> fMetaObjects;
    TRestProcessManager* fProcessManager = nullptr;
    TRestRun* fConfiguredRun = nullptr;
    static std::atomic<bool> fStopRequested;

   public:
    TRestManager(const std::string& instanceName, const YAML::Node& node);
    TRestManager(const std::string& fileName, const std::string& sectionName);
    ~TRestManager() override = default;

    std::string GetClassName() const override { return "TRestManager"; }

    void Run();

    void LoadConfig() override;
    void Initialize() override {}
    void SaveMetadata();

    TRestMetadata* GetMetadataClass(const std::string& className) const {
        for (const auto& meta : fMetaObjects) {
            if (meta && meta->GetClassName() == className) return meta.get();
        }
        return nullptr;
    }

    TRestRun* GetRunInfo() const { return fConfiguredRun; }

    static void RequestStop() { fStopRequested.store(true, std::memory_order_relaxed); }

    static bool StopRequested() { return fStopRequested.load(std::memory_order_relaxed); }

    static void ResetStopRequest() { fStopRequested.store(false, std::memory_order_relaxed); }

   private:
    void RunMultithreaded(unsigned int nThreads);
};
#endif
