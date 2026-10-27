#ifndef TRESTMANAGER_H
#define TRESTMANAGER_H

#include <atomic>
#include <memory>
#include <vector>

#include "TRestMetadata.h"

class TRestRun;

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

    /// \brief Metadata objects of class T or derived from it (e.g. all the drift media), in declaration order.
    template <class T>
    std::vector<T*> GetMetadataOfType() const {
        std::vector<T*> result;
        for (const auto& meta : fMetaObjects) {
            if (auto* object = dynamic_cast<T*>(meta.get())) result.push_back(object);
        }
        return result;
    }

    static void RequestStop() { fStopRequested.store(true, std::memory_order_relaxed); }

    static bool StopRequested() { return fStopRequested.load(std::memory_order_relaxed); }

    static void ResetStopRequest() { fStopRequested.store(false, std::memory_order_relaxed); }

   private:
    void RunMultithreaded(unsigned int nThreads);
};
#endif
