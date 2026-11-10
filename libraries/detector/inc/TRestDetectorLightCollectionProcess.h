#ifndef TRESTDETECTORLIGHTCOLLECTIONPROCESS_H
#define TRESTDETECTORLIGHTCOLLECTIONPROCESS_H

#include <functional>
#include <string>

#include "Math/Vector3D.h"
#include "TRandom3.h"
#include "TRestEventProcess.h"

class TRestDetectorHitsEvent;
class TRestDetectorLightResponse;
class TRestDetectorReadout;

/// \class TRestDetectorLightCollectionProcess
/// \brief Collection of the scintillation photons of the hits (TRestDetectorScintillationProcess) by the
/// readout channels and their detection (photoelectrons, quantum efficiency of the photosensors), with a
/// model for each kind of scintillator: `localCapture` (opaque scintillator, the channels near the photons
/// capture them with a probability that depends on the distance, e.g. the fibres of LiquidO) or
/// `attenuation` (transparent scintillator, the photons reach the readout of their channel attenuated and
/// delayed with the distance, e.g. a veto with its PMT).
///
/// The parameters (YAML keys), the observables and an example are described in the class documentation
/// (TRestDetectorLightCollectionProcess.cxx).
///
/// \author Luis Obis <luis.antonio.obis@gmail.com>
/// \author JuanAn García <juanangp@unizar.es>
class TRestDetectorLightCollectionProcess : public TRestEventProcess {
   private:
    const TRestDetectorReadout* fReadout = nullptr;
    const TRestDetectorLightResponse* fLight = nullptr;
    /// Capture probability versus the distance (mm), built from captureProbability (localCapture)
    std::function<double(double)> fCapture;  //!
    TRandom3 fRandom;                        //!

    /// Observables
    double fNPhotoelectrons = 0;   //!
    int fNHitsWithoutChannel = 0;  //!

    /// \brief Photoelectrons of the photons reaching a photosensor (Binomial(photons, quantumEfficiency)).
    int Detect(int photons);

    /// \brief The photoelectrons of the photons of a hit captured by the channels in range (localCapture).
    void CaptureLocally(const ROOT::Math::XYZVector& position, int photons, double time,
                        TRestDetectorHitsEvent& output);
    /// \brief The photoelectrons of the photons of a hit reaching the readout of its channel (attenuation).
    void Attenuate(const ROOT::Math::XYZVector& position, int photons, double time,
                   TRestDetectorHitsEvent& output);

   public:
    /// `model`: "localCapture" or "attenuation"
    std::string fModel = "localCapture";

    /// Name of the readout (in TRestDetectorReadoutManager) that collects the light. It can be omitted if
    /// the manager contains a single readout.
    std::string fReadoutName = "";

    /// Name of the TRestDetectorMedium with the light response (attenuation: attenuationLength,
    /// effectiveLightSpeed). It can be omitted if the manager contains a single medium.
    std::string fMediumName = "";

    /// `captureProbability` (localCapture): probability that a photon produced at a distance r (mm) from a
    /// channel is captured by it and reaches its photosensor, a ROOT formula of x = r (e.g.
    /// "0.03*exp(-x*x/200)") or a graph or histogram of a ROOT file, "file.root:name" (e.g. from an optical
    /// simulation of Geant4)
    std::string fCaptureProbability = "";

    /// `maxDistance` (localCapture): channels farther than this collect no light
    TRestWithUnits fMaxDistance = 0;

    /// `quantumEfficiency`: probability that a photon reaching the photosensor (SiPM, PMT) of a channel gives
    /// a photoelectron
    double fQuantumEfficiency = 1;

    /// Seed of the random generator (0: random seed). Otherwise the generator is re-seeded at each event
    /// with TRestDetectorHitsEvent::EventSeed (seed, event and subevent IDs): independent of the threads.
    unsigned long fSeed = 0;

    /// \brief Default constructor (no configuration).
    TRestDetectorLightCollectionProcess();
    /// \brief Builds the process from an in-memory YAML section (LoadConfig).
    TRestDetectorLightCollectionProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Builds the process from the section sectionName of a YAML file (LoadConfig).
    TRestDetectorLightCollectionProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the parameters; throws if the model is unknown, quantumEfficiency is not in [0, 1], or
    /// for localCapture if maxDistance is not positive or captureProbability is not a valid formula or graph.
    void LoadConfig() override;
    /// \brief Gets the readout (and for attenuation the light response of the medium) and registers the
    /// observables NPhotoelectrons and NHitsWithoutChannel.
    void InitProcess() override;
    /// \brief Distributes the photons of each hit among the channels collecting them and converts them into
    /// photoelectrons.
    /// \return false if the event has no photoelectrons.
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief Nothing to do at the end of the processing.
    void EndProcess() override {}

    /// \brief Capture probability at a distance (mm) from a channel (localCapture; 0 beyond maxDistance).
    double GetCaptureProbability(double distance) const;

    /// \brief Input event type, "TRestDetectorHitsEvent".
    std::string GetInputEvent() const override { return "TRestDetectorHitsEvent"; }
    /// \brief Output event type, "TRestDetectorHitsEvent".
    std::string GetOutputEvent() const override { return "TRestDetectorHitsEvent"; }

    /// \brief Returns the class name, "TRestDetectorLightCollectionProcess".
    std::string GetClassName() const override { return "TRestDetectorLightCollectionProcess"; }
};

#endif
