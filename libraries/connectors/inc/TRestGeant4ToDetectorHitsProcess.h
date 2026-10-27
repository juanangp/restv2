#ifndef TRESTGEANT4TODETECTORHITSPROCESS_H
#define TRESTGEANT4TODETECTORHITSPROCESS_H

#include <set>
#include <string>
#include <vector>

#include "TRestEventProcess.h"

/// \class TRestGeant4ToDetectorHitsProcess
/// \brief Transforms the hits of a TRestGeant4Event into the hits of a TRestDetectorHitsEvent, only those
/// of the selected geometry volumes (the sensitive ones by default).
class TRestGeant4ToDetectorHitsProcess : public TRestEventProcess {
   private:
    /// IDs (TRestGeant4Metadata volume map) of the volumes matching fVolumes, or of the sensitive ones
    std::set<int> fVolumeIDs;

   public:
    /// Regular expressions of the physical (or, if none matches, logical) volumes whose hits are
    /// transferred. The hits of the sensitive volumes are transferred if it is empty.
    std::vector<std::string> fVolumes = {};

    TRestGeant4ToDetectorHitsProcess();
    TRestGeant4ToDetectorHitsProcess(const std::string& instanceName, const YAML::Node& node);
    TRestGeant4ToDetectorHitsProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestGeant4Event"; }
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }

    std::string GetClassName() const override { return "TRestGeant4ToDetectorHitsProcess"; }
};

#endif
