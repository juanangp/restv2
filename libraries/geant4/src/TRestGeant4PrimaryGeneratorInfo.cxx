#include <TRandom.h>
#include "TRestGeant4PrimaryGeneratorInfo.h"

#include <TAxis.h>
#include <TMath.h>

#include <fstream>
#include <iostream>

#include "TRestMetadata.h"
#include "TRestTools.h"

using namespace std;
using namespace TRestGeant4PrimaryGeneratorTypes;

static const bool TRestGeant4PrimaryGeneratorInfo_FieldsRegistered = []() {
    auto& reg = TRestMetadataFieldRegistry::Instance();

    // Standard structural definitions mapping correctly to the clean YAML file keys
    reg.RegisterField<TRestGeant4PrimaryGeneratorInfo>(
        "type", &TRestGeant4PrimaryGeneratorInfo::fSpatialGeneratorType);
    reg.RegisterField<TRestGeant4PrimaryGeneratorInfo>(
        "shape", &TRestGeant4PrimaryGeneratorInfo::fSpatialGeneratorShape);
    
    // FIXED: Swapped out legacy verbose strings for the real flat YAML keys
    reg.RegisterField<TRestGeant4PrimaryGeneratorInfo>(
        "from", &TRestGeant4PrimaryGeneratorInfo::fSpatialGeneratorFrom);
    reg.RegisterField<TRestGeant4PrimaryGeneratorInfo>(
        "position", &TRestGeant4PrimaryGeneratorInfo::fSpatialGeneratorPosition);
    reg.RegisterField<TRestGeant4PrimaryGeneratorInfo>(
        "rotationAxis", &TRestGeant4PrimaryGeneratorInfo::fSpatialGeneratorRotationAxis);
    reg.RegisterField<TRestGeant4PrimaryGeneratorInfo>(
        "rotationValue", &TRestGeant4PrimaryGeneratorInfo::fSpatialGeneratorRotationValue);
    reg.RegisterField<TRestGeant4PrimaryGeneratorInfo>(
        "size", &TRestGeant4PrimaryGeneratorInfo::fSpatialGeneratorSize);

    // Operational densities and boundary maps
    reg.RegisterField<TRestGeant4PrimaryGeneratorInfo>(
        "spatialDensityFunction", &TRestGeant4PrimaryGeneratorInfo::fSpatialGeneratorSpatialDensityFunction);
    reg.RegisterField<TRestGeant4PrimaryGeneratorInfo>(
        "worldSize", &TRestGeant4PrimaryGeneratorInfo::fSpatialGeneratorWorldSize);
    reg.RegisterField<TRestGeant4PrimaryGeneratorInfo>(
        "source", &TRestGeant4PrimaryGeneratorInfo::fParticleSources);
    reg.RegisterField<TRestGeant4PrimaryGeneratorInfo>(
        "seed", &TRestGeant4PrimaryGeneratorInfo::fSeed);

    return true;
}();

TRestGeant4PrimaryGeneratorInfo::TRestGeant4PrimaryGeneratorInfo() : TRestMetadata() {
    fName = "TRestGeant4PrimaryGeneratorInfo";
}

TRestGeant4PrimaryGeneratorInfo::TRestGeant4PrimaryGeneratorInfo(const std::string& name,
                                                                 const YAML::Node& node)
    : TRestMetadata(name, node) {
    LoadConfig();
}

TRestGeant4PrimaryGeneratorInfo::~TRestGeant4PrimaryGeneratorInfo() {
    RemoveParticleSources();
}

void TRestGeant4PrimaryGeneratorInfo::LoadConfig() {
    RemoveParticleSources();
    UpdateParamsFromYAML<TRestGeant4PrimaryGeneratorInfo>(fNode);

    ReadYAMLVerbose(fNode);
    UpdateYAMLFromParams<TRestGeant4PrimaryGeneratorInfo>(fNode);
}

void TRestGeant4PrimaryGeneratorInfo::SetGeneratedParticleSeed(long seed) {
    fSeed = seed;
    fTemplateRandom.seed(static_cast<std::mt19937::result_type>(seed));
}

