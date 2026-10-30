#ifndef TRESTDETECTORREADOUTMANAGER_H
#define TRESTDETECTORREADOUTMANAGER_H

#include <map>
#include <string>
#include <vector>

#include "TRestMetadata.h"

class TGeoManager;
class TRestDetectorReadout;

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
    /// \brief Parses YAML readout requests (readouts are imported by SetRunInfo or LoadReadout).
    virtual void LoadConfig() override;
    /// \brief Imports requested readouts into the internal lookup map.
    virtual void Initialize() override {};

    /// \brief Imports the requested readouts. Called by SetRunInfo; call it directly to use the manager
    /// without a run (then kSelfFile readouts cannot be loaded).
    void LoadReadout();

    using TRestMetadata::WriteMetadata;
    /// \brief inputFileName value meaning "the input file of the run this manager belongs to"
    /// (TRestRun::GetInputFile).
    inline static const std::string kSelfFile = "self";

    /// \brief Sets the run and imports the readouts (LoadReadout).
    void SetRunInfo(TRestRun* run) override;

    /// \brief Writes the manager YAML plus, for each imported readout, its geometry and only the decoding
    /// in use (TRestDetectorReadout::Export), so the file is self-contained. The stored inputFileName of
    /// those readouts is kSelfFile, so it survives renaming or moving the file.
    void WriteMetadata(TFile* file) override;

    void ListReadouts();

    /// \brief Returns true when a readout instance with the given name exists.
    bool HasReadout(const std::string& name) const { return fReadoutMap.find(name) != fReadoutMap.end(); }
    /// \brief Retrieves a readout by instance name, or nullptr when absent.
    TRestDetectorReadout* GetReadout(const std::string& name) const;

    void ViewReadoutGeometry(const std::vector<std::string>& names = {},
                             const std::string& option = "ogl") const;
    void ViewActiveEvent(const std::vector<int>& activeChannels,
                         const std::vector<std::string>& names = {}) const;

   private:
    void ViewImpl(const std::vector<int>& activeChannels, const std::vector<std::string>& names,
                  const std::string& option) const;
    mutable TGeoManager* fViewGeo = nullptr;  //!
};

#endif
