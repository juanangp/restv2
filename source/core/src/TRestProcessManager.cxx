#include "TRestProcessManager.h"
#include "TRestTools.h"


static const bool TRestProcessManager_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestProcessManager>("inputAnalysisStorage", &TRestProcessManager::fInputAnalysisStorage);
    reg.RegisterField<TRestProcessManager>("inputEventStorage", &TRestProcessManager::fInputEventStorage);
    reg.RegisterField<TRestProcessManager>("outputEventStorage", &TRestProcessManager::fOuputEventStorage);
    reg.RegisterField<TRestProcessManager>("eventsToProcess", &TRestProcessManager::fEventsToProcess);
    return true;
}();

// ---------------------------------------------------------------------------
// Self-registration
// ---------------------------------------------------------------------------
namespace {
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestProcessManager",
        [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestManager>(instanceName, params);
        });
    return true;
}();
}  // namespace

/// \brief Constructs a generic readout metadata object with default name.
TRestProcessManager::TRestProcessManager() : TRestMetadata() {
    fName = "TRestProcessManager";
}

TRestProcessManager::TRestProcessManager(const std::string& instanceName, const YAML::Node& node)
    : TRestMetadata(instanceName, node) {
    LoadConfig();
}

TRestProcessManager::TRestProcessManager(const std::string& fileName, const std::string& sectionName)
    : TRestMetadata(fileName, sectionName) {
    LoadConfig();
}

/// \brief Initializes geometry and decoding from YAML.
void TRestProcessManager::LoadConfig() {

    UpdateParamsFromYAML<TRestProcessManager>(fNode);

    //Sync resolved parameters to the node
    UpdateYAMLFromParams<TRestProcessManager>(fNode);
}

void TRestProcessManager::LoadProcesses() {

  for (const auto& element : fNode){
        const auto key = element.first.as<std::string>();
        auto value = element.second;

        if(value.IsScalar())continue;

        std::unique_ptr<TRestMetadata> meta = 
        MetadataClassRegistry::Instance().Create(key, value);

        if(meta) meta->PrintMetadata();
        else continue;

        fMetaObjects.emplace_back(meta);

        std::unique_ptr<TRestEventProcess> proc(dynamic_cast<TRestEventProcess*>(meta.release()));
        if(proc) fProcessChain.emplace_back(std::unique_ptr<TRestEventProcess>(proc));
    }

}

void TRestProcessManager::Run(){

 for (auto& proc : fProcessChain) {
        proc->SetRunInfo(fRunInfo);
        proc->Initialize();
    }

  if (fOutputEventStorage && !fRunInfo.HasOutputFileOpen()) {
     fRunInfo->OpenOutputFile();
  }

  for (const auto &proc : fProcessChain) {
    if(proc.fInputEvent != "None" && !proc.fInputEvent.empty() ){
      const std::string inputEvent = proc.GetInputEvent();
      const std::string outputEvent = proc.GetOutputEvent();

        if (fEventPool.find(outputName) == fEventPool.end()) {
            std::string inputClassName;
            if (inputName != "None" && inputName != "") {
                inputClassName = (fEventPool.find(inputName) != fEventPool.end())
                                     ? fEventPool[inputName]->GetClassName()
                                     : restRun.GetInputEvent(inputName).GetClassName();
            } else {
                inputClassName = fPipelineOutputClasses[i];
                if (inputClassName.empty()) {
                    throw std::runtime_error("TRestManager::Run - outputClass must be specified for generator process '" + procClassName + "'");
                }
            }

            auto outEvent = EventRegistry::Instance().Create(inputClassName, outputName);

            if (fOutputEventStorage) {
                restRun.RegisterEvent<TRestEvent>(outputName, *outEvent);
            }

            fEventPool[outputName] = std::move(outEvent);
        }


    
    }
  }

}
