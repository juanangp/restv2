#include "TRestGeant4QuenchingProcess.h"


#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

using namespace std;

static const bool TRestGeant4QuenchingInfo_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestGeant4QuenchingInfo>("name", &TRestGeant4QuenchingInfo::fVolume);
    reg.RegisterField<TRestGeant4QuenchingInfo>("model", &TRestGeant4QuenchingInfo::fModel);
    reg.RegisterField<TRestGeant4QuenchingInfo>("birksConstant",
                                                     &TRestGeant4QuenchingInfo::fBirksConstant);
    reg.RegisterField<TRestGeant4QuenchingInfo>("birksFallbackStepLength",
                                                     &TRestGeant4QuenchingInfo::fBirksFallbackStepLength);
    return true;
}();


TRestGeant4QuenchingInfo::TRestGeant4QuenchingInfo() : TRestMetadata() {
    fName = "TRestGeant4QuenchingInfo";
}

TRestGeant4QuenchingInfo::TRestGeant4QuenchingInfo(
    const std::string& name, const YAML::Node& node) : TRestMetadata(name, node) {
    LoadConfig();
}

void TRestGeant4QuenchingInfo::LoadConfig() {
    UpdateParamsFromYAML<TRestGeant4QuenchingInfo>(fNode);
    UpdateYAMLFromParams<TRestGeant4QuenchingInfo>(fNode);
}

static const bool TRestGeant4QuenchingProcess_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();
    reg.RegisterField<TRestGeant4QuenchingProcess>("applyToHitEnergies",
                                                     &TRestGeant4QuenchingProcess::fApplyToHitEnergies);
    reg.RegisterField<TRestGeant4QuenchingProcess>("volume",
                                                     &TRestGeant4QuenchingProcess::fUserVolumes);
    reg.RegisterField<TRestGeant4QuenchingProcess>("birksConstant",
                                                     &TRestGeant4QuenchingProcess::fBirksConstant);
    reg.RegisterField<TRestGeant4QuenchingProcess>("birksFallbackStepLength",
                                                     &TRestGeant4QuenchingProcess::fBirksFallbackStepLength);

    return true;
}();

namespace {
// Registration in MetadataClassRegistry
const bool kRegistered = []() {
    MetadataClassRegistry::Instance().Register(
        "TRestGeant4QuenchingProcess", [](const std::string& instanceName, const YAML::Node& params) {
            return std::make_unique<TRestGeant4QuenchingProcess>(instanceName, params);
        });
    return true;
}();


}  // namespace

