#ifndef TRESGEANT4QUENCHINGPROCESS_H
#define TRESGEANT4QUENCHINGPROCESS_H

#include "TRestEventProcess.h"
#include "TRestGeant4Event.h"

class TRestGeant4QuenchingInfo : public TRestMetadata {
   public:
    std::string fVolume = "";
    std::string fModel = "auto";
    TRestWithUnits fBirksConstant = 0;
    TRestWithUnits fBirksFallbackStepLength = 0;

    TRestGeant4QuenchingInfo();
    TRestGeant4QuenchingInfo(const std::string& name, const YAML::Node& node);
    void LoadConfig() override;
    void Initialize() override {}
    std::string GetClassName() const override { return "TRestGeant4QuenchingInfo"; }
};

class TRestGeant4QuenchingProcess : public TRestEventProcess {
   private:
    struct QuenchingObservables {
        double sensitiveQuenched = 0.0;
        double sensitiveVolumeEnergyBefore = 0.0;
        double sensitiveVolumeEnergyAfter = 0.0;

        void clear() {
            sensitiveQuenched = 0.0;
            sensitiveVolumeEnergyBefore = 0.0;
            sensitiveVolumeEnergyAfter = 0.0;
        }
    };

    QuenchingObservables fObs;

   public:
    std::vector<TRestGeant4QuenchingInfo> fUserVolumes;

    std::set<std::string> fVolumes;
    std::map<std::string, std::string> fVolumeModels;
    std::map<std::string, double> fVolumeBirksConstants;
    std::map<std::string, double> fVolumeBirksFallbackStepLengths;

    TRestWithUnits fBirksConstant = 0.000126;       // mm/keV, equivalent to 0.126 mm/MeV
    TRestWithUnits fBirksFallbackStepLength = 0.5;  // mm
    bool fApplyToHitEnergies = true;

    TRestGeant4Metadata* fGeant4Metadata = nullptr;

    std::set<std::string> GetVolumes() const {return fVolumes;}

    TRestGeant4QuenchingProcess();
    TRestGeant4QuenchingProcess(const std::string& instanceName, const YAML::Node& node);
    TRestGeant4QuenchingProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads analysis parameters (ranges, thresholds and options) from YAML.
    virtual void LoadConfig() override;
    /// \brief Initializes per-run state and declares produced observables.
    void InitProcess() override;
    /// \brief Analyzes one input event and fills signal/event-level observables.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Finalizes process-level counters and summary observables.
    void EndProcess() override;

    std::string GetInputEvent() const override { return "TRestGeant4Event"; }
    std::string GetOutputEvent() const override { return "TRestGeant4Event"; }

    std::string GetClassName() const override { return "TRestGeant4QuenchingProcess"; }
    // void PrintMetadata() override;
};

#endif
