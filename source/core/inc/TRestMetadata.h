#pragma once

#include <yaml-cpp/yaml.h>

#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "TRestLogManager.h"

// ============================================================
//  TRestMetadata
//  Abstract base for all metadata/configuration objects.
//  Configuration is loaded from a YAML node or file.
// ============================================================
class TRestMetadata {
   protected:
    std::string fConfigFileName = "";
    std::string fSectionName    = "";
    YAML::Node  fNode;
    TRestLogManager::REST_Verbose_Level fVerboseLevel =
        TRestLogManager::REST_Verbose_Level::REST_Info;

   public:
    virtual std::string GetClassName() const = 0;

    explicit TRestMetadata(const std::string& sectionName, const YAML::Node& node);
    explicit TRestMetadata(const std::string& fileName,    const std::string& sectionName);

    void SetConfigFileName(const std::string& f) { fConfigFileName = f; }
    void SetSectionName(const std::string& s)    { fSectionName    = s; }
    void SetYAMLNode(const YAML::Node& node)      { fNode           = node; }

    virtual ~TRestMetadata() = default;

    virtual void LoadConfig()     = 0;
    virtual void Initialize()     = 0;
    virtual void PrintMetadata()  = 0;

    void ReadYAMLVerbose(YAML::Node& node);
};

class MetadataRegistry {
   public:
    using Creator = std::function<
        std::unique_ptr<TRestMetadata>(const std::string&, const YAML::Node&)>;

    static MetadataRegistry& Instance() {
        static MetadataRegistry inst;
        return inst;
    }

    MetadataRegistry(const MetadataRegistry&)            = delete;
    MetadataRegistry& operator=(const MetadataRegistry&) = delete;

    void Register(const std::string& type, Creator creator) {
        creators.emplace(type, std::move(creator));
    }

    std::unique_ptr<TRestMetadata> Create(const std::string& type,
                                          const std::string& name,
                                          const YAML::Node&  params) const {
        auto it = creators.find(type);
        if (it == creators.end())
            throw std::runtime_error("MetadataRegistry: unknown type '" + type + "'");
        return it->second(name, params);
    }

    bool Contains(const std::string& type) const { return creators.count(type) != 0; }

   private:
    MetadataRegistry() = default;
    std::map<std::string, Creator> creators;
};
