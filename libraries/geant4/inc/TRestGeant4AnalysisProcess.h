#ifndef TRESTGEANT4ANALYSISPROCESS_H
#define TRESTGEANT4ANALYSISPROCESS_H

#include <set>
#include <string>
#include <vector>

#include "TRestEventProcess.h"

class TRestGeant4Event;
class TRestGeant4GeometryInfo;

/// \class TRestGeant4SelectionInfo
/// \brief Selection of the energy deposits of a TRestGeant4Event: volumes (regular expressions), particles
/// and processes (Geant4 names). An empty list selects any.
///
/// An entry of the `selections` list of TRestGeant4AnalysisProcess (keys `name`, `volumes`, `particles`,
/// `processes`, `splitByParticle`, `entering`, `perVolume`, `interactions` and `trackEdepVolumes`); the
/// observables it gives are described in TRestGeant4AnalysisProcess.
class TRestGeant4SelectionInfo : public TRestMetadata {
   public:
    std::string fSelectionName = "";  ///< `name`: prefix of the observables, <name>_*
    /// `volumes`: regular expressions of physical (or logical) volume names; empty: the sensitive volumes
    std::vector<std::string> fVolumes = {};
    std::vector<std::string> fParticles = {};  ///< `particles`: Geant4 particle names; empty: any
    std::vector<std::string> fProcesses = {};  ///< `processes`: Geant4 process names; empty: any
    /// Particles with their own energy observable, <name>_<particle>_Energy
    std::vector<std::string> fSplitByParticle = {};
    /// If true, the <name>_Entry* observables of the particle entering the volumes are added
    bool fEntering = false;
    /// If true, the energy of each volume of the selection, <name>_<volume>_Energy, and the maximum,
    /// <name>_MaxVolumeEnergy, are added
    bool fPerVolume = false;
    /// If true, the selected hits with or without energy (interaction points, e.g. nCapture of neutrons)
    /// give <name>_NInteractions and the vectors <name>_InteractionX/Y/Z, _InteractionTime,
    /// _InteractionKineticEnergy, _InteractionSecondariesEnergy, _InteractionTrackEdep and
    /// _InteractionTrackMaxVolumeEdep
    bool fInteractions = false;
    /// Volumes (regular expressions) of _InteractionTrackEdep and _InteractionTrackMaxVolumeEdep, the energy
    /// deposited in them by the track of each interaction and all its descendants; empty: all the volumes
    std::vector<std::string> fTrackEdepVolumes = {};

    /// \brief Empty selection (all its lists empty).
    TRestGeant4SelectionInfo();
    /// \brief Selection configured from an entry of `selections`.
    TRestGeant4SelectionInfo(const std::string& name, const YAML::Node& node);
    /// \brief Reads the registered fields from the YAML node and writes them back.
    void LoadConfig() override;
    /// \brief No-op.
    void Initialize() override {}
    /// \brief Returns "TRestGeant4SelectionInfo".
    std::string GetClassName() const override { return "TRestGeant4SelectionInfo"; }
};

/// \class TRestGeant4AnalysisProcess
/// \brief Observables of a TRestGeant4Event, one value per event but the interactions: global and primary
/// ones and, for each
/// selection, energy (also per particle), mean position, first hit time, tracks and the particle entering
/// its volumes.
///
/// The input event is passed unchanged to the output. Configuration keys: `selections` (a list of
/// TRestGeant4SelectionInfo) and the common process keys (e.g. `observables`). The registered observables
/// (totalEdep, sensitiveVolumeEnergy, nTracks, boundingSize, energyPrimary, x/y/zOriginPrimary,
/// x/y/zDirectionPrimary, thetaPrimary, phiPrimary, eventPrimaryParticleName, subEventPrimaryParticleName
/// and the <name>_* ones of each selection), the YAML example and the differences with legacy are
/// documented in TRestGeant4AnalysisProcess.cxx (detailed description below).
///
/// \author Javier Galán <javier.galan@unizar.es>
/// \author Gloria Luzón
/// \author Álvaro Ezquerro <aezquerro@unizar.es>
/// \author JuanAn García <juanangp@unizar.es>
class TRestGeant4AnalysisProcess : public TRestEventProcess {
   private:
    struct GlobalObservables {
        double totalEdep = 0;
        double sensitiveVolumeEnergy = 0;
        int nTracks = 0;
        double boundingSize = 0;
        double energyPrimary = 0;
        double xOriginPrimary = 0, yOriginPrimary = 0, zOriginPrimary = 0;
        double xDirectionPrimary = 0, yDirectionPrimary = 0, zDirectionPrimary = 0;
        double thetaPrimary = 0, phiPrimary = 0;
        std::string eventPrimaryParticleName;
        std::string subEventPrimaryParticleName;
    };

