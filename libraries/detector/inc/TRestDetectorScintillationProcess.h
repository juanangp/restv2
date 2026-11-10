#ifndef TRESTDETECTORSCINTILLATIONPROCESS_H
#define TRESTDETECTORSCINTILLATIONPROCESS_H

#include <string>

#include "TRandom3.h"
#include "TRestEventProcess.h"

class TRestDetectorLightResponse;

/// \class TRestDetectorScintillationProcess
/// \brief Primary scintillation: converts the energy of each hit into the number of scintillation photons
/// produced (Poisson, light yield of the medium), at the same position and time. The collection of the
/// light and its detection (photoelectrons) are TRestDetectorLightCollectionProcess.
///
/// The parameters (YAML keys), the observables and an example are described in the class documentation
/// (TRestDetectorScintillationProcess.cxx).
///
/// \author JuanAn García <juanangp@unizar.es>
class TRestDetectorScintillationProcess : public TRestEventProcess {
   private:
    const TRestDetectorLightResponse* fLight = nullptr;
    TRandom3 fRandom;  //!

    /// Observables
    double fNPhotons = 0;  //!

   public:
    /// Name of the TRestDetectorMedium with the light response (lightYield). It can be omitted if the
    /// manager contains a single medium.
    std::string fMediumName = "";

    /// Seed of the random generator (0: random seed). Otherwise the generator is re-seeded at each event
    /// with TRestDetectorHitsEvent::EventSeed (seed, event and subevent IDs): independent of the threads.
    unsigned long fSeed = 0;

    /// \brief Default constructor (no configuration).
    TRestDetectorScintillationProcess();
    /// \brief Builds the process from an in-memory YAML section (LoadConfig).
    TRestDetectorScintillationProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds the process from the section sectionName of a YAML file (LoadConfig).
    TRestDetectorScintillationProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the parameters from the YAML node.
    void LoadConfig() override;
    /// \brief Gets the light response of the medium (lightYield > 0 needed) and registers the observable
    /// NPhotons.
    void InitProcess() override;
    /// \brief Converts the energy of each hit into scintillation photons.
    /// \return false if the event has no photons.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Nothing to do at the end of the processing.
    void EndProcess() override {}

    /// \brief Input event type, "TRestDetectorHitsEvent".
    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    /// \brief Output event type, "TRestDetectorHitsEvent".
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }

    /// \brief Returns the class name, "TRestDetectorScintillationProcess".
    std::string GetClassName() const override { return "TRestDetectorScintillationProcess"; }
};

#endif
