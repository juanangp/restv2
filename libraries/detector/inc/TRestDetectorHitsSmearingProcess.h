#ifndef TRESTDETECTORHITSSMEARINGPROCESS_H
#define TRESTDETECTORHITSSMEARINGPROCESS_H

#include <string>

#include "TRandom3.h"
#include "TRestEventProcess.h"

class TRestDetectorChargeResponse;

/// \class TRestDetectorHitsSmearingProcess
/// \brief Applies the energy resolution of the detector to a TRestDetectorHitsEvent: the energy of all
/// the hits is multiplied by one gaussian gain per event.
///
/// The parameters (YAML keys), the observables and an example are described in the class documentation
/// (TRestDetectorHitsSmearingProcess.cxx).
///
/// \author Javier G. Garza
/// \author Javier Galán <javier.galan@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
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

    /// \brief Default constructor (no configuration).
    TRestDetectorHitsSmearingProcess();
    /// \brief Builds the process from an in-memory YAML section (LoadConfig).
    TRestDetectorHitsSmearingProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds the process from the section sectionName of a YAML file (LoadConfig).
    TRestDetectorHitsSmearingProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the parameters from the YAML node.
    void LoadConfig() override;
    /// \brief Checks the parameters and, with subtractFano, gets the charge response of the medium.
    void InitProcess() override;
    /// \brief Multiplies the energy of all the hits by one gaussian gain (see the class description).
    /// \return false if the event has no hits.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Warns of the number of events not smeared (resolution below the Fano contribution).
    void EndProcess() override;

    /// \brief Input event type, "TRestDetectorHitsEvent".
    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    /// \brief Output event type, "TRestDetectorHitsEvent".
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }

    /// \brief Returns the class name, "TRestDetectorHitsSmearingProcess".
    std::string GetClassName() const override { return "TRestDetectorHitsSmearingProcess"; }
};

#endif
