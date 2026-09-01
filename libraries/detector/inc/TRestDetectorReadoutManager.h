#pragma once

#include <map>
#include <string>
#include <vector>

#include "TRestDetectorReadout.h"
#include "TRestMetadata.h"

/// \brief Describes one readout import request declared in configuration.
struct TRestReadoutRequest {
    std::string fInstanceName;
    std::string fGeometryName;
    std::string fDecodingName;
};

/// \class TRestDetectorReadoutManager
/// \brief Loads and owns a set of detector readouts imported from a ROOT geometry container.
class TRestDetectorReadoutManager : public TRestMetadata {
   private:
    std::map<std::string, TRestDetectorReadout*> fReadoutMap;
    std::vector<TRestReadoutRequest> fRequestedReadouts;
    std::string fInputFileName;

   public:
    /// \brief Builds an empty readout manager.
    TRestDetectorReadoutManager();
    using TRestMetadata::TRestMetadata;
    /// \brief Releases imported readouts and owned resources.
    virtual ~TRestDetectorReadoutManager();

    // MANDATORY OVERRIDES FOR TRESTMETADATA
    virtual std::string GetClassName() const override { return "TRestDetectorReadoutManager"; }
    /// \brief Parses YAML readout requests and source file information.
    virtual void LoadConfig() override;
    /// \brief Imports requested readouts into the internal lookup map.
    virtual void Initialize() override;
    /// \brief Prints a summary of configured and loaded readouts.
    virtual void PrintMetadata() const override;

    /// \brief Returns true when a readout instance with the given name exists.
    bool HasReadout(const std::string& name) const { return fReadoutMap.find(name) != fReadoutMap.end(); }
    /// \brief Retrieves a readout by instance name, or nullptr when absent.
    TRestDetectorReadout* GetReadout(const std::string& name) const;
};
