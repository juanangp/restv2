#ifndef TRESTMETADATA_H
#define TRESTMETADATA_H

#include <TFile.h>
#include <yaml-cpp/yaml.h>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <vector>

#include "TRestLogManager.h"
#include "TRestSystemOfUnits.h"

class TRestRun;

class TRestMetadataFieldRegistry;

/// \class TRestMetadata
/// \brief Abstract base class for metadata and configuration objects.
///
/// Instances are configured from YAML nodes or files and define a common
/// lifecycle interface (`LoadConfig`, `Initialize`, `PrintMetadata`).
class TRestMetadata {
   protected:
    /// Metadata instance name.
    std::string fName = "";

    /// Source YAML file name, when configuration was loaded from file.
    std::string fConfigFileName = "";

    /// Section name used to locate this metadata in YAML.
    std::string fSectionName = "";

    /// Raw YAML node associated with this metadata instance.
    YAML::Node fNode;

    /// Verbose level of the messages of this instance: `verbose` of its configuration, otherwise the global
    /// one (restManager --v)
    TRestLogManager::REST_Verbose_Level fVerboseLevel = TRestLogManager::globalVerboseLevel;

    /// Run this metadata belongs to (not owned), or nullptr. Set by TRestRun on the metadata it rebuilds
    /// from an input file, and by the process managers on their processes.
    TRestRun* fRunInfo = nullptr;

   public:
    /// \brief Context of the messages (RESTInfo, RESTDebug...) written in the methods of this instance: its
    /// name and verbose level. Found before the global ::GetLogContext inside the methods of the metadata.
    /// Public so the process managers can pass it to TRestLogScope.
    TRestLogContext GetLogContext() const { return {fVerboseLevel, &fName}; }

   private:
    /// \brief Reads `verbose` of the node (fVerboseLevel), and writes it so it is stored with the metadata
    void ReadVerbose();

   protected:
    /// \brief Creates a metadata of a section of a container (TRestManager, TRestProcessManager); without
    /// its own `verbose` it takes the one of this instance
    std::unique_ptr<TRestMetadata> CreateChildMetadata(const std::string& instanceName,
                                                       YAML::Node node) const;

   public:
    /// \brief Returns the concrete class name.
    /// \return Class name string.
    virtual std::string GetClassName() const { return "TRestMetadata"; }

    /// \brief Returns the metadata instance name.
    /// \return Instance name.
    std::string GetName() const { return fName; }

    /// \brief Constructs metadata from explicit YAML node.
    /// \param instanceName Metadata instance name.
    /// \param node YAML node with metadata configuration.
    explicit TRestMetadata(const std::string& instanceName, const YAML::Node& node);

    /// \brief Constructs metadata by loading a YAML file/section.
    /// \param fileName YAML file path.
    /// \param sectionName Section name in YAML.
    explicit TRestMetadata(const std::string& fileName, const std::string& sectionName);

    /// \brief Default constructor.
    TRestMetadata() = default;

    /// \brief Sets metadata instance name.
    /// \param name Instance name.
    void SetName(const std::string& name) { fName = name; }

    /// \brief Sets the source config file name.
    /// \param f Config file path.
    void SetConfigFileName(const std::string& f) { fConfigFileName = f; }

    /// \brief Sets the YAML section name.
    /// \param s Section name.
    void SetSectionName(const std::string& s) { fSectionName = s; }

    /// \brief Get the associated YAML node.
    /// \param node YAML node.
    YAML::Node GetYAMLNode() { return fNode; }

    /// \brief Sets the associated YAML node, and the verbose level of its `verbose`.
    /// \param node YAML node.
    void SetYAMLNode(const YAML::Node& node) {
        fNode = node;
        ReadVerbose();
    }

    /// \brief Virtual destructor.
    virtual ~TRestMetadata() = default;

    /// \brief Loads configuration into internal members.
    virtual void LoadConfig() = 0;

    /// \brief Performs initialization after configuration loading.
    virtual void Initialize() = 0;

    /// \brief Prints metadata summary.
    virtual void PrintMetadata() const;

