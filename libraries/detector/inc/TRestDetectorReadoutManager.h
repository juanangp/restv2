#ifndef TRESTDETECTORREADOUTMANAGER_H
#define TRESTDETECTORREADOUTMANAGER_H

#include <map>
#include <string>
#include <vector>

#include "TRestDetectorReadout.h"
#include "TRestMetadata.h"

/// \brief Describes one readout import request declared in configuration.
class TRestReadoutInfo : public TRestMetadata {
   public:
    std::string fInstanceName = "";
    std::string fDecodingName = "";
    std::string fInputFileName = "";

    TRestReadoutInfo();
    TRestReadoutInfo(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestReadoutInfo"; }
};

/// \class TRestDetectorReadoutManager
/// \brief Loads and owns a set of detector readouts imported from a ROOT geometry container.
class TRestDetectorReadoutManager : public TRestMetadata {

   public:
    /// \brief Builds an empty readout manager.
    TRestDetectorReadoutManager();
    TRestDetectorReadoutManager(const std::string& name, const YAML::Node& node);
    TRestDetectorReadoutManager(const std::string& fileName, const std::string& sectionName);

    /// \brief Releases imported readouts and owned resources.
    virtual ~TRestDetectorReadoutManager();

    std::map<std::string, TRestDetectorReadout*> fReadoutMap;
    std::vector<TRestReadoutInfo> fReadoutInfo;

    // MANDATORY OVERRIDES FOR TRESTMETADATA
    virtual std::string GetClassName() const override { return "TRestDetectorReadoutManager"; }
    /// \brief Parses YAML readout requests and source file information.
    virtual void LoadConfig() override;
    /// \brief Imports requested readouts into the internal lookup map.
    virtual void Initialize() override { };

    void LoadReadout();

    void ListReadouts();

    /// \brief Returns true when a readout instance with the given name exists.
    bool HasReadout(const std::string& name) const { return fReadoutMap.find(name) != fReadoutMap.end(); }
    /// \brief Retrieves a readout by instance name, or nullptr when absent.
    TRestDetectorReadout* GetReadout(const std::string& name) const;

    void ViewReadoutGeometry(const std::vector<std::string>& names = {}, const std::string& option = "ogl") const;
    void ViewActiveEvent(const std::vector<int>& activeChannels,
                     const std::vector<std::string>& names = {}) const;
   private:
    void ViewImpl(const std::vector<int>& activeChannels, const std::vector<std::string>& names,
              const std::string& option) const;
    mutable TGeoManager* fViewGeo = nullptr;  //!

};

#endif
