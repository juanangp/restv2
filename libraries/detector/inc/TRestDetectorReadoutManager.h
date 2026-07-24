#pragma once

#include <map>
#include <string>
#include <vector>

#include "TRestDetectorReadout.h"
#include "TRestMetadata.h"

struct TRestReadoutRequest {
    std::string fInstanceName;
    std::string fGeometryName;
    std::string fDecodingName;
};

class TRestDetectorReadoutManager : public TRestMetadata {
   private:
    std::map<std::string, TRestDetectorReadout*> fReadoutMap;
    std::vector<TRestReadoutRequest> fRequestedReadouts;
    std::string fInputFileName;

   public:
    TRestDetectorReadoutManager();
    using TRestMetadata::TRestMetadata;
    virtual ~TRestDetectorReadoutManager();

    // MANDATORY OVERRIDES FOR TRESTMETADATA
    virtual std::string GetClassName() const override { return "TRestDetectorReadoutManager"; }
    virtual void LoadConfig() override;
    virtual void Initialize() override;
    virtual void PrintMetadata() override;

    bool HasReadout(const std::string& name) const { return fReadoutMap.find(name) != fReadoutMap.end(); }
    TRestDetectorReadout* GetReadout(const std::string& name) const;
};