    static void WriteMetadata(TFile* file, const std::string& instanceName, const YAML::Node& configNode);
    virtual void WriteMetadata(TFile* file);
    static YAML::Node ReadMetadata(TFile* file, const std::string& instanceName);

    /// \brief Sets the run this metadata belongs to. Overridable to finish loading what depends on the
    /// run (e.g. readouts stored in the run input file, see TRestDetectorReadoutManager).
    virtual void SetRunInfo(TRestRun* run) { fRunInfo = run; }
    TRestRun* GetRunInfo() const { return fRunInfo; }

    inline TRestLogManager::REST_Verbose_Level GetVerboseLevel() const { return fVerboseLevel; }

    /// \brief Throws if the configuration node has keys that are not registered fields of this instance
    /// (besides class, name and verbose). Called by MetadataClassRegistry::Create; to be called by who
    /// creates a metadata from a configuration in another way (e.g. the TRestManager of restManager).
    void CheckConfig() const;

    /// \brief Keys of the configuration node read by other objects sharing it (e.g. the spatial generator
    /// reads the generator section of TRestGeant4PrimaryGeneratorInfo), valid for CheckConfig
    virtual std::set<std::string> GetSharedNodeKeys() const { return {}; }

    template <typename ClassName, typename TNode>
    void UpdateParamsFromYAML(const TNode& processedNode);

    template <typename ClassName, typename TNode>
    void UpdateYAMLFromParams(TNode& nodeToUpdate);
};

/// \class MetadataClassRegistry
/// \brief Runtime factory registry for TRestMetadata-derived classes.
class MetadataClassRegistry {
   public:
    /// Function type used to create metadata instances.
    using Creator = std::function<std::unique_ptr<TRestMetadata>(const std::string&, const YAML::Node&)>;

    /// \brief Returns the singleton registry instance.
    /// \return Singleton reference.
    static MetadataClassRegistry& Instance() {
        static MetadataClassRegistry inst;
        return inst;
    }

    MetadataClassRegistry(const MetadataClassRegistry&) = delete;
    MetadataClassRegistry& operator=(const MetadataClassRegistry&) = delete;

    /// \brief Registers a metadata creator for a type key.
    /// \param type Type name key.
    /// \param creator Creator functor.
    void Register(const std::string& type, Creator creator) { creators.emplace(type, std::move(creator)); }

    /// \brief Creates a metadata object by type.
    /// \param type Type key.
    /// \param instanceName Metadata instance name.
    /// \param params YAML node with parameters.
    /// \return Newly created metadata instance.
    /// \brief Creates a metadata object by type explicitly specified.
    /// \param check Throw if params has keys that are not fields of the class (TRestMetadata::CheckConfig);
    /// false for metadata read from a file, written by REST.
    std::unique_ptr<TRestMetadata> Create(const std::string& type, const std::string& instanceName,
                                          const YAML::Node& params, bool check = true) const;

    /// \brief Overload: Creates a metadata object extracting the type from params["class"].
    std::unique_ptr<TRestMetadata> Create(const std::string& instanceName, const YAML::Node& params) const {
        if (!params["class"] || !params["class"].IsScalar()) {
            throw std::runtime_error("MetadataClassRegistry: 'class' key missing or invalid in YAML params " +
                                     instanceName);
        }

        std::string type = params["class"].as<std::string>();

        return Create(type, instanceName, params);
    }

    /// \brief Checks whether a type is registered.
    /// \param type Type key.
    /// \return `true` if the type exists in the registry.
    bool Contains(const std::string& type) const { return creators.count(type) != 0; }

   private:
    MetadataClassRegistry() = default;
    std::map<std::string, Creator> creators;
};

/// \class TRestMetadataFieldRegistry
/// \brief Fields of the metadata classes: the YAML key of each member, how it is read from the configuration
/// and written back. Only registered fields are read, and CheckFields throws for the keys of a
/// configuration that are not registered.
class TRestMetadataFieldRegistry {
   public:
    struct FieldActions {
        std::string yamlKey;
        std::function<void(TRestMetadata*, const YAML::Node&)> readFunc;
        std::function<void(TRestMetadata*, YAML::Node&)> writeFunc;
        std::function<bool(const TRestMetadata*)> appliesTo;
    };

