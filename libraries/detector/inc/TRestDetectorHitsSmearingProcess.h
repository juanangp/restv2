#ifndef TRESTDETECTORHITSSMEARINGPROCESS_H
#define TRESTDETECTORHITSSMEARINGPROCESS_H

#include <string>

#include "TRandom3.h"
#include "TRestEventProcess.h"

class TRestDetectorChargeResponse;

/// \class TRestDetectorHitsSmearingProcess
/// \brief Applies the energy resolution of the detector to a TRestDetectorHitsEvent: the energy of all
/// the hits is multiplied by one gaussian gain per event.
class TRestDetectorHitsSmearingProcess : public TRestEventProcess {
   private:
    const TRestDetectorChargeResponse* fCharge = nullptr;
    TRandom3 fRandom;  //!
    /// Events whose resolution is below the Fano contribution (not smeared)
    long fNBelowFano = 0;  //!

   public:
    /// Energy at which the resolution is known
    TRestWithUnits fEnergyReference = 5.9;  // keV
    /// Resolution (FWHM, in %) at fEnergyReference
    double fResolutionReference = 15;
    /// Only smear with the resolution not explained by the Fano factor of the medium (already applied by
    /// TRestDetectorHitsDriftProcess)
    bool fSubtractFano = false;
    /// Name of the TRestDetectorMedium (only with subtractFano). It can be omitted if the manager contains a
    /// single medium.
    std::string fMediumName = "";
    /// Seed of the random generator (0: random seed). Otherwise the generator is re-seeded at each event
    /// with TRestDetectorHitsEvent::EventSeed (seed, event and subevent IDs): independent of the threads.
    unsigned long fSeed = 0;

    TRestDetectorHitsSmearingProcess();
    TRestDetectorHitsSmearingProcess(const std::string& instanceName, const YAML::Node& node);
    TRestDetectorHitsSmearingProcess(const std::string& fileName, const std::string& sectionName);

    void LoadConfig() override;
    void InitProcess() override;
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    void EndProcess() override;

    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }

    std::string GetClassName() const override { return "TRestDetectorHitsSmearingProcess"; }
};

#endif