namespace {
string ToLower(string value) {
    transform(value.begin(), value.end(), value.begin(),
              [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return value;
}

/*string NormalizeQuenchingModel(const string& model) {
    const auto normalized = ToLower(model);
    if (normalized == "auto" || normalized == "lindhard" || normalized == "birks") {
        return normalized;
    }

    RESTWarning << "TRestGeant4QuenchingProcess: Unknown quenching model '" << model
                << "'. Falling back to auto." << RESTendl;
    return "auto";
}*/

string InferQuenchingModel(const string& requestedModel, const string& userVolumeExpression,
                           const string& volumeName) {
    if (requestedModel != "auto") {
        return requestedModel;
    }

    const auto haystack = ToLower(userVolumeExpression + " " + volumeName);
    if (haystack.find("scintillator") != string::npos || haystack.find("veto") != string::npos) {
        return "birks";
    }
    return "lindhard";
}

double Clamp01(double value) {
    if (value < 0) {
        return 0;
    }
    if (value > 1) {
        return 1;
    }
    return value;
}

double LindhardQuenchingFactor(double recoilEnergy, int A, int Z) {
    if (recoilEnergy <= 0 || A <= 0 || Z <= 0) {
        return 1.0;
    }

    const double gamma = 11.5 * recoilEnergy * TMath::Power(Z, -7.0 / 3.0);
    const double g = 3 * TMath::Power(gamma, 0.15) + 0.7 * TMath::Power(gamma, 0.6) + gamma;
    const double k = 0.133 * TMath::Power(Z, 2.0 / 3.0) * TMath::Power(A, -1.0 / 2.0);
    return Clamp01(k * g / (1 + k * g));
}

bool IsNeutralParticleWithoutBirksQuenching(const string& particleName) {
    const auto name = ToLower(particleName);
    return name == "gamma" || name == "opticalphoton" || name == "geantino" || name == "chargedgeantino" ||
           name.find("neutrino") != string::npos;
}

double EstimateStepLength(
    const TRestGeant4Track& track,
    int hitIndex,
    double fallbackStepLength) {
    const auto nHits = static_cast<int>(track.GetNumberOfHits());
    if (nHits <= 1) {
        return fallbackStepLength;
    }

    const auto volumeId =
        track.GetHitVolumeID(hitIndex);

    const auto position =
        track.GetHitPosition(hitIndex);

    double stepLength =
        numeric_limits<double>::max();

    if (hitIndex > 0 &&
        track.GetHitVolumeID(hitIndex - 1) == volumeId) {

        stepLength = min(
            stepLength,
            (position - track.GetHitPosition(hitIndex - 1)).R()
        );
    }

    if (hitIndex + 1 < nHits &&
        track.GetHitVolumeID(hitIndex + 1) == volumeId) {

        stepLength = min(
            stepLength,
            (track.GetHitPosition(hitIndex + 1) - position).R()
        );
    }

    if (!std::isfinite(stepLength) ||
        stepLength <= 0 ||
        stepLength == numeric_limits<double>::max()) {

        return fallbackStepLength;
    }

    return stepLength;
}

double BirksQuenchingFactor(double energy, double stepLength, double birksConstant) {
    if (energy <= 0 || stepLength <= 0 || birksConstant <= 0) {
        return 1.0;
    }

    const double dEdx = energy / stepLength;
    return Clamp01(1.0 / (1.0 + birksConstant * dEdx));
}

}

/// \brief Default constructor for raw signal analysis processing.
TRestGeant4QuenchingProcess::TRestGeant4QuenchingProcess() : TRestEventProcess() {
    fName = "TRestGeant4QuenchingProcess";
}

/// \brief Constructor from an in-memory YAML node.
TRestGeant4QuenchingProcess::TRestGeant4QuenchingProcess(const std::string& instanceName,
                                                             const YAML::Node& node)
    : TRestEventProcess(instanceName, node) {
    LoadConfig();
}

/// \brief Constructor from a file and YAML section name.
TRestGeant4QuenchingProcess::TRestGeant4QuenchingProcess(const std::string& fileName,
                                                             const std::string& sectionName)
    : TRestEventProcess(fileName, sectionName) {
    LoadConfig();
}

/// \brief Loads process parameters and synchronizes resolved values back to YAML.
void TRestGeant4QuenchingProcess::LoadConfig() {
    TRestEventProcess::LoadConfig();

    if (!fNode || fNode.IsNull()) {
        RESTError << "TRestGeant4QuenchingProcess::LoadConfig YAML node is missing" << RESTendl;
        return;
    }

    UpdateParamsFromYAML<TRestGeant4QuenchingProcess>(fNode);

    for (auto& userVolume : fUserVolumes) {
        if(userVolume.fBirksConstant == 0)userVolume.fBirksConstant = fBirksConstant;
        if(userVolume.fBirksFallbackStepLength == 0)userVolume.fBirksFallbackStepLength = fBirksFallbackStepLength;
        //userVolume.UpdateYAML();
    }
    
    // Sync resolved parameters to the node
    UpdateYAMLFromParams<TRestGeant4QuenchingProcess>(fNode);
}

/// \brief Registers all event observables produced by this process.
void TRestGeant4QuenchingProcess::InitProcess() {
    if(fRunInfo)fGeant4Metadata = (TRestGeant4Metadata*)fRunInfo->GetMetadataClass("TRestGeant4Metadata");
    const auto geometryInfo = fGeant4Metadata->GetGeant4GeometryInfo();
    
    fVolumes.clear();
    fVolumeModels.clear();
    fVolumeBirksConstants.clear();
    fVolumeBirksFallbackStepLengths.clear();

    RegisterObservable("sensitiveQuenched", fObs.sensitiveQuenched);
    RegisterObservable("sensitiveVolumeEnergyBefore", fObs.sensitiveVolumeEnergyBefore);
    RegisterObservable("sensitiveVolumeEnergyAfter", fObs.sensitiveVolumeEnergyAfter);

    // check all the user volume expressions are valid and correspond to at least a volume
    for (auto& userVolume : fUserVolumes) {
        set<string> physicalVolumes = {};
        //const auto allVol = geometryInfo.GetAllLogicalVolumes();
        const auto  volMatching = geometryInfo.GetAllLogicalVolumesMatchingExpression(userVolume.fVolume);
        for (const auto& volume : volMatching) {
              physicalVolumes.insert(volume);
        }
        if (physicalVolumes.empty()) {
            // maybe it refers to a logical volume
            const auto logMatching = geometryInfo.GetAllLogicalVolumesMatchingExpression(userVolume.fVolume);
            for (const auto& logicalVolume : logMatching) {
               physicalVolumes.insert(logicalVolume);
            }
        }

        if (physicalVolumes.empty()) {
            RESTError << "TRestGeant4QuenchingProcess: No volume found matching expression: " << userVolume.fVolume
                        << RESTendl;
            continue;
        }

        for (const auto& physicalVolume : physicalVolumes) {
            const auto volumeName = geometryInfo.GetAlternativeNameFromGeant4PhysicalName(physicalVolume);
            const string volumeNameString = volumeName;
            RESTDebug<<"Volume: "<<physicalVolume<<" "<<volumeName<<RESTendl;
            fVolumes.insert(volumeNameString);
            fVolumeModels[volumeNameString] =
                InferQuenchingModel(userVolume.fModel, userVolume.fVolume, volumeNameString);
            fVolumeBirksConstants[volumeNameString] = userVolume.fBirksFallbackStepLength;
            fVolumeBirksFallbackStepLengths[volumeNameString] =
                userVolume.fBirksFallbackStepLength;
        }
    }

    RESTDebug << "TRestGeant4QuenchingProcess initialized with volumes" << RESTendl;
    for (const auto& volume : fVolumes) {
        RESTDebug << " " << volume << " (" << fVolumeModels[volume] << ")" << RESTendl;
    }

}

/// \brief Computes per-signal and event-level pulse-shape observables for one event.
bool TRestGeant4QuenchingProcess::ProcessEvent(const TRestEvent& input, TRestEvent& output) {
    output.CopyFrom(&input);

    auto* g4Event = dynamic_cast<TRestGeant4Event*>(&output);
    if (!g4Event) {
        throw std::runtime_error("TRestGeant4QuenchingProcess: output is not a TRestGeant4Event");
    }
    g4Event->SetGeant4Metadata(fGeant4Metadata);

    fObs.clear();
    fObs.sensitiveVolumeEnergyBefore = 0.0;
    fObs.sensitiveVolumeEnergyAfter = 0.0;
    
    const auto sensitiveVolumes = fGeant4Metadata->GetSensitiveVolumes();
    for(const auto &sensitiveVolumeName : sensitiveVolumes){
        fObs.sensitiveVolumeEnergyBefore += g4Event->GetEnergyInVolume(sensitiveVolumeName);
    }

    std::map<std::string, double> localQuenchedEnergies;
    for (const auto& volName : g4Event->fEventData.crossVolumeNames) {
        localQuenchedEnergies[volName] = 0.0;
    }

    g4Event->ClearCache(); 

    bool missingHadronicInfoWarningPrinted = false;

     for (int trackIndex = 0; trackIndex < static_cast<int>(g4Event->GetNumberOfTracks()); ++trackIndex) {

    auto track = g4Event->GetTrack(trackIndex);

    const std::string particleName =
        track.GetParticleName();

    const size_t nHits =
        track.GetNumberOfHits();

    double trackNewDepositedEnergy = 0.0;

    for (size_t hitIndex = 0; hitIndex < nHits; ++hitIndex) {

        const std::string physicalVolumeName =
            track.GetHitVolumeName(hitIndex);

        std::string cleanVolumeName = fGeant4Metadata->GetGeant4GeometryInfo().GetGeant4NameFiltered(physicalVolumeName);

        const double depositedEnergy =
            track.GetHitEnergy(hitIndex);

        double quenchingFactor = 1.0;

        if ((fVolumes.count(cleanVolumeName) ||
             fVolumes.count(physicalVolumeName)) &&
            depositedEnergy > 0) {

            const std::string targetVol =
                fVolumes.count(cleanVolumeName)
                    ? cleanVolumeName
                    : physicalVolumeName;

            const auto model =
                fVolumeModels.count(targetVol)
                    ? fVolumeModels[targetVol]
                    : std::string("lindhard");

            if (model == "birks") {

                if (!IsNeutralParticleWithoutBirksQuenching(
                        particleName)) {

                    const double stepLength =
                        EstimateStepLength(
                            track,
                            static_cast<int>(hitIndex),
                            fVolumeBirksFallbackStepLengths[targetVol]
                        );

                    quenchingFactor =
                        BirksQuenchingFactor(
                            depositedEnergy,
                            stepLength,
                            fVolumeBirksConstants[targetVol]
                        );
                }

            } else if (model == "lindhard") {

                if (track.GetHadronicOk()) {

                    const std::string isotopeName =
                        track.GetHitHadronicTargetIsotopeName(hitIndex);

                    const int isotopeA =
                        track.GetHitHadronicTargetIsotopeA(hitIndex);

                    const int isotopeZ =
                        track.GetHitHadronicTargetIsotopeZ(hitIndex);

                    if (!isotopeName.empty()) {
                        quenchingFactor =
                            LindhardQuenchingFactor(
                                depositedEnergy,
                                isotopeA,
                                isotopeZ
                            );
                    }

                } else if (!missingHadronicInfoWarningPrinted) {

                    RESTWarning
                        << "TRestGeant4QuenchingProcess: "
                           "Lindhard quenching requested but "
                           "hadronic target info not available."
                        << RESTendl;

                    missingHadronicInfoWarningPrinted = true;
                }
            }
        } else {
          continue;
        }

        const auto visibleEnergy =
            depositedEnergy * quenchingFactor;

        if (fApplyToHitEnergies) {
            track.SetHitEnergy(
                hitIndex,
                visibleEnergy
            );
        }

        if (visibleEnergy > 0) {

            trackNewDepositedEnergy +=
                visibleEnergy;

            localQuenchedEnergies[
                physicalVolumeName
            ] += visibleEnergy;
        }
    }

    g4Event->fEventData.trackDepositedEnergy[trackIndex] =
        trackNewDepositedEnergy;
}

    g4Event->ClearCache(); 
    g4Event->RefreshViews();

    for(const auto &sensitiveVolumeName : sensitiveVolumes){  
      fObs.sensitiveVolumeEnergyAfter += g4Event->GetEnergyInVolume(sensitiveVolumeName);
    }
    g4Event->fEventData.sensitiveVolumeEnergy = fObs.sensitiveVolumeEnergyAfter;

    fObs.sensitiveQuenched = TMath::Abs(fObs.sensitiveVolumeEnergyAfter - fObs.sensitiveVolumeEnergyBefore) > 1e-2;
    RESTDebug << fObs.sensitiveVolumeEnergyBefore << " " << fObs.sensitiveVolumeEnergyAfter << " " << fObs.sensitiveQuenched << RESTendl;

    return true;
}


/// \brief Finalization hook for the analysis process.
void TRestGeant4QuenchingProcess::EndProcess() {}
