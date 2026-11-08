#ifndef TRESGEANT4QUENCHINGPROCESS_H
#define TRESGEANT4QUENCHINGPROCESS_H

#include <set>
#include <string>

#include "TRestEventProcess.h"
class TRestGeant4Event;
class TRestGeant4Metadata;

/// \class TRestGeant4QuenchingInfo
/// \brief An entry of the `volume` list of TRestGeant4QuenchingProcess: the volumes (`name`, a regular
/// expression) and their quenching `model`, with the Birks parameters (0: the ones of the process).
class TRestGeant4QuenchingInfo : public TRestMetadata {
   public:
    /// `name`: regular expression of physical volume names (or, if none matches, of logical ones)
    std::string fVolume = "";
    /// `model`: lindhard or birks, required
    std::string fModel = "";
    TRestWithUnits fBirksConstant = 0;            ///< `birksConstant` (mm/keV, 0: the process one)
    TRestWithUnits fBirksFallbackStepLength = 0;  ///< `birksFallbackStepLength` (mm, 0: the process one)

    /// \brief Entry with the default values.
    TRestGeant4QuenchingInfo();
    /// \brief Entry configured from an element of `volume`.
    TRestGeant4QuenchingInfo(const std::string& name, const YAML::Node& node);
    /// \brief Reads the fields, checks `model` and writes them back.
    void LoadConfig() override;
    /// \brief No-op.
    void Initialize() override {}
    /// \brief Returns "TRestGeant4QuenchingInfo".
    std::string GetClassName() const override { return "TRestGeant4QuenchingInfo"; }
};

/// \class TRestGeant4QuenchingProcess
/// \brief Quenching of the energy deposits of a TRestGeant4Event: the energy of the hits in the selected
/// volumes is replaced by the visible one, and the deposited energies of the event (total, per volume,
/// per volume, particle and process, per track and in the sensitive volumes) are recomputed from the
/// hits.
///
/// Models, per volume:
/// - `lindhard`: the hit energy is multiplied by the Lindhard factor of the hadronic target nucleus of the
///   hit (A, Z), for the hits with a target (A > 0). It needs the simulation with
///   storeHadronicTargetInfo: true; otherwise InitProcess throws (a configuration error).
/// - `birks`: Birks law, factor 1 / (1 + kB dE/dx) with dE/dx the hit energy over the step length (the
///   distance to the previous hit of the track in the same volume, birksFallbackStepLength for the first
///   one). Not applied to gamma, opticalphoton, geantino, chargedgeantino and neutrinos.
///
/// Only the volumes given are quenched, each one with its model (required): those of `volume` and, with
/// `model`, all the other volumes of the geometry; the rest keep their energy.
///
///     quenching:
///       class: "TRestGeant4QuenchingProcess"
///       birksConstant: 0.000126            # mm/keV, default of the volumes
///       birksFallbackStepLength: 0.5 mm    # default of the volumes
///       applyToHitEnergies: true           # false: the hits keep their energy
///       model: "lindhard"                  # optional: the other volumes of the geometry
///       volume:
///         - name: "gasVolume"
///           model: "lindhard"
///         - name: "^scintillatorVolume"
///           model: "birks"
///           birksConstant: 0.0001      # mm/keV
///       observables: ["all"]
///
/// Observables: sensitiveVolumeEnergyBefore and sensitiveVolumeEnergyAfter (keV, energy in the sensitive
/// volumes before and after the quenching) and sensitiveQuenched (1 if they differ by more than 0.01 keV,
/// 0 otherwise). It throws at initialization if the run has no TRestGeant4Metadata or if a volume uses the
/// lindhard model and the simulation has no storeHadronicTargetInfo; a volume entry without model is a
/// configuration error; an expression without matching volume is reported as an error and skipped.
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
    std::vector<TRestGeant4QuenchingInfo> fUserVolumes;  ///< `volume`: the volume entries

    std::set<std::string> fVolumes;  ///< The volume names resolved from the expressions (InitProcess)
    std::map<std::string, std::string> fVolumeModels;     ///< Model of each resolved volume
    std::map<std::string, double> fVolumeBirksConstants;  ///< Birks constant of each resolved volume
    std::map<std::string, double> fVolumeBirksFallbackStepLengths;  ///< Fallback step of each volume

    /// `birksConstant`: default Birks constant of the volumes
    TRestWithUnits fBirksConstant = 0.000126;  // mm/keV, equivalent to 0.126 mm/MeV
    /// mm, the Birks step length of a hit is its distance to the previous hit of the track; this one if
    /// there is none in the same volume (first hit of the track)
    TRestWithUnits fBirksFallbackStepLength = 0.5;
    /// `model`: lindhard or birks for all the volumes of the geometry not given in `volume` (with the
    /// Birks parameters of the process); empty: only the volumes of `volume` are quenched
    std::string fModel = "";
    /// `applyToHitEnergies`: if true the hit energies are replaced by the visible ones (the event energies
    /// are recomputed in any case)
    bool fApplyToHitEnergies = true;

    TRestGeant4Metadata* fGeant4Metadata = nullptr;  ///< The simulation metadata of the run (InitProcess)

    /// \brief The volume names resolved from the expressions
    std::set<std::string> GetVolumes() const { return fVolumes; }

    /// \brief Process without configuration.
    TRestGeant4QuenchingProcess();
    /// \brief Process configured from a YAML node (the pipeline section).
    TRestGeant4QuenchingProcess(const std::string& instanceName, const YAML::Node& node);
    /// \brief Process configured from the section `sectionName` of a YAML file.
    TRestGeant4QuenchingProcess(const std::string& fileName, const std::string& sectionName);

    /// \brief Reads the common process keys, `volume`, `birksConstant`, `birksFallbackStepLength` and
    /// `applyToHitEnergies`; the volume entries without Birks parameters take the process ones.
    virtual void LoadConfig() override;
    /// \brief Gets the TRestGeant4Metadata of the run, registers the observables and resolves the volume
    /// expressions with its geometry (model and Birks parameters of each volume). Throws without metadata,
    /// or if a volume uses lindhard and the simulation has no storeHadronicTargetInfo.
    void InitProcess() override;
    /// \brief Copies the input event to the output, quenches the energy of its hits in the selected
    /// volumes and recomputes the event energies.
    /// \return Always true (the event is not rejected).
    bool ProcessEvent(const TRestEvent& input, TRestEvent& output) override;
    /// \brief No-op.
    void EndProcess() override;

    /// \brief Returns "TRestGeant4Event".
    std::string GetInputEvent() const override { return "TRestGeant4Event"; }
    /// \brief Returns "TRestGeant4Event" (the input with the quenched energies).
    std::string GetOutputEvent() const override { return "TRestGeant4Event"; }

    /// \brief Returns "TRestGeant4QuenchingProcess".
    std::string GetClassName() const override { return "TRestGeant4QuenchingProcess"; }
    // void PrintMetadata() override;
};

#endif