    /// The particle entering the volumes of a selection: the ancestor tracks (or themselves) of the tracks
    /// with energy in the selection which start outside the volumes; values of the one with the lowest ID
    struct EnteringObservables {
        int nEntering = 0;
        std::string particle, parentParticle, creatorProcess;
        double entryEnergy = 0, entryTime = 0;
        double entryX = 0, entryY = 0, entryZ = 0;
        double endX = 0, endY = 0, endZ = 0;
        double initialEnergy = 0;
        double initialX = 0, initialY = 0, initialZ = 0;

        void clear();
    };

    /// A selection resolved to IDs, and its observables
    struct Selection {
        std::string name;
        std::set<int> volumeIDs;  ///< The sensitive volumes if none is given
        std::set<std::string> particles;
        std::set<int> processIDs;
        std::vector<std::string> splitParticles;
        bool entering = false;
        bool interactions = false;
        std::set<int> trackEdepVolumeIDs;  ///< Empty: all the volumes
        /// With perVolume: the volume IDs (in increasing order) and the energy of each one
        std::vector<int> perVolumeIDs;
        std::vector<double> volumeEnergy;
        double maxVolumeEnergy = 0;

        double energy = 0;
        double meanPosX = 0, meanPosY = 0, meanPosZ = 0;
        double firstHitTime = 0;
        int nTracks = 0;
        std::vector<double> splitEnergy;   ///< One per splitParticles
        std::vector<size_t> trackIndices;  ///< Tracks with energy in the selection, for the entering one
        EnteringObservables enteringObs;

        /// With interactions: one entry per selected hit
        int nInteractions = 0;
        std::vector<double> interactionX, interactionY, interactionZ, interactionTime;
        std::vector<double> interactionKineticEnergy, interactionSecondariesEnergy;
        std::vector<double> interactionTrackEdep, interactionTrackMaxVolumeEdep;
    };

    GlobalObservables fObs;
    std::vector<Selection> fSelectionObs;
    const TRestGeant4GeometryInfo* fGeometryInfo = nullptr;

    void ProcessSelections(const TRestGeant4Event& event);
    void ProcessEntering(const TRestGeant4Event& event, Selection& selection);
    void ProcessInteractions(const TRestGeant4Event& event);

   public:
    /// Selections, each one gives <name>_Energy, _MeanPosX/Y/Z, _FirstHitTime, _NTracks,
    /// <name>_<particle>_Energy for each splitByParticle and, with entering, <name>_NEntering,
    /// _EntryParticle, _EntryParentParticle, _EntryCreatorProcess, _EntryEnergy, _EntryTime, _EntryX/Y/Z,
    /// _EndX/Y/Z, _InitialEnergy and _InitialX/Y/Z, with perVolume <name>_<volume>_Energy and
    /// _MaxVolumeEnergy, with interactions <name>_NInteractions and _Interaction*. If empty, one per
    /// sensitive entry of activeVolumes, named as it and with entering
    std::vector<TRestGeant4SelectionInfo> fSelections;

    /// \brief Process without configuration.
    TRestGeant4AnalysisProcess();
    /// \brief Process configured from a YAML node (the pipeline section).
    TRestGeant4AnalysisProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Process configured from the section `sectionName` of a YAML file.
    TRestGeant4AnalysisProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the common process keys and `selections`.
    void LoadConfig() override;
    /// \brief Resolves the selections with the TRestGeant4Metadata of the run (volume, particle and process
    /// IDs; the default ones from the sensitive `activeVolumes`) and registers the observables. Throws for
    /// a volume list without match, an unknown particle or process, or a missing metadata.
    void InitProcess() override;
    /// \brief Copies the input event to the output and computes the observables of the event.
    /// \return Always true (the event is not rejected).
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief No-op.
    void EndProcess() override {}

    /// \brief Returns "TRestGeant4Event".
    std::string GetInputEvent() const override { return "TRestGeant4Event"; }
    /// \brief Returns "TRestGeant4Event" (the input, unchanged).
    std::string GetOutputEvent() const override { return "TRestGeant4Event"; }

    /// \brief Returns "TRestGeant4AnalysisProcess".
    std::string GetClassName() const override { return "TRestGeant4AnalysisProcess"; }
};

#endif
