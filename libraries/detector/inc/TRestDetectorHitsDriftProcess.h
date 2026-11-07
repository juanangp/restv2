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
    double fTransversalDiffusion = 0;
    double fAttachment = 0;

    TRestDetectorHitsDriftProcess();
    TRestDetectorHitsDriftProcess(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorHitsDriftProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override {}

    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }

    std::string GetClassName() const override { return "TRestDetectorHitsDriftProcess"; }
};

#endif
