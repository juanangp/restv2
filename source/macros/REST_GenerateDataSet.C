#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "TRestCut.h"
#include "TRestDataSet.h"

/// Generates the dataset of the section `dataSetName` of a YAML file (TRestDataSet, see its documentation),
/// with the TRestCut of its `cuts` (sections of the same file), and writes it in outputFile (if empty, the
/// outputFileName of the dataset). The input files are its filePattern, or inputFiles if given.
///
///    restRoot
///    REST_GenerateDataSet("dataset.yaml", "dataSet")
int REST_GenerateDataSet(const std::string& yamlFile, const std::string& dataSetName = "dataSet",
                         const std::string& outputFile = "", const std::string& inputFiles = "") {
    try {
        TRestDataSet dataSet(yamlFile, dataSetName);
        dataSet.CheckConfig();
        if (!inputFiles.empty()) dataSet.fFilePattern = inputFiles;

        std::vector<std::unique_ptr<TRestCut>> cuts;
        std::vector<TRestCut*> cutPointers;
        for (const auto& cutName : dataSet.fCutNames) {
            cuts.push_back(std::make_unique<TRestCut>(yamlFile, cutName));
            cuts.back()->CheckConfig();
            cutPointers.push_back(cuts.back().get());
        }

        const std::string fileName = outputFile.empty() ? dataSet.fOutputFileName : outputFile;
        if (fileName.empty()) {
            std::cerr << "REST_GenerateDataSet: no output file (outputFileName of " << dataSetName << ")"
                      << std::endl;
            return 1;
        }

        dataSet.Generate(cutPointers);
        dataSet.Write(fileName);
        dataSet.PrintMetadata();
    } catch (const std::exception& error) {
        std::cerr << "REST_GenerateDataSet: " << error.what() << std::endl;
        return 1;
    }
    return 0;
}
