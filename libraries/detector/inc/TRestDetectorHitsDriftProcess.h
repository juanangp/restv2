#ifndef TRESTDETECTORHITSDRIFTPROCESS_H
#define TRESTDETECTORHITSDRIFTPROCESS_H

#include <string>
#include <vector>

#include "TRandom3.h"
#include "TRestEventProcess.h"

class TRestDetectorChargeResponse;
class TRestDetectorReadout;

/// \class TRestDetectorHitsDriftProcess
/// \brief Converts the energy deposits of a TRestDetectorHitsEvent into ionization electrons and drifts
/// them to the readout through a TRestDetectorMedium at its drift field (replaces the legacy
/// TRestDetectorElectronDiffusionProcess and TRestDetectorGarfieldDriftProcess).
///
/// The parameters (YAML keys), the observables and an example are described in the class documentation
/// (TRestDetectorHitsDriftProcess.cxx).
class TRestDetectorHitsDriftProcess : public TRestEventProcess {
   private:
    const TRestDetectorReadout* fReadout = nullptr;
    const TRestDetectorChargeResponse* fCharge = nullptr;
    TRandom3 fRandom;  //!

   public:
    /// Values accepted by fFluctuations
    inline static const std::vector<std::string> kFluctuations = {"fano", "poisson", "none"};

    /// Name of the readout (in TRestDetectorReadoutManager) that gives the drift distance. It can be
    /// omitted if the manager contains a single readout.
    std::string fReadoutName = "";

    /// Name of the TRestDetectorMedium. It can be omitted if the manager contains a single medium.
    std::string fMediumName = "";

    /// Fluctuation of the number of electrons: fano (gaussian with the medium Fano factor), poisson or none.
    std::string fFluctuations = "fano";

    /// Maximum number of electrons per event (0: no limit). Above it, each electron carries the energy of
    /// several (as legacy, an effective W-value), to limit the processing time.
    int fMaxHits = 1000;

    /// Seed of the random generator (0: random seed). Otherwise the generator is re-seeded at each event
    /// with TRestDetectorHitsEvent::EventSeed (seed, event and subevent IDs): independent of the threads.
    unsigned long fSeed = 0;

    /// Written with the transport used, from the medium at its drift field (not inputs): longitudinal and
    /// transversal diffusion (sqrt(cm)) and attachment (1/cm)
    double fLongitudinalDiffusion = 0;
    /// Transversal diffusion used (sqrt(cm)), written by InitProcess (not an input)
    double fTransversalDiffusion = 0;
    /// Attachment used (1/cm), written by InitProcess (not an input)
    double fAttachment = 0;

    /// \brief Default constructor (no configuration).
    TRestDetectorHitsDriftProcess();
    /// \brief Builds the process from an in-memory YAML section (LoadConfig).
    TRestDetectorHitsDriftProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds the process from the section sectionName of a YAML file (LoadConfig).
    TRestDetectorHitsDriftProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the parameters from the YAML node.
    void LoadConfig() override;
    /// \brief Gets the readout and the charge response of the medium (wValue > 0 needed, and fanoFactor > 0
    /// with fano fluctuations) and writes the transport used to the metadata. Throws if one of them is
    /// missing.
    void InitProcess() override;
    /// \brief Converts the hits into drifted electrons (see the class description).
    /// \return false if no electron survives.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Nothing to do at the end of the processing.
    void EndProcess() override {}

    /// \brief Input event type, "TRestDetectorHitsEvent".
    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    /// \brief Output event type, "TRestDetectorHitsEvent".
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }

    /// \brief Returns the class name, "TRestDetectorHitsDriftProcess".
    std::string GetClassName() const override { return "TRestDetectorHitsDriftProcess"; }
};

#endif
