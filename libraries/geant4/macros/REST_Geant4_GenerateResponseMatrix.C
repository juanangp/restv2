#ifndef REST_GEANT4_GENERATERESPONSEMATRIX_C
#define REST_GEANT4_GENERATERESPONSEMATRIX_C

#include <TH2D.h>

#include <ROOT/RDataFrame.hxx>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "TRestGeant4Metadata.h"
#include "TRestRun.h"
#include "TRestTools.h"

/// Response matrix of a detector simulation: for each file of `filePattern` (an analysis of a restG4
/// simulation, e.g. with TRestGeant4AnalysisProcess), the TH2D of varY (e.g. the energy detected) versus
/// varX (e.g. the primary energy) of the analysis tree entries that fulfill `cut` (an RDataFrame
/// expression, none if empty), with the binning `range` "(nBinsX, xLow, xHigh, nBinsY, yLow, yHigh)",
/// normalized to the bin size of both axes and to the events simulated (nEvents of the
/// TRestGeant4Metadata): the probability density of detecting y for a primary x. It is written as a binary
/// table of floats, one row per X bin with the Y bins (legacy TRestTools::ExportBinaryTable), in
/// <runTag>.N<nBinsY>f, and the overall efficiency (entries in the histogram / events simulated) is printed.
///
/// Legacy cut "g4Ana_boundingSize < 10 && g4Ana_containsProcessPhot > 0": in restv2 the photoelectric
/// events are those with energy in a selection with `processes: ["phot"]` of TRestGeant4AnalysisProcess,
/// e.g. "g4Ana_boundingSize < 10 && g4Ana_phot_Energy > 0".
///
///    restRoot
///    REST_Geant4_GenerateResponseMatrix("G4Analysis_*.root")
int REST_Geant4_GenerateResponseMatrix(const std::string& filePattern,
                                       const std::string& varX = "g4Ana_energyPrimary",
                                       const std::string& varY = "g4Ana_totalEdep",
                                       const std::string& range = "(150,0,15,150,0,15)",
                                       const std::string& cut = "") {
    int nBinsX = 0, nBinsY = 0;
    double xLow = 0, xHigh = 0, yLow = 0, yHigh = 0;
    if (std::sscanf(range.c_str(), " (%d , %lf , %lf , %d , %lf , %lf )", &nBinsX, &xLow, &xHigh, &nBinsY,
                    &yLow, &yHigh) != 6 ||
        nBinsX <= 0 || nBinsY <= 0 || xHigh <= xLow || yHigh <= yLow) {
        std::cerr << "REST_Geant4_GenerateResponseMatrix: range must be (nBinsX, xLow, xHigh, nBinsY, yLow, "
                     "yHigh)"
                  << std::endl;
        return 1;
    }
    const auto files = TRestTools::GetFilesMatchingPattern(filePattern);
    if (files.empty()) {
        std::cerr << "REST_Geant4_GenerateResponseMatrix: no file matches " << filePattern << std::endl;
        return 1;
    }
    for (const auto& fileName : files) {
        std::cout << "Reading file: " << fileName << std::endl;
        TRestRun run(fileName);
        const auto metadata = run.GetMetadataOfType<TRestGeant4Metadata>();
        if (metadata.empty() || metadata.front()->GetNumberOfEvents() <= 0) {
            std::cerr << "REST_Geant4_GenerateResponseMatrix: " << fileName
                      << " has no TRestGeant4Metadata with events" << std::endl;
            return 1;
        }
        const double events = metadata.front()->GetNumberOfEvents();

        ROOT::RDataFrame dataFrame("AnalysisTree", fileName);
        ROOT::RDF::RNode frame = dataFrame;
        if (!cut.empty()) frame = frame.Filter(cut);
        auto histogram =
            frame.Histo2D({"response", "Response;X;Y", nBinsX, xLow, xHigh, nBinsY, yLow, yHigh}, varX, varY);

        // Values in the units of the X and Y axes, per event simulated
        const double normX = (xHigh - xLow) / nBinsX;
        const double normY = (yHigh - yLow) / nBinsY;
        const std::string outputFile = run.GetRunTag() + ".N" + std::to_string(nBinsY) + "f";
        std::ofstream output(outputFile, std::ios::binary);
        for (int x = 1; x <= nBinsX; x++) {
            for (int y = 1; y <= nBinsY; y++) {
                const float value = histogram->GetBinContent(x, y) / normX / normY / events;
                output.write(reinterpret_cast<const char*>(&value), sizeof(value));
            }
        }
        std::cout << "Written binary table: " << outputFile << std::endl;
        std::cout << "Overall efficiency: " << histogram->Integral() / events << std::endl;
        std::cout << "Number of primaries: " << events << std::endl;
    }
    return 0;
}
#endif
