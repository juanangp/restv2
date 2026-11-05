#include "TransportSimulation.h"

#include <cmath>
#include <ctime>
#include <filesystem>
#include <memory>
#include <stdexcept>

#include "Garfield/MediumCdTe.hh"
#include "Garfield/MediumDiamond.hh"
#include "Garfield/MediumGaAs.hh"
#include "Garfield/MediumGaN.hh"
#include "Garfield/MediumMagboltz.hh"
#include "Garfield/MediumSilicon.hh"
#include "TFile.h"
#include "TRestDetectorMedium.h"
#include "TRestDetectorTransportManager.h"

namespace {
// Garfield units: V/cm, cm/ns, eV, Torr. REST units: V/mm, mm/us, keV, bar
constexpr double kVcmPerVmm = 10.;
constexpr double kTorrPerBar = 750.062;
const double kEV = REST_Units::REST_Units_Map["eV"].second;

/// Garfield medium of the definition at a condition: gases (Magboltz computes their table) and solids of
/// a single component known by Garfield
std::unique_ptr<Garfield::Medium> CreateMedium(const TRestDetectorMedium& definition,
                                               const TRestDetectorTransportTable& simulation, double pressure,
                                               double temperature, bool verbose) {
    const std::string error = "restGarfield: medium '" + definition.GetName() + "': ";
    if (definition.IsGas()) {
        auto gas = CreateGas(definition, pressure, temperature);
        const auto& [minField, maxField] = simulation.fFieldRange;
        gas->SetFieldGrid(minField * kVcmPerVmm, maxField * kVcmPerVmm, simulation.fFieldNodes,
                          simulation.fLogField);
        gas->SetMaxElectronEnergy(simulation.fMaxElectronEnergy / kEV);
        gas->GenerateGasTable(simulation.fCollisions, verbose);
        // Adjusts the Townsend coefficient of the table with its excitation and ionisation rates (lambda
        // only matters in microscopic tracking)
        if (simulation.fUsePenning && !gas->EnablePenningTransfer(simulation.fPenningRate, 0.)) {
            throw std::runtime_error(error + "cannot enable the Penning transfer");
        }
        return gas;
    }
    const auto& components = definition.fComponents;
    const std::string material = components.size() == 1 ? components.begin()->first : "";
    std::unique_ptr<Garfield::Medium> medium;
    if (definition.fState == "solid") {
        if (material == "Si") medium = std::make_unique<Garfield::MediumSilicon>();
        if (material == "GaAs") medium = std::make_unique<Garfield::MediumGaAs>();
        if (material == "GaN") medium = std::make_unique<Garfield::MediumGaN>();
        if (material == "CdTe") medium = std::make_unique<Garfield::MediumCdTe>();
        if (material == "diamond") medium = std::make_unique<Garfield::MediumDiamond>();
    }
    if (!medium) {
        throw std::runtime_error(error +
                                 "not supported by Garfield (gases, or solids Si, GaAs, GaN, CdTe or "
                                 "diamond with a single component)");
    }
    medium->SetTemperature(temperature);
    return medium;
}

}  // namespace

std::unique_ptr<Garfield::MediumMagboltz> CreateGas(const TRestDetectorMedium& medium, double pressure,
                                                    double temperature) {
    const std::string error = "restGarfield: medium '" + medium.GetName() + "': ";
    if (!medium.IsGas()) throw std::runtime_error(error + "not a gas");
    if (medium.fComponents.empty()) throw std::runtime_error(error + "no components");
    if (medium.fComponents.size() > 6) throw std::runtime_error(error + "up to 6 gas components");
    auto gas = std::make_unique<Garfield::MediumMagboltz>();
    std::string names[6];
    double fractions[6] = {0, 0, 0, 0, 0, 0};
    size_t i = 0;
    for (const auto& [component, fraction] : medium.fComponents) {
        names[i] = component;
        fractions[i++] = fraction;
    }
    if (!gas->SetComposition(names[0], fractions[0], names[1], fractions[1], names[2], fractions[2], names[3],
                             fractions[3], names[4], fractions[4], names[5], fractions[5])) {
        throw std::runtime_error(error + "invalid gas components for Garfield");
    }
    gas->SetTemperature(temperature);
    gas->SetPressure(pressure * kTorrPerBar);
    return gas;
}

