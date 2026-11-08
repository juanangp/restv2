#ifndef REST_GEANT4_LISTISOTOPES_C
#define REST_GEANT4_LISTISOTOPES_C

#include <TCanvas.h>
#include <TH1D.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "TRestGeant4Event.h"
#include "TRestGeant4Track.h"
#include "TRestRun.h"

/// \brief Particles and isotopes produced in a restG4 simulation (legacy REST_Geant4_ListIsotopes,
/// REST_Geant4_FindIsotopes and REST_Geant4_FindGammasEmitted): the number of tracks of each particle but
/// e-, e+ and gamma (isotopes, alphas, neutrons...), in decreasing order, with the volumes where they are
/// created (GetInitialVolume), written also to `outputFile` if given ("name count" per line, as legacy);
/// and the spectrum of the initial kinetic energy (keV) of the gammas, drawn, in [gammaMin, gammaMax] (the
/// whole range if gammaMax <= gammaMin) with `bins` bins.
///
/// \param fileName restG4 file with TRestGeant4Event (with tracks).
/// \param outputFile Text file for the particle counts (not written if empty).
/// \param gammaMin Minimum of the gamma spectrum (keV).
/// \param gammaMax Maximum of the gamma spectrum (keV); the whole range if gammaMax <= gammaMin.
/// \param bins Number of bins of the gamma spectrum.
/// \return 0 on success, 1 if the file has no TRestGeant4Event.
///
///    restRoot
///    REST_Geant4_ListIsotopes("Run1_Pb210_Shielding.root", "isotopes.txt", 0, 100)
int REST_Geant4_ListIsotopes(const std::string& fileName, const std::string& outputFile = "",
                             double gammaMin = 0, double gammaMax = 0, int bins = 500) {
    TRestRun run(fileName);
    if (!run.HasEvent("TRestGeant4Event")) {
        std::cerr << "REST_Geant4_ListIsotopes: " << fileName << " has no TRestGeant4Event" << std::endl;
        return 1;
    }
    auto& event = run.GetEvent<TRestGeant4Event>("TRestGeant4Event");
    event.SetRestRun(&run);

    std::map<std::string, long long> counts;
    std::map<std::string, std::map<std::string, long long>> volumes;
    std::vector<double> gammaEnergies;
    for (long long entry = 0; entry < run.GetEntries(); entry++) {
        if (!run.GetEntry(entry)) break;
        for (const auto& track : event.GetTracks()) {
            const std::string particle = track.GetParticleName();
            if (particle == "gamma") {
                gammaEnergies.push_back(track.GetInitialKineticEnergy());
                continue;
            }
            if (particle == "e-" || particle == "e+") continue;
            counts[particle]++;
            volumes[particle][track.GetInitialVolume()]++;
        }
    }

    std::vector<std::pair<std::string, long long>> sorted(counts.begin(), counts.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    std::cout << "REST_Geant4_ListIsotopes: " << run.GetEntries() << " entries of " << fileName << std::endl;
    std::ofstream output;
    if (!outputFile.empty()) output.open(outputFile);
    for (const auto& [particle, count] : sorted) {
        std::cout << "  " << particle << ": " << count << " (";
        bool firstVolume = true;
        for (const auto& [volume, volumeCount] : volumes[particle]) {
            std::cout << (firstVolume ? "" : ", ") << volume << ": " << volumeCount;
            firstVolume = false;
        }
        std::cout << ")" << std::endl;
        if (output.is_open()) output << particle << " " << count << std::endl;
    }
    if (output.is_open()) std::cout << "Written to " << outputFile << std::endl;

    if (gammaEnergies.empty()) {
        std::cout << "No gammas" << std::endl;
        return 0;
    }
    if (gammaMax <= gammaMin) {
        gammaMin = *std::min_element(gammaEnergies.begin(), gammaEnergies.end());
        gammaMax = *std::max_element(gammaEnergies.begin(), gammaEnergies.end()) * 1.001;
    }
    auto* histogram =
        new TH1D("gammas", "Gammas emitted;Initial kinetic energy (keV);Gammas", bins, gammaMin, gammaMax);
    for (const double energy : gammaEnergies) histogram->Fill(energy);
    std::cout << gammaEnergies.size() << " gammas, " << histogram->Integral() << " in [" << gammaMin << ", "
              << gammaMax << "] keV" << std::endl;
    auto* canvas = new TCanvas("gammasCanvas", "Gammas emitted");
    canvas->cd();
    histogram->Draw();
    canvas->Update();
    return 0;
}
#endif
