#ifndef REST_GEANT4_READEVENTS_C
#define REST_GEANT4_READEVENTS_C

#include <iostream>
#include <string>

#include "TRestGeant4Event.h"
#include "TRestRun.h"

/// Prints (PrintG4Event) the events of a restG4 file between the entries `first` and `last` (included; -1:
/// the last entry) with energy in the sensitive volumes in [energyMin, energyMax] keV (any energy if
/// energyMax < energyMin), and the number of them: legacy REST_Geant4_ReadNEvents (without range),
/// REST_Geant4_ReadNEvents_inROI and, with all the entries, the events in a region of interest of
/// REST_Geant4_GetROIEvents. maxTracks and maxHits limit what is printed of each event (-1: all).
///
///    restRoot
///    REST_Geant4_ReadEvents("Run1_NLDBD_Test.root", 0, 10)
///    REST_Geant4_ReadEvents("Run1_NLDBD_Test.root", 0, -1, 2400, 2500, 5, 0)
int REST_Geant4_ReadEvents(const std::string& fileName, long long first = 0, long long last = 0,
                           double energyMin = 0, double energyMax = -1, int maxTracks = -1,
                           int maxHits = -1) {
    TRestRun run(fileName);
    if (!run.HasEvent("TRestGeant4Event")) {
        std::cerr << "REST_Geant4_ReadEvents: " << fileName << " has no TRestGeant4Event" << std::endl;
        return 1;
    }
    auto& event = run.GetEvent<TRestGeant4Event>("TRestGeant4Event");
    event.SetRestRun(&run);

    if (last < 0 || last >= run.GetEntries()) last = run.GetEntries() - 1;
    const bool energyRange = energyMax >= energyMin;
    long long selected = 0;
    for (long long entry = first; entry <= last; entry++) {
        if (!run.GetEntry(entry)) break;
        const double energy = event.GetSensitiveVolumeEnergy();
        if (energyRange && (energy < energyMin || energy > energyMax)) continue;
        selected++;
        std::cout << "Entry " << entry << ": " << energy << " keV in the sensitive volumes" << std::endl;
        event.PrintG4Event(maxTracks, maxHits);
    }
    std::cout << "REST_Geant4_ReadEvents: " << selected << " events in the entries " << first << " to "
              << last;
    if (energyRange) std::cout << " with energy in [" << energyMin << ", " << energyMax << "] keV";
    std::cout << std::endl;
    return 0;
}
#endif
