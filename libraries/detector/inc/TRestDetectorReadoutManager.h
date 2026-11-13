#ifndef TRESTDETECTORREADOUTMANAGER_H
#define TRESTDETECTORREADOUTMANAGER_H

#include <map>
#include <string>
#include <vector>

#include "TRestMetadata.h"

class TGeoManager;
class TRestDetectorReadout;

/// \class TRestReadoutInfo
/// \brief Describes one readout import request declared in configuration: an entry of the `readouts` list
/// of a TRestDetectorReadoutManager (keys instanceName, decodingName and inputFileName).
class TRestReadoutInfo : public TRestMetadata {
   public:
    /// `instanceName`: name of the readout in the file (the name of its YAML section when exported)
    std::string fInstanceName = "";
    /// `decodingName`: name of the decoding stored with the geometry (the one given to Export)
    std::string fDecodingName = "";
    /// `inputFileName`: ROOT file with the readout, or TRestDetectorReadoutManager::kSelfFile
    std::string fInputFileName = "";

    /// \brief Builds an empty entry.
    TRestReadoutInfo();
    /// \brief Builds the entry from its YAML node (LoadConfig).
    TRestReadoutInfo(const std::string& name, const YAML::Node& node);
    /// \brief Reads the entry keys.
    void LoadConfig() override;
    /// \brief Nothing to initialise.
    void Initialize() override {}
    /// \brief Returns the class name, "TRestReadoutInfo".
    std::string GetClassName() const override { return "TRestReadoutInfo"; }
};

/// \class TRestDetectorReadoutManager
/// \brief Loads and owns a set of detector readouts imported from a ROOT geometry container.
///
/// All the readouts are loaded (e.g. to view an event in the whole detector), but the data of a file are
/// those of a single readout, `readout` (e.g. "micromegas" or "veto"; the detectors are split by
/// TRestGeant4ToDetectorHitsProcess volumes in simulations and by TRestRawSignalChannelSelectionProcess in
/// acquired data). The processes (and TRestDataSetGainMap) use it unless they give their own `readout`
/// (SelectReadout).
///
/// Parameters:
/// - readout: instance name of the readout of the data; empty: the only readout loaded.
/// - readouts: list of readouts to import (TRestReadoutInfo): instanceName, decodingName (the name given
///   to TRestDetectorReadout::Export) and inputFileName (the ROOT file written by Export, or kSelfFile; it
///   can be a URL, downloaded and cached with TRestTools::GetLocalFile, e.g. a versioned readout of
///   rest-common).
///
/// The readouts are imported when the manager gets its run (SetRunInfo) or is read from a file
/// (LoadFromFile). When the manager is written to an output file (WriteMetadata), the imported readouts
/// are stored in it, so the output file is self-contained: only the data readout if `readout` is given (the
/// others are not stored, nor listed in the stored `readouts`), all of them otherwise. A dataset of the file
/// keeps that readout only, e.g. one plane of TREX-DM.
///
///    readouts:
///      class: "TRestDetectorReadoutManager"
///      readout: "micromegas"  # empty: the only readout loaded
///      readouts:
///        - {instanceName: "micromegas", decodingName: "iaxo_d1_iaxolab", inputFileName: "readout.root"}
///        - {instanceName: "veto", decodingName: "veto", inputFileName: "veto_readout.root"}
///
/// \author JuanAn García <juanangp@unizar.es>
class TRestDetectorReadoutManager : public TRestMetadata {
   public:
    /// \brief Builds an empty readout manager.
    TRestDetectorReadoutManager();
    /// \brief Builds the manager from an in-memory YAML section (LoadConfig; nothing is imported yet).
    TRestDetectorReadoutManager(const std::string& name, const YAML::Node& node);
    /// \brief Builds the manager from the section sectionName of a YAML file (LoadConfig).
    TRestDetectorReadoutManager(const std::string& fileName, const std::string& sectionName);

    /// \brief Releases imported readouts and owned resources.
    virtual ~TRestDetectorReadoutManager();

