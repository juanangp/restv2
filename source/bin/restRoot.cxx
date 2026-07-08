#include <TRint.h>
#include <TStyle.h>
#include <TSystem.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

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
            libraries.push_back(entry.path());
        }
    }

    std::sort(libraries.begin(), libraries.end());

    bool loadedAny = false;
    for (const auto& lib : libraries) {
        if (gSystem->Load(lib.string().c_str()) >= 0) {
            loadedAny = true;
        }
    }

    if (!loadedAny) {
        std::cerr << "restRoot: no REST shared libraries could be loaded from " << libDir << "\n";
        return false;
    }

    if (std::filesystem::exists(includeDir) && std::filesystem::is_directory(includeDir)) {
        gSystem->AddIncludePath(TString::Format(" -I%s", includeDir.string().c_str()));
    }

    return true;
}
}  // namespace

int main(int argc, char* argv[]) {
    std::cout << "---------------------Welcome to RESTRoot--------------------\n";

    if (!InitializeRestRuntimeFromRestPath()) {
        return 1;
    }

    gStyle->SetPalette(1);
    gStyle->SetTimeOffset(0);

    TRint theApp("App", &argc, argv);
    theApp.Run();

    return 0;
}