void RunTransportSimulation(const TRestDetectorMedium& medium, TRestDetectorTransportTable& simulation,
                            bool verbose) {
    const auto mediumFile = medium.GetMediumFile();
    if (mediumFile.has_parent_path()) std::filesystem::create_directories(mediumFile.parent_path());
    const std::string fileName = mediumFile.string();
    const double pressure = simulation.fPressure;
    const auto fields = simulation.GetFieldGrid();
    if (simulation.fUsePenning && !medium.IsGas()) {
        throw std::runtime_error("restGarfield: usePenning is only for gases (" + medium.GetName() + ")");
    }
    const double penning = simulation.fUsePenning ? simulation.fPenningRate : 0;

    for (const double temperature : simulation.GetTemperatures(medium)) {
        const std::string key = TRestDetectorTransportTable::GetKey(pressure, temperature, fields.front(),
                                                                    fields.back(), penning);
        // The file is created by the first table: TFile::Open would print an error if it does not exist
        bool exists = false;
        if (std::filesystem::exists(fileName)) {
            std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "READ"));
            exists = file && !file->IsZombie() &&
                     TRestDetectorTransportTable::HasTable(file.get(), medium.GetName(), key);
        }
        if (!simulation.fOverwrite && exists) {
            RESTInfo << "restGarfield: table " << key << " already in " << fileName
                     << " (set overwrite: true to compute it again)" << RESTendl;
            continue;
        }

        RESTInfo << "restGarfield: computing table " << key << " of " << medium.GetName() << RESTendl;
        auto garfieldMedium = CreateMedium(medium, simulation, pressure, temperature, verbose);

        TRestDetectorTransportTable table;
        table.fMedium = medium.GetName();
        table.fPressure = pressure;
        table.fTemperature = temperature;
        table.fPressureScaling = medium.IsGas();
        table.fPenning = penning;
        // Carriers that the Garfield medium provides (its velocity call fails otherwise)
        for (const double field : fields) table.fFields.push_back(field);
        for (const std::string carrier : {"electron", "hole", "ion"}) {
            TRestDetectorTransportTable::Carrier values;
            bool available = true;
            for (const double field : fields) {
                const double e = field * kVcmPerVmm;
                double vx = 0, vy = 0, vz = 0, dl = 0, dt = 0, alpha = 0, eta = 0;
                if (carrier == "electron") {
                    available &= garfieldMedium->ElectronVelocity(0, 0, e, 0, 0, 0, vx, vy, vz);
                    garfieldMedium->ElectronDiffusion(0, 0, e, 0, 0, 0, dl, dt);
                    garfieldMedium->ElectronTownsend(0, 0, e, 0, 0, 0, alpha);
                    garfieldMedium->ElectronAttachment(0, 0, e, 0, 0, 0, eta);
                } else if (carrier == "hole") {
                    available &= garfieldMedium->HoleVelocity(0, 0, e, 0, 0, 0, vx, vy, vz);
                    garfieldMedium->HoleDiffusion(0, 0, e, 0, 0, 0, dl, dt);
                    garfieldMedium->HoleTownsend(0, 0, e, 0, 0, 0, alpha);
                    garfieldMedium->HoleAttachment(0, 0, e, 0, 0, 0, eta);
                } else {
                    available &= garfieldMedium->IonVelocity(0, 0, e, 0, 0, 0, vx, vy, vz);
                    garfieldMedium->IonDiffusion(0, 0, e, 0, 0, 0, dl, dt);
                }
                if (!available) break;
                // cm/ns -> mm/us
                values.fDriftVelocity.push_back(std::sqrt(vx * vx + vy * vy + vz * vz) * 1e4);
                values.fLongitudinalDiffusion.push_back(dl);
                values.fTransversalDiffusion.push_back(dt);
                values.fTownsend.push_back(alpha);
                values.fAttachment.push_back(eta);
            }
            if (available) table.fCarriers[carrier] = values;
        }
        if (!table.HasCarrier("electron")) {
            throw std::runtime_error("restGarfield: no electron transport for " + medium.GetName());
        }

        // The configuration used: the simulation section (all its conditions) and the program
        YAML::Node generator = YAML::Clone(simulation.GetYAMLNode());
        generator["program"] = medium.IsGas() ? "Magboltz (Garfield++)" : "Garfield++";
        const std::time_t now = std::time(nullptr);
        char date[32];
        std::strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        generator["date"] = date;

        std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "UPDATE"));
        if (!file || file->IsZombie()) throw std::runtime_error("restGarfield: cannot open " + fileName);
        table.Write(file.get(), medium.GetDefinition(), generator);
        RESTInfo << "restGarfield: table " << key << " written to " << fileName << RESTendl;
    }
}