    /// Imported readouts by instance name (owned by the manager)
    std::map<std::string, TRestDetectorReadout*> fReadoutMap;
    /// Readouts to import (`readouts`)
    std::vector<TRestReadoutInfo> fReadoutInfo;
    /// Readout of the data of the file (instance name); empty: the only readout loaded
    std::string fDataReadoutName = "";

    // MANDATORY OVERRIDES FOR TRESTMETADATA
    /// \brief Returns the class name, "TRestDetectorReadoutManager".
    virtual std::string GetClassName() const override { return "TRestDetectorReadoutManager"; }
    /// \brief Parses YAML readout requests (readouts are imported by SetRunInfo or LoadReadout).
    virtual void LoadConfig() override;
    /// \brief Nothing to initialise (the readouts are imported by LoadReadout).
    virtual void Initialize() override {};

    /// \brief Imports the requested readouts (deleting those already imported): for each entry, reads the
    /// readout YAML and imports its geometry and the decoding decodingName from inputFileName. An entry that
    /// cannot be imported gives an error and is skipped. Called by SetRunInfo; call it directly to use the
    /// manager without a run (then kSelfFile readouts cannot be loaded).
    void LoadReadout();

    using TRestMetadata::WriteMetadata;
    /// \brief inputFileName value meaning "the file this manager is read from" (LoadFromFile, e.g. a run or
    /// a dataset file), otherwise the input file of the run this manager belongs to (TRestRun::GetInputFile).
    inline static const std::string kSelfFile = "self";

    /// \brief Sets the run and imports the readouts (LoadReadout).
    void SetRunInfo(TRestRun* run) override;

    /// \brief Imports the readouts, those of kSelfFile from the file the manager is read from (e.g. a
    /// dataset); nothing if they are already imported (SetRunInfo of the run of the file).
    void LoadFromFile(TFile* file) override;

    /// \brief Writes the manager YAML plus, for each imported readout, its geometry and only the decoding
    /// in use (TRestDetectorReadout::Export), so the file is self-contained. The stored inputFileName of
    /// those readouts is kSelfFile, so it survives renaming or moving the file.
    void WriteMetadata(TFile* file) override;

    /// \brief Prints the name and class of each imported readout.
    void ListReadouts();

    /// \brief Returns true when a readout instance with the given name exists.
    bool HasReadout(const std::string& name) const { return fReadoutMap.find(name) != fReadoutMap.end(); }
    /// \brief Retrieves a readout by instance name, or nullptr when absent.
    TRestDetectorReadout* GetReadout(const std::string& name) const;

    /// \brief Name of the readout of the data: `readout`, or the only readout loaded; empty if there is
    /// none (no `readout` and several or no readouts loaded)
    std::string GetDataReadoutName() const;
    /// \brief Readout of the data (GetDataReadoutName), nullptr if there is none
    TRestDetectorReadout* GetDataReadout() const { return GetReadout(GetDataReadoutName()); }

    /// \brief Readout used by `user` (e.g. a process) with its `readout` parameter `name`: the data readout
    /// if it is empty (then `name` is set to it, to be stored), otherwise that readout, with a warning if it
    /// is not the data readout. Throws if the readout is not loaded.
    TRestDetectorReadout* SelectReadout(std::string& name, const std::string& user) const;

    /// \brief Draws the geometry of several readouts together.
    /// \param names Instance names of the readouts to draw (empty: all).
    /// \param option ROOT drawing option (e.g. "ogl").
    void ViewReadoutGeometry(const std::vector<std::string>& names = {},
                             const std::string& option = "ogl") const;
    /// \brief Draws the readouts with the channels activeChannels (DAQ IDs) highlighted.
    /// \param names Instance names of the readouts to draw (empty: all).
    void ViewActiveEvent(const std::vector<int>& activeChannels,
                         const std::vector<std::string>& names = {}) const;

   private:
    void ViewImpl(const std::vector<int>& activeChannels, const std::vector<std::string>& names,
                  const std::string& option) const;
    mutable TGeoManager* fViewGeo = nullptr;  //!
    /// File of kSelfFile while LoadFromFile imports the readouts
    TFile* fSelfFile = nullptr;  //!
};

#endif
