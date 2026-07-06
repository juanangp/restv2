#include <TROOT.h>
#include <TRint.h>
#include <TStyle.h>
#include <TSystem.h>

int main(int argc, char *argv[]) {

    printf("---------------------Welcome to RESTRoot--------------------\n");

    TRint theApp("App", &argc, argv);

    // Load REST libraries
    gSystem->Load("libRestTools.so");
    gSystem->Load("libRestCore.so");
    gSystem->Load("libRestRaw.so");
    gSystem->Load("libRestDetector.so");

    // Add REST headers to the ROOT include path
    if (const char *restPath = gSystem->Getenv("REST_PATH")) {
        TString incFlag = TString::Format(" -I%s/include", restPath);
        gSystem->AddIncludePath(incFlag);
    }

    gStyle->SetPalette(1);
    gStyle->SetTimeOffset(0);

    theApp.Run();

    return 0;
}
