#ifndef REST_GEANT4_VIEWGEOMETRY_C
#define REST_GEANT4_VIEWGEOMETRY_C

#include <TEveGeoNode.h>
#include <TEveManager.h>
#include <TFile.h>
#include <TGeoManager.h>

#include <iostream>
#include <string>

#include "TRestGDMLParser.h"
#include "TRestTools.h"

/// Draws the geometry of a restG4 file (the TGeoManager "Geometry", as simulated) or of a GDML file (its
/// entities resolved, the parameters with their default values; see restG4 --update-gdml): with OpenGL by
/// default, with option "eve" in TEve (volumes of density <= 0.01 g/cm3 transparent, <= 0.001 g/cm3
/// hidden, as vacuum).
///
///    restRoot
///    REST_Geant4_ViewGeometry("Run1_NLDBD_Test.root")
///    REST_Geant4_ViewGeometry("geometry/setup.gdml", "eve")
int REST_Geant4_ViewGeometry(const std::string& fileName, const std::string& option = "") {
    TGeoManager* geometry = nullptr;
    if (TRestTools::isRootFile(fileName)) {
        TFile* file = TFile::Open(fileName.c_str(), "READ");
        if (file && !file->IsZombie()) file->GetObject("Geometry", geometry);
    } else if (fileName.find(".gdml") != std::string::npos) {
        // Entities resolved (TRestGDMLParser::Import), with the default values of its parameters
        geometry = TRestGDMLParser::Import(fileName);
    } else {
        std::cerr << "REST_Geant4_ViewGeometry: only .root and .gdml files" << std::endl;
        return 1;
    }
    if (!geometry || !geometry->GetTopVolume()) {
        std::cerr << "REST_Geant4_ViewGeometry: no geometry in " << fileName << std::endl;
        return 1;
    }

    if (option.empty()) {
        geometry->GetTopVolume()->Draw("ogl");
        return 0;
    }
    if (TRestTools::ToLower(option) != "eve") {
        std::cerr << "REST_Geant4_ViewGeometry: unknown option '" << option << "' (empty or eve)"
                  << std::endl;
        return 1;
    }
    auto* eve = TEveManager::Create();
    auto* topNode = new TEveGeoTopNode(geometry, geometry->GetTopNode());
    topNode->SetVisLevel(10);
    eve->AddGlobalElement(topNode);
    for (int i = 0; i < geometry->GetListOfVolumes()->GetEntries(); i++) {
        auto* volume = geometry->GetVolume(i);
        const double density = volume->GetMaterial()->GetDensity();
        if (density <= 0.01) {
            volume->SetTransparency(95);
            if (density <= 0.001) volume->SetVisibility(kFALSE);
        } else {
            volume->SetTransparency(50);
        }
    }
    eve->FullRedraw3D(kTRUE);
    return 0;
}
#endif
