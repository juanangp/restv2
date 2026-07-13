#pragma once

#include <yaml-cpp/yaml.h>

#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <typeindex>

#include "TRestLogManager.h"

#include <TFile.h>

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


    /// Verbosity level used by REST logging.
    TRestLogManager::REST_Verbose_Level fVerboseLevel = TRestLogManager::REST_Verbose_Level::REST_Info;

   public:
    /// \brief Returns the concrete class name.
    /// \return Class name string.
    virtual std::string GetClassName() const = 0;

    /// \brief Returns the metadata instance name.
    /// \return Instance name.
    std::string GetName() const { return fName; }

    /// \brief Constructs metadata from explicit YAML node.
    /// \param instanceName Metadata instance name.
    /// \param node YAML node with metadata configuration.
    explicit TRestMetadata(const std::string& instanceName,
                           const YAML::Node& node);

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

    /// \brief Sets the associated YAML node.
    /// \param node YAML node.
    void SetYAMLNode(const YAML::Node& node) { fNode = node; }

    /// \brief Virtual destructor.
    virtual ~TRestMetadata() = default;

    /// \brief Loads configuration into internal members.
    virtual void LoadConfig() = 0;

    /// \brief Performs initialization after configuration loading.
    virtual void Initialize() = 0;

    /// \brief Prints metadata summary.
    virtual void PrintMetadata() = 0;

    static void WriteMetadata(TFile* file, const std::string& instanceName, const YAML::Node& configNode);
    static YAML::Node ReadMetadata(TFile* file, const std::string& instanceName);

    /// \brief Reads and applies verbose-level configuration from YAML.
    /// \param node YAML node containing verbosity information.
    void ReadYAMLVerbose(YAML::Node& node);

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
    using Creator = std::function<std::unique_ptr<TRestMetadata>(const std::string&,
                                                                 const YAML::Node&)>;

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
    std::unique_ptr<TRestMetadata> Create(const std::string& type, const std::string& instanceName,
                                          const YAML::Node& params) const {
        auto it = creators.find(type);
        if (it == creators.end()) throw std::runtime_error("MetadataClassRegistry: unknown type '" + type + "'");
        return it->second(instanceName, params);
    }

    /// \brief Overload: Creates a metadata object extracting the type from params["class"].
    std::unique_ptr<TRestMetadata> Create(const std::string& instanceName,
                                          const YAML::Node& params) const {
        if (!params["class"] || !params["class"].IsScalar()) {
            throw std::runtime_error("MetadataClassRegistry: 'class' key missing or invalid in YAML params "+instanceName);
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

class TRestMetadataFieldRegistry {
public:
    struct FieldActions {
        std::type_index classType;       
        std::type_index memberClassType; 
        std::function<void(TRestMetadata*, const YAML::Node&)> readFunc;
        std::function<void(TRestMetadata*, YAML::Node&)> writeFunc;

        // Añadimos un constructor explícito para inicializar los type_index correctamente
        FieldActions(std::type_index cType, std::type_index mType,
                     std::function<void(TRestMetadata*, const YAML::Node&)> rFunc,
                     std::function<void(TRestMetadata*, YAML::Node&)> wFunc)
            : classType(cType), memberClassType(mType), readFunc(rFunc), writeFunc(wFunc) {}
    };

    static TRestMetadataFieldRegistry& Instance() {
        static TRestMetadataFieldRegistry inst;
        return inst;
    }

    TRestMetadataFieldRegistry(const TRestMetadataFieldRegistry&) = delete;
    TRestMetadataFieldRegistry& operator=(const TRestMetadataFieldRegistry&) = delete;

    template <typename Class, typename MemberClass, typename T>
    void RegisterField(const std::string& yamlKey, T MemberClass::* memberPtr) {
        
        auto readFunc = [yamlKey, memberPtr](TRestMetadata* base, const YAML::Node& n) {
            if (auto* obj = dynamic_cast<MemberClass*>(base)) {
                try { 
                    if (n && n[yamlKey]) {
                        obj->*memberPtr = n[yamlKey].as<T>(); 
                    }
                } catch (...) {
                    std::cerr << "Error al leer campo '" << yamlKey << "'" << std::endl;
                }
            }
        };

        auto writeFunc = [yamlKey, memberPtr](TRestMetadata* base, YAML::Node& n) {
            if (auto* obj = dynamic_cast<MemberClass*>(base)) {
                n[yamlKey] = obj->*memberPtr;
            }
        };

        // Construimos el objeto llamando al nuevo constructor de la estructura de forma directa
        FieldActions actions(
            std::type_index(typeid(Class)),
            std::type_index(typeid(MemberClass)),
            readFunc,
            writeFunc
        );

        // Guardamos las acciones indexadas bajo el tipo de clase exacto de la plantilla
        fFieldMaps[actions.classType].push_back(actions);
    }

    void ApplyFields(std::type_index typeIdx, TRestMetadata* instance, const YAML::Node& params) {
        if (!instance || !params) return;
        auto it = fFieldMaps.find(typeIdx);
        if (it != fFieldMaps.end()) {
            for (const auto& actions : it->second) actions.readFunc(instance, params);
        }
    }

    void ApplyFieldsToYAML(std::type_index typeIdx, TRestMetadata* instance, YAML::Node& params) {
        if (!instance) return;
        auto it = fFieldMaps.find(typeIdx);
        if (it != fFieldMaps.end()) {
            for (const auto& actions : it->second) actions.writeFunc(instance, params);
        }
    }

private:
    TRestMetadataFieldRegistry() = default;
    std::map<std::type_index, std::vector<FieldActions>> fFieldMaps;
};

template <typename ClassName, typename TNode>
inline void TRestMetadata::UpdateParamsFromYAML(const TNode& processedNode) {
    auto typeIdx = std::type_index(typeid(ClassName));
    TRestMetadataFieldRegistry::Instance().ApplyFields(typeIdx, this, processedNode);
}

template <typename ClassName, typename TNode>
inline void TRestMetadata::UpdateYAMLFromParams(TNode& nodeToUpdate) {
    auto typeIdx = std::type_index(typeid(ClassName));
    TRestMetadataFieldRegistry::Instance().ApplyFieldsToYAML(typeIdx, this, nodeToUpdate);
}
