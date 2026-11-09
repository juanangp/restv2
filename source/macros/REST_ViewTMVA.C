#include <TCanvas.h>

#include <exception>
#include <string>

#include "TRestDataSetTMVA.h"
#include "TRestLogManager.h"

/// \brief Draws the training results of a TRestDataSetTMVA stored in a dataset processed by it
/// (TRestDataSetTMVA::DrawResults): the ROC curve, the score distributions of the training and test samples
/// and the separation (and importance, for a BDT) of the observables.
///
/// \param fileName Dataset processed by a TRestDataSetTMVA.
/// \param name Name of the process (empty: the first TRestDataSetTMVA of the dataset).
/// \param outputFileName File where the canvas is saved (e.g. "bdt.pdf"; empty: not saved).
/// \return 0 on success, 1 if the process cannot be read from the file.
///
///    restRoot
///    REST_ViewTMVA("Dataset_Background_TMVA.root", "bdt")
///    REST_ViewTMVA("Dataset_Background_TMVA.root", "bdt", "bdt.pdf")
int REST_ViewTMVA(const std::string& fileName, const std::string& name = "",
                  const std::string& outputFileName = "") {
    TRestDataSetTMVA tmva;
    try {
        tmva.ReadFromFile(fileName, name);
    } catch (const std::exception& error) {
        RESTError << "REST_ViewTMVA: " << error.what() << RESTendl;
        return 1;
    }
    TCanvas* canvas = tmva.DrawResults();
    if (!outputFileName.empty()) canvas->Print(outputFileName.c_str());
    return 0;
}
