#include <TRint.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TROOT.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <TRestLogManager.h>
#include <TRestTools.h>

namespace {
bool InitializeRestRuntimeFromRestPath() {
    const char* restPathEnv = gSystem->Getenv("REST_PATH");
    if (!restPathEnv || restPathEnv[0] == '\0') {
        std::cerr << "restRoot: REST_PATH is not set.\n";
        return false;
    }

    const std::filesystem::path restPath(restPathEnv);
    const std::filesystem::path libDir = restPath / "lib";
    const std::filesystem::path includeDir = restPath / "include";

    if (!std::filesystem::exists(libDir) || !std::filesystem::is_directory(libDir)) {
        std::cerr << "restRoot: library directory not found: " << libDir << "\n";
        return false;
    }

    std::vector<std::filesystem::path> libraries;
    for (const auto& entry : std::filesystem::directory_iterator(libDir)) {
        if (!entry.is_regular_file()) continue;
        const auto fileName = entry.path().filename().string();
        if (fileName.rfind("libRest", 0) == 0 && entry.path().extension() == ".so") {
            if (fileName == "libRestG4.so") continue; 
            libraries.push_back(entry.path());
        }
    }

    std::sort(libraries.begin(), libraries.end());

    bool loadedAny = false;
    for (const auto& lib : libraries) {
        std::cout<<"Loading "<<lib<<std::endl;
        if (gSystem->Load(lib.string().c_str()) >= 0) {
            loadedAny = true;
        }
    }

    if (!loadedAny) {
        RESTError << "restRoot: no REST shared libraries could be loaded from " << libDir << RESTendl;
        return false;
    }

    if (std::filesystem::exists(includeDir) && std::filesystem::is_directory(includeDir)) {
        gSystem->AddIncludePath(TString::Format(" -I%s", includeDir.string().c_str()));
    }

    const std::filesystem::path macrosDir = restPath / "macros";
    if (std::filesystem::exists(macrosDir) && std::filesystem::is_directory(macrosDir)) {
        TString currentMacroPath = gROOT->GetMacroPath();
        gROOT->SetMacroPath(currentMacroPath + TString::Format(":%s", macrosDir.string().c_str()));

        for (const auto& entry : std::filesystem::directory_iterator(macrosDir)) {
            if (!entry.is_regular_file()) continue;
            
            if (entry.path().extension() == ".C") {
                gROOT->LoadMacro(entry.path().string().c_str());
            }
        }
    }


    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << "---------------------Welcome to RESTRoot--------------------\n";

    if (!InitializeRestRuntimeFromRestPath()) {
        return 1;
    }

    std::string fileToOpen = "";
    std::vector<char*> cleanArgv;

    for (int i = 0; i < argc; ++i) {
        std::string arg = argv[i];
        std::string fullPath = TRestTools::GetFullPath(arg);

        if (TRestTools::isValidTRestRun(fullPath)) {
            fileToOpen = fullPath; // Guardamos la ruta absoluta para nuestra macro
            continue;
        }
        
        cleanArgv.push_back(argv[i]);
    }

    cleanArgv.push_back(nullptr);

    int cleanArgc = static_cast<int>(cleanArgv.size()) - 1;
    char** cleanArgvPtr = cleanArgv.data();

    gStyle->SetPalette(1);
    gStyle->SetTimeOffset(0);

    TRint theApp("App", &cleanArgc, cleanArgvPtr);

    if (!fileToOpen.empty()) {
            std::string command = "REST_OpenInputFile(\"" + fileToOpen + "\");";
            gROOT->ProcessLine(command.c_str());
    }

    theApp.Run();
    return 0;
}
