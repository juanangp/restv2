#include <iostream>
#include <memory>

#include "TFile.h"
#include "TRestFibreReadout.h"

TRestFibreReadout* fibreReadout = nullptr;

/// \brief Builds a TRestFibreReadout from the `readout` section of a YAML file (BuildGeometry, with the
/// decoding file of the configuration if given) and exports it, with its decoding named decodingName
/// ("nominal_decoding" by default), to a ROOT file that a TRestDetectorReadoutManager can import
/// (instanceName: the name of the readout, decodingName: the same name). If gdmlFile is given, it also
/// writes there the placements of the fibres (TRestFibreReadout::WriteGDMLPlacements), to include in the
/// GDML of the simulation as an external entity inside the volume of the scintillator:
///
///    <!DOCTYPE gdml [ <!ENTITY fibres SYSTEM "fibres.xml"> ]>
///    ...
///    <volume name="chamberVolume">
///        <materialref ref="LiquidScintillator"/>
///        <solidref ref="chamberSolid"/>
///        &fibres;
///    </volume>
///
/// The readout is kept in the global fibreReadout (nullptr if it cannot be built).
/// \param yamlFile YAML file with a `readout` section of class TRestFibreReadout.
/// \param outputFile ROOT file written (RECREATE).
/// \param gdmlFile File with the placements of the fibres (empty: not written).
/// \param decodingName Name of the decoding stored with the readout.
/// \param checkOverlaps Checks the overlaps of the geometry (TRestDetectorReadout::CheckOverlaps) before
/// exporting it: nothing is written if there is any.
/// \return 0 on success, 1 on any error (the exit code of `restRoot -b -q`, e.g. in a CI).
///
/// Usage in restRoot:
///
///    restRoot
///    [0] REST_Detector_GenerateFibreReadout("LiquidO.yaml", "LiquidO_readout.root", "geometry/fibres.xml")
int REST_Detector_GenerateFibreReadout(const std::string& yamlFile, const std::string& outputFile,
                                       const std::string& gdmlFile = "",
                                       const std::string& decodingName = "nominal_decoding",
                                       bool checkOverlaps = false) {
    delete fibreReadout;
    fibreReadout = new TRestFibreReadout(yamlFile, "readout");

    try {
        fibreReadout->BuildGeometry();
    } catch (const std::exception& e) {
        std::cerr << "[-] Cannot build the readout: " << e.what() << std::endl;
        delete fibreReadout;
        fibreReadout = nullptr;
        return 1;
    }

    if (checkOverlaps && fibreReadout->CheckOverlaps() > 0) {
        std::cerr << "[-] Geometry overlaps in the readout " << fibreReadout->GetName() << std::endl;
        return 1;
    }

    std::unique_ptr<TFile> fOut(TFile::Open(outputFile.c_str(), "RECREATE"));
    if (!fOut || fOut->IsZombie()) {
        std::cerr << "[-] Cannot open the output file " << outputFile << std::endl;
        return 1;
    }
    if (!fibreReadout->Export(fOut.get(), fibreReadout->GetName(), decodingName)) {
        std::cerr << "[-] Cannot export the readout to " << outputFile << std::endl;
        return 1;
    }
    fOut->Close();

    if (!gdmlFile.empty()) {
        if (fibreReadout->WriteGDMLPlacements(gdmlFile)) {
            std::cout << "[+] " << fibreReadout->GetNumberOfFibres() << " fibre placements written to "
                      << gdmlFile << std::endl;
        } else {
            std::cerr << "[-] Cannot write the fibre placements to " << gdmlFile << std::endl;
            return 1;
        }
    }
    return 0;
}
