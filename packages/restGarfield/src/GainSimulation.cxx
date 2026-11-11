#include "GainSimulation.h"

#include <cmath>
#include <ctime>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

#include "Garfield/AvalancheMicroscopic.hh"
#include "Garfield/ComponentConstant.hh"
#include "Garfield/GeometrySimple.hh"
#include "Garfield/MediumMagboltz.hh"
#include "Garfield/Sensor.hh"
#include "Garfield/SolidBox.hh"
#include "TFile.h"
#include "TRestDetectorTransportManager.h"
#include "TransportSimulation.h"

namespace {
// Garfield units: V/cm, cm, eV. REST units: V/mm, mm
constexpr double kVcmPerVmm = 10.;
constexpr double kCmPerMm = 0.1;
/// Initial energy of the primary electron (eV)
constexpr double kInitialEnergy = 0.1;
/// Lateral half size of the gap in units of the gap (the avalanche does not reach the sides)
constexpr double kLateralSize = 50.;

/// Sizes (electrons reaching the anode, z = 0) of the avalanches of one electron started at the mesh
/// (z = gap) in a uniform field (V/cm) across a gap (cm)
std::vector<double> SimulateAvalanches(Garfield::MediumMagboltz& gas, double gap, double field, int n) {
    const double half = kLateralSize * gap;
    Garfield::SolidBox box(0, 0, gap / 2, half, half, gap / 2);
    Garfield::GeometrySimple geometry;
    geometry.AddSolid(&box, &gas);
    Garfield::ComponentConstant component;
    component.SetGeometry(&geometry);
    // Field along +z: the electrons drift to -z, from the mesh to the anode
    component.SetElectricField(0, 0, field);
    Garfield::Sensor sensor;
    sensor.AddComponent(&component);
    sensor.SetArea(-half, -half, 0, half, half, gap);

    Garfield::AvalancheMicroscopic avalanche;
    avalanche.SetSensor(&sensor);

    std::vector<double> sizes;
    const double tolerance = 1e-6 * gap;
    for (int i = 0; i < n; i++) {
        avalanche.AvalancheElectron(0, 0, gap - tolerance, 0, kInitialEnergy);
        int collected = 0;
        for (unsigned int e = 0; e < avalanche.GetNumberOfElectronEndpoints(); e++) {
            double x0, y0, z0, t0, e0, x1, y1, z1, t1, e1;
            int status;
            avalanche.GetElectronEndpoint(e, x0, y0, z0, t0, e0, x1, y1, z1, t1, e1, status);
            if (z1 <= tolerance) collected++;
        }
        sizes.push_back(collected);
    }
    return sizes;
}
}  // namespace

void RunGainSimulation(const TRestDetectorMedium& medium, TRestDetectorGainTable& simulation, bool verbose) {
    if (!medium.IsGas()) {
        throw std::runtime_error("restGarfield: gain tables are only for gases (" + medium.GetName() + ")");
    }
    const auto mediumFile = medium.GetMediumFile();
    if (mediumFile.has_parent_path()) std::filesystem::create_directories(mediumFile.parent_path());
    const std::string fileName = mediumFile.string();
    const double gap = simulation.fGap;
    const double penning = simulation.fUsePenning ? simulation.fPenningRate : 0;

    for (const double pressure : simulation.GetPressures(medium)) {
        const auto fields = simulation.GetFieldGrid(pressure);
        for (const double temperature : simulation.GetTemperatures(medium)) {
            const std::string key = TRestDetectorGainTable::GetKey(pressure, temperature, gap, fields.front(),
                                                                   fields.back(), penning);
            bool exists = false;
            if (std::filesystem::exists(fileName)) {
                const MediumFileLock lock(mediumFile);
                std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "READ"));
                exists = file && !file->IsZombie() &&
                         TRestDetectorGainTable::HasTable(file.get(), medium.GetName(), key);
            }
            if (!simulation.fOverwrite && exists) {
                RESTInfo << "restGarfield: gain table " << key << " already in " << fileName
                         << " (set overwrite: true to compute it again)" << RESTendl;
                continue;
            }

            RESTInfo << "restGarfield: computing gain table " << key << " of " << medium.GetName()
                     << RESTendl;
            auto gas = CreateGas(medium, pressure, temperature);
            if (penning > 0 && !gas->EnablePenningTransfer(penning, 0.)) {
                throw std::runtime_error("restGarfield: cannot enable the Penning transfer for " +
                                         medium.GetName());
            }

            TRestDetectorGainTable table;
            table.fMedium = medium.GetName();
            table.fPressure = pressure;
            table.fTemperature = temperature;
            table.fTableGap = gap;
            table.fPenning = penning;
            std::vector<std::vector<double>> sizes;
            for (const double field : fields) {
                sizes.push_back(
                    SimulateAvalanches(*gas, gap * kCmPerMm, field * kVcmPerVmm, simulation.fAvalanches));
                // Mean gain and Polya theta from the moments: sigma^2 / G^2 = 1 / (1 + theta)
                double sum = 0, sum2 = 0;
                for (const double size : sizes.back()) {
                    sum += size;
                    sum2 += size * size;
                }
                const double n = sizes.back().size();
                const double mean = sum / n;
                const double variance = (sum2 - n * mean * mean) / (n - 1);
                table.fFields.push_back(field);
                table.fGain.push_back(mean);
                table.fTheta.push_back(variance > 0 ? mean * mean / variance - 1 : 0);
                if (verbose) {
                    RESTInfo << "restGarfield: E = " << field * kVcmPerVmm << " V/cm, gain " << mean
                             << ", theta " << table.fTheta.back() << RESTendl;
                }
            }

            YAML::Node generator = YAML::Clone(simulation.GetYAMLNode());
            generator["program"] = "Garfield++ AvalancheMicroscopic (uniform field)";
            const std::time_t now = std::time(nullptr);
            char date[32];
            std::strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
            generator["date"] = date;

            // Locked (other processes may write other media or tables) until the file is closed
            const MediumFileLock lock(mediumFile);
            std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "UPDATE"));
            if (!file || file->IsZombie()) throw std::runtime_error("restGarfield: cannot open " + fileName);
            table.Write(file.get(), medium.GetDefinition(), generator, sizes);
            RESTInfo << "restGarfield: gain table " << key << " written to " << fileName << RESTendl;
        }
    }
}