    static TRestMetadataFieldRegistry& Instance() {
        static TRestMetadataFieldRegistry inst;
        return inst;
    }

    TRestMetadataFieldRegistry(const TRestMetadataFieldRegistry&) = delete;
    TRestMetadataFieldRegistry& operator=(const TRestMetadataFieldRegistry&) = delete;

    /// \brief A metadata member configured from its own section: SetYAMLNode(node[yamlKey]) and LoadConfig
    template <typename Class, typename Nested,
              std::enable_if_t<std::is_base_of_v<TRestMetadata, Nested>, int> = 0>
    void RegisterNestedField(const std::string& yamlKey, Nested Class::* memberPtr) {
        auto readFunc = [this, yamlKey, memberPtr](TRestMetadata* base, const YAML::Node& node) {
            auto* object = dynamic_cast<Class*>(base);
            if (!object || !node || !node[yamlKey]) return;
            auto& nested = object->*memberPtr;
            nested.SetYAMLNode(node[yamlKey]);
            nested.LoadConfig();
            CheckFields(&nested, node[yamlKey]);
        };
        auto writeFunc = [this, yamlKey, memberPtr](TRestMetadata* base, YAML::Node& node) {
            auto* object = dynamic_cast<Class*>(base);
            if (!object) return;
            auto& nested = object->*memberPtr;
            YAML::Node nestedNode = nested.GetYAMLNode();
            if (!nestedNode) return;
            ApplyFieldsToYAML(&nested, nestedNode);
            node[yamlKey] = nestedNode;
        };
        Add<Class>(yamlKey, readFunc, writeFunc);
    }

    /// \brief A metadata member of a polymorphic class created by factory(yamlKey, node[yamlKey]) (e.g.
    /// TRestGeant4EnergyDistribution::Create from its `type`)
    template <typename Class, typename Pointer, typename Factory>
    void RegisterFactoryField(const std::string& yamlKey, Pointer Class::* memberPtr, Factory factory) {
        auto readFunc = [this, yamlKey, memberPtr, factory](TRestMetadata* base, const YAML::Node& node) {
            auto* object = dynamic_cast<Class*>(base);
            if (!object || !node || !node[yamlKey]) return;
            object->*memberPtr = factory(yamlKey, node[yamlKey]);
            CheckFields((object->*memberPtr).get(), node[yamlKey]);
        };
        auto writeFunc = [this, yamlKey, memberPtr](TRestMetadata* base, YAML::Node& node) {
            auto* object = dynamic_cast<Class*>(base);
            if (!object || !(object->*memberPtr)) return;
            YAML::Node childNode = (object->*memberPtr)->GetYAMLNode();
            if (!childNode) return;
            ApplyFieldsToYAML((object->*memberPtr).get(), childNode);
            node[yamlKey] = childNode;
        };
        Add<Class>(yamlKey, readFunc, writeFunc);
    }

    /// \brief A list of metadata, each one built from a map of the sequence node[yamlKey] (or from the map
    /// itself if it is not a sequence)
    template <typename Class, typename T, std::enable_if_t<std::is_base_of_v<TRestMetadata, T>, int> = 0>
    void RegisterField(const std::string& yamlKey, std::vector<T> Class::* memberPtr) {
        auto readFunc = [this, yamlKey, memberPtr](TRestMetadata* base, const YAML::Node& node) {
            auto* object = dynamic_cast<Class*>(base);
            if (!object || !node || !node[yamlKey]) return;
            const YAML::Node source = node[yamlKey];
            auto& values = object->*memberPtr;
            values.clear();
            auto add = [&](const YAML::Node& child) {
                if (!child.IsMap()) throw std::runtime_error("each element must be a map");
                values.emplace_back(yamlKey, YAML::Node(child));
                CheckFields(&values.back(), child);
            };
            if (source.IsSequence()) {
                for (const auto& child : source) add(child);
            } else {
                add(source);
            }
        };

        auto writeFunc = [yamlKey, memberPtr](TRestMetadata* base, YAML::Node& n) {
            auto* object = dynamic_cast<Class*>(base);
            if (!object) return;
            // Each element is written in its own node (the one it keeps): assigning n[yamlKey] redirects
            // the node already there, so a single map element is not replaced by a sequence
            auto& values = object->*memberPtr;
            YAML::Node existing = n[yamlKey];
            YAML::Node sequence(YAML::NodeType::Sequence);
            for (auto& value : values) {
                YAML::Node child = value.GetYAMLNode();
                if (!child) child = YAML::Node(YAML::NodeType::Map);
                value.template UpdateYAMLFromParams<T>(child);
                if (values.size() == 1 && child.is(existing)) return;
                sequence.push_back(child);
            }
            n[yamlKey] = sequence;
        };

        Add<Class>(yamlKey, readFunc, writeFunc);
    }

