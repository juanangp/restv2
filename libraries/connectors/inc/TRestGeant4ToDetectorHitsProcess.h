#ifndef TRESTGEANT4TODETECTORHITSPROCESS_H
#define TRESTGEANT4TODETECTORHITSPROCESS_H

#include <set>
#include <string>
#include <vector>

#include "TRestEventProcess.h"

/// \class TRestGeant4ToDetectorHitsProcess
/// \brief Transforms the hits of a TRestGeant4Event into the hits of a TRestDetectorHitsEvent, only those
/// of the selected geometry volumes (the sensitive ones by default).
///
/// \author Igor G. Irastorza
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
class TRestGeant4ToDetectorHitsProcess : public TRestEventProcess {
   private:
    /// IDs (TRestGeant4Metadata volume map) of the volumes matching fVolumes, or of the sensitive ones
    std::set<int> fVolumeIDs;

   public:
    /// Regular expressions of the physical (or, if none matches, logical) volumes whose hits are
    /// transferred. The hits of the sensitive volumes are transferred if it is empty.
    std::vector<std::string> fVolumes = {};

    /// \brief Default constructor, with the default parameters.
    TRestGeant4ToDetectorHitsProcess();
    /// \brief Constructor from the YAML section `node` of the instance `instanceName` (calls LoadConfig).
    TRestGeant4ToDetectorHitsProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Constructor from the section `sectionName` of the YAML file `fileName` (calls LoadConfig).
    TRestGeant4ToDetectorHitsProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the registered fields from the YAML section.
    void LoadConfig() override;
    /// \brief Finds the IDs of the selected volumes in the TRestGeant4Metadata of the run (throws if there is
    /// none or no volume matches).
    void InitProcess() override;
    /// \brief Transfers the hits with positive energy of the selected volumes as XYZ hits.
    /// \return False if no hit is transferred.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestGeant4Event"; }
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }

    std::string GetClassName() const override { return "TRestGeant4ToDetectorHitsProcess"; }
};

#endif
