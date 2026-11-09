#ifndef TRESTDATASETTMVA_H
#define TRESTDATASETTMVA_H

#include <TGraph.h>
#include <TH1D.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "TRestDataSetProcess.h"

class TCanvas;

/// \class TRestDataSetTMVA
/// \brief Classifier of ROOT TMVA (a boosted decision tree by default) trained with a signal (the reference)
/// and a background dataset, adding its score to the dataset processed. See the class documentation in
/// TRestDataSetTMVA.cxx.
class TRestDataSetTMVA : public TRestDataSetProcess {
   private:
    /// ROC curve of the test sample (objects of the folder of the process)
    std::unique_ptr<TGraph> fROC;  //!
    /// Score distributions: score_signal, score_background (test), score_signal_train, score_background_train
    std::map<std::string, std::unique_ptr<TH1D>> fScores;  //!

   protected:
    /// \brief Trains the method with the reference (signal, with `cuts`) and the background dataset (with
    /// `backgroundCuts`), split in training and test samples (`splitOptions`): the weights and the results
    /// of the evaluation on the test sample
    void ComputeParameters(const TRestDataSet& reference, ROOT::RDF::RNode frame) override;
    /// \brief Reads the ROC curve and the score distributions
    void ReadObjects(TDirectory* directory) override;

   public:
    /// Observables (columns of the datasets), the input variables of the method
    std::vector<std::string> fObservables = {};
    /// Background dataset (a pattern must match a single dataset)
    std::string fBackgroundFileName = "";
    /// Names of the TRestCut of the manager applied to the background
    std::vector<std::string> fBackgroundCutNames = {};
    /// Method of TMVA (BDT, Likelihood, Fisher, MLP...)
    std::string fMethod = "BDT";
    /// Options of the method (TMVA string)
    std::string fOptions = "";
    /// Split of the samples in training and test events (options of PrepareTrainingAndTestTree)
    std::string fSplitOptions = "SplitMode=Random:NormMode=NumEvents:!V";

    /// Result: weights of the trained method (TMVA XML)
    std::string fWeights = "";
    /// Result: area under the ROC curve (test sample)
    double fROCIntegral = 0;
    /// Result: separation of the score between signal and background (test sample)
    double fSeparation = 0;
    /// Result: Kolmogorov test of the signal score between the training and test samples
    double fKSSignal = 0;
    /// Result: Kolmogorov test of the background score between the training and test samples
    double fKSBackground = 0;
    /// Result: separation of each observable between signal and background
    std::vector<double> fVariableSeparation = {};
    /// Result: importance of each observable in the BDT (empty for other methods)
    std::vector<double> fVariableImportance = {};

    TRestDataSetTMVA();
    /// \brief Process read from its YAML section (LoadConfig)
    TRestDataSetTMVA(const std::string& instanceName, const YAML::Node& node);
    ~TRestDataSetTMVA() override;

    std::string GetClassName() const override { return "TRestDataSetTMVA"; }
    /// \brief Reads the fields; throws without observables, without backgroundFileName or with an unknown
    /// method
    void LoadConfig() override;

    /// \brief Adds the column GetColumn() with the score of the method for each entry
    void Apply(TRestDataSet& dataSet) const override;
    /// \brief Writes the ROC curve (roc) and the score distributions
    void WriteObjects(TDirectory* directory) const override;

    /// \brief Draws the results in three pads: the ROC curve (with rocIntegral), the score distributions
    /// (test filled, training as points, normalized, with the Kolmogorov tests) and the separation of each
    /// observable (with the importance for a BDT). The objects drawn are copies owned by the pads.
    /// \param canvas Canvas to draw in (divided in three pads); nullptr: a new one
    /// \return The canvas
    TCanvas* DrawResults(TCanvas* canvas = nullptr) const;

    /// \brief Name of the column of the score, <name>_score
    std::string GetColumn() const { return fName + "_score"; }
};

#endif