    /// \brief A member of a type convertible from YAML (YAML::convert), including YAML::Node for structured
    /// configurations read by the class itself
    template <typename Class, typename MemberClass, typename T>
    void RegisterField(const std::string& yamlKey, T MemberClass::* memberPtr) {
        auto readFunc = [yamlKey, memberPtr](TRestMetadata* base, const YAML::Node& n) {
            auto* object = dynamic_cast<MemberClass*>(base);
            if (object && n && n[yamlKey]) object->*memberPtr = n[yamlKey].as<T>();
        };

        auto writeFunc = [yamlKey, memberPtr](TRestMetadata* base, YAML::Node& n) {
            auto* object = dynamic_cast<MemberClass*>(base);
            if (!object) return;
            if constexpr (std::is_same_v<T, YAML::Node>) {
                if (!(object->*memberPtr) || (object->*memberPtr).IsNull()) return;  // not given
            }
            n[yamlKey] = object->*memberPtr;
        };

        Add<Class>(yamlKey, readFunc, writeFunc);
    }

    /// \brief The configuration of Class may contain metadata (maps with `class`, e.g. the processes of a
    /// TRestProcessManager), created by Class itself
    template <typename Class>
    void RegisterChildMetadata() {
        fChildContainers.push_back(
            [](const TRestMetadata* base) { return dynamic_cast<const Class*>(base) != nullptr; });
    }

    /// \brief Reads the registered fields of the instance from params; throws if a value cannot be read
    void ApplyFields(TRestMetadata* instance, const YAML::Node& params);

    void ApplyFieldsToYAML(TRestMetadata* instance, YAML::Node& params);

    /// \brief Keys of the registered fields of the instance (its class and its base classes)
    std::set<std::string> GetFieldKeys(const TRestMetadata* instance) const;

    /// \brief Throws if the node has keys that are not registered fields of the instance, besides class,
    /// name, verbose, the keys of GetSharedNodeKeys and the metadata of RegisterChildMetadata classes
    void CheckFields(const TRestMetadata* instance, const YAML::Node& node) const;

   private:
    TRestMetadataFieldRegistry() = default;

    template <typename Class>
    void Add(const std::string& yamlKey, std::function<void(TRestMetadata*, const YAML::Node&)> readFunc,
             std::function<void(TRestMetadata*, YAML::Node&)> writeFunc) {
        auto appliesTo = [](const TRestMetadata* base) {
            return dynamic_cast<const Class*>(base) != nullptr;
        };
        fFieldMaps[std::type_index(typeid(Class))].push_back(
            {yamlKey, std::move(readFunc), std::move(writeFunc), appliesTo});
    }

    /// Fields of each registered class
    std::map<std::type_index, std::vector<FieldActions>> fFieldMaps;
    std::vector<std::function<bool(const TRestMetadata*)>> fChildContainers;
};

template <typename ClassName, typename TNode>
inline void TRestMetadata::UpdateParamsFromYAML(const TNode& processedNode) {
    TRestMetadataFieldRegistry::Instance().ApplyFields(this, processedNode);
}

template <typename ClassName, typename TNode>
inline void TRestMetadata::UpdateYAMLFromParams(TNode& nodeToUpdate) {
    TRestMetadataFieldRegistry::Instance().ApplyFieldsToYAML(this, nodeToUpdate);
}

#endif
