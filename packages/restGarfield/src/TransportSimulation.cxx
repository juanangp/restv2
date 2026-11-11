#include "TransportSimulation.h"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

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
                                               const TRestDetectorTransportTable& simulation,
                                               const std::vector<double>& fields, double pressure,
                                               double temperature, bool verbose) {
    const std::string error = "restGarfield: medium '" + definition.GetName() + "': ";
    if (definition.IsGas()) {
        auto gas = CreateGas(definition, pressure, temperature);
        // The nodes of the table (or a contiguous part of them, with the same spacing)
        gas->SetFieldGrid(fields.front() * kVcmPerVmm, fields.back() * kVcmPerVmm, fields.size(),
                          simulation.fLogField && fields.size() > 1);
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

using Carriers = std::map<std::string, TRestDetectorTransportTable::Carrier>;

/// Transport parameters at the fields: the carriers (electron, hole, ion) that the Garfield medium provides
/// (its velocity call fails otherwise)
Carriers ComputeCarriers(const TRestDetectorMedium& definition, const TRestDetectorTransportTable& simulation,
                         const std::vector<double>& fields, double pressure, double temperature,
                         bool verbose) {
    auto garfieldMedium = CreateMedium(definition, simulation, fields, pressure, temperature, verbose);
    Carriers carriers;
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
        if (available) carriers[carrier] = values;
    }
    return carriers;
}

/// The vectors of a carrier, in the order they are sent between processes
std::vector<std::vector<double>*> Columns(TRestDetectorTransportTable::Carrier& carrier) {
    return {&carrier.fDriftVelocity, &carrier.fLongitudinalDiffusion, &carrier.fTransversalDiffusion,
            &carrier.fTownsend, &carrier.fAttachment};
}

/// Writes the carriers of some fields to a pipe: per carrier, whether it is available and its vectors
bool WriteCarriers(FILE* pipe, Carriers& carriers, size_t nFields) {
    for (const std::string name : {"electron", "hole", "ion"}) {
        const char available = carriers.count(name);
        if (fwrite(&available, 1, 1, pipe) != 1) return false;
        if (!available) continue;
        for (auto* column : Columns(carriers[name])) {
            if (column->size() != nFields) return false;
            if (fwrite(column->data(), sizeof(double), nFields, pipe) != nFields) return false;
        }
    }
    return true;
}

/// Reads the carriers of some fields (WriteCarriers) and appends them to `carriers`
bool ReadCarriers(FILE* pipe, Carriers& carriers, size_t nFields, bool first) {
    for (const std::string name : {"electron", "hole", "ion"}) {
        char available = 0;
        if (fread(&available, 1, 1, pipe) != 1) return false;
        // A carrier is kept if it is available at all the fields
        if (!available) {
            carriers.erase(name);
            continue;
        }
        if (!first && !carriers.count(name)) {
            TRestDetectorTransportTable::Carrier ignored;
            for (auto* column : Columns(ignored)) {
                column->resize(nFields);
                if (fread(column->data(), sizeof(double), nFields, pipe) != nFields) return false;
            }
            continue;
        }
        for (auto* column : Columns(carriers[name])) {
            const size_t offset = column->size();
            column->resize(offset + nFields);
            if (fread(column->data() + offset, sizeof(double), nFields, pipe) != nFields) return false;
        }
    }
    return true;
}

/// ComputeCarriers with the fields split in up to `jobs` processes (Magboltz is not thread safe): each child
/// computes a contiguous part of the grid (at least 2 nodes, the same nodes with the same spacing) and
/// sends the values through a pipe
Carriers ComputeInParallel(const TRestDetectorMedium& definition,
                           const TRestDetectorTransportTable& simulation, const std::vector<double>& fields,
                           double pressure, double temperature, bool verbose, int jobs) {
    const size_t nodes = fields.size();
    const size_t parts = std::max<size_t>(1, std::min<size_t>(jobs, nodes / 2));
    struct Child {
        pid_t pid;
        FILE* pipe;
        size_t nFields;
    };
    std::vector<Child> children;
    size_t begin = 0;
    for (size_t part = 0; part < parts; part++) {
        // Parts of nodes / parts nodes (the first ones one more): at least 2
        const size_t size = nodes / parts + (part < nodes % parts ? 1 : 0);
        const std::vector<double> partFields(fields.begin() + begin, fields.begin() + begin + size);
        begin += size;
        int fd[2];
        if (pipe(fd) != 0) throw std::runtime_error("restGarfield: cannot create a pipe");
        std::cout.flush();
        std::fflush(nullptr);
        const pid_t pid = fork();
        if (pid < 0) throw std::runtime_error("restGarfield: cannot create a process");
        if (pid == 0) {
            close(fd[0]);
            int status = 1;
            try {
                Carriers carriers =
                    ComputeCarriers(definition, simulation, partFields, pressure, temperature, verbose);
                FILE* output = fdopen(fd[1], "wb");
                if (output && WriteCarriers(output, carriers, partFields.size()) && fclose(output) == 0) {
                    status = 0;
                }
            } catch (const std::exception& error) {
                std::cerr << "restGarfield: " << error.what() << std::endl;
            }
            std::cout.flush();
            // Without the exit handlers of the parent (ROOT)
            _exit(status);
        }
        close(fd[1]);
        children.push_back({pid, fdopen(fd[0], "rb"), partFields.size()});
    }
    RESTInfo << "restGarfield: " << nodes << " field nodes in " << parts << " processes" << RESTendl;

    Carriers carriers;
    bool ok = true;
    for (size_t i = 0; i < children.size(); i++) {
        auto& child = children[i];
        ok &= child.pipe && ReadCarriers(child.pipe, carriers, child.nFields, i == 0);
        if (child.pipe) fclose(child.pipe);
        int status = 0;
        waitpid(child.pid, &status, 0);
        ok &= WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
    if (!ok) throw std::runtime_error("restGarfield: a process computing the field nodes failed");
    return carriers;
}

}  // namespace

MediumFileLock::MediumFileLock(const std::filesystem::path& mediumFile) {
    const auto directory = mediumFile.has_parent_path() ? mediumFile.parent_path() : ".";
    fDescriptor = open(directory.c_str(), O_RDONLY);
    if (fDescriptor < 0 || flock(fDescriptor, LOCK_EX) != 0) {
        if (fDescriptor >= 0) close(fDescriptor);
        throw std::runtime_error("restGarfield: cannot lock " + directory.string());
    }
}

MediumFileLock::~MediumFileLock() {
    flock(fDescriptor, LOCK_UN);
    close(fDescriptor);
}

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
                            bool verbose, int jobs) {
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
            const MediumFileLock lock(mediumFile);
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

        TRestDetectorTransportTable table;
        table.fMedium = medium.GetName();
        table.fPressure = pressure;
        table.fTemperature = temperature;
        table.fPressureScaling = medium.IsGas();
        table.fPenning = penning;
        for (const double field : fields) table.fFields.push_back(field);
        // Magboltz (gases) in parallel processes; the solids are fast
        if (jobs > 1 && medium.IsGas()) {
            table.fCarriers =
                ComputeInParallel(medium, simulation, fields, pressure, temperature, verbose, jobs);
        } else {
            table.fCarriers = ComputeCarriers(medium, simulation, fields, pressure, temperature, verbose);
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

        // Locked (other processes may write other media or tables) until the file is closed
        const MediumFileLock lock(mediumFile);
        std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "UPDATE"));
        if (!file || file->IsZombie()) throw std::runtime_error("restGarfield: cannot open " + fileName);
        table.Write(file.get(), medium.GetDefinition(), generator);
        RESTInfo << "restGarfield: table " << key << " written to " << fileName << RESTendl;
    }
}