void TRestGeant4PrimaryGeneratorInfo::UpdateGeneratedParticles() {
    fGeneratedParticles.clear();
    fGeneratedParticles.resize(fParticleSources.size());
    const bool initializeTemplates = fGeneratedParticleTemplates.size() != fParticleSources.size();
    if (initializeTemplates) fGeneratedParticleTemplates.assign(fParticleSources.size(), {});

    for (size_t sourceIndex = 0; sourceIndex < fParticleSources.size(); ++sourceIndex) {
        const auto& source = fParticleSources[sourceIndex];
        auto& templates = fGeneratedParticleTemplates[sourceIndex];

        if (initializeTemplates && !source.fGenFilename.empty() && source.fGenFilename.find(".dat") != std::string::npos) {
            const std::string filename = TRestTools::GetFullPath(source.fGenFilename);
            std::ifstream input(filename);
            if (!input.is_open()) {
                RESTError << "File not found: " << source.fGenFilename << RESTendl;
                exit(1);
            }

            int generatorEvents = 0;
            std::string line;
            for (int i = 0; i < 24 && std::getline(input, line); ++i) {
                const auto position = line.find("@nevents=");
                if (position != std::string::npos) generatorEvents = std::stoi(line.substr(position + 9));
            }
            if (generatorEvents <= 0) {
                RESTError << "Problem reading generator file: " << filename << RESTendl;
                exit(1);
            }

            for (int event = 0; event < generatorEvents; ++event) {
                while (std::getline(input, line) && line.find("@event_start") == std::string::npos) {}
                std::getline(input, line);
                int particleCount = 0;
                input >> particleCount;
                std::vector<TRestGeant4ParticleState> eventParticles;
                for (int particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
                    int particleId = 0;
                    double particleTime = 0.0;
                    double momx = 0.0, momy = 0.0, momz = 0.0;
                    double mass = 0.0;
                    std::string particleName;
                    input >> particleId >> particleTime >> momx >> momy >> momz >> particleName;

                    TRestGeant4ParticleState particle;
                    double energy = -1.0;
                    if (particleId == 3) {
                        mass = 0.511;
                        energy = std::sqrt(momx * momx + momy * momy + momz * momz + mass * mass) - mass;
                        particle.SetParticleName("e-");
                        particle.SetParticleCharge(-1);
                    } else if (particleId == 1) {
                        energy = std::sqrt(momx * momx + momy * momy + momz * momz);
                        particle.SetParticleName("gamma");
                    } else {
                        continue;
                    }
                    particle.SetEnergy(1000.0 * energy);
                    particle.SetDirection(ROOT::Math::XYZVector(momx, momy, momz).Unit());
                    eventParticles.push_back(particle);
                }
                templates.push_back(std::move(eventParticles));
            }
        } else if (initializeTemplates) {
            TRestGeant4ParticleState particle;
            particle.SetParticleName(source.GetParticleName());
            particle.SetParticleCharge(source.GetParticleCharge());
            particle.SetExcitationLevel(source.GetExcitationLevel());
            particle.SetOrigin(source.GetOrigin());
            particle.SetDirection(ROOT::Math::XYZVector(source.fAngularDistribution.fDirection[0],
                                                       source.fAngularDistribution.fDirection[1],
                                                       source.fAngularDistribution.fDirection[2]));
            particle.SetEnergy(source.fEnergyDistribution.fEnergy);
            templates.push_back({particle});
        }

        if (!templates.empty()) {
            size_t selected = 0;
            if (templates.size() > 1) {
                if (auto* decayRandom = source.GetRandomMethod()) {
                    selected = static_cast<size_t>(decayRandom->Integer(static_cast<UInt_t>(templates.size())));
                } else {
                    std::uniform_int_distribution<size_t> distribution(0, templates.size() - 1);
                    selected = distribution(fTemplateRandom);
                }
            }
            fGeneratedParticles[sourceIndex] = templates[selected];
        }
    }
}

bool TRestGeant4PrimaryGeneratorInfo::HasGeneratedParticleTemplates() const {
    for (const auto& templates : fGeneratedParticleTemplates) {
        if (templates.size() > 1) return true;
    }
    return false;
}

const std::vector<TRestGeant4ParticleState>&
TRestGeant4PrimaryGeneratorInfo::GetGeneratedParticles(size_t sourceIndex) const {
    static const std::vector<TRestGeant4ParticleState> empty;
    return sourceIndex < fGeneratedParticles.size() ? fGeneratedParticles[sourceIndex] : empty;
}

void TRestGeant4PrimaryGeneratorInfo::RemoveParticleSources() {
    fParticleSources.clear();
    fGeneratedParticles.clear();
    fGeneratedParticleTemplates.clear();
}

// --- Print Metadata Logger Implementation ---

void TRestGeant4PrimaryGeneratorInfo::Print() const {
    const auto typeEnum = StringToSpatialGeneratorTypes(fSpatialGeneratorType);
    RESTMetadata << "Generator type: " << SpatialGeneratorTypesToString(typeEnum) << RESTendl;

    if (typeEnum != SpatialGeneratorTypes::POINT) {
        const auto shapeEnum = StringToSpatialGeneratorShapes(fSpatialGeneratorShape);
        RESTMetadata << "Generator shape: " << SpatialGeneratorShapesToString(shapeEnum);
        if (shapeEnum == SpatialGeneratorShapes::GDML) {
            RESTMetadata << "::" << fSpatialGeneratorFrom << RESTendl;
        } else {
            if (shapeEnum == SpatialGeneratorShapes::BOX) {
                RESTMetadata << ", (length, width, height): ";
            } else if (shapeEnum == SpatialGeneratorShapes::SPHERE) {
                RESTMetadata << ", (radius, , ): ";
            } else if (shapeEnum == SpatialGeneratorShapes::WALL) {
                RESTMetadata << ", (length, width, ): ";
            } else if (shapeEnum == SpatialGeneratorShapes::CIRCLE) {
                RESTMetadata << ", (radius, , ): ";
            } else if (shapeEnum == SpatialGeneratorShapes::CYLINDER) {
                RESTMetadata << ", (radius, length, ): ";
            }

            // Unpacking modern MathCore vector points coordinates safely
            RESTMetadata << fSpatialGeneratorSize[0] << ", " << fSpatialGeneratorSize[1] << ", "
                         << fSpatialGeneratorSize[2] << RESTendl;
        }
    }
    RESTMetadata << "Generator center : (" << fSpatialGeneratorPosition[0] << ","
                 << fSpatialGeneratorPosition[1] << "," << fSpatialGeneratorPosition[2] << ") mm" << RESTendl;
    RESTMetadata << "Generator rotation : (" << fSpatialGeneratorRotationAxis[0] << ","
                 << fSpatialGeneratorRotationAxis[1] << "," << fSpatialGeneratorRotationAxis[2]
                 << "), angle: " << fSpatialGeneratorRotationValue * TMath::RadToDeg() << "º" << RESTendl;
}
