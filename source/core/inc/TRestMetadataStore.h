#ifndef TRESTMETADATASTORE_H
#define TRESTMETADATASTORE_H

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "TRestMetadata.h"

class TFile;
class TRestRun;

/// \class TRestMetadataStore
/// \brief The metadata stored in a REST file (RESTMetadataStore), created as objects: used by TRestRun and
/// TRestDataSet to give access to the metadata of their file (e.g. a readout or a TRestGeant4Metadata with
/// its geometry).
///
/// Each metadata is created with MetadataClassRegistry (its class must be registered: its library loaded,
/// as in restRoot and restManager) from its stored configuration, not checked, and loads its objects
/// from the file (TRestMetadata::LoadFromFile, e.g. the readouts and geometries). A metadata that cannot
/// be created is skipped with an error message.
class TRestMetadataStore {
   private:
    std::vector<std::unique_ptr<TRestMetadata>> fMetadata;

   public:
    TRestMetadataStore();
    ~TRestMetadataStore();
    TRestMetadataStore(TRestMetadataStore&&) noexcept;
    TRestMetadataStore& operator=(TRestMetadataStore&&) noexcept;

    /// \brief Creates the metadata of a file (replacing the previous ones), except those of `skipClasses`
    /// \param run Run set to each metadata (SetRunInfo) before LoadFromFile, nullptr for none (dataset)
    void Load(TFile* file, TRestRun* run = nullptr, const std::set<std::string>& skipClasses = {});
    void Clear();

    const std::vector<std::unique_ptr<TRestMetadata>>& GetAll() const { return fMetadata; }
    bool IsEmpty() const { return fMetadata.empty(); }
    /// \brief The metadata of an instance name, nullptr if none
    TRestMetadata* Get(const std::string& instanceName) const;
    /// \brief The first metadata of a class (GetClassName), nullptr if none
    TRestMetadata* GetClass(const std::string& className) const;
    /// \brief The metadata of class T or derived from it, in the order of the file
    template <class T>
    std::vector<T*> GetOfType() const {
        std::vector<T*> result;
        for (const auto& metadata : fMetadata) {
            if (auto* object = dynamic_cast<T*>(metadata.get())) result.push_back(object);
        }
        return result;
    }
};

#endif
